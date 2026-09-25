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
// etherAdapter — publish path stub (see PublishQueue.h)
// ============================================================================
#include "PublishQueue.h"

#include "AdapterLog.h"

namespace EtherAdapter {

PublishQueue::PublishQueue(const ZmqConfig& cfg, AdapterStats* stats)
    : _cfg(cfg), _stats(stats) {}

PublishQueue::~PublishQueue() {
    stop();
}

void PublishQueue::start() {
    if (!_cfg.enabled) {
        EA_LOG_INFO << "publish path disabled ([zmq].enabled = false); "
                 << "ZMQ PUB is reserved but not implemented yet";
        return;
    }
    if (_running.exchange(true)) return;
    _thread = std::thread(&PublishQueue::run, this);
    EA_LOG_WARN << "publish path enabled (endpoint " << _cfg.pubEndpoint
             << ") — ZMQ PUB is NOT implemented yet; batches are drained and dropped";
}

void PublishQueue::stop() {
    if (!_running.exchange(false)) return;
    if (_thread.joinable()) _thread.join();
}

void PublishQueue::offer(const RowBatch& batch) {
    if (!_cfg.enabled || !_running.load(std::memory_order_relaxed)) return;
    RowBatch copy = batch;   // deep copy: the parser moves the original into the write queue
    _q.enqueue(std::move(copy));
}

void PublishQueue::run() {
    while (_running.load(std::memory_order_relaxed)) {
        RowBatch b;
        if (_q.wait_dequeue_timed(b, 10000)) {
            const uint64_t rows = (uint64_t)b.rowCount();
            const uint64_t before = _rowsDrained.fetch_add(rows);
            _stats->publishStub.fetch_add(rows, std::memory_order_relaxed);

            // TODO(zmq): publish `b` on the PUB socket bound to _cfg.pubEndpoint,
            // e.g. frames of [device_id][ts][values...] once per row.
            if (before / 100000 != (before + rows) / 100000) {
                EA_LOG_INFO << "publish stub: " << (before + rows)
                         << " row(s) drained (ZMQ PUB not implemented)";
            }
        }
    }
}

} // namespace EtherAdapter
