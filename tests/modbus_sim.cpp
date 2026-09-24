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
// modbus_sim — modbus device simulator for end-to-end testing
//
// Simulates a modbus(2) device:
//   1. listens on the REQUEST port (device_table.device_port — the port the
//      adapter connects to for polling) for 12-byte register-read requests:
//        [transId 2][0x0000 2][length 2][unit 1][funCode 1][data 2][readCount 2]
//   2. builds the register response using the configured header mode
//      (9/7/2/0, matching data_header_table.frame_type) and pushes it to the
//      adapter's unified ingest port (device_table.local_server_port) — the
//      same way the real device uploads its data.
//
// Usage:
//   modbus_sim [options]
//     -r <requestPort>   port to listen for requests        (default 10004)
//     -h <host>          adapter host                       (default 127.0.0.1)
//     -s <serverPort>    unified ingest port for responses  (default 60382)
//     -n <count>         requests to serve                  (default 3)
//     -m <mode>          response header 9|7|2|0            (default 9)
//     -v <baseValue>     first register value               (default 1000)
//     -b <bindPort>      source port of the push link      (default 0 = ephemeral)
//     -B <bindIp>        source IP of the push link        (default: OS choice;
//                        e.g. 127.0.0.2 to simulate a device on its own IP)
//     -q                 quiet
//
// With the standard fixture (data_proto_table factor 0.1 on reg1/reg2) the
// stored values are (1000+k)*0.1 and (1100+k)*0.1 for the k-th poll.
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SockT;
const SockT kInvalidSock = INVALID_SOCKET;
#define CLOSE_SOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SockT;
const SockT kInvalidSock = -1;
#define CLOSE_SOCK close
#endif

static void put16(std::string& s, uint16_t v) {
    s.push_back((char)(v >> 8));
    s.push_back((char)(v & 0xFF));
}

static uint16_t get16(const char* p) {
    return (uint16_t)(((unsigned char)p[0] << 8) | (unsigned char)p[1]);
}

static bool sendAll(SockT s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = (int)send(s, data.data() + sent, (int)(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

int main(int argc, char* argv[]) {
    int requestPort = 10004;
    int serverPort  = 60382;
    int count       = 3;
    int mode        = 9;
    int baseValue   = 1000;
    int bindPort    = 0;               // push link source port (0 = ephemeral)
    std::string bindIp;                // push link source IP ("" = OS choice)
    std::string host = "127.0.0.1";
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) requestPort = atoi(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) host = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) serverPort = atoi(argv[++i]);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) count = atoi(argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) mode = atoi(argv[++i]);
        else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) baseValue = atoi(argv[++i]);
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) bindPort = atoi(argv[++i]);
        else if (strcmp(argv[i], "-B") == 0 && i + 1 < argc) bindIp = argv[++i];
        else if (strcmp(argv[i], "-q") == 0) quiet = true;
        else {
            printf("unknown option: %s (see source header for usage)\n", argv[i]);
            return 1;
        }
    }

    if (mode != 0 && mode != 2 && mode != 7 && mode != 9) {
        printf("invalid header mode %d (must be 0/2/7/9)\n", mode);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    SockT lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == kInvalidSock) {
        printf("ERROR: socket() failed\n");
        return 2;
    }
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)requestPort);
    if (bind(lfd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
        listen(lfd, 4) != 0) {
        printf("ERROR: cannot listen on request port %d\n", requestPort);
        CLOSE_SOCK(lfd);
        return 2;
    }

    printf("modbus_sim: listening for register requests on port %d "
           "(serving %d request(s), header mode %d, responses -> %s:%d from source port %d)\n",
           requestPort, count, mode, host.c_str(), serverPort, requestPort);

    // Persistent push link: the response connection is kept open (like a real
    // device). Serving several responses over one link also avoids TIME_WAIT
    // churn on the source port/4-tuple.
    SockT pushSock = kInvalidSock;
    auto closePush = [&]() {
        if (pushSock != kInvalidSock) { CLOSE_SOCK(pushSock); pushSock = kInvalidSock; }
    };
    auto ensurePush = [&]() -> bool {
        if (pushSock != kInvalidSock) return true;

        SockT s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == kInvalidSock) return false;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));

        if (!bindIp.empty() || bindPort != 0) {
            struct sockaddr_in laddr;
            memset(&laddr, 0, sizeof(laddr));
            laddr.sin_family = AF_INET;
            laddr.sin_port = htons((unsigned short)bindPort);
            if (bindIp.empty()) {
                laddr.sin_addr.s_addr = htonl(INADDR_ANY);
            } else if (inet_pton(AF_INET, bindIp.c_str(), &laddr.sin_addr) != 1) {
                printf("WARNING: invalid bind IP %s — using the OS choice\n", bindIp.c_str());
                laddr.sin_addr.s_addr = htonl(INADDR_ANY);
            }
            if (bind(s, (struct sockaddr*)&laddr, sizeof(laddr)) != 0) {
                printf("WARNING: cannot bind push source %s:%d — using the OS choice\n",
                       bindIp.c_str(), bindPort);
            }
        }

        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons((unsigned short)serverPort);
        if (inet_pton(AF_INET, host.c_str(), &saddr.sin_addr) != 1 ||
            connect(s, (struct sockaddr*)&saddr, sizeof(saddr)) != 0) {
            CLOSE_SOCK(s);
            return false;
        }
        pushSock = s;
        return true;
    };

    int served = 0;
    while (served < count) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        SockT c = accept(lfd, (struct sockaddr*)&peer, &plen);
        if (c == kInvalidSock) continue;

        // Serve several requests on the same link (like a real device keeps
        // the polling connection open).
        while (served < count) {
            // ── receive one 12-byte request ──
            char req[12];
            size_t got = 0;
            while (got < 12) {
                int n = (int)recv(c, req + got, (int)(12 - got), 0);
                if (n <= 0) break;
                got += (size_t)n;
            }
            if (got < 12) {
                if (got > 0) printf("modbus_sim: short request (%u byte(s)) — reconnecting\n", (unsigned)got);
                break;   // peer closed / error: go back to accept
            }

            const int k = served;
            const uint16_t tid       = get16(req + 0);
            const uint16_t protoId   = get16(req + 2);
            const uint16_t length    = get16(req + 4);
            const uint8_t  unit      = (uint8_t)req[6];
            const uint8_t  funCode   = (uint8_t)req[7];
            const uint16_t regAddr   = get16(req + 8);
            const uint16_t readCount = get16(req + 10);

            if (!quiet) {
                printf("modbus_sim: request #%d: tid=%u pid=%u len=%u unit=%u fun=%u addr=%u count=%u\n",
                       k, (unsigned)tid, (unsigned)protoId, (unsigned)length,
                       (unsigned)unit, (unsigned)funCode, (unsigned)regAddr, (unsigned)readCount);
            } else {
                printf("modbus_sim: request #%d served (tid=%u, %u register(s))\n",
                       k, (unsigned)tid, (unsigned)readCount);
            }

            // ── build the response ──
            const int regs = (readCount > 0 && readCount <= 64) ? (int)readCount : 2;
            const int dataBytes = regs * 2;

            std::string data;
            for (int i = 0; i < regs; ++i) {
                const uint16_t v = (uint16_t)((i == 0) ? (baseValue + k)
                                                       : (baseValue + 100 + i + k));
                data.push_back((char)(v >> 8));
                data.push_back((char)(v & 0xFF));
            }

            std::string resp;
            switch (mode) {
            case 9:   // MBAP + funCode + byteCount
                put16(resp, tid);
                put16(resp, 0x0000);
                put16(resp, (uint16_t)(3 + dataBytes));
                resp.push_back((char)unit);
                resp.push_back((char)funCode);
                resp.push_back((char)dataBytes);
                break;
            case 7:   // MBAP only
                put16(resp, tid);
                put16(resp, 0x0000);
                put16(resp, (uint16_t)(1 + dataBytes));
                resp.push_back((char)unit);
                break;
            case 2:   // funCode + byteCount
                resp.push_back((char)funCode);
                resp.push_back((char)dataBytes);
                break;
            default:  // 0: raw register bytes
                break;
            }
            resp += data;

            // ── push the data to the adapter's unified ingest port ──
            bool pushed = ensurePush() && sendAll(pushSock, resp);
            if (!pushed) {
                closePush();
                pushed = ensurePush() && sendAll(pushSock, resp);   // one reconnect attempt
            }
            if (pushed) {
                if (!quiet) {
                    printf("modbus_sim: response pushed (%u byte(s), header %d, reg[0] raw=%d)\n",
                           (unsigned)resp.size(), mode, baseValue + k);
                }
                ++served;
            } else {
                printf("ERROR: response push to %s:%d failed\n", host.c_str(), serverPort);
                break;   // retry the whole link on the next accept
            }
        }
        CLOSE_SOCK(c);
    }

    closePush();
    CLOSE_SOCK(lfd);
    printf("modbus_sim: done\n");
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
