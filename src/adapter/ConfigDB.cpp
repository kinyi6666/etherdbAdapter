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
// etherAdapter — SQLite configuration database reader (see ConfigDB.h)
// ============================================================================
#include "ConfigDB.h"

#include "AdapterLog.h"

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace EtherAdapter {

// ---------------------------------------------------------------------------
// EtDBStmt::BindType values (EtherDB client SDK, EtDBClient.h). Duplicated as
// plain integers because this translation unit must NOT include the SDK
// headers: they define EtherDB::Logger, which would collide with our logging.
// Keep in sync with EtDBStmt::BindType in the SDK.
// ---------------------------------------------------------------------------
enum BindType : int {
    kBindTimestamp = 0,
    kBindBool      = 1,
    kBindTinyInt   = 2,
    kBindSmallInt  = 3,
    kBindInt       = 4,
    kBindBigInt    = 5,
    kBindFloat     = 6,
    kBindDouble    = 7,
};

// ============================================================================
// Small SQLite row helpers — accept INTEGER / FLOAT / TEXT columns so that
// the tables may have been imported from CSV ("0x01", "73", "1.0", ...).
// ============================================================================
namespace {

int64_t readInt64(sqlite3_stmt* st, int col, int64_t dflt) {
    switch (sqlite3_column_type(st, col)) {
    case SQLITE_INTEGER: return sqlite3_column_int64(st, col);
    case SQLITE_FLOAT:   return (int64_t)sqlite3_column_double(st, col);
    case SQLITE_TEXT: {
        const char* t = (const char*)sqlite3_column_text(st, col);
        if (!t) return dflt;
        char* end = nullptr;
        long long v = strtoll(t, &end, 0);   // base 0: handles 0x..
        return (end != t) ? (int64_t)v : dflt;
    }
    default: return dflt;
    }
}

uint32_t readUInt32(sqlite3_stmt* st, int col, uint32_t dflt) {
    switch (sqlite3_column_type(st, col)) {
    case SQLITE_INTEGER: return (uint32_t)sqlite3_column_int64(st, col);
    case SQLITE_FLOAT:   return (uint32_t)sqlite3_column_double(st, col);
    case SQLITE_TEXT: {
        const char* t = (const char*)sqlite3_column_text(st, col);
        if (!t) return dflt;
        char* end = nullptr;
        unsigned long v = strtoul(t, &end, 0);
        return (end != t) ? (uint32_t)v : dflt;
    }
    default: return dflt;
    }
}

double readDouble(sqlite3_stmt* st, int col, double dflt) {
    switch (sqlite3_column_type(st, col)) {
    case SQLITE_FLOAT:   return sqlite3_column_double(st, col);
    case SQLITE_INTEGER: return (double)sqlite3_column_int64(st, col);
    case SQLITE_TEXT: {
        const char* t = (const char*)sqlite3_column_text(st, col);
        if (!t) return dflt;
        char* end = nullptr;
        double v = strtod(t, &end);
        return (end != t) ? v : dflt;
    }
    default: return dflt;
    }
}

std::string readText(sqlite3_stmt* st, int col) {
    const unsigned char* t = sqlite3_column_text(st, col);
    return t ? std::string((const char*)t) : std::string();
}

// "custom_data(1)" / "modbus (2)" / "4" -> protocol number (0 when unknown)
int parseConnProto(const std::string& text) {
    size_t l = text.find('(');
    size_t r = (l == std::string::npos) ? std::string::npos : text.find(')', l);
    if (l != std::string::npos && r != std::string::npos && r > l + 1) {
        int v = std::atoi(text.substr(l + 1, r - l - 1).c_str());
        if (v > 0) return v;
    }
    if (!text.empty() && text[0] >= '0' && text[0] <= '9')
        return std::atoi(text.c_str());
    return 0;
}

// ----------------------------------------------------------------------------
// Wire type codes for data_proto_table.field_type.
//
// NOTE: adjust this table if the real device protocol definition differs —
// everything else in the adapter is table-driven, so no other code changes.
//
//   code  kind                raw size
//    0    Bool                1
//    1    IntSigned  (INT16)  2
//    2    IntSigned  (INT32)  4
//    3    Float32             4
//    4    Float64             8
//    5    IntSigned  (INT8)   1
//    6    IntUnsigned (UINT8) 1
//    7    IntUnsigned (UINT16)2
//    8    IntUnsigned (UINT32)4
//    9    IntSigned  (INT64)  8
//   10    IntUnsigned (UINT64)8
// ----------------------------------------------------------------------------
bool mapFieldType(int code, FieldKind* kind, uint8_t* size) {
    switch (code) {
    case 0:  *kind = FieldKind::Bool;        *size = 1; return true;
    case 1:  *kind = FieldKind::IntSigned;   *size = 2; return true;
    case 2:  *kind = FieldKind::IntSigned;   *size = 4; return true;
    case 3:  *kind = FieldKind::Float32;     *size = 4; return true;
    case 4:  *kind = FieldKind::Float64;     *size = 8; return true;
    case 5:  *kind = FieldKind::IntSigned;   *size = 1; return true;
    case 6:  *kind = FieldKind::IntUnsigned; *size = 1; return true;
    case 7:  *kind = FieldKind::IntUnsigned; *size = 2; return true;
    case 8:  *kind = FieldKind::IntUnsigned; *size = 4; return true;
    case 9:  *kind = FieldKind::IntSigned;   *size = 8; return true;
    case 10: *kind = FieldKind::IntUnsigned; *size = 8; return true;
    default: return false;
    }
}

// ----------------------------------------------------------------------------
// Decide how one field is materialized into EtherDB:
//   outSize    stored bytes per value
//   bindType   EtDBStmt::BindType value (parameter binding)
//   colType    EtherDB column type for CREATE TABLE
//   asDouble   true only for FLOAT32/FLOAT64 (Cell::v.d), integers use v.i
//
// Unsigned integers are widened to the next signed type so no value can
// overflow the column. data_proto_table.factor is IGNORED: the database stores
// raw values (see the design notes), so no integer field is scaled to DOUBLE.
// ----------------------------------------------------------------------------
void materializeField(FieldDesc& f, int index) {
    f.name = makeIdentifier(f.name, "f" + std::to_string(index));

    switch (f.kind) {
    case FieldKind::Bool:
        f.asDouble = false;
        f.outSize  = 1;
        f.bindType = kBindBool;
        f.colType  = "BOOL";
        break;

    case FieldKind::Float32:
        f.asDouble = true;    // raw float value, stored as FLOAT
        f.outSize  = 4;
        f.bindType = kBindFloat;
        f.colType  = "FLOAT";
        break;

    case FieldKind::Float64:
        f.asDouble = true;
        f.outSize  = 8;
        f.bindType = kBindDouble;
        f.colType  = "DOUBLE";
        break;

    case FieldKind::IntSigned:
        f.asDouble = false;
        f.outSize  = f.size;
        switch (f.size) {
        case 1:  f.bindType = kBindTinyInt;  f.colType = "TINYINT";  break;
        case 2:  f.bindType = kBindSmallInt; f.colType = "SMALLINT"; break;
        case 4:  f.bindType = kBindInt;      f.colType = "INT";      break;
        default: f.bindType = kBindBigInt;   f.colType = "BIGINT";   break;
        }
        break;

    case FieldKind::IntUnsigned:
        f.asDouble = false;
        switch (f.size) {   // widen: no unsigned column type in the SDK bind API
        case 1:  f.outSize = 2; f.bindType = kBindSmallInt; f.colType = "SMALLINT"; break;
        case 2:  f.outSize = 4; f.bindType = kBindInt;      f.colType = "INT";      break;
        default: f.outSize = 8; f.bindType = kBindBigInt;   f.colType = "BIGINT";   break;
        }
        break;
    }
}

} // namespace

// ============================================================================
// makeIdentifier — see DeviceModel.h
// ============================================================================
std::string makeIdentifier(const std::string& raw, const std::string& fallback) {
    if (raw.empty()) return fallback;
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) return fallback;
    if (out[0] >= '0' && out[0] <= '9') out.insert(out.begin(), '_');
    return out;
}

// ============================================================================
// ConfigDB
// ============================================================================
ConfigDB::~ConfigDB() {
    close();
}

bool ConfigDB::open(const std::string& path, std::string* err) {
    close();
    int rc = sqlite3_open_v2(path.c_str(), &_db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        if (err) {
            *err = "cannot open config db '" + path + "': " +
                   (_db ? sqlite3_errmsg(_db) : "unknown error");
        }
        close();
        return false;
    }
    EA_LOG_INFO << "config db opened: " << path;
    return true;
}

void ConfigDB::close() {
    if (_db) {
        sqlite3_close(_db);
        _db = nullptr;
    }
}

bool ConfigDB::loadAll(std::string* err) {
    if (!_db) {
        if (err) *err = "config db not open";
        return false;
    }
    _devices.clear();
    _specs.clear();

    if (!loadHeaders(err)) return false;
    if (!loadFields(err))  return false;
    if (!loadDevices(err)) return false;

    rebuildIndexes();
    EA_LOG_INFO << "config loaded: " << _devices.size() << " device(s), "
             << _specs.size() << " protocol(s), "
             << _listenPorts.size() << " listen port(s)";
    return true;
}

bool ConfigDB::loadHeaders(std::string* err) {
    // Current schema carries the modbus request columns (slave_addr / fun_code /
    // start_addr / addr_num); older databases used unit / data / read_count and
    // may lack them entirely. Modbus polling is disabled when they are missing.
    const char* sqlNew =
        "SELECT data_proto_id, frame_type, frame_len, start_flag, end_flag, endian, "
        "       slave_addr, fun_code, start_addr, addr_num "
        "FROM data_header_table";
    const char* sqlLegacy =
        "SELECT data_proto_id, frame_type, frame_len, start_flag, end_flag, endian, "
        "       unit, fun_code, data, read_count "
        "FROM data_header_table";
    const char* sqlMinimal =
        "SELECT data_proto_id, frame_type, frame_len, start_flag, end_flag, endian "
        "FROM data_header_table";

    sqlite3_stmt* st = nullptr;
    bool withModbus = (sqlite3_prepare_v2(_db, sqlNew, -1, &st, nullptr) == SQLITE_OK);
    if (!withModbus) {
        if (sqlite3_prepare_v2(_db, sqlLegacy, -1, &st, nullptr) == SQLITE_OK) {
            withModbus = true;
            EA_LOG_WARN << "data_header_table uses the legacy modbus columns "
                        << "(unit/data/read_count); re-import the configuration to "
                        << "upgrade to slave_addr/start_addr/addr_num";
        } else if (sqlite3_prepare_v2(_db, sqlMinimal, -1, &st, nullptr) == SQLITE_OK) {
            EA_LOG_WARN << "data_header_table has no modbus request columns; "
                        << "modbus polling disabled";
        } else {
            if (err) *err = std::string("data_header_table: ") + sqlite3_errmsg(_db);
            return false;
        }
    }

    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        int pid = (int)readInt64(st, 0, -1);
        if (pid <= 0) continue;

        FrameSpec s;
        s.dataProtoId = pid;
        s.frameType   = (int)readInt64(st, 1, 0);
        s.frameLen    = (int)readInt64(st, 2, 0);
        s.startFlag   = (uint8_t)readUInt32(st, 3, 0);
        s.endFlag     = (uint8_t)readUInt32(st, 4, 0);
        s.endian      = (int)readInt64(st, 5, 0);
        if (withModbus) {
            s.slaveAddr = (int)readInt64(st, 6, 0);
            s.funCode   = (int)readInt64(st, 7, 0);
            s.startAddr = (int)readInt64(st, 8, -1);
            s.addrNum   = (int)readInt64(st, 9, 0);
        }
        if (s.startFlag != 0 && s.endFlag != 0 && s.startFlag != s.endFlag) {
            EA_LOG_INFO << "data_header_table: proto " << pid << " start_flag 0x"
                        << std::hex << (unsigned)s.startFlag << std::dec << " != end_flag 0x"
                        << std::hex << (unsigned)s.endFlag << std::dec
                        << "; using them as the leading / trailing event delimiter";
        }
        _specs[pid]   = std::move(s);
        ++n;
    }
    sqlite3_finalize(st);

    EA_LOG_INFO << "config: " << n << " frame header(s) loaded";
    return true;
}

bool ConfigDB::loadFields(std::string* err) {
    const char* sql =
        "SELECT data_proto_id, field_name, unit, field_type, byte_offset, "
        "       bit_offset, bit_len, precision, factor "
        "FROM data_proto_table ORDER BY data_proto_id";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        if (err) *err = std::string("data_proto_table: ") + sqlite3_errmsg(_db);
        return false;
    }

    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        int pid = (int)readInt64(st, 0, -1);
        if (pid <= 0) continue;

        FieldDesc f;
        f.name = readText(st, 1);
        f.unit = readText(st, 2);

        int code = (int)readInt64(st, 3, -1);
        FieldKind kind;
        uint8_t size = 0;
        if (!mapFieldType(code, &kind, &size)) {
            EA_LOG_WARN << "data_proto_table: unknown field_type " << code
                     << " for data_proto_id " << pid << ", field '"
                     << f.name << "' skipped";
            continue;
        }
        f.kind = kind;
        f.size = size;

        f.byteOffset = readUInt32(st, 4, 0);
        f.bitOffset  = (uint8_t)readUInt32(st, 5, 0);
        f.bitLen     = (uint8_t)readUInt32(st, 6, 0);
        f.precision  = (int)readInt64(st, 7, 0);
        const double factor = readDouble(st, 8, 1.0);
        if (factor != 0.0 && factor != 1.0) {
            EA_LOG_WARN << "data_proto_table: field '" << f.name << "' (proto " << pid
                        << ") has factor " << factor
                        << " — factors are ignored, raw values are stored";
        }

        if (f.bitLen > 0 && (int)f.bitOffset + (int)f.bitLen > (int)f.size * 8) {
            EA_LOG_WARN << "data_proto_table: field '" << f.name << "' (proto " << pid
                     << "): bit_offset+bit_len exceed the field width; bit extraction disabled";
            f.bitOffset = 0;
            f.bitLen    = 0;
        }

        FrameSpec& spec = _specs[pid];   // create a default spec when the header row is missing
        spec.dataProtoId = pid;
        materializeField(f, (int)spec.fields.size());
        uint32_t end = f.byteOffset + f.size;
        if (end > spec.payloadMin) spec.payloadMin = end;
        spec.fields.push_back(std::move(f));
        ++n;
    }
    sqlite3_finalize(st);

    EA_LOG_INFO << "config: " << n << " field(s) loaded";
    return true;
}

bool ConfigDB::loadDevices(std::string* err) {
    // Row identity is device_id (the channelID column was dropped from the
    // plant export); rows without an ip or device_id are annotations and skipped.
    const char* sql =
        "SELECT device_id, device_name, device_ip, device_port, "
        "       conn_proto, local_server_port, data_proto_id, endian "
        "FROM device_table";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        if (err) *err = std::string("device_table: ") + sqlite3_errmsg(_db);
        return false;
    }

    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const int rowIndex = n;   // fallback id for rows without a device_id

        std::string deviceId = readText(st, 0);
        std::string ip       = readText(st, 2);
        if (deviceId.empty() && ip.empty()) continue;      // comment/annotation row
        if (deviceId.empty()) {
            EA_LOG_WARN << "device_table: row " << (rowIndex + 1)
                        << " has no device_id; row skipped";
            continue;
        }
        if (ip.empty()) {
            EA_LOG_WARN << "device_table: device " << deviceId
                        << " has no device_ip; row skipped";
            continue;
        }

        DeviceDesc d;
        const std::string fallbackId = "device_" + std::to_string(rowIndex + 1);
        d.deviceId = makeIdentifier(deviceId, fallbackId);

        d.deviceName      = readText(st, 1);
        d.ip              = ip;
        d.devicePort      = (uint16_t)readUInt32(st, 3, 0);
        d.connProto       = parseConnProto(readText(st, 4));
        d.localServerPort = (uint16_t)readUInt32(st, 5, 0);
        d.dataProtoId     = (int)readInt64(st, 6, 0);
        d.endian          = (int)readInt64(st, 7, 0);

        if (d.connProto == 0) {
            EA_LOG_WARN << "device_table: device " << d.deviceId
                     << " has no usable conn_proto; assuming custom_data(1)";
            d.connProto = CONN_CUSTOM_DATA;
        }
        if (d.localServerPort == 0) {
            EA_LOG_WARN << "device_table: device " << d.deviceId
                     << " has no local_server_port; "
                     << "device will only be reachable on ports opened by other rows "
                     << "or [server].extraListenPorts";
        }

        if (const FrameSpec* s = spec(d.dataProtoId)) {
            d.spec = *s;
        } else {
            EA_LOG_WARN << "device " << d.deviceId << ": data_proto_id " << d.dataProtoId
                     << " is not defined in data_header_table / data_proto_table; "
                     << "frames from this device cannot be parsed";
        }

        _devices.push_back(std::move(d));
        ++n;
    }
    sqlite3_finalize(st);

    EA_LOG_INFO << "config: " << n << " device(s) loaded";
    return true;
}

// ============================================================================
// Peer groups + indexes
//
// Devices sharing one (ip, device_port) endpoint form ONE group so that a
// single TCP connection can carry the periodic STATUS sampling and one or more
// EVENT streams with different point counts — the frame type in the custom
// header selects the device (and therefore the EtherDB table). The first member
// (lowest data_proto_id) is the status device.
// ============================================================================
void ConfigDB::rebuildIndexes() {
    _groups.clear();
    _groupByIpPort.clear();
    _groupByIpUnique.clear();
    _listenPorts.clear();
    _httpListenPorts.clear();
    _httpByPort.clear();

    // ── 1. listen ports ──
    for (const DeviceDesc& d : _devices) {
        if (d.localServerPort == 0) continue;
        if (d.connProto == CONN_HTTP) {
            _httpListenPorts.push_back(d.localServerPort);
            _httpByPort.emplace(d.localServerPort, &d);   // first http device wins
        } else if (d.connProto == CONN_CUSTOM_DATA || d.connProto == CONN_MODBUS) {
            _listenPorts.push_back(d.localServerPort);
        }
        // other protocols (mqtt, ...) are not served yet
    }
    std::sort(_listenPorts.begin(), _listenPorts.end());
    _listenPorts.erase(std::unique(_listenPorts.begin(), _listenPorts.end()),
                       _listenPorts.end());
    std::sort(_httpListenPorts.begin(), _httpListenPorts.end());
    _httpListenPorts.erase(std::unique(_httpListenPorts.begin(), _httpListenPorts.end()),
                           _httpListenPorts.end());

    // ── 2. group devices by endpoint ──
    _groups.reserve(_devices.size());
    for (size_t i = 0; i < _devices.size(); ++i) {
        DeviceDesc& d = _devices[i];

        size_t gi = SIZE_MAX;
        if (d.devicePort != 0) {
            const std::string key = d.ip + ":" + std::to_string((unsigned)d.devicePort);
            auto it = _groupByIpPort.find(key);
            if (it != _groupByIpPort.end()) {
                gi = it->second;
            } else {
                gi = _groups.size();
                _groups.emplace_back();
                _groups[gi].key = key;
                _groupByIpPort.emplace(key, gi);
            }
        } else {
            // No source port configured (e.g. http): a standalone group, never
            // merged with another row, so the port-based match cannot collide.
            gi = _groups.size();
            _groups.emplace_back();
            _groups[gi].key = d.ip + ":0#" + std::to_string(i);
        }
        _groups[gi].members.push_back(&d);
    }

    // ── 3. rank members: status first, then events; wire up the routing ──
    for (DeviceGroup& g : _groups) {
        std::sort(g.members.begin(), g.members.end(),
                  [](const DeviceDesc* a, const DeviceDesc* b) {
                      if (a->dataProtoId != b->dataProtoId)
                          return a->dataProtoId < b->dataProtoId;
                      return a->deviceId < b->deviceId;
                  });

        const int baseProto = g.members.front()->dataProtoId;
        for (size_t k = 0; k < g.members.size(); ++k) {
            DeviceDesc* d = const_cast<DeviceDesc*>(g.members[k]);
            d->group      = &g;
            d->isEvent    = (k > 0);
            d->eventType  = d->isEvent ? (d->dataProtoId - baseProto) : 0;
            // Status data (the normal 1 Hz sampling) is submitted as soon as it
            // arrives; event bursts keep batching (they are high rate).
            d->immediateFlush = (!d->isEvent && d->connProto == CONN_CUSTOM_DATA);
        }
        if (g.members.size() > 1) {
            std::string ids;
            for (const DeviceDesc* m : g.members) {
                if (!ids.empty()) ids += ", ";
                ids += m->deviceId + "(" + std::to_string(m->spec.frameType) + ")";
            }
            EA_LOG_INFO << "endpoint " << g.key << " carries " << g.members.size()
                        << " measurement sets: " << ids
                        << " (first = status, rest = events)";
        }
    }

    // ── 4. ip-only fallback when exactly one group uses that ip ──
    std::unordered_map<std::string, int> ipGroups;
    for (const DeviceGroup& g : _groups) {
        const size_t colon = g.key.find(':');
        const std::string ip = g.key.substr(0, colon);
        ++ipGroups[ip];
    }
    for (size_t gi = 0; gi < _groups.size(); ++gi) {
        const std::string& key = _groups[gi].key;
        const std::string ip = key.substr(0, key.find(':'));
        if (ipGroups[ip] == 1) _groupByIpUnique[ip] = gi;
    }
}

const DeviceGroup* ConfigDB::matchGroup(const std::string& ip, uint16_t peerPort) const {
    auto it = _groupByIpPort.find(ip + ":" + std::to_string((unsigned)peerPort));
    if (it != _groupByIpPort.end()) return &_groups[it->second];

    auto it2 = _groupByIpUnique.find(ip);
    if (it2 != _groupByIpUnique.end()) return &_groups[it2->second];

    return nullptr;
}

const DeviceDesc* ConfigDB::matchDevice(const std::string& ip, uint16_t peerPort) const {
    const DeviceGroup* g = matchGroup(ip, peerPort);
    return g ? g->primary() : nullptr;
}

const DeviceDesc* ConfigDB::matchHttpDevice(const std::string& ip, uint16_t peerPort,
                                            uint16_t httpPort) const {
    if (const DeviceDesc* d = matchDevice(ip, peerPort)) {
        if (d->connProto == CONN_HTTP) return d;
    }
    auto it = _httpByPort.find(httpPort);
    return (it == _httpByPort.end()) ? nullptr : it->second;
}

} // namespace EtherAdapter
