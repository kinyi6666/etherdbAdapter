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
// HttpIngest — single HTTP server (see HttpIngest.h)
// ============================================================================
#include "HttpIngest.h"

#include "AdapterLog.h"

#include <net/Buffer.h>
#include <net/EventLoop.h>
#include <net/TcpConnection.h>
#include <net/TcpServer.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace EtherAdapter {

using EtherDB::Timestamp;
using EtherDB::Net::Buffer;
using EtherDB::Net::EventLoop;
using EtherDB::Net::InetAddress;
using EtherDB::Net::TcpConnectionPtr;
using EtherDB::Net::TcpServer;

namespace {

const size_t kMaxRequest = 1024 * 1024;   // 1 MiB request safety cap

// ---------------------------------------------------------------------------
// Tiny flat-JSON object parser.
//   Values: number / true / false / null / "string" (strings are ignored).
//   Keys are matched to data_proto_table field names; unmatched keys and
//   missing fields leave the cell NULL.
// Returns the number of matched keys, or -1 on a syntax error.
// ---------------------------------------------------------------------------
int parseJsonFields(const std::string& body, const FrameSpec& spec, Cell* cells) {
    const size_t n = body.size();
    size_t i = 0;

    auto skipWs = [&]() {
        while (i < n && (body[i] == ' ' || body[i] == '\t' || body[i] == '\r' || body[i] == '\n'))
            ++i;
    };
    auto parseString = [&](std::string& out) -> bool {
        if (i >= n || body[i] != '"') return false;
        ++i;
        out.clear();
        while (i < n) {
            const char c = body[i++];
            if (c == '\\') {
                if (i >= n) return false;
                const char e = body[i++];
                switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                default:  out.push_back(e);    break;
                }
            } else if (c == '"') {
                return true;
            } else {
                out.push_back(c);
            }
        }
        return false;
    };

    skipWs();
    if (i >= n || body[i] != '{') return -1;
    ++i;
    skipWs();
    if (i < n && body[i] == '}') return 0;   // "{}"

    int matched = 0;
    for (;;) {
        skipWs();
        std::string key;
        if (!parseString(key)) return -1;
        skipWs();
        if (i >= n || body[i] != ':') return -1;
        ++i;
        skipWs();

        // ── value ──
        int  fidx = -1;
        for (int k = 0; k < (int)spec.fields.size(); ++k) {
            if (spec.fields[k].name == key) { fidx = k; break; }
        }

        bool   isNull = false;
        double num = 0.0;
        bool   haveNum = false;

        if (i < n && body[i] == '"') {
            std::string ignored;
            if (!parseString(ignored)) return -1;   // string value → ignored
        } else if (body.compare(i, 4, "null") == 0) {
            i += 4;
            isNull = true;
        } else if (body.compare(i, 4, "true") == 0) {
            i += 4;
            num = 1.0; haveNum = true;
        } else if (body.compare(i, 5, "false") == 0) {
            i += 5;
            num = 0.0; haveNum = true;
        } else {
            char* end = nullptr;
            num = std::strtod(body.c_str() + i, &end);
            if (end == body.c_str() + i) return -1;
            i = (size_t)(end - body.c_str());
            haveNum = true;
        }

        if (fidx >= 0) {
            ++matched;
            Cell& c = cells[fidx];
            const FieldDesc& f = spec.fields[fidx];
            if (isNull || !haveNum) {
                c.isNull = 1;
            } else {
                c.isNull = 0;
                if (f.asDouble)                  c.v.d = num;
                else if (f.kind == FieldKind::Bool) c.v.i = (num != 0.0) ? 1 : 0;
                else                             c.v.i = (int64_t)std::llround(num);
            }
        }

        skipWs();
        if (i < n && body[i] == ',') { ++i; continue; }
        if (i < n && body[i] == '}') { ++i; break; }
        return -1;
    }
    return matched;
}

// Parse the request line + headers: method must be POST, returns the
// Content-Length (0 when absent).
bool parseHeaderBlock(const std::string& head, size_t* contentLen, std::string* method) {
    const size_t lineEnd = head.find("\r\n");
    if (lineEnd == std::string::npos) return false;

    const std::string line = head.substr(0, lineEnd);
    const size_t sp = line.find(' ');
    *method = (sp == std::string::npos) ? line : line.substr(0, sp);

    *contentLen = 0;
    size_t pos = lineEnd + 2;
    while (pos < head.size()) {
        size_t e = head.find("\r\n", pos);
        if (e == std::string::npos) e = head.size();
        const std::string h = head.substr(pos, e - pos);
        pos = e + 2;

        // case-insensitive "content-length:" prefix
        static const char kCL[] = "content-length:";
        if (h.size() >= sizeof(kCL) - 1) {
            bool match = true;
            for (size_t k = 0; k < sizeof(kCL) - 1; ++k) {
                if (std::tolower((unsigned char)h[k]) != kCL[k]) { match = false; break; }
            }
            if (match) {
                *contentLen = (size_t)std::strtoul(h.c_str() + sizeof(kCL) - 1, nullptr, 10);
            }
        }
    }
    return true;
}

const char* statusText(int code) {
    switch (code) {
    case 200: return "200 OK";
    case 400: return "400 Bad Request";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 413: return "413 Payload Too Large";
    default:  return "500 Internal Server Error";
    }
}

} // namespace

// ============================================================================
// HttpIngest
// ============================================================================
HttpIngest::HttpIngest(EventLoop* loop, const ConfigDB& db, const AdapterConfig& cfg,
                       WriteQueue* writeQueue, AdapterStats* stats)
    : _loop(loop), _db(db), _cfg(cfg), _writeQueue(writeQueue), _stats(stats) {}

HttpIngest::~HttpIngest() {
    stop();
}

bool HttpIngest::start(std::string* err) {
    uint16_t port = _cfg.http.port;
    if (port == 0) {
        const std::vector<uint16_t>& ports = _db.httpListenPorts();
        if (ports.empty()) {
            EA_LOG_INFO << "http ingest: no http(3) device configured — not started";
            return true;
        }
        port = ports.front();
        if (ports.size() > 1) {
            EA_LOG_WARN << "http ingest: multiple http ports configured ("
                        << ports.size() << ") — only ONE HTTP server is supported; using "
                        << port;
        } else {
            EA_LOG_INFO << "http ingest: using port " << port
                        << " (from device_table.local_server_port)";
        }
    }

    // Warn when the port collides with a raw/modbus listener.
    for (uint16_t p : _db.listenPorts()) {
        if (p == port) {
            EA_LOG_WARN << "http ingest: port " << port
                        << " is also a raw_data/modbus listen port — the HTTP server may fail to bind";
            break;
        }
    }

    InetAddress addr(port, false, false);
    _server.reset(new TcpServer(_loop, addr, "http-ingest"));
    _server->setConnectionCallback(
        [this](const TcpConnectionPtr& conn) { onConnection(conn); });
    _server->setMessageCallback(
        [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp t) {
            onMessage(conn, buf, t);
        });
    _server->start();
    _port = port;

    EA_LOG_INFO << "http ingest listening on 0.0.0.0:" << port
                << " (POST a flat JSON object; connection: close)";
    return true;
}

void HttpIngest::stop() {
    _server.reset();
    _conns.clear();
}

void HttpIngest::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        _conns[conn.get()] = ConnState();
    } else {
        _conns.erase(conn.get());
    }
}

void HttpIngest::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp t) {
    const size_t len = buf->readableBytes();
    if (len == 0) return;
    _stats->bytesIn.fetch_add(len, std::memory_order_relaxed);

    ConnState& st = _conns[conn.get()];
    st.buf.append(buf->peek(), len);
    buf->retrieveAll();

    if (st.buf.size() > kMaxRequest) {
        EA_LOG_WARN << "http: request from " << conn->peerAddress().toIpPort()
                    << " exceeds the 1 MiB cap — rejected";
        _stats->httpErrors.fetch_add(1, std::memory_order_relaxed);
        sendResponse(conn, 413, "{\"ok\":false}");
        return;
    }

    if (!st.headerParsed) {
        const size_t hdrEnd = st.buf.find("\r\n\r\n");
        if (hdrEnd == std::string::npos) return;   // wait for the full header block

        size_t contentLen = 0;
        std::string method;
        if (!parseHeaderBlock(st.buf.substr(0, hdrEnd), &contentLen, &method)) {
            _stats->httpErrors.fetch_add(1, std::memory_order_relaxed);
            sendResponse(conn, 400, "{\"ok\":false,\"error\":\"header\"}");
            return;
        }
        if (method != "POST") {
            _stats->httpErrors.fetch_add(1, std::memory_order_relaxed);
            sendResponse(conn, 405, "{\"ok\":false,\"error\":\"only POST\"}");
            return;
        }
        st.headerParsed = true;
        st.contentLen   = contentLen;
        st.buf.erase(0, hdrEnd + 4);
    }

    if (st.buf.size() < st.contentLen) return;   // wait for the body

    const std::string body = st.buf.substr(0, st.contentLen);
    handleRequest(conn, body, t);
}

void HttpIngest::handleRequest(const TcpConnectionPtr& conn, const std::string& body,
                               Timestamp t) {
    const DeviceDesc* dev = _db.matchHttpDevice(conn->peerAddress().toIp(),
                                                conn->peerAddress().toPort(), _port);
    if (!dev) {
        EA_LOG_WARN << "http: unknown peer " << conn->peerAddress().toIpPort()
                    << " (no matching device_table row for this HTTP port)";
        _stats->httpErrors.fetch_add(1, std::memory_order_relaxed);
        sendResponse(conn, 404, "{\"ok\":false,\"error\":\"unknown device\"}");
        return;
    }
    if (dev->spec.fields.empty()) {
        EA_LOG_WARN << "http device " << dev->deviceId
                    << ": no field layout (data_proto_table) — request rejected";
        _stats->httpErrors.fetch_add(1, std::memory_order_relaxed);
        sendResponse(conn, 400, "{\"ok\":false,\"error\":\"no field layout\"}");
        return;
    }

    RowBatch batch;
    batch.dev = dev;
    batch.ts.push_back(t.microSecondsSinceEpoch() / 1000);      // receive time
    batch.cells.assign((size_t)dev->fieldCount(), Cell());
    for (Cell& c : batch.cells) c.isNull = 1;                   // missing keys → NULL

    const int matched = parseJsonFields(body, dev->spec, batch.cells.data());
    if (matched < 0) {
        _stats->httpErrors.fetch_add(1, std::memory_order_relaxed);
        sendResponse(conn, 400, "{\"ok\":false,\"error\":\"json\"}");
        return;
    }

    _writeQueue->enqueue(std::move(batch));

    const uint64_t reqs = _stats->httpRequests.fetch_add(1, std::memory_order_relaxed) + 1;
    _stats->httpRows.fetch_add(1, std::memory_order_relaxed);
    if (reqs == 1 || reqs % 100 == 0) {
        EA_LOG_INFO << "http ingest: " << reqs << " request(s) accepted (last: "
                    << dev->deviceId << ", " << matched << "/" << dev->fieldCount()
                    << " field(s))";
    }

    sendResponse(conn, 200,
                 "{\"ok\":true,\"device\":\"" + dev->deviceId +
                 "\",\"matched\":" + std::to_string(matched) + "}");
}

void HttpIngest::sendResponse(const TcpConnectionPtr& conn, int code,
                              const std::string& body) {
    std::string out = "HTTP/1.1 ";
    out += statusText(code);
    out += "\r\nContent-Type: application/json\r\nContent-Length: ";
    out += std::to_string(body.size());
    out += "\r\nConnection: close\r\n\r\n";
    out += body;

    conn->send(out);
    conn->shutdown();   // close once the response is flushed
}

} // namespace EtherAdapter
