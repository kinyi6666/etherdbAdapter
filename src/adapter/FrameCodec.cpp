// ============================================================================
// etherAdapter — frame codec (see FrameCodec.h)
// ============================================================================
#include "FrameCodec.h"

#include <algorithm>
#include <cstring>

namespace EtherAdapter {

FrameLenMode parseFrameLenMode(const std::string& text) {
    if (text == "total")   return FrameLenMode::Total;
    if (text == "payload") return FrameLenMode::Payload;
    return FrameLenMode::Auto;
}

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

enum class Probe { Ok, NeedMore, Corrupt };

// Decide which frame-length interpretation fits the bytes at `p`.
// Uses the end flag when configured AND the field extents (payloadMin), so a
// wrong guess is caught before any row is produced.
Probe probeFrame(const DeviceDesc& dev, const uint8_t* p, size_t avail,
                 FrameLenMode* outMode, size_t* outTotal) {
    const FrameSpec& spec = dev.spec;
    const bool be = dev.bigEndian();

    int lenVal = be ? (((int)p[2] << 8) | p[3]) : (p[2] | ((int)p[3] << 8));
    if (lenVal <= 0) lenVal = spec.frameLen;
    if (lenVal <= 0) return Probe::Corrupt;

    const size_t tail = spec.endFlag ? 1 : 0;
    const size_t minPayload = (size_t)spec.payloadMin;

    const size_t tTotal   = (size_t)lenVal;
    const size_t tPayload = FrameCodec::kHeaderSize + (size_t)lenVal + tail;

    const bool fitT = tTotal   >= FrameCodec::kHeaderSize + tail &&
                      tTotal   <= FrameCodec::kMaxFrame &&
                      (tTotal   - FrameCodec::kHeaderSize - tail) >= minPayload;
    const bool fitP = tPayload >= FrameCodec::kHeaderSize + tail &&
                      tPayload <= FrameCodec::kMaxFrame &&
                      (size_t)lenVal >= minPayload;

    if (spec.endFlag == 0) {
        // No tail marker available: trust "total" when it fits, else "payload".
        if (fitT) { *outMode = FrameLenMode::Total;   *outTotal = tTotal;   return Probe::Ok; }
        if (fitP) { *outMode = FrameLenMode::Payload; *outTotal = tPayload; return Probe::Ok; }
        return Probe::Corrupt;
    }

    if (fitT && avail >= tTotal && (uint8_t)p[tTotal - 1] == spec.endFlag) {
        *outMode = FrameLenMode::Total; *outTotal = tTotal; return Probe::Ok;
    }
    if (fitP && avail >= tPayload && (uint8_t)p[tPayload - 1] == spec.endFlag) {
        *outMode = FrameLenMode::Payload; *outTotal = tPayload; return Probe::Ok;
    }

    size_t need = 0;
    if (fitT) need = std::max(need, tTotal);
    if (fitP) need = std::max(need, tPayload);
    if (need == 0) return Probe::Corrupt;
    if (avail < need) return Probe::NeedMore;
    return Probe::Corrupt;
}

} // namespace

// ============================================================================
// decodeFrame — one frame -> one row
// ============================================================================
bool FrameCodec::decodeFrame(const DeviceDesc& dev, const char* frameBase,
                             size_t frameBytes, Cell* row) {
    const FrameSpec& spec = dev.spec;
    const int nFields = (int)spec.fields.size();
    if (nFields == 0) return false;

    const size_t tail = spec.endFlag ? 1 : 0;
    if (frameBytes < kHeaderSize + tail) return false;

    const uint8_t* payload = (const uint8_t*)frameBase + kHeaderSize;
    const size_t payloadLen = frameBytes - kHeaderSize - tail;

    if ((size_t)spec.payloadMin > payloadLen) return false;   // fields do not fit

    const bool be = dev.bigEndian();
    for (int i = 0; i < nFields; ++i) {
        const FieldDesc& f = spec.fields[i];
        if ((size_t)f.byteOffset + f.size > payloadLen) return false;

        const uint8_t* p = payload + f.byteOffset;
        switch (f.kind) {
        case FieldKind::Float32:
            row[i].v.d = loadFloat32(p, be) * f.factor;
            break;
        case FieldKind::Float64:
            row[i].v.d = loadFloat64(p, be) * f.factor;
            break;
        case FieldKind::Bool:
            row[i].v.i = (loadRaw(p, 1, be) != 0) ? 1 : 0;
            break;
        default: {
            int64_t raw = decodeInt(p, f, be);
            if (f.asDouble) row[i].v.d = (double)raw * f.factor;
            else            row[i].v.i = raw;
            break;
        }
        }
    }
    return true;
}

// ============================================================================
// feed — stream slicing
// ============================================================================
int FrameCodec::feed(const DeviceDesc& dev, DeviceStream& st,
                     const char* data, size_t len, int64_t recvMs, RowBatch& batch) {
    const FrameSpec& spec = dev.spec;
    st.bytesIn += len;

    if (spec.fields.empty()) {
        // No usable field layout: frames cannot become rows. Drop the bytes;
        // the warning was already emitted when the configuration was loaded.
        return 0;
    }

    st.buf.insert(st.buf.end(), data, data + len);
    if ((int)st.rowScratch.size() < dev.fieldCount())
        st.rowScratch.resize((size_t)dev.fieldCount());

    const bool be = dev.bigEndian();
    const size_t tail = spec.endFlag ? 1 : 0;

    int rows = 0;
    size_t pos = 0;
    const size_t n = st.buf.size();
    const uint8_t* base = (const uint8_t*)st.buf.data();

    while (true) {
        // ── 1. re-sync to the start flag ──
        if (spec.startFlag != 0) {
            size_t p = pos;
            while (p < n && base[p] != spec.startFlag) ++p;
            if (p != pos) { st.resyncs += (p - pos); pos = p; }
        }
        if (n - pos < kHeaderSize) break;

        const uint8_t* h = base + pos;

        // ── 2. frame length: probe once, then use the resolved mode ──
        size_t total = 0;
        if (!st.resolved) {
            FrameLenMode chosen = FrameLenMode::Auto;
            size_t chosenTotal = 0;
            Probe pr = probeFrame(dev, h, n - pos, &chosen, &chosenTotal);
            if (pr == Probe::NeedMore) break;
            if (pr == Probe::Corrupt) { ++pos; ++st.resyncs; continue; }
            st.lenMode = chosen;
            st.resolved = true;
            st.probeFlips = 0;
        }

        {
            int lenVal = be ? (((int)h[2] << 8) | h[3]) : (h[2] | ((int)h[3] << 8));
            if (lenVal <= 0) lenVal = spec.frameLen;
            if (lenVal <= 0) { ++pos; ++st.resyncs; continue; }

            total = (st.lenMode == FrameLenMode::Total)
                        ? (size_t)lenVal
                        : (kHeaderSize + (size_t)lenVal + tail);
            if (total > kMaxFrame || total < kHeaderSize + tail) {
                ++pos; ++st.resyncs; continue;
            }
        }

        if (n - pos < total) break;   // wait for more bytes

        // ── 3. decode one frame ──
        Cell* row = st.rowScratch.data();
        if (!decodeFrame(dev, (const char*)h, total, row)) {
            ++st.badFrames;
            // The resolved mode may be wrong (frame_len semantics differ from
            // the probe): flip once and keep going from this position.
            if (st.probeFlips < 1 && st.lenMode != FrameLenMode::Auto) {
                st.lenMode = (st.lenMode == FrameLenMode::Total)
                                 ? FrameLenMode::Payload : FrameLenMode::Total;
                ++st.probeFlips;
            }
            ++pos; ++st.resyncs;
            continue;
        }

        // End flag is informational once the mode is fixed (auto probing used
        // it to pick the mode); a mismatch does not invalidate the frame.
        if (spec.endFlag != 0 && base[pos + total - 1] != spec.endFlag)
            ++st.endFlagMiss;

        st.probeFlips = 0;
        ++st.frames;
        batch.pushRow(recvMs, row);
        ++rows;
        pos += total;
    }

    // ── 4. compact the stream buffer ──
    if (pos > 0)
        st.buf.erase(st.buf.begin(), st.buf.begin() + (std::ptrdiff_t)pos);
    if (st.buf.size() > 256 * 1024) {   // mis-sync / runaway peer: keep the tail
        st.buf.erase(st.buf.begin(), st.buf.end() - 256 * 1024);
        ++st.resyncs;
    }
    return rows;
}

} // namespace EtherAdapter
