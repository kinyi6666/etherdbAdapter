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
// etherAdapter — pipeline data types and lock-free queues
//
//   network thread ──RawChunk──> ingest queue ──> parser thread
//   parser thread  ──RowBatch──> write queue  ──> EtherDB writer thread
//   parser thread  ──RowBatch──> publish queue──> ZMQ PUB (reserved / stub)
//
// Queues are moodycamel (the same header the EtherDB server uses), tuned with
// larger blocks + block recycling and thread-local producer/consumer tokens.
// ============================================================================
#ifndef ETHERADAPTER_PIPELINE_H
#define ETHERADAPTER_PIPELINE_H

#include "DeviceModel.h"

#include <base/blockingconcurrentqueue.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace EtherAdapter {

// ---------------------------------------------------------------------------
// Pipeline statistics (plain atomics, one cache line each in practice)
// ---------------------------------------------------------------------------
struct AdapterStats {
    std::atomic<uint64_t> connsOpened{0};
    std::atomic<uint64_t> chunks{0};        // received raw chunks
    std::atomic<uint64_t> bytesIn{0};       // received bytes
    std::atomic<uint64_t> unmatched{0};     // chunks dropped: unknown peer / non-raw protocol
    std::atomic<uint64_t> frames{0};        // parsed frames
    std::atomic<uint64_t> rowsParsed{0};
    std::atomic<uint64_t> rowsWritten{0};   // rows accepted by EtherDB
    std::atomic<uint64_t> batchesWritten{0};
    std::atomic<uint64_t> writeErrors{0};   // failed batch/submit errors
    std::atomic<uint64_t> publishStub{0};   // rows seen by the publish stub

    // ── modbus ──
    std::atomic<uint64_t> modbusRequests{0};   // register-read requests sent

    // ── http ──
    std::atomic<uint64_t> httpRequests{0};     // POST requests accepted
    std::atomic<uint64_t> httpRows{0};         // rows handed to the write queue
    std::atomic<uint64_t> httpErrors{0};       // rejected requests
};

// Queue traits mirroring the EtherDB server side (DServerQueue.h).
struct AdapterQueueTraits : public moodycamel::ConcurrentQueueDefaultTraits {
    static const size_t BLOCK_SIZE = 128;
    static const bool   RECYCLE_ALLOCATED_BLOCKS = true;
};

// ---------------------------------------------------------------------------
// Network thread -> parser thread. One chunk = a run of bytes received on one
// connection (frames are sliced later, on the parser thread — the design
// keeps ALL protocol parsing on a single thread).
// ---------------------------------------------------------------------------
struct RawChunk {
    const DeviceDesc* dev = nullptr;
    int64_t           recvMs = 0;
    std::vector<char> data;
};

// ---------------------------------------------------------------------------
// Parser thread -> writer / publish threads.
// Flattened row-major storage:
//   rows    = ts.size()
//   columns = dev->fieldCount()
//   cells.size() == rows * columns
// ---------------------------------------------------------------------------
struct RowBatch {
    const DeviceDesc*    dev = nullptr;
    std::vector<int64_t> ts;     // one per row (ms since epoch)
    std::vector<Cell>    cells;  // row-major, rows * fieldCount

    int  rowCount() const { return (int)ts.size(); }
    bool empty()    const { return ts.empty(); }
    void clear() { ts.clear(); cells.clear(); }

    void reserveRows(int rows) {
        ts.reserve((size_t)rows);
        if (dev) cells.reserve((size_t)rows * (size_t)dev->fieldCount());
    }

    void pushRow(int64_t t, const Cell* row) {
        ts.push_back(t);
        cells.insert(cells.end(), row, row + dev->fieldCount());
    }
};

using IngestQueue = moodycamel::BlockingConcurrentQueue<RawChunk, AdapterQueueTraits>;
using WriteQueue  = moodycamel::BlockingConcurrentQueue<RowBatch, AdapterQueueTraits>;
using PublishQueueT = moodycamel::BlockingConcurrentQueue<RowBatch, AdapterQueueTraits>;

} // namespace EtherAdapter

#endif // ETHERADAPTER_PIPELINE_H
