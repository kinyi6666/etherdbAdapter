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
// ModbusPoller — periodic register-read requests for conn_proto=modbus(2)
// devices.
//
// One worker thread per device (the number of polled devices is small): the
// thread keeps a TCP link (SDK ETDB::Client::TcpClient) to the device's
// REQUEST port (device_table.device_port — the port the device LISTENS on for
// polling), sends a 12-byte register-read request every
// [modbus].pollIntervalMs, and discards anything the device pushes back on
// that link: response data is expected on the unified ingest port
// (IngestServer) and is parsed by ParserWorker.
//
// Request layout (see etherAdapter.txt design):
//   [transId 2B][0x0000 2B][length=6 2B][unit 1B][funCode 1B][data 2B][readCount 2B]
// transId cycles 1..0xFF per device (0xFF resets to 1 before use).
//
// All request fields come from data_header_table (unit / fun_code / data /
// read_count); a device without them is skipped with a warning.
// ============================================================================
#ifndef ETHERADAPTER_MODBUSPOLLER_H
#define ETHERADAPTER_MODBUSPOLLER_H

#include "AdapterConfig.h"
#include "ConfigDB.h"
#include "Pipeline.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace EtherAdapter {

class ModbusPoller {
public:
    ModbusPoller(const AdapterConfig& cfg, const ConfigDB& db, AdapterStats* stats);
    ~ModbusPoller();

    ModbusPoller(const ModbusPoller&) = delete;
    ModbusPoller& operator=(const ModbusPoller&) = delete;

    // Start one polling thread per modbus device that has a configured
    // register request. Always succeeds (devices with incomplete request
    // configuration are skipped with a warning).
    void start();
    void stop();

    int deviceCount() const { return (int)_sessions.size(); }

private:
    struct Session;

    void runDevice(Session* s);
    void drainIncoming(Session* s);
    void sleepMs(int ms);
    void closeSession(Session* s);   // caller must NOT hold s->mx

    const AdapterConfig& _cfg;
    const ConfigDB&      _db;
    AdapterStats*        _stats;

    std::vector<std::unique_ptr<Session>> _sessions;
    std::atomic<bool> _running{false};
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_MODBUSPOLLER_H
