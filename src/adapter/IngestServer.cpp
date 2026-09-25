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
// etherAdapter — device data ingest (see IngestServer.h)
// ============================================================================
#include "IngestServer.h"

#include "AdapterLog.h"
#include <net/Buffer.h>
#include <net/EventLoop.h>
#include <net/TcpConnection.h>
#include <net/TcpServer.h>

#include <algorithm>

namespace EtherAdapter {

using EtherDB::Timestamp;
using EtherDB::Net::Buffer;
using EtherDB::Net::EventLoop;
using EtherDB::Net::InetAddress;
using EtherDB::Net::TcpConnection;
using EtherDB::Net::TcpConnectionPtr;
using EtherDB::Net::TcpServer;

IngestServer::IngestServer(EventLoop* loop, const ConfigDB& db,
                           const ServerConfig& cfg, IngestQueue* queue,
                           AdapterStats* stats)
    : _loop(loop), _db(db), _cfg(cfg), _queue(queue), _stats(stats) {}

IngestServer::~IngestServer() {
    stop();
}

bool IngestServer::start(std::string* err) {
    std::vector<uint16_t> ports = _db.listenPorts();
    ports.insert(ports.end(), _cfg.extraListenPorts.begin(), _cfg.extraListenPorts.end());
    std::sort(ports.begin(), ports.end());
    ports.erase(std::unique(ports.begin(), ports.end()), ports.end());

    if (ports.empty()) {
        if (err) {
            *err = "no listen ports: device_table.local_server_port is empty and "
                   "[server].extraListenPorts is not configured";
        }
        return false;
    }

    for (uint16_t port : ports) {
        InetAddress addr(port, false, false);   // 0.0.0.0:<port>
        std::string name = "ingest:" + std::to_string((unsigned)port);

        std::unique_ptr<TcpServer> srv(new TcpServer(_loop, addr, name));

        srv->setConnectionCallback(
            [this](const TcpConnectionPtr& conn) { onConnection(conn); });
        srv->setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp t) {
                onMessage(conn, buf, t);
            });

        srv->start();
        EA_LOG_INFO << "listening for device data on 0.0.0.0:" << (unsigned)port;
        _servers.push_back(std::move(srv));
    }
    return true;
}

void IngestServer::stop() {
    _servers.clear();
    _groupByConn.clear();
}

void IngestServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        const std::string ip = conn->peerAddress().toIp();
        const uint16_t port = conn->peerAddress().toPort();

        const DeviceGroup* group = _db.matchGroup(ip, port);
        _groupByConn[conn.get()] = group;
        _stats->connsOpened.fetch_add(1, std::memory_order_relaxed);

        if (group && group->primary()) {
            const DeviceDesc* dev = group->primary();
            EA_LOG_INFO << "device connected: " << dev->deviceId
                     << " (proto " << dev->connProto << ", " << group->members.size()
                     << " measurement set(s)) from " << ip << ":" << (unsigned)port;
        } else {
            EA_LOG_WARN << "unknown peer connected: " << ip << ":" << (unsigned)port
                     << " (no device_table row matches ip:port)";
        }
    } else {
        auto it = _groupByConn.find(conn.get());
        if (it != _groupByConn.end()) {
            if (it->second && it->second->primary())
                EA_LOG_INFO << "device disconnected: " << it->second->primary()->deviceId;
            _groupByConn.erase(it);
        }
    }
}

void IngestServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp t) {
    const size_t len = buf->readableBytes();
    if (len == 0) return;

    _stats->bytesIn.fetch_add(len, std::memory_order_relaxed);

    const DeviceGroup* group = nullptr;
    auto it = _groupByConn.find(conn.get());
    if (it != _groupByConn.end()) group = it->second;

    if (!group) {   // lazy re-match (peer port may not have been known at connect time)
        group = _db.matchGroup(conn->peerAddress().toIp(), conn->peerAddress().toPort());
        if (group) _groupByConn[conn.get()] = group;
    }

    const DeviceDesc* dev = group ? group->primary() : nullptr;
    const bool ingestable = dev &&
        (dev->connProto == CONN_CUSTOM_DATA || dev->connProto == CONN_MODBUS) &&
        !dev->spec.fields.empty();
    if (!ingestable) {
        const uint64_t n = _stats->unmatched.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n == 1 || n % 1000 == 0) {
            if (!dev) {
                EA_LOG_WARN << "dropping " << len << " byte(s) from unknown peer "
                            << conn->peerAddress().toIpPort() << " (unmatched chunks: " << n << ")";
            } else if (dev->connProto == CONN_HTTP || dev->connProto == CONN_MQTT) {
                EA_LOG_WARN << "dropping " << len << " byte(s) from " << dev->deviceId
                            << ": conn_proto " << dev->connProto
                            << " data is served by its own channel (http/mqtt), not this port "
                            << "(unmatched chunks: " << n << ")";
            } else {
                EA_LOG_WARN << "dropping " << len << " byte(s) from " << dev->deviceId
                            << ": no field layout for data_proto_id " << dev->dataProtoId
                            << " (unmatched chunks: " << n << ")";
            }
        }
        buf->retrieveAll();
        return;
    }

    RawChunk chunk;
    chunk.group  = group;
    chunk.recvMs = t.microSecondsSinceEpoch() / 1000;
    chunk.data.assign(buf->peek(), buf->peek() + len);
    buf->retrieveAll();

    _queue->enqueue(std::move(chunk));
    _stats->chunks.fetch_add(1, std::memory_order_relaxed);
}

} // namespace EtherAdapter
