// ============================================================================
// etherAdapter — planned ingest channels (see IngestStubs.h)
// ============================================================================
#include "IngestStubs.h"

#include "AdapterLog.h"

namespace EtherAdapter {

HttpIngest::HttpIngest(const AdapterConfig& cfg) : _cfg(cfg) {}

bool HttpIngest::start(std::string*) {
    EA_LOG_INFO << "HTTP ingest: stub (conn_proto=http(3) devices are not served yet)";
    return true;
}

void HttpIngest::stop() {}

MqttIngest::MqttIngest(const AdapterConfig& cfg) : _cfg(cfg) {}

bool MqttIngest::start(std::string*) {
    EA_LOG_INFO << "MQTT ingest: stub (conn_proto=mttq(4) devices are not served yet)";
    return true;
}

void MqttIngest::stop() {}

ModbusIngest::ModbusIngest(const AdapterConfig& cfg) : _cfg(cfg) {}

bool ModbusIngest::start(std::string*) {
    EA_LOG_INFO << "MODBUS ingest: stub (conn_proto=modbus(2) devices are not served yet)";
    return true;
}

void ModbusIngest::stop() {}

} // namespace EtherAdapter
