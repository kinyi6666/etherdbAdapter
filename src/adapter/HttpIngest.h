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
// HttpIngest — ONE HTTP server for slow device data
//
// The listen port is [http].port, or (when 0) the local_server_port of the
// conn_proto=http(3) devices — exactly one server is started. Devices POST a
// flat JSON object whose keys are data_proto_table.field_name values:
//
//   POST / HTTP/1.1
//   Content-Length: 26
//   {"item1": 12.5, "item2": 7}
//
// Missing keys become NULL cells. Parsed rows go straight to the write queue;
// EtherDBWriter stores http rows through plain INSERT SQL (no prepared-
// statement binding — HTTP carries slow data only).
//
// Device match: peer (ip, port) first, then the http device registered for
// the server port.
// ============================================================================
#ifndef ETHERADAPTER_HTTPINGEST_H
#define ETHERADAPTER_HTTPINGEST_H

#include "AdapterConfig.h"
#include "ConfigDB.h"
#include "Pipeline.h"

#include <base/Timestamp.h>
#include <net/Callbacks.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace EtherDB {
namespace Net {
class EventLoop;
class TcpServer;
class TcpConnection;
class Buffer;
} // namespace Net
} // namespace EtherDB

namespace EtherAdapter {

class HttpIngest {
public:
    HttpIngest(EtherDB::Net::EventLoop* loop, const ConfigDB& db,
               const AdapterConfig& cfg, WriteQueue* writeQueue, AdapterStats* stats);
    ~HttpIngest();

    HttpIngest(const HttpIngest&) = delete;
    HttpIngest& operator=(const HttpIngest&) = delete;

    // Bind + listen. Returns true with *err unset when no http device is
    // configured (HTTP ingest is then simply not started).
    bool start(std::string* err);
    void stop();

    uint16_t port() const { return _port; }

private:
    struct ConnState {
        std::string buf;              // accumulated request bytes
        bool        headerParsed = false;
        size_t      contentLen = 0;
    };

    void onConnection(const EtherDB::Net::TcpConnectionPtr& conn);
    void onMessage(const EtherDB::Net::TcpConnectionPtr& conn,
                   EtherDB::Net::Buffer* buf, EtherDB::Timestamp t);
    void handleRequest(const EtherDB::Net::TcpConnectionPtr& conn,
                       const std::string& body, EtherDB::Timestamp t);
    void sendResponse(const EtherDB::Net::TcpConnectionPtr& conn, int code,
                      const std::string& body);

    EtherDB::Net::EventLoop* _loop;
    const ConfigDB&          _db;
    const AdapterConfig&     _cfg;
    WriteQueue*              _writeQueue;
    AdapterStats*            _stats;

    std::unique_ptr<EtherDB::Net::TcpServer> _server;
    std::unordered_map<const EtherDB::Net::TcpConnection*, ConnState> _conns;
    uint16_t _port = 0;
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_HTTPINGEST_H
