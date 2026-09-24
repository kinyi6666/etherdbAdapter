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
// etherAdapter — device data ingest (muduo TCP servers)
//
// One muduo TcpServer per listen port. Listen ports come from
// device_table.local_server_port (distinct values) plus the optional
// [server].extraListenPorts. Every received chunk is matched against
// device_table by (peer ip, peer port) and pushed into the ingest queue —
// frame slicing/parsing happens later on the single parser thread.
// ============================================================================
#ifndef ETHERADAPTER_INGESTSERVER_H
#define ETHERADAPTER_INGESTSERVER_H

#include "AdapterConfig.h"
#include "ConfigDB.h"
#include "Pipeline.h"

#include <base/Timestamp.h>
#include <net/Callbacks.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace EtherDB {
namespace Net {
class EventLoop;
class TcpServer;
class TcpConnection;
class Buffer;
} // namespace Net
} // namespace EtherDB

namespace EtherAdapter {

class IngestServer {
public:
    IngestServer(EtherDB::Net::EventLoop* loop, const ConfigDB& db,
                 const ServerConfig& cfg, IngestQueue* queue, AdapterStats* stats);
    ~IngestServer();

    IngestServer(const IngestServer&) = delete;
    IngestServer& operator=(const IngestServer&) = delete;

    // Bind + listen on all configured ports. Returns false (with *err) when
    // there is nothing to listen on.
    bool start(std::string* err);

    // Stop and destroy the servers (call on the loop thread).
    void stop();

private:
    void onConnection(const EtherDB::Net::TcpConnectionPtr& conn);
    void onMessage(const EtherDB::Net::TcpConnectionPtr& conn,
                   EtherDB::Net::Buffer* buf, EtherDB::Timestamp t);

    EtherDB::Net::EventLoop* _loop;
    const ConfigDB&          _db;
    ServerConfig             _cfg;
    IngestQueue*             _queue;
    AdapterStats*            _stats;

    std::vector<std::unique_ptr<EtherDB::Net::TcpServer>> _servers;
    // Connection -> device binding (loop-thread only, no locking needed).
    std::unordered_map<const EtherDB::Net::TcpConnection*, const DeviceDesc*> _devByConn;
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_INGESTSERVER_H
