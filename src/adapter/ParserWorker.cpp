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
// etherAdapter — parser thread (see ParserWorker.h)
// ============================================================================
#include "ParserWorker.h"

#include "AdapterLog.h"

namespace EtherAdapter {

ParserWorker::ParserWorker(IngestQueue* ingest, WriteQueue* write, PublishQueue* publish,
                           int batchRows, int flushIntervalMs, AdapterStats* stats)
    : _ingest(ingest),
      _write(write),
      _publish(publish),
      _batchRows(batchRows > 0 ? batchRows : 1),
      _flushIntervalMs(flushIntervalMs > 0 ? flushIntervalMs : 20),
      _stats(stats) {}

ParserWorker::~ParserWorker() {
    stop();
}

void ParserWorker::start() {
    if (_running.exchange(true)) return;
    _thread = std::thread(&ParserWorker::run, this);
    EA_LOG_INFO << "parser thread started (batch rows " << _batchRows
             << ", idle flush " << _flushIntervalMs << " ms)";
}

void ParserWorker::stop() {
    if (!_running.exchange(false)) return;
    if (_thread.joinable()) _thread.join();
    EA_LOG_INFO << "parser thread stopped";
}

void ParserWorker::run() {
    const int64_t idleUs = 10000;   // dequeue poll: 10 ms

    RowSink sink;
    sink.fn  = &ParserWorker::appendRow;
    sink.ctx = this;

    while (_running.load(std::memory_order_relaxed)) {
        RawChunk chunk;
        if (_ingest->wait_dequeue_timed(chunk, idleUs)) {
            if (!chunk.group || chunk.data.empty()) continue;

            // Per-endpoint stream buffer (created on first data).
            DeviceStream& st = _streams.emplace(chunk.group, DeviceStream()).first->second;

            _touched.clear();
            const DeviceDesc* first = chunk.group->primary();
            const int rows = (first && first->connProto == CONN_MODBUS)
                ? FrameCodec::feedModbus(*first, st, chunk.data.data(),
                                         chunk.data.size(), chunk.recvMs, sink)
                : FrameCodec::feed(*chunk.group, st, chunk.data.data(),
                                   chunk.data.size(), chunk.recvMs, sink);
            if (rows > 0) {
                _stats->frames.fetch_add((uint64_t)rows, std::memory_order_relaxed);
                _stats->rowsParsed.fetch_add((uint64_t)rows, std::memory_order_relaxed);
            }

            // A frame type with no device_table row would be dropped silently;
            // report it once per endpoint so the config gap is visible.
            if (st.unknownType != st.unknownTypeLogged) {
                if (st.unknownTypeLogged == 0) {
                    EA_LOG_WARN << "endpoint " << chunk.group->key << ": data with an "
                                << "unconfigured frame type arrived — add a device_table "
                                << "row (and data_header_table.frame_type) for it, "
                                << "otherwise those frames are dropped";
                }
                st.unknownTypeLogged = st.unknownType;
            }

            // Status data (~1 Hz) is submitted as soon as it arrives; event
            // bursts stay batched until batchRows (or the idle flush).
            for (const DeviceDesc* d : _touched) {
                auto it = _batches.find(d);
                if (it == _batches.end() || it->second.empty()) continue;
                if (d->immediateFlush || it->second.rowCount() >= _batchRows)
                    flushDevice(d);
            }
        } else {
            flushAll();   // idle: push whatever partial batches exist
        }
    }

    flushAll();
}

// ---------------------------------------------------------------------------
// RowSink callback (parser thread): one decoded record -> its device's batch.
// ---------------------------------------------------------------------------
void ParserWorker::appendRow(void* ctx, const DeviceDesc* dev, int64_t ts, const Cell* row) {
    ParserWorker* self = static_cast<ParserWorker*>(ctx);

    RowBatch& batch = self->_batches[dev];
    if (!batch.dev) {
        batch.dev = dev;
        batch.reserveRows(self->_batchRows);
    }
    batch.pushRow(ts, row);

    if (dev->isEvent)
        self->_stats->eventRows.fetch_add(1, std::memory_order_relaxed);

    if (self->_touched.empty() || self->_touched.back() != dev)
        self->_touched.push_back(dev);
}

void ParserWorker::flushDevice(const DeviceDesc* dev) {
    auto it = _batches.find(dev);
    if (it == _batches.end() || it->second.empty()) return;

    RowBatch& batch = it->second;
    if (_publish) _publish->offer(batch);

    _write->enqueue(std::move(batch));   // leaves `batch` empty

    batch.dev = dev;                     // ready for the next rows
    batch.reserveRows(_batchRows);
}

void ParserWorker::flushAll() {
    for (auto& kv : _batches) {
        if (!kv.second.empty()) flushDevice(kv.first);
    }
}

} // namespace EtherAdapter
