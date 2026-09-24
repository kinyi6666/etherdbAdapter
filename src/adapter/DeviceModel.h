// ============================================================================
// etherAdapter — runtime device / protocol model
//
// Built from the SQLite configuration tables:
//   device_table      -> DeviceDesc
//   data_header_table -> FrameSpec (frame framing: flags + length + endian)
//   data_proto_table  -> FieldDesc list (one entry per sensor value)
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

// Connection protocol codes from device_table.conn_proto ("raw_data(1)" ...)
enum ConnProto {
    CONN_RAW_DATA = 1,
    CONN_MODBUS   = 2,
    CONN_HTTP     = 3,
    CONN_MQTT     = 4,
};

// ---------------------------------------------------------------------------
// One parsed value slot. Interpretation depends on FieldDesc::asDouble:
//   asDouble == false -> v.i (integer raw value, factor == 1)
//   asDouble == true  -> v.d (raw * factor)
// ---------------------------------------------------------------------------
struct Cell {
    union {
        int64_t i;
        double  d;
    } v;
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
    double      factor = 1.0;   // physical value = raw * factor
    int         precision = 0;  // decimal digits (informational)

    // ── materialization (computed when the config is loaded) ──
    uint8_t     outSize = 0;    // stored bytes per value (1/2/4/8)
    int         bindType = 0;   // EtDBStmt::BindType value (SDK enum)
    std::string colType;        // EtherDB column type for CREATE TABLE
    bool        asDouble = false; // factor != 1 -> scale to double on parse
};

// ---------------------------------------------------------------------------
// Frame framing description (data_header_table row)
// ---------------------------------------------------------------------------
struct FrameSpec {
    int     dataProtoId = 0;
    int     frameType   = 0;   // expected frame type byte (informational)
    int     frameLen    = 0;   // length value carried in the frame header
    uint8_t startFlag   = 0;   // 0 = accept any byte as frame start
    uint8_t endFlag     = 0;   // 0 = no tail byte
    int     endian      = 0;   // 0 = little, 1 = big (from the header row)
    uint32_t payloadMin = 0;   // max(field.byteOffset + size) — frame fit check

    std::vector<FieldDesc> fields;
};

// ---------------------------------------------------------------------------
// One device (device_table row) with its resolved frame protocol
// ---------------------------------------------------------------------------
struct DeviceDesc {
    int         channelId = 0;
    std::string deviceId;        // EtherDB table name (fallback: channel_<id>)
    std::string deviceName;
    std::string ip;              // device ip (peer address)
    uint16_t    devicePort = 0;  // device's bound source port (peer port)
    int         connProto  = 0;  // ConnProto value (1 = raw_data)
    uint16_t    localServerPort = 0;
    int         dataProtoId = 0;
    int         endian = 0;      // device-level override (0 = use header's)

    FrameSpec   spec;            // resolved protocol (by value)

    int  fieldCount() const { return (int)spec.fields.size(); }

    // Effective byte order for payload decoding.
    bool bigEndian() const {
        int e = (endian != 0) ? endian : spec.endian;
        return e != 0;
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
