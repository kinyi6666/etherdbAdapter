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
// etherAdapter — frame codec: stream slicing + field decoding
//
// custom_data frames carry the 6-byte custom header (see customHeader below):
//
//   [frameType 1B][flag 1B][frameLen 2B][sequenceId 2B][payload ...][flag 1B]
//
//   frameType   selects the measurement set of the frame: the frame_type of the
//               device_table row that owns the matching data_proto_id. Several
//               rows may share one (ip, device_port) endpoint — the first is
//               the periodic STATUS sampling, the others are EVENT streams with
//               a different (usually smaller) point count or a higher rate.
//               An event frame is logically ANOTHER device: it is parsed with
//               its own rules and written to its own EtherDB "event table".
//   flag        frame delimiter byte, meaningful for EVENT frames only: it is
//               what identifies the event parsing rule inside the shared TCP
//               stream (data_header_table.start_flag / end_flag; both normally
//               hold the same byte, but differing values are honoured as the
//               leading / trailing delimiter). Status frames ignore it.
//   frameLen    payload bytes carried by this frame, little endian. The payload
//               holds frameLen / data_header_table.frame_len records, so several
//               records can share one header (0 = exactly one record).
//   sequenceId  reserved for future use: read but never validated or decoded.
//
// Field decoding is fully table-driven (byte_offset / bit_offset / bit_len /
// field_type), little- or big-endian per device. data_proto_table.factor is
// ignored — the database stores raw values.
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

// Wire layout of the custom_data header (little endian on the wire).
#pragma pack(push, 1)
struct customHeader
{
    unsigned char frameType;  // measurement set selector (data_header_table.frame_type)
    unsigned char startFlag;  // frame delimiter: start_flag == end_flag
    uint16_t frameLen;        // payload bytes; records = frameLen / data_header_table.frame_len
    uint16_t sequenceId;      // reserved — not validated, not decoded
};
#pragma pack(pop)

// Per-device stream slicing state, owned by the parser thread.
struct DeviceStream {
    std::vector<char> buf;           // pending (unparsed) bytes
    std::vector<Cell> rowScratch;    // decode scratch, one record

    // statistics
    uint64_t frames      = 0;
    uint64_t bytesIn     = 0;
    uint64_t resyncs     = 0;   // bytes dropped while re-synchronizing
    uint64_t badFrames   = 0;
    uint64_t flagMiss    = 0;   // frames whose delimiter byte did not match
    uint64_t unknownType = 0;   // frames whose frame type matches no device
    uint64_t unknownTypeLogged = 0;   // already reported to the log
};

class FrameCodec {
public:
    // Feed received bytes into the per-endpoint stream and append every decoded
    // row to `sink` (the row's device follows from the frame type). Frames
    // carry 1..N records; each record becomes one row. Returns rows appended.
    static int feed(const DeviceGroup& group, DeviceStream& st,
                    const char* data, size_t len, int64_t recvMs, const RowSink& sink);

    // Feed received bytes for a MODBUS device (register-read responses arrive
    // on the unified ingest listener). The response header length comes from
    // frame_type: 9 = MBAP(7)+funCode+byteCount, 7 = MBAP only, 2 = funCode+
    // byteCount, 0 = raw register bytes. Registers decode big-endian unless
    // the endian configuration says little. Returns rows appended.
    static int feedModbus(const DeviceDesc& dev, DeviceStream& st,
                          const char* data, size_t len, int64_t recvMs, const RowSink& sink);

    // Decode ONE record's payload into `row` (must hold spec.fields.size()
    // cells). Returns false when the record is malformed (field outside the
    // payload). Shared by the custom_data and modbus paths.
    static bool decodePayload(const FrameSpec& spec, bool be, const char* payload,
                              size_t payloadLen, Cell* row);

    static const size_t kHeaderSize = sizeof(customHeader);   // 6 bytes
    static const size_t kMaxFrame   = 64 * 1024;
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_FRAMECODEC_H
