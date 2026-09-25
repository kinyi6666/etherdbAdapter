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
// etherAdapter — single parser thread
//
//   ingest queue -> [FrameCodec slices + decodes] -> per-device RowBatch
//                -> write queue (EtherDB) / publish queue (ZMQ stub)
//
// The design keeps ALL protocol parsing on one thread; the network threads
// only copy bytes into the ingest queue.
//
// A chunk carries a peer GROUP rather than a single device: the frame type in
// the custom header picks the member (status table or an event table). Status
// data (~1 Hz) is flushed as soon as it arrives; event bursts stay batched.
// ============================================================================
#ifndef ETHERADAPTER_PARSERWORKER_H
#define ETHERADAPTER_PARSERWORKER_H

#include "DeviceModel.h"
#include "FrameCodec.h"
#include "Pipeline.h"
#include "PublishQueue.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <vector>

namespace EtherAdapter {

class ParserWorker {
public:
    ParserWorker(IngestQueue* ingest, WriteQueue* write, PublishQueue* publish,
                 int batchRows, int flushIntervalMs, AdapterStats* stats);
    ~ParserWorker();

    ParserWorker(const ParserWorker&) = delete;
    ParserWorker& operator=(const ParserWorker&) = delete;

    void start();
    void stop();

private:
    void run();
    void flushDevice(const DeviceDesc* dev);
    void flushAll();

    // RowSink callback: append one decoded record to its device's batch.
    static void appendRow(void* ctx, const DeviceDesc* dev, int64_t ts, const Cell* row);

    IngestQueue*  _ingest;
    WriteQueue*   _write;
    PublishQueue* _publish;
    int           _batchRows;
    int           _flushIntervalMs;
    AdapterStats* _stats;

    std::thread       _thread;
    std::atomic<bool> _running{false};

    // Parser-thread-only state.
    std::unordered_map<const DeviceGroup*, DeviceStream> _streams;
    std::unordered_map<const DeviceDesc*, RowBatch>     _batches;
    std::vector<const DeviceDesc*> _touched;   // devices seen in the current chunk
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_PARSERWORKER_H
