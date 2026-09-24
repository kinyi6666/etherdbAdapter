// ============================================================================
// etherAdapter — frame codec: stream slicing + field decoding
//
// A raw_data frame is laid out as (see data_header_table):
//
//   [start_flag 1B][frame_type 1B][frame_len 2B][payload ...][end_flag 1B]
//
// `frame_len` may either count the whole frame ("total") or only the payload
// ("payload"); the auto mode probes each protocol once at runtime using the
// end flag AND the configured field extents (frame specs carry payloadMin).
//
// Field decoding is fully table-driven (byte_offset / bit_offset / bit_len /
// type / factor), little- or big-endian per device.
// ============================================================================
#ifndef ETHERADAPTER_FRAMECODEC_H
#define ETHERADAPTER_FRAMECODEC_H

#include "DeviceModel.h"
#include "Pipeline.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace EtherAdapter {

// How data_header_table.frame_len is interpreted ([parse].frameLenMode).
enum class FrameLenMode : uint8_t {
    Auto = 0,   // probe at runtime (default)
    Total,      // frame_len counts the whole frame
    Payload,    // frame_len counts the payload only
};

FrameLenMode parseFrameLenMode(const std::string& text);

// Per-device stream slicing state, owned by the parser thread.
struct DeviceStream {
    FrameLenMode lenMode  = FrameLenMode::Auto;
    bool         resolved = false;   // lenMode probed and fixed
    std::vector<char> buf;           // pending (unparsed) bytes
    std::vector<Cell> rowScratch;    // decode scratch, one row

    int probeFlips = 0;              // mode flips after a bad decode

    // statistics
    uint64_t frames      = 0;
    uint64_t bytesIn     = 0;
    uint64_t resyncs     = 0;   // bytes dropped while re-synchronizing
    uint64_t badFrames   = 0;
    uint64_t endFlagMiss = 0;   // frames whose tail byte != end_flag
};

class FrameCodec {
public:
    // Feed received bytes into the per-device stream and append every complete
    // frame as one row to `batch` (batch.dev must equal &dev, ts = recvMs).
    // Returns the number of rows appended.
    static int feed(const DeviceDesc& dev, DeviceStream& st,
                    const char* data, size_t len, int64_t recvMs, RowBatch& batch);

    // Decode the payload of one frame into `row` (must hold fieldCount cells).
    // `frameBytes` is the total frame size including header and end flag.
    // Returns false when the frame is malformed (field outside the payload...).
    static bool decodeFrame(const DeviceDesc& dev, const char* frameBase,
                            size_t frameBytes, Cell* row);

    static const size_t kHeaderSize = 4;   // start_flag + frame_type + len(2)
    static const size_t kMaxFrame   = 64 * 1024;
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_FRAMECODEC_H
