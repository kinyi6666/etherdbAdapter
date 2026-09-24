// ============================================================================
// etherAdapter — parser thread (see ParserWorker.h)
// ============================================================================
#include "ParserWorker.h"

#include "AdapterLog.h"

namespace EtherAdapter {

ParserWorker::ParserWorker(IngestQueue* ingest, WriteQueue* write, PublishQueue* publish,
                           FrameLenMode frameLenMode, int batchRows, int flushIntervalMs,
                           AdapterStats* stats)
    : _ingest(ingest),
      _write(write),
      _publish(publish),
      _frameLenMode(frameLenMode),
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

    while (_running.load(std::memory_order_relaxed)) {
        RawChunk chunk;
        if (_ingest->wait_dequeue_timed(chunk, idleUs)) {
            if (!chunk.dev || chunk.data.empty()) continue;

            // Per-device stream buffer (created on first data). A configured
            // non-auto frameLenMode (total/payload) skips the runtime probe.
            auto ins = _streams.emplace(chunk.dev, DeviceStream());
            DeviceStream& st = ins.first->second;
            if (ins.second) {
                st.lenMode  = _frameLenMode;
                st.resolved = (_frameLenMode != FrameLenMode::Auto);
            }

            RowBatch& batch = _batches[chunk.dev];
            if (!batch.dev) {
                batch.dev = chunk.dev;
                batch.reserveRows(_batchRows);
            }

            const int rows = FrameCodec::feed(*chunk.dev, st, chunk.data.data(),
                                              chunk.data.size(), chunk.recvMs, batch);
            if (rows > 0) {
                _stats->frames.fetch_add((uint64_t)rows, std::memory_order_relaxed);
                _stats->rowsParsed.fetch_add((uint64_t)rows, std::memory_order_relaxed);
            }
            if (batch.rowCount() >= _batchRows) flushBatch(chunk.dev);
        } else {
            flushAll();   // idle: push whatever partial batches exist
        }
    }

    flushAll();
}

void ParserWorker::flushBatch(const DeviceDesc* dev) {
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
        if (!kv.second.empty()) flushBatch(kv.first);
    }
}

} // namespace EtherAdapter
