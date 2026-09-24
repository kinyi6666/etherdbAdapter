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
// etherAdapter — EtherDB writer thread (see EtherDBWriter.h)
// ============================================================================
#include "EtherDBWriter.h"

#include "AdapterLog.h"

#include <EtDBClient.h>

#include <algorithm>
#include <cstring>

namespace EtherAdapter {

using ETDB::Client::EtDBClient;
using ETDB::Client::EtDBResult;
using ETDB::Client::EtDBStmt;

// ---------------------------------------------------------------------------
// Per-device sink: one prepared INSERT + columnar staging storage.
//
// `data` is laid out column-major so bindParamBatch() can point straight into
// it:   [ts: capacity*8][field0: capacity*outSize][field1: ...]...
// ---------------------------------------------------------------------------
struct EtherDBWriter::Sink {
    EtDBStmt* stmt = nullptr;

    std::vector<uint8_t>  data;      // columnar value storage
    std::vector<size_t>   colBase;   // byte offset of each column inside `data`
    std::vector<EtDBStmt::MultiBind> binds;   // reused every batch

    int      capacity = 0;           // rows per execute()
    uint32_t colCount = 0;           // 1 (ts) + field count
    size_t   rowBytes = 0;           // 8 + sum(field.outSize)
};

EtherDBWriter::EtherDBWriter(const EtherDBConfig& cfg, const ConfigDB& db,
                             WriteQueue* queue, AdapterStats* stats)
    : _cfg(cfg), _db(db), _queue(queue), _stats(stats) {}

EtherDBWriter::~EtherDBWriter() {
    stop();
}

// ============================================================================
// Startup: connect, create schema, prepare statements, start the thread
// ============================================================================
bool EtherDBWriter::start(std::string* err) {
    if (!connectClient(err)) return false;
    if (!ensureDatabase(err)) return false;

    int prepared = 0;
    int sqlDevices = 0;
    for (const DeviceDesc& dev : _db.devices()) {
        if (dev.spec.fields.empty()) continue;
        if (dev.connProto != CONN_RAW_DATA && dev.connProto != CONN_MODBUS &&
            dev.connProto != CONN_HTTP) {
            continue;   // mqtt etc. — not served yet
        }
        if (_cfg.createTables && !ensureTable(dev, err)) return false;

        if (dev.connProto == CONN_HTTP) {
            ++sqlDevices;   // slow path: plain INSERT SQL, no prepared sink
            continue;
        }
        if (!prepareSink(dev, err)) return false;
        ++prepared;
    }
    if (prepared == 0 && sqlDevices == 0) {
        EA_LOG_WARN << "no device with a usable field layout — "
                    << "the writer has nothing to insert";
    }

    _running.store(true);
    _thread = std::thread(&EtherDBWriter::run, this);
    EA_LOG_INFO << "EtherDB writer started (" << prepared << " columnar sink(s), "
                << sqlDevices << " SQL sink(s), db " << _dbName << ")";
    return true;
}

bool EtherDBWriter::connectClient(std::string* err) {
    EtDBClient::init();
    _client.reset(new EtDBClient());
    if (!_client->connect(_cfg.host, _cfg.port, _cfg.user.c_str(),
                          _cfg.password.c_str(), "")) {
        if (err) {
            *err = "cannot connect to EtherDB at " + _cfg.host + ":" +
                   std::to_string((unsigned)_cfg.port) +
                   " (is the server running?)";
        }
        return false;
    }
    EA_LOG_INFO << "connected to EtherDB at " << _cfg.host << ":" << (unsigned)_cfg.port;
    return true;
}

bool EtherDBWriter::ensureDatabase(std::string* err) {
    _dbName = makeIdentifier(_cfg.db, "adapter");

    if (_cfg.createDatabase) {
        EtDBResult r = _client->query("CREATE DATABASE IF NOT EXISTS " + _dbName +
                                      " KEEP 3650 REPLICA 1 PRECISION us");
        if (!r.error().empty()) {
            if (err) *err = "CREATE DATABASE " + _dbName + " failed: " + r.error();
            return false;
        }
    }

    EtDBResult r = _client->query("USE " + _dbName);
    if (!r.error().empty()) {
        if (err) {
            *err = "USE " + _dbName + " failed: " + r.error() +
                   " (create the database first or set [etherdb].createDatabase = true)";
        }
        return false;
    }
    EA_LOG_INFO << "using EtherDB database " << _dbName;
    return true;
}

std::string EtherDBWriter::createTableSql(const DeviceDesc& dev) const {
    std::string sql = "CREATE TABLE IF NOT EXISTS " + dev.deviceId + " (ts TIMESTAMP";
    for (const FieldDesc& f : dev.spec.fields) {
        sql += ", ";
        sql += f.name;
        sql += ' ';
        sql += f.colType;
    }
    sql += ")";
    return sql;
}

bool EtherDBWriter::ensureTable(const DeviceDesc& dev, std::string* err) {
    EtDBResult r = _client->query(createTableSql(dev));
    if (!r.error().empty()) {
        if (err) *err = "CREATE TABLE " + dev.deviceId + " failed: " + r.error();
        return false;
    }
    // Warm the meta cache so the prepared INSERT resolves the column types.
    _client->fetchTableMeta(_dbName, dev.deviceId);
    return true;
}

bool EtherDBWriter::prepareSink(const DeviceDesc& dev, std::string* err) {
    std::unique_ptr<Sink> sink(new Sink());
    sink->capacity = _cfg.batchRows > 0 ? _cfg.batchRows : 1;
    sink->colCount = (uint32_t)dev.fieldCount() + 1;   // + ts

    // Column byte layout: [column][capacity rows].
    sink->colBase.resize(sink->colCount);
    size_t off = 0;
    auto layout = [&](uint32_t k, size_t size) {
        sink->colBase[k] = off;
        off += size * (size_t)sink->capacity;
    };
    layout(0, 8);   // ts
    for (int k = 0; k < dev.fieldCount(); ++k) layout((uint32_t)k + 1, dev.spec.fields[k].outSize);
    sink->rowBytes = off / (size_t)sink->capacity;
    sink->data.assign(off, 0);
    sink->binds.resize(sink->colCount);

    std::string sql = "INSERT INTO " + dev.deviceId + " VALUES(";
    for (uint32_t k = 0; k < sink->colCount; ++k) {
        if (k) sql += ", ";
        sql += '?';
    }
    sql += ")";

    sink->stmt = _client->createStmt();
    if (!sink->stmt || !sink->stmt->prepare(sql)) {
        if (err) {
            *err = "prepare INSERT for " + dev.deviceId +
                   " failed (table schema <-> field layout mismatch?)";
        }
        if (sink->stmt) {
            sink->stmt->close();
            delete sink->stmt;
        }
        return false;
    }

    EA_LOG_INFO << "prepared sink for " << dev.deviceId << ": " << dev.fieldCount()
             << " field(s), batch " << sink->capacity << " rows ("
             << sink->rowBytes << " B/row staging)";
    _sinks[&dev] = std::move(sink);
    return true;
}

void EtherDBWriter::stop() {
    if (_running.exchange(false)) {
        if (_thread.joinable()) _thread.join();
    }
    for (auto& kv : _sinks) {
        Sink* sink = kv.second.get();
        if (sink && sink->stmt) {
            sink->stmt->close();
            delete sink->stmt;
            sink->stmt = nullptr;
        }
    }
    _sinks.clear();
    if (_client) _client->close();
    EA_LOG_INFO << "EtherDB writer stopped";
}

// ============================================================================
// Writer thread
// ============================================================================
void EtherDBWriter::run() {
    while (_running.load(std::memory_order_relaxed)) {
        RowBatch batch;
        if (_queue->wait_dequeue_timed(batch, 50000)) {   // 50 ms poll
            if (batch.dev && !batch.empty()) writeBatch(batch);
        }
    }
    // Drain what is left after stop() was requested.
    RowBatch batch;
    while (_queue->try_dequeue(batch)) {
        if (batch.dev && !batch.empty()) writeBatch(batch);
    }
}

void EtherDBWriter::writeBatch(const RowBatch& batch) {
    if (!batch.dev) return;

    // HTTP devices carry slow data and are written through plain INSERT SQL
    // (no prepared-statement binding).
    if (batch.dev->connProto == CONN_HTTP) {
        writeBatchSql(batch);
        return;
    }

    auto it = _sinks.find(batch.dev);
    if (it == _sinks.end() || !it->second || !it->second->stmt) return;
    Sink& sink = *it->second;

    const DeviceDesc& dev = *batch.dev;
    const int fields = dev.fieldCount();
    const int rows   = batch.rowCount();

    int done = 0;
    while (done < rows) {
        const int n = std::min(rows - done, sink.capacity);

        // ── ts column ──
        std::memcpy(sink.data.data() + sink.colBase[0],
                    batch.ts.data() + done, (size_t)n * 8);

        // ── field columns (row-major source -> column-major staging) ──
        const Cell* cells = batch.cells.data() + (size_t)done * (size_t)fields;
        for (int k = 0; k < fields; ++k) {
            const FieldDesc& f = dev.spec.fields[k];
            uint8_t* col = sink.data.data() + sink.colBase[(size_t)k + 1];

            if (f.asDouble) {
                if (f.outSize == 8) {
                    for (int r = 0; r < n; ++r) {
                        const double d = cells[(size_t)r * fields + k].v.d;
                        std::memcpy(col + (size_t)r * 8, &d, 8);
                    }
                } else {   // 4-byte float column
                    for (int r = 0; r < n; ++r) {
                        const float x = (float)cells[(size_t)r * fields + k].v.d;
                        std::memcpy(col + (size_t)r * 4, &x, 4);
                    }
                }
            } else {
                switch (f.outSize) {
                case 1:
                    for (int r = 0; r < n; ++r)
                        col[r] = (uint8_t)(int8_t)cells[(size_t)r * fields + k].v.i;
                    break;
                case 2:
                    for (int r = 0; r < n; ++r) {
                        const int16_t x = (int16_t)cells[(size_t)r * fields + k].v.i;
                        std::memcpy(col + (size_t)r * 2, &x, 2);
                    }
                    break;
                case 4:
                    for (int r = 0; r < n; ++r) {
                        const int32_t x = (int32_t)cells[(size_t)r * fields + k].v.i;
                        std::memcpy(col + (size_t)r * 4, &x, 4);
                    }
                    break;
                default:
                    for (int r = 0; r < n; ++r) {
                        const int64_t x = cells[(size_t)r * fields + k].v.i;
                        std::memcpy(col + (size_t)r * 8, &x, 8);
                    }
                    break;
                }
            }
        }

        // ── bind columns and execute ──
        sink.binds[0].type   = EtDBStmt::TYPE_TIMESTAMP;
        sink.binds[0].buffer = sink.data.data() + sink.colBase[0];
        sink.binds[0].stride = 8;
        sink.binds[0].numRows = n;
        for (int k = 0; k < fields; ++k) {
            const FieldDesc& f = dev.spec.fields[k];
            EtDBStmt::MultiBind& mb = sink.binds[(size_t)k + 1];
            mb.type   = f.bindType;
            mb.buffer = sink.data.data() + sink.colBase[(size_t)k + 1];
            mb.stride = f.outSize;
            mb.numRows = n;
        }

        if (!sink.stmt->bindParamBatch(sink.binds.data(), (int)sink.binds.size())) {
            EA_LOG_ERROR << "bindParamBatch failed for " << dev.deviceId
                      << " (" << n << " rows)";
            _stats->writeErrors.fetch_add(1, std::memory_order_relaxed);
            done += n;
            continue;
        }

        const int affected = sink.stmt->execute(_client->connection()->currentdbId());
        if (affected < 0) {
            EA_LOG_ERROR << "INSERT failed for " << dev.deviceId << " (" << n << " rows)";
            _stats->writeErrors.fetch_add(1, std::memory_order_relaxed);
        } else {
            _stats->rowsWritten.fetch_add((uint64_t)affected, std::memory_order_relaxed);
            _stats->batchesWritten.fetch_add(1, std::memory_order_relaxed);
            const int errs = sink.stmt->errorRows();
            if (errs > 0) {
                _stats->writeErrors.fetch_add((uint64_t)errs, std::memory_order_relaxed);
                EA_LOG_WARN << "INSERT " << dev.deviceId << ": " << errs
                         << " of " << n << " row(s) rejected (affected " << affected << ")";
            }
        }
        done += n;
    }
}

// ============================================================================
// HTTP sink: one INSERT SQL statement per batch (200 rows max per statement).
// NULL cells are written as SQL NULL; doubles use %.10g formatting.
// ============================================================================
namespace {
std::string formatDouble(double v) {
    std::ostringstream os;
    os.precision(10);
    os << v;
    return os.str();
}
} // namespace

void EtherDBWriter::writeBatchSql(const RowBatch& batch) {
    const DeviceDesc& dev = *batch.dev;
    const int fields = dev.fieldCount();
    const int rows   = batch.rowCount();
    if (fields == 0 || rows == 0) return;

    static const int kMaxRowsPerStmt = 200;

    int done = 0;
    while (done < rows) {
        const int n = std::min(rows - done, kMaxRowsPerStmt);

        std::string sql;
        sql.reserve(128 + (size_t)n * (16 + (size_t)fields * 8));
        sql += "INSERT INTO ";
        sql += dev.deviceId;
        sql += " VALUES ";
        for (int r = 0; r < n; ++r) {
            if (r) sql += ',';
            sql += '(';
            sql += std::to_string(batch.ts[(size_t)(done + r)]);
            const Cell* rowCells = batch.cells.data() + (size_t)(done + r) * (size_t)fields;
            for (int k = 0; k < fields; ++k) {
                sql += ',';
                const Cell& c = rowCells[k];
                if (c.isNull) { sql += "NULL"; continue; }
                const FieldDesc& f = dev.spec.fields[k];
                if (f.asDouble)                    sql += formatDouble(c.v.d);
                else if (f.kind == FieldKind::Bool) sql += (c.v.i ? '1' : '0');
                else                               sql += std::to_string(c.v.i);
            }
            sql += ')';
        }

        EtDBResult r = _client->query(sql);
        if (!r.error().empty()) {
            EA_LOG_ERROR << "http insert failed for " << dev.deviceId << " (" << n
                         << " row(s)): " << r.error();
            _stats->writeErrors.fetch_add(1, std::memory_order_relaxed);
        } else {
            _stats->rowsWritten.fetch_add((uint64_t)n, std::memory_order_relaxed);
            _stats->batchesWritten.fetch_add(1, std::memory_order_relaxed);
        }
        done += n;
    }
}

} // namespace EtherAdapter
