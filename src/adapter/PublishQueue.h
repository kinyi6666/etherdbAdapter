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
// etherAdapter — publish path (reserved for ZMQ PUB)
//
// Design (etherAdapter.txt): parse -> publish queue -> ZMQ PUB.
// TODAY this is a stub: with [zmq].enabled = false (default) offer() is a
// no-op and nothing is copied; when enabled, batches are copied into the
// queue and drained by a consumer thread which currently drops them.
//
// Integration point for the future ZMQ PUB: replace the body of run() with
// a zmq_send() on a PUB socket bound to _cfg.pubEndpoint.
// ============================================================================
#ifndef ETHERADAPTER_PUBLISHQUEUE_H
#define ETHERADAPTER_PUBLISHQUEUE_H

#include "AdapterConfig.h"
#include "Pipeline.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace EtherAdapter {

class PublishQueue {
public:
    PublishQueue(const ZmqConfig& cfg, AdapterStats* stats);
    ~PublishQueue();

    PublishQueue(const PublishQueue&) = delete;
    PublishQueue& operator=(const PublishQueue&) = delete;

    void start();
    void stop();

    // Called on the parser thread. Copies the batch only when the publish
    // path is enabled (the caller keeps ownership of `batch`).
    void offer(const RowBatch& batch);

private:
    void run();

    ZmqConfig     _cfg;
    AdapterStats* _stats;
    PublishQueueT _q;

    std::thread       _thread;
    std::atomic<bool> _running{false};
    std::atomic<uint64_t> _rowsDrained{0};
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_PUBLISHQUEUE_H
