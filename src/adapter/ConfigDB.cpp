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

// "raw_data(1)" / "modbus (2)" / "4" -> protocol number (0 when unknown)
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
//   asDouble   factor != 1 -> value stored as double (raw * factor)
//
// Unsigned integers are widened to the next signed type so no value can
// overflow the column. Integer fields with a scaling factor are stored as
// DOUBLE (the physical value is fractional).
// ----------------------------------------------------------------------------
void materializeField(FieldDesc& f, int index) {
    f.name = makeIdentifier(f.name, "f" + std::to_string(index));

    double factor = (f.factor == 0.0) ? 1.0 : f.factor;  // 0 factor means "unset"
    f.factor = factor;

    bool scaled = (factor != 1.0);
    if (scaled && f.kind != FieldKind::Float32 && f.kind != FieldKind::Float64 &&
        f.kind != FieldKind::Bool &&
        f.kind != FieldKind::IntSigned && f.kind != FieldKind::IntUnsigned) {
        scaled = false;
    }

    switch (f.kind) {
    case FieldKind::Bool:
        f.asDouble = false;
        f.outSize  = 1;
        f.bindType = kBindBool;
        f.colType  = "BOOL";
        break;

    case FieldKind::Float32:
        f.asDouble = true;    // physical value (raw * factor) stored as double
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
        if (scaled) {
            f.asDouble = true;
            f.outSize  = 8;
            f.bindType = kBindDouble;
            f.colType  = "DOUBLE";
        } else {
            f.asDouble = false;
            f.outSize  = f.size;
            switch (f.size) {
            case 1:  f.bindType = kBindTinyInt;  f.colType = "TINYINT";  break;
            case 2:  f.bindType = kBindSmallInt; f.colType = "SMALLINT"; break;
            case 4:  f.bindType = kBindInt;      f.colType = "INT";      break;
            default: f.bindType = kBindBigInt;   f.colType = "BIGINT";   break;
            }
        }
        break;

    case FieldKind::IntUnsigned:
        if (scaled) {
            f.asDouble = true;
            f.outSize  = 8;
            f.bindType = kBindDouble;
            f.colType  = "DOUBLE";
        } else {
            f.asDouble = false;
            switch (f.size) {   // widen: no unsigned column type in the SDK bind API
            case 1:  f.outSize = 2; f.bindType = kBindSmallInt; f.colType = "SMALLINT"; break;
            case 2:  f.outSize = 4; f.bindType = kBindInt;      f.colType = "INT";      break;
            default: f.outSize = 8; f.bindType = kBindBigInt;   f.colType = "BIGINT";   break;
            }
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
    const char* sql =
        "SELECT data_proto_id, frame_type, frame_len, start_flag, end_flag, endian "
        "FROM data_header_table";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        if (err) *err = std::string("data_header_table: ") + sqlite3_errmsg(_db);
        return false;
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
        f.factor     = readDouble(st, 8, 1.0);

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
    const char* sql =
        "SELECT channelID, device_id, device_name, device_ip, device_port, "
        "       conn_proto, local_server_port, data_proto_id, endian "
        "FROM device_table";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &st, nullptr) != SQLITE_OK) {
        if (err) *err = std::string("device_table: ") + sqlite3_errmsg(_db);
        return false;
    }

    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        int64_t ch = readInt64(st, 0, -1);
        if (ch <= 0) continue;   // header/comment rows

        std::string ip = readText(st, 3);
        if (ip.empty()) {
            EA_LOG_WARN << "device_table: channel " << ch << " has no device_ip; row skipped";
            continue;
        }

        DeviceDesc d;
        d.channelId = (int)ch;

        d.deviceId = readText(st, 1);
        if (d.deviceId.empty()) d.deviceId = "channel_" + std::to_string(ch);
        d.deviceId = makeIdentifier(d.deviceId, "channel_" + std::to_string(ch));

        d.deviceName      = readText(st, 2);
        d.ip              = ip;
        d.devicePort      = (uint16_t)readUInt32(st, 4, 0);
        d.connProto       = parseConnProto(readText(st, 5));
        d.localServerPort = (uint16_t)readUInt32(st, 6, 0);
        d.dataProtoId     = (int)readInt64(st, 7, 0);
        d.endian          = (int)readInt64(st, 8, 0);

        if (d.connProto == 0) {
            EA_LOG_WARN << "device_table: channel " << ch
                     << " has no usable conn_proto; assuming raw_data(1)";
            d.connProto = CONN_RAW_DATA;
        }
        if (d.localServerPort == 0) {
            EA_LOG_WARN << "device_table: channel " << ch << " has no local_server_port; "
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

void ConfigDB::rebuildIndexes() {
    _byIpPort.clear();
    _byIpUnique.clear();
    _listenPorts.clear();

    // Count devices per ip first (to know which ips map to exactly one device).
    std::unordered_map<std::string, int> ipCount;
    for (const DeviceDesc& d : _devices) {
        ++ipCount[d.ip];
        if (d.localServerPort != 0) _listenPorts.push_back(d.localServerPort);
    }

    for (size_t i = 0; i < _devices.size(); ++i) {
        const DeviceDesc& d = _devices[i];
        if (d.devicePort != 0) {
            std::string key = d.ip + ":" + std::to_string((unsigned)d.devicePort);
            auto it = _byIpPort.find(key);
            if (it == _byIpPort.end()) {
                _byIpPort.emplace(std::move(key), i);
            } else {
                EA_LOG_WARN << "device_table: duplicate ip:port " << d.ip << ":"
                         << (unsigned)d.devicePort << " (channels "
                         << _devices[it->second].channelId << " and " << d.channelId
                         << "); keeping the first";
            }
        }
        if (ipCount[d.ip] == 1) _byIpUnique[d.ip] = i;
    }

    std::sort(_listenPorts.begin(), _listenPorts.end());
    _listenPorts.erase(std::unique(_listenPorts.begin(), _listenPorts.end()),
                       _listenPorts.end());
}

const DeviceDesc* ConfigDB::matchDevice(const std::string& ip, uint16_t peerPort) const {
    auto it = _byIpPort.find(ip + ":" + std::to_string((unsigned)peerPort));
    if (it != _byIpPort.end()) return &_devices[it->second];

    auto it2 = _byIpUnique.find(ip);
    if (it2 != _byIpUnique.end()) return &_devices[it2->second];

    return nullptr;
}

} // namespace EtherAdapter
