// ============================================================================
// etherAdapter — planned ingest channels (HTTP / MQTT / MODBUS) — stubs
//
// Design: "2. http 接入 暂时 stub / 3. mttq 订阅接入 暂时 stub".
// These classes exist so the startup sequence, configuration story and logs
// make the planned channels visible; no data path is implemented yet.
// ============================================================================
#ifndef ETHERADAPTER_INGESTSTUBS_H
#define ETHERADAPTER_INGESTSTUBS_H

#include "AdapterConfig.h"

#include <string>

namespace EtherAdapter {

// HTTP push ingest (device_table.conn_proto = "http(3)").
// Planned: an HTTP endpoint bound to the device's local_server_port which
// turns request bodies into RawChunks on the ingest queue.
class HttpIngest {
public:
    explicit HttpIngest(const AdapterConfig& cfg);
    bool start(std::string* err);   // stub: always succeeds
    void stop();
private:
    const AdapterConfig& _cfg;
};

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

// MODBUS ingest (device_table.conn_proto = "modbus(2)").
// Planned: a master that polls the devices in device_table with a schedule
// derived from data_proto_table, then feeds the ingest queue.
class ModbusIngest {
public:
    explicit ModbusIngest(const AdapterConfig& cfg);
    bool start(std::string* err);   // stub: always succeeds
    void stop();
private:
    const AdapterConfig& _cfg;
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_INGESTSTUBS_H
