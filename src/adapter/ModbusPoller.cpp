// Copyright (c) 2026 Liu jinwei <kinyi6666@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ============================================================================
// ModbusPoller — register-read polling (see ModbusPoller.h)
// ============================================================================
#include "ModbusPoller.h"

#include "AdapterLog.h"
#include "DeviceModel.h"

#include <tcpClient.h>   // ETDB::Client::TcpClient (SDK, cross-platform sockets)

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace EtherAdapter {

using ETDB::Client::TcpClient;
using ETDB::Client::TcpSocket;
using ETDB::Client::INVALID_TCP_SOCKET;

// ---------------------------------------------------------------------------
// Socket helpers. The connect path mirrors RTUConnector::connectServer from
// dcs 1.0: non-blocking connect + writability wait + SO_ERROR check, so a dead
// device never blocks the polling thread for the OS connect timeout.
// ---------------------------------------------------------------------------
namespace {

int lastSocketError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool connectInProgress(int e) {
#ifdef _WIN32
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEINVAL;
#else
    return e == EINPROGRESS;
#endif
}

// Non-blocking connect with a timeout; the socket is returned in BLOCKING mode
// (with a send timeout) so TcpClient::sendAll/recv can be used directly.
TcpSocket nonBlockingConnect(const std::string& ip, uint16_t port, int timeoutMs) {
    TcpSocket fd = TcpClient::create();
    if (!TcpClient::valid(fd)) return INVALID_TCP_SOCKET;

#ifdef _WIN32
    u_long nb = 1;
    if (ioctlsocket(fd, FIONBIO, &nb) != 0) { TcpClient::close(fd); return INVALID_TCP_SOCKET; }
#else
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        TcpClient::close(fd); return INVALID_TCP_SOCKET;
    }
#endif

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        TcpClient::close(fd);
        return INVALID_TCP_SOCKET;
    }

    int rc = ::connect(fd, (struct sockaddr*)&addr, (int)sizeof(addr));
    if (rc != 0) {
        const int err = lastSocketError();
        if (!connectInProgress(err)) { TcpClient::close(fd); return INVALID_TCP_SOCKET; }

        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec  = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
#ifdef _WIN32
        rc = ::select(0, nullptr, &wfds, nullptr, &tv);
#else
        rc = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
#endif
        if (rc <= 0) { TcpClient::close(fd); return INVALID_TCP_SOCKET; }

        int soErr = 0;
        socklen_t len = sizeof(soErr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&soErr, &len) != 0 || soErr != 0) {
            TcpClient::close(fd);
            return INVALID_TCP_SOCKET;
        }
    }

    // back to blocking mode + a send timeout, so a stalled peer cannot block
    // the polling thread indefinitely
#ifdef _WIN32
    u_long blk = 0;
    ioctlsocket(fd, FIONBIO, &blk);
    DWORD sndTimeout = 1000;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sndTimeout, sizeof(sndTimeout));
#else
    fcntl(fd, F_SETFL, flags);
    struct timeval sndtv;
    sndtv.tv_sec  = 1;
    sndtv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &sndtv, sizeof(sndtv));
#endif
    return fd;
}

} // namespace

// ---------------------------------------------------------------------------
// Per-device polling session
// ---------------------------------------------------------------------------
struct ModbusPoller::Session {
    const DeviceDesc* dev = nullptr;
    std::thread       thread;

    std::mutex mx;                                  // guards `fd`
    TcpSocket  fd = INVALID_TCP_SOCKET;

    uint16_t tid = 1;                               // transaction id (1..0xFE)
    int      failStreak = 0;                        // consecutive connect failures
    uint64_t sent = 0, conns = 0, errors = 0, pushedBack = 0;
};

// ---------------------------------------------------------------------------
// Request builder: 12 bytes, big endian, transId cycling 1..0xFE
//   [transId 2][0x0000 2][length=6 2][unit 1][funCode 1][data 2][readCount 2]
// ---------------------------------------------------------------------------
static void buildRequest(uint8_t out[12], uint16_t& tid, const FrameSpec& spec) {
    if (tid == 0xFF) tid = 1;
    const uint16_t t = tid++;
    out[0]  = (uint8_t)(t >> 8);
    out[1]  = (uint8_t)(t & 0xFF);
    out[2]  = 0x00;
    out[3]  = 0x00;                                    // protocol id
    out[4]  = 0x00;
    out[5]  = 0x06;                                    // length (fixed, see design)
    out[6]  = (uint8_t)(spec.unit & 0xFF);
    out[7]  = (uint8_t)(spec.funCode & 0xFF);
    out[8]  = (uint8_t)((spec.dataAddr >> 8) & 0xFF);
    out[9]  = (uint8_t)(spec.dataAddr & 0xFF);
    out[10] = (uint8_t)((spec.readCount >> 8) & 0xFF);
    out[11] = (uint8_t)(spec.readCount & 0xFF);
}

ModbusPoller::ModbusPoller(const AdapterConfig& cfg, const ConfigDB& db, AdapterStats* stats)
    : _cfg(cfg), _db(db), _stats(stats) {}

ModbusPoller::~ModbusPoller() {
    stop();
}

void ModbusPoller::start() {
    TcpClient::init();   // idempotent (WSAStartup on Windows)

    for (const DeviceDesc& d : _db.devices()) {
        if (d.connProto != CONN_MODBUS) continue;

        if (!d.spec.hasModbusRequest()) {
            EA_LOG_WARN << "modbus device " << d.deviceId << " (channel " << d.channelId
                        << "): no register request configured — set unit/fun_code/data/"
                        << "read_count in data_header_table for protocol " << d.dataProtoId
                        << "; device skipped by the poller";
            continue;
        }
        if (d.spec.fields.empty()) {
            EA_LOG_WARN << "modbus device " << d.deviceId
                        << ": no field layout — response parsing would be empty; skipped";
            continue;
        }
        if (!d.spec.isModbusHeaderLen()) {
            EA_LOG_WARN << "modbus device " << d.deviceId << ": frame_type " << d.spec.frameType
                        << " is not a valid response header length (0/2/7/9)";
            continue;
        }

        std::unique_ptr<Session> s(new Session());
        s->dev = &d;
        _sessions.push_back(std::move(s));
    }

    if (_sessions.empty()) {
        EA_LOG_INFO << "modbus poller: no device to poll";
        return;
    }

    _running.store(true, std::memory_order_release);
    for (auto& s : _sessions)
        s->thread = std::thread(&ModbusPoller::runDevice, this, s.get());

    EA_LOG_INFO << "modbus poller started: " << _sessions.size() << " device(s), interval "
                << _cfg.modbus.pollIntervalMs << " ms";
}

void ModbusPoller::stop() {
    const bool wasRunning = _running.exchange(false, std::memory_order_acq_rel);

    // Shut the links down under the mutex so blocked connects/sends/recvs in
    // the device threads return immediately. The threads own close().
    for (auto& s : _sessions) {
        std::lock_guard<std::mutex> lk(s->mx);
        if (TcpClient::valid(s->fd)) {
            TcpClient::shutdown(s->fd, 2);   // unblock
            TcpClient::close(s->fd);
            s->fd = INVALID_TCP_SOCKET;
        }
    }
    for (auto& s : _sessions) {
        if (s->thread.joinable()) s->thread.join();
    }

    if (wasRunning || !_sessions.empty()) {
        uint64_t sent = 0, errs = 0;
        for (auto& s : _sessions) { sent += s->sent; errs += s->errors; }
        EA_LOG_INFO << "modbus poller stopped (" << _sessions.size() << " device(s), "
                    << sent << " request(s) sent, " << errs << " error(s))";
    }
    _sessions.clear();
}

void ModbusPoller::closeSession(Session* s) {
    std::lock_guard<std::mutex> lk(s->mx);
    if (TcpClient::valid(s->fd)) {
        TcpClient::close(s->fd);
        s->fd = INVALID_TCP_SOCKET;
    }
}

void ModbusPoller::sleepMs(int ms) {
    while (ms > 0 && _running.load(std::memory_order_relaxed)) {
        const int step = (ms > 100) ? 100 : ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        ms -= step;
    }
}

// ============================================================================
// Device thread
// ============================================================================
void ModbusPoller::runDevice(Session* s) {
    const DeviceDesc& dev = *s->dev;
    const int intervalMs = _cfg.modbus.pollIntervalMs;

    while (_running.load(std::memory_order_relaxed)) {
        // ── ensure a connection ──
        bool connected;
        {
            std::lock_guard<std::mutex> lk(s->mx);
            connected = TcpClient::valid(s->fd);
        }

        if (!connected) {
            TcpSocket fd = nonBlockingConnect(dev.ip, dev.devicePort,
                                              _cfg.modbus.connectTimeoutMs);
            if (!TcpClient::valid(fd)) {
                ++s->errors;
                ++s->failStreak;
                if (s->errors == 1 || s->errors % 20 == 0) {
                    EA_LOG_WARN << "modbus device " << dev.deviceId << ": connect to "
                                << dev.ip << ":" << (unsigned)dev.devicePort << " failed ("
                                << s->errors << " failure(s))";
                }
                // Backoff mirrors RTUConnector (dcs 1.0): skip up to 8 cycles.
                const int backoff = std::min(s->failStreak * 2, 8);
                sleepMs(intervalMs * backoff);
                continue;
            }
            s->failStreak = 0;
            TcpClient::setNoDelay(fd);
            {
                std::lock_guard<std::mutex> lk(s->mx);
                s->fd = fd;
            }
            ++s->conns;
            EA_LOG_INFO << "modbus device " << dev.deviceId << ": connected to "
                        << dev.ip << ":" << (unsigned)dev.devicePort
                        << " (poll every " << intervalMs << " ms)";
        }

        if (!_running.load(std::memory_order_relaxed)) break;

        // ── send the register-read request ──
        uint8_t req[12];
        buildRequest(req, s->tid, dev.spec);

        int sentLen = 0;
        {
            std::lock_guard<std::mutex> lk(s->mx);
            if (TcpClient::valid(s->fd)) sentLen = TcpClient::sendAll(s->fd, req, 12);
        }
        if (sentLen != 12) {
            ++s->errors;
            EA_LOG_WARN << "modbus device " << dev.deviceId
                        << ": request send failed — reconnecting";
            closeSession(s);
            sleepMs(500);
            continue;
        }
        ++s->sent;
        _stats->modbusRequests.fetch_add(1, std::memory_order_relaxed);

        // ── discard anything pushed back on the request link ──
        drainIncoming(s);

        sleepMs(intervalMs);
    }

    closeSession(s);
    EA_LOG_INFO << "modbus device " << dev.deviceId << ": poller stopped ("
                << s->sent << " request(s), " << s->conns << " connect(s), "
                << s->pushedBack << " byte(s) discarded from the request link)";
}

// Discard everything the device sends back on the request link (the response
// data is expected on the unified ingest port instead). Never blocks.
void ModbusPoller::drainIncoming(Session* s) {
    for (;;) {
        TcpSocket fd;
        {
            std::lock_guard<std::mutex> lk(s->mx);
            fd = s->fd;
        }
        if (!TcpClient::valid(fd)) return;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
#ifdef _WIN32
        const int rc = ::select(0, &rfds, nullptr, nullptr, &tv);
#else
        const int rc = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
#endif
        if (rc <= 0) return;   // nothing readable (errors surface on the next send)

        char buf[1024];
        const int n = TcpClient::recv(fd, buf, (int)sizeof(buf), 0);
        if (n <= 0) {
            closeSession(s);   // peer closed / error
            return;
        }
        s->pushedBack += (uint64_t)n;
    }
}

} // namespace EtherAdapter
