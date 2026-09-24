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
// etherAdapter — planned ingest channel: MQTT subscribe — stub
//
// Design: "3. mttq 订阅接入 暂时 stub".
// (HTTP ingest lives in HttpIngest.*; MODBUS polling in ModbusPoller.*.)
// ============================================================================
#ifndef ETHERADAPTER_INGESTSTUBS_H
#define ETHERADAPTER_INGESTSTUBS_H

#include "AdapterConfig.h"

#include <string>

namespace EtherAdapter {

// MQTT subscribe ingest (device_table.conn_proto = "mttq(4)" / mqtt).
// Planned: subscribe to per-device topics on the broker, feed the ingest queue.
class MqttIngest {
public:
    explicit MqttIngest(const AdapterConfig& cfg);
    bool start(std::string* err);   // stub: always succeeds
    void stop();
private:
    const AdapterConfig& _cfg;
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_INGESTSTUBS_H
