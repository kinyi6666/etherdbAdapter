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
// etherAdapter — frame codec (see FrameCodec.h)
// ============================================================================
#include "FrameCodec.h"

#include "AdapterLog.h"

#include <algorithm>
#include <cstring>

namespace EtherAdapter {

namespace {

// Load `size` raw bytes as an integer honouring the byte order.
inline uint64_t loadRaw(const uint8_t* p, uint8_t size, bool be) {
    uint64_t v = 0;
    if (be) {
        for (uint8_t i = 0; i < size; ++i) v = (v << 8) | p[i];
    } else {
        for (uint8_t i = 0; i < size; ++i) v |= (uint64_t)p[i] << (8 * (uint32_t)i);
    }
    return v;
}

inline int64_t signExtend(uint64_t v, int bits) {
    if (bits >= 64) return (int64_t)v;
    uint64_t m = 1ull << (bits - 1);
    return (int64_t)((v ^ m) - m);
}

// Decode one integer field value (handles bit fields and signedness).
inline int64_t decodeInt(const uint8_t* p, const FieldDesc& f, bool be) {
    uint64_t v = loadRaw(p, f.size, be);
    if (f.bitLen > 0) {
        uint64_t mask = (f.bitLen >= 64) ? ~0ull : ((1ull << f.bitLen) - 1);
        v = (v >> f.bitOffset) & mask;
        return (f.kind == FieldKind::IntSigned) ? signExtend(v, f.bitLen) : (int64_t)v;
    }
    return (f.kind == FieldKind::IntSigned) ? signExtend(v, (int)f.size * 8) : (int64_t)v;
}

inline double loadFloat32(const uint8_t* p, bool be) {
    uint32_t bits = 0;
    std::memcpy(&bits, p, 4);
    if (be) {
        bits = ((bits & 0x000000FFu) << 24) | ((bits & 0x0000FF00u) << 8) |
               ((bits & 0x00FF0000u) >> 8)  | ((bits & 0xFF000000u) >> 24);
    }
    float f = 0.0f;
    std::memcpy(&f, &bits, 4);
    return (double)f;
}

inline double loadFloat64(const uint8_t* p, bool be) {
    uint64_t bits = 0;
    std::memcpy(&bits, p, 8);
    if (be) {
        bits = ((bits & 0x00000000000000FFull) << 56) |
               ((bits & 0x000000000000FF00ull) << 40) |
               ((bits & 0x0000000000FF0000ull) << 24) |
               ((bits & 0x00000000FF000000ull) << 8)  |
               ((bits & 0x000000FF00000000ull) >> 8)  |
               ((bits & 0x0000FF0000000000ull) >> 24) |
               ((bits & 0x00FF000000000000ull) >> 40) |
               ((bits & 0xFF00000000000000ull) >> 56);
    }
    double d = 0.0;
    std::memcpy(&d, &bits, 8);
    return d;
}

} // namespace

// ============================================================================
// decodePayload — one record's payload bytes -> one row
// (shared by the custom_data and modbus paths)
// ============================================================================
bool FrameCodec::decodePayload(const FrameSpec& spec, bool be, const char* payloadBase,
                               size_t payloadLen, Cell* row) {
    const uint8_t* payload = (const uint8_t*)payloadBase;
    const int nFields = (int)spec.fields.size();
    if (nFields == 0) return false;
    if ((size_t)spec.payloadMin > payloadLen) return false;   // fields do not fit

    for (int i = 0; i < nFields; ++i) {
        const FieldDesc& f = spec.fields[i];
        if ((size_t)f.byteOffset + f.size > payloadLen) return false;

        row[i].isNull = 0;
        const uint8_t* p = payload + f.byteOffset;
        switch (f.kind) {
        case FieldKind::Float32:
            row[i].v.d = loadFloat32(p, be);
            break;
        case FieldKind::Float64:
            row[i].v.d = loadFloat64(p, be);
            break;
        case FieldKind::Bool:
            row[i].v.i = (loadRaw(p, 1, be) != 0) ? 1 : 0;
            break;
        default: {
            const int64_t raw = decodeInt(p, f, be);
            if (f.asDouble) row[i].v.d = (double)raw;
            else            row[i].v.i = raw;
            break;
        }
        }
    }
    return true;
}

// ============================================================================
// feed — custom_data stream slicing
//
// One frame = custom header + payload + optional delimiter byte. The header
// carries the payload length and the frame type, so NO runtime probing is
// needed; the frame type picks the device (status table or one event table)
// inside the peer group and therefore the field layout to decode with.
// ============================================================================
int FrameCodec::feed(const DeviceGroup& group, DeviceStream& st,
                     const char* data, size_t len, int64_t recvMs, const RowSink& sink) {
    st.bytesIn += len;
    st.buf.insert(st.buf.end(), data, data + len);

    int rows = 0;
    size_t pos = 0;
    const size_t n = st.buf.size();
    const uint8_t* base = (const uint8_t*)st.buf.data();

    while (n - pos >= kHeaderSize) {
        const uint8_t* h = base + pos;
        const uint8_t frameType = h[0];
        const uint8_t flag      = h[1];
        const size_t  frameLen  = (size_t)h[2] | ((size_t)h[3] << 8);
        // h[4] / h[5] = sequenceId: reserved for future use, not validated.

        // ── 1. frame type -> device (status or event measurement set) ──
        const DeviceDesc* dev = group.byFrameType((int)frameType);
        if (!dev) {
            ++st.unknownType;
            ++pos; ++st.resyncs;
            continue;
        }
        const FrameSpec& spec = dev->spec;
        if (spec.fields.empty()) { ++pos; ++st.resyncs; continue; }

        // ── 2. delimiter bytes — EVENT frames only ──
        // data_header_table.start_flag / end_flag describe the event burst
        // boundaries: that is how a DIFFERENT parsing rule (logically another
        // device, with its own EtherDB table) is recognized inside one TCP
        // stream. Status frames carry no delimiter, so the byte is ignored.
        // start_flag and end_flag are normally the same byte; when they differ
        // the leading and trailing delimiters are taken separately.
        const uint8_t leadFlag  = dev->isEvent ? spec.startFlag : 0;
        const uint8_t trailFlag = dev->isEvent ? spec.endFlag   : 0;
        if (leadFlag != 0 && flag != leadFlag) {
            ++st.flagMiss;
            ++pos; ++st.resyncs;
            continue;
        }

        // ── 3. payload length: header value, 0 = one configured record ──
        size_t payloadBytes = frameLen;
        if (payloadBytes == 0)
            payloadBytes = (size_t)(spec.frameLen > 0 ? spec.frameLen : 0);
        if (payloadBytes == 0 || payloadBytes < (size_t)spec.payloadMin) {
            ++pos; ++st.resyncs;
            continue;
        }

        const size_t tail       = trailFlag ? 1 : 0;
        const size_t frameBytes = kHeaderSize + payloadBytes + tail;
        if (frameBytes > kMaxFrame) { ++pos; ++st.resyncs; continue; }
        if (n - pos < frameBytes) break;   // wait for more bytes

        // The trailing delimiter is informational once the length framed it.
        if (tail && base[pos + frameBytes - 1] != trailFlag) ++st.flagMiss;

        // ── 4. records: one header can carry several records ──
        size_t recordBytes = (spec.frameLen > 0) ? (size_t)spec.frameLen : payloadBytes;
        size_t records = 1;
        if (recordBytes > 0 && payloadBytes >= recordBytes &&
            payloadBytes % recordBytes == 0) {
            records = payloadBytes / recordBytes;
        }

        if (st.rowScratch.size() < (size_t)dev->fieldCount())
            st.rowScratch.resize((size_t)dev->fieldCount());

        bool ok = true;
        for (size_t k = 0; k < records; ++k) {
            const char* p = (const char*)(h + kHeaderSize) + k * recordBytes;
            if (!decodePayload(spec, dev->bigEndian(), p, recordBytes,
                               st.rowScratch.data())) {
                ok = false;
                break;
            }
            sink.push(dev, recvMs, st.rowScratch.data());
            ++rows;
        }
        if (ok) ++st.frames; else ++st.badFrames;
        pos += frameBytes;
    }

    // ── 5. compact the stream buffer ──
    if (pos > 0)
        st.buf.erase(st.buf.begin(), st.buf.begin() + (std::ptrdiff_t)pos);
    if (st.buf.size() > 256 * 1024) {   // mis-sync / runaway peer: keep the tail
        st.buf.erase(st.buf.begin(), st.buf.end() - 256 * 1024);
        ++st.resyncs;
    }
    return rows;
}

// ============================================================================
// feedModbus — register-read responses from conn_proto=modbus devices
//
// The response header length is carried by frame_type:
//   9 = [tid 2][pid 2][len 2][unit 1][funCode 1][byteCount 1] + registers
//   7 = [tid 2][pid 2][len 2][unit 1] + registers
//   2 = [funCode 1][byteCount 1] + registers
//   0 = raw register bytes (no header)
// Register data is big-endian by default (Model: 0/absent = big, 1 = little).
// Frames without an MBAP header (0/2) have no sync marker and are sliced at the
// fixed length  header + max(field byteOffset+size).
// ============================================================================
int FrameCodec::feedModbus(const DeviceDesc& dev, DeviceStream& st,
                           const char* data, size_t len, int64_t recvMs, const RowSink& sink) {
    const FrameSpec& spec = dev.spec;
    st.bytesIn += len;

    if (spec.fields.empty()) return 0;

    if (!spec.isModbusHeaderLen()) {
        if (st.badFrames++ == 0) {
            EA_LOG_ERROR << "modbus device " << dev.deviceId << ": frame_type "
                         << spec.frameType
                         << " is not a valid response header length (0/2/7/9); data dropped";
        }
        return 0;
    }

    const int hdr = spec.frameType;               // 0 / 2 / 7 / 9
    const size_t payloadNeed = (size_t)spec.payloadMin;
    if (payloadNeed == 0) {
        if (st.badFrames++ == 0) {
            EA_LOG_ERROR << "modbus device " << dev.deviceId
                         << ": no field layout — cannot slice responses";
        }
        return 0;
    }

    st.buf.insert(st.buf.end(), data, data + len);
    if ((int)st.rowScratch.size() < dev.fieldCount())
        st.rowScratch.resize((size_t)dev.fieldCount());

    const bool be = dev.modbusBigEndian();
    int rows = 0;
    size_t pos = 0;
    const size_t n = st.buf.size();
    const uint8_t* base = (const uint8_t*)st.buf.data();

    while (true) {
        if (hdr == 7 || hdr == 9) {
            // ── MBAP-framed response: re-sync on the protocol id (0x0000) ──
            size_t p = pos;
            while (p + 4 <= n) {
                if (base[p + 2] == 0 && base[p + 3] == 0) break;
                ++p;
            }
            if (p + 4 > n) {
                // Not enough bytes to complete a sync check: drop the scanned
                // prefix but keep the last 3 bytes (a split pid may complete).
                const size_t have = n - pos;
                const size_t keep = (have > 3) ? 3 : have;
                const size_t drop = have - keep;
                st.resyncs += drop;
                pos += drop;
                break;
            }
            if (p != pos) { st.resyncs += (p - pos); pos = p; }
            if (n - pos < 6) break;    // need the full MBAP prefix

            const size_t mbapLen = ((size_t)base[pos + 4] << 8) | base[pos + 5];
            if (mbapLen < 1) { ++pos; ++st.resyncs; continue; }

            const size_t total = 6 + mbapLen;
            if (total > kMaxFrame || total < (size_t)hdr) { ++pos; ++st.resyncs; continue; }
            if (n - pos < total) break;    // wait for more bytes

            const size_t payloadLen = total - (size_t)hdr;
            if (payloadLen < payloadNeed) { ++pos; ++st.resyncs; continue; }

            if (!decodePayload(spec, be, (const char*)base + pos + hdr, payloadLen,
                               st.rowScratch.data())) {
                ++st.badFrames; ++pos; ++st.resyncs; continue;
            }
            ++st.frames;
            sink.push(&dev, recvMs, st.rowScratch.data());
            ++rows;
            pos += total;
        } else {
            // ── hdr 0/2: fixed-length frames (header + configured payload) ──
            const size_t total = (size_t)hdr + payloadNeed;
            if (total > kMaxFrame) return rows;   // configuration problem
            if (n - pos < total) break;

            if (!decodePayload(spec, be, (const char*)base + pos + hdr, payloadNeed,
                               st.rowScratch.data())) {
                ++st.badFrames; ++pos; ++st.resyncs; continue;
            }
            ++st.frames;
            sink.push(&dev, recvMs, st.rowScratch.data());
            ++rows;
            pos += total;
        }
    }

    // ── compact the stream buffer ──
    if (pos > 0)
        st.buf.erase(st.buf.begin(), st.buf.begin() + (std::ptrdiff_t)pos);
    if (st.buf.size() > 256 * 1024) {   // mis-sync / runaway peer: keep the tail
        st.buf.erase(st.buf.begin(), st.buf.end() - 256 * 1024);
        ++st.resyncs;
    }
    return rows;
}

} // namespace EtherAdapter
