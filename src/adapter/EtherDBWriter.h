// ============================================================================
// etherAdapter — EtherDB writer thread
//
// Consumes RowBatch items and inserts them through the EtherDB client SDK
// (ETDB::Client) using column-bound prepared statements
// (EtDBStmt::bindParamBatch + execute) — the fastest insert path the SDK
// offers. One prepared statement + columnar staging buffer per device.
//
// On start the writer creates (optionally) the target database and one table
// per device:  <device_id>(ts TIMESTAMP, <field...>)
// ============================================================================
#ifndef ETHERADAPTER_ETHERDBWRITER_H
#define ETHERADAPTER_ETHERDBWRITER_H

#include "AdapterConfig.h"
#include "ConfigDB.h"
#include "Pipeline.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

// Keep the SDK headers out of this header.
namespace ETDB {
namespace Client {
class EtDBClient;
class EtDBStmt;
} // namespace Client
} // namespace ETDB

namespace EtherAdapter {

class EtherDBWriter {
public:
    EtherDBWriter(const EtherDBConfig& cfg, const ConfigDB& db,
                  WriteQueue* queue, AdapterStats* stats);
    ~EtherDBWriter();

    EtherDBWriter(const EtherDBWriter&) = delete;
    EtherDBWriter& operator=(const EtherDBWriter&) = delete;

    // Connect, create database/tables, prepare per-device INSERT statements
    // and start the writer thread. Returns false with *err on fatal errors.
    bool start(std::string* err);
    void stop();

    const EtherDBConfig& config() const { return _cfg; }

private:
    struct Sink;   // per-device statement + columnar staging buffers

    bool connectClient(std::string* err);
    bool ensureDatabase(std::string* err);
    bool ensureTable(const DeviceDesc& dev, std::string* err);
    bool prepareSink(const DeviceDesc& dev, std::string* err);
    std::string createTableSql(const DeviceDesc& dev) const;

    void run();
    void writeBatch(const RowBatch& batch);

    EtherDBConfig   _cfg;
    const ConfigDB& _db;
    WriteQueue*     _queue;
    AdapterStats*   _stats;

    std::string _dbName;   // sanitized target database name (from [etherdb].db)

    std::unique_ptr<ETDB::Client::EtDBClient> _client;
    std::unordered_map<const DeviceDesc*, std::unique_ptr<Sink>> _sinks;

    std::thread       _thread;
    std::atomic<bool> _running{false};
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_ETHERDBWRITER_H
