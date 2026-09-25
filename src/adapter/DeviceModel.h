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
// etherAdapter — runtime device / protocol model
//
// Built from the SQLite configuration tables:
//   device_table      -> DeviceDesc (grouped into DeviceGroup by ip:port)
//   data_header_table -> FrameSpec (framing + frame type + endian)
//   data_proto_table  -> FieldDesc list (one entry per sensor value)
//
// Devices that share one (ip, device_port) endpoint form ONE DeviceGroup: the
// first row is the periodic status device, further rows are event devices, and
// the custom header's frame type selects which one a frame belongs to. That is
// how a single TCP connection carries both the 1 Hz status sampling and an
// event burst with a different point count.
// ============================================================================
#ifndef ETHERADAPTER_DEVICEMODEL_H
#define ETHERADAPTER_DEVICEMODEL_H

#include <cstdint>
#include <string>
#include <vector>

namespace EtherAdapter {

// ---------------------------------------------------------------------------
// Raw (wire) field kinds, derived from data_proto_table.field_type
// ---------------------------------------------------------------------------
enum class FieldKind : uint8_t {
    IntSigned = 0,
    IntUnsigned,
    Float32,
    Float64,
    Bool,
};

// Connection protocol codes from device_table.conn_proto ("custom_data(1)" ...)
enum ConnProto {
    CONN_CUSTOM_DATA = 1,   // custom-header framed device data (old "raw_data")
    CONN_MODBUS      = 2,
    CONN_HTTP        = 3,
    CONN_MQTT        = 4,
};

// ---------------------------------------------------------------------------
// One parsed value slot. Interpretation depends on FieldDesc::asDouble:
//   asDouble == false -> v.i (raw integer value, stored as-is)
//   asDouble == true  -> v.d (raw FLOAT32/FLOAT64 value)
// isNull marks a missing value (HTTP JSON without that key).
//
// NOTE: data_proto_table.factor is IGNORED — the database stores raw values
// (see the design note in etherAdapter.txt).
// ---------------------------------------------------------------------------
struct Cell {
    union {
        int64_t i;
        double  d;
    } v;
    uint8_t isNull = 0;
};

// ---------------------------------------------------------------------------
// One sensor value definition (data_proto_table row) + materialization info
// ---------------------------------------------------------------------------
struct FieldDesc {
    // ── from data_proto_table ──
    std::string name;           // field_name (used as the EtherDB column name)
    std::string unit;
    FieldKind   kind = FieldKind::IntSigned;
    uint8_t     size = 0;       // raw byte width in the frame (1/2/4/8)
    uint32_t    byteOffset = 0; // offset into the frame payload
    uint8_t     bitOffset = 0;  // sub-byte extraction (bitLen == 0: whole bytes)
    uint8_t     bitLen = 0;
    int         precision = 0;  // decimal digits (informational only)

    // ── materialization (computed when the config is loaded) ──
    uint8_t     outSize = 0;    // stored bytes per value (1/2/4/8)
    int         bindType = 0;   // EtDBStmt::BindType value (SDK enum)
    std::string colType;        // EtherDB column type for CREATE TABLE
    bool        asDouble = false; // true for FLOAT32/FLOAT64 (v.d), integers use v.i
};

// ---------------------------------------------------------------------------
// Frame framing description (data_header_table row)
// ---------------------------------------------------------------------------
// data_header_table.frame_type selects WHICH measurement set a frame carries:
//   * status frame — the normal periodic sampling (typically 1 Hz, submitted
//     as soon as it arrives) -> the first device_table row for the ip:port;
//   * event frame  — a burst with a different (usually smaller) point count
//     and/or a higher sampling rate -> the matching event device row, whose
//     device_id names the EtherDB "event table".
struct FrameSpec {
    int     dataProtoId = 0;
    int     frameType   = 0;   // custom_data: frame type byte in the custom header
                               // modbus:      response header length (9/7/2/0)
    int     frameLen    = 0;   // custom_data: payload bytes of ONE record
    // Event burst delimiters — only meaningful for EVENT devices: they are how
    // a different parsing rule (logically another device, with its own table)
    // is recognized inside one TCP stream. Both columns normally hold the SAME
    // byte; when they differ the leading and trailing delimiters are taken
    // separately. 0 = no delimiter (status frames carry none).
    uint8_t startFlag   = 0;
    uint8_t endFlag     = 0;
    int     endian      = 0;   // 0 = little, 1 = big (from the header row)
    uint32_t payloadMin = 0;   // max(field.byteOffset + size) — frame fit check

    // ── Modbus register polling (data_header_table: slave_addr / fun_code /
    //    start_addr / addr_num). For conn_proto=modbus devices, frame_type
    //    carries the RESPONSE header length: 9 = MBAP(7)+funCode+byteCount,
    //    7 = MBAP only, 2 = funCode+byteCount, 0 = raw register bytes. ──
    int  slaveAddr = 0;    // modbus slave / unit id
    int  funCode   = 0;    // function code (e.g. 3 = read holding registers)
    int  startAddr = -1;   // register start address (-1 = polling not configured)
    int  addrNum   = 0;    // number of registers to read

    bool hasModbusRequest() const { return startAddr >= 0 && addrNum > 0; }
    bool isModbusHeaderLen() const {
        return frameType == 0 || frameType == 2 || frameType == 7 || frameType == 9;
    }

    std::vector<FieldDesc> fields;
};

// ---------------------------------------------------------------------------
// One device (device_table row) with its resolved frame protocol
// ---------------------------------------------------------------------------
struct DeviceGroup;   // one (ip, device_port) endpoint — see below

struct DeviceDesc {
    std::string deviceId;        // EtherDB table name (fallback: device_<n>)
    std::string deviceName;
    std::string ip;              // device ip (peer address)
    uint16_t    devicePort = 0;  // device's bound source port (peer port)
    int         connProto  = 0;  // ConnProto value (1 = custom_data)
    uint16_t    localServerPort = 0;
    int         dataProtoId = 0;
    int         endian = 0;      // device-level override (0 = use header's)

    FrameSpec   spec;            // resolved protocol (by value)

    // ── routing, filled in by ConfigDB::rebuildIndexes ──
    const DeviceGroup* group = nullptr;  // peers sharing (ip, device_port)
    bool isEvent = false;                // not the first row of its peer group
    int  eventType = 0;                  // data_proto_id - group base data_proto_id
    bool immediateFlush = false;         // status data: submit as soon as it arrives

    int  fieldCount() const { return (int)spec.fields.size(); }

    // Effective byte order for custom_data payload decoding.
    bool bigEndian() const {
        int e = (endian != 0) ? endian : spec.endian;
        return e != 0;
    }

    // Byte order for Modbus register data: big endian by default (the Modbus
    // protocol is big-endian); endian == 1 explicitly selects little endian.
    bool modbusBigEndian() const {
        int e = (endian != 0) ? endian : spec.endian;
        return e != 1;
    }
};

// ---------------------------------------------------------------------------
// All device_table rows sharing one (ip, device_port) endpoint. The FIRST
// member (lowest data_proto_id) is the status device; the remaining members are
// event devices told apart by data_header_table.frame_type in the custom
// header. This is how ONE TCP connection carries both the periodic status
// sampling and an event burst with a different point count.
// ---------------------------------------------------------------------------
struct DeviceGroup {
    std::string key;                            // "ip:port"
    std::vector<const DeviceDesc*> members;      // [0] = status (default device)

    const DeviceDesc* primary() const {
        return members.empty() ? nullptr : members.front();
    }

    // Device whose data_header_table.frame_type equals `ft` (nullptr = unknown).
    const DeviceDesc* byFrameType(int ft) const {
        for (const DeviceDesc* d : members) {
            if (d->spec.frameType == ft) return d;
        }
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Make a safe SQL identifier out of a raw config string:
//   [A-Za-z0-9_] kept, everything else becomes '_', leading digit gets '_'
//   prefix; empty input returns `fallback`.
std::string makeIdentifier(const std::string& raw, const std::string& fallback);

} // namespace EtherAdapter

#endif // ETHERADAPTER_DEVICEMODEL_H
