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
