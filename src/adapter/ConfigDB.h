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
// etherAdapter — SQLite configuration database reader
//
// Reads the three configuration tables and builds the runtime registry
// (devices + protocol frame specs). The database is opened READ-ONLY: it is
// intended to be maintained by the plant tooling (or the csv2sqlite helper).
//
//   device_table      : per-device configuration (ip/port/protocol ids...)
//   data_header_table : framing + frame type per data_proto_id
//   data_proto_table  : sensor field layout per data_proto_id
//
// Device rows sharing one (ip, device_port) are grouped into a DeviceGroup:
// the first row is the status device, further rows are event devices selected
// by the frame type carried in the custom_data header.
// ============================================================================
#ifndef ETHERADAPTER_CONFIGDB_H
#define ETHERADAPTER_CONFIGDB_H

#include "AdapterConfig.h"
#include "DeviceModel.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace EtherAdapter {

class ConfigDB {
public:
    ConfigDB() = default;
    ~ConfigDB();

    ConfigDB(const ConfigDB&) = delete;
    ConfigDB& operator=(const ConfigDB&) = delete;

    // Open the configuration database (read-only).
    bool open(const std::string& path, std::string* err);
    void close();

    // Load all three tables and build the runtime registry.
    bool loadAll(std::string* err);

    const std::vector<DeviceDesc>& devices() const { return _devices; }

    // All peer groups (one per distinct "ip:device_port" endpoint, plus one
    // standalone group per device without a source port — e.g. http).
    const std::vector<DeviceGroup>& groups() const { return _groups; }

    // Distinct (non-zero) local_server_port values of the custom_data + modbus
    // devices — these are served by IngestServer (the unified TCP listener).
    const std::vector<uint16_t>& listenPorts() const { return _listenPorts; }

    // Distinct local_server_port values of the http(3) devices — a SINGLE HTTP
    // server is started on the first port reported here.
    const std::vector<uint16_t>& httpListenPorts() const { return _httpListenPorts; }

    // Protocol spec lookup by data_proto_id (nullptr when absent).
    const FrameSpec* spec(int dataProtoId) const {
        auto it = _specs.find(dataProtoId);
        return it == _specs.end() ? nullptr : &it->second;
    }

    // Device lookup used by the ingest path:
    //   1. exact (ip, source port) match
    //   2. ip-only match when the ip maps to exactly one group
    // Returns nullptr when the peer cannot be matched.
    const DeviceGroup* matchGroup(const std::string& ip, uint16_t peerPort) const;

    // Primary (status) device of the matching group; used by the http and
    // modbus paths where a single device is expected.
    const DeviceDesc* matchDevice(const std::string& ip, uint16_t peerPort) const;

    // Device lookup for HTTP requests: the peer match above must resolve to an
    // http device, otherwise the http device registered for `httpPort` is used
    // (the single HTTP server serves exactly one http device per port).
    const DeviceDesc* matchHttpDevice(const std::string& ip, uint16_t peerPort,
                                      uint16_t httpPort) const;

private:
    bool loadHeaders(std::string* err);
    bool loadFields(std::string* err);
    bool loadDevices(std::string* err);
    void rebuildIndexes();

    sqlite3* _db = nullptr;

    std::vector<DeviceDesc>            _devices;
    std::unordered_map<int, FrameSpec> _specs;       // data_proto_id -> spec
    std::vector<uint16_t>              _listenPorts;     // custom_data + modbus ports
    std::vector<uint16_t>              _httpListenPorts; // http ports

    // Peer groups: devices sharing one (ip, device_port).
    std::vector<DeviceGroup>               _groups;
    std::unordered_map<std::string, size_t> _groupByIpPort;   // "ip:port" -> index
    std::unordered_map<std::string, size_t> _groupByIpUnique; // "ip" -> index if unique
    std::unordered_map<uint16_t, const DeviceDesc*> _httpByPort; // http port -> device
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_CONFIGDB_H
