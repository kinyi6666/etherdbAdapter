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
// etherAdapter — main configuration (etherAdapter.cfg, INI style)
//
// The cfg file describes *process-level* settings only. Per-device protocol
// configuration (devices / frame headers / field layouts) lives in the SQLite
// configuration database, see ConfigDB.
// ============================================================================
#ifndef ETHERADAPTER_ADAPTERCONFIG_H
#define ETHERADAPTER_ADAPTERCONFIG_H

#include <cstdint>
#include <string>
#include <vector>

namespace EtherAdapter {

// ---------------------------------------------------------------------------
// [server]
// ---------------------------------------------------------------------------
struct ServerConfig {
    // Extra listen ports besides the local_server_port values declared in
    // device_table. Comma separated. Useful for tests or manual runs.
    std::vector<uint16_t> extraListenPorts;
    int maxConnections = 1024;
};

// ---------------------------------------------------------------------------
// [sqlite]
// ---------------------------------------------------------------------------
struct SqliteConfig {
    // Configuration database holding device_table / data_header_table /
    // data_proto_table. A relative path resolves against the executable dir.
    std::string path = "config/etherAdapter.db";
};

// ---------------------------------------------------------------------------
// [etherdb]
// ---------------------------------------------------------------------------
struct EtherDBConfig {
    std::string host = "127.0.0.1";
    uint16_t    port = 7040;
    std::string user = "root";
    std::string password = "etherdbdata";
    std::string db = "adapter";         // target database (created if missing)
    bool        createDatabase = true;  // CREATE DATABASE IF NOT EXISTS on start
    bool        createTables   = true;  // CREATE TABLE IF NOT EXISTS per device
    int         batchRows      = 4000;  // rows per inserted batch (per device)
    int         flushIntervalMs = 20;   // flush a partial batch after idle time
};

// ---------------------------------------------------------------------------
// [http] — HTTP ingest (single server; slow data, INSERT-SQL write path)
// ---------------------------------------------------------------------------
struct HttpConfig {
    // HTTP listen port. 0 = derive from device_table: the local_server_port of
    // the conn_proto=http(3) devices (exactly ONE HTTP server is started).
    uint16_t port = 0;
};

// ---------------------------------------------------------------------------
// [modbus] — register-read polling for conn_proto=modbus(2) devices
// ---------------------------------------------------------------------------
struct ModbusConfig {
    // Interval between register-read requests, per device (ms).
    int pollIntervalMs = 3000;
    // Connect timeout for the device link (ms).
    int connectTimeoutMs = 1500;
};

// ---------------------------------------------------------------------------
// [log]
// ---------------------------------------------------------------------------
struct LogConfig {
    std::string dir   = "log";
    std::string level = "info";   // trace|debug|info|warn|error
};

// ---------------------------------------------------------------------------
// [zmq] — publish path (reserved; ZMQ PUB is not implemented yet)
// ---------------------------------------------------------------------------
struct ZmqConfig {
    bool        enabled = false;
    std::string pubEndpoint = "tcp://*:5555";
};

// ---------------------------------------------------------------------------
// Aggregated configuration
// ---------------------------------------------------------------------------
struct AdapterConfig {
    ServerConfig  server;
    SqliteConfig  sqlite;
    EtherDBConfig etherdb;
    HttpConfig    http;
    ModbusConfig  modbus;
    LogConfig     log;
    ZmqConfig     zmq;

    std::string exeDir;      // directory of the running executable
    std::string configFile;  // cfg file actually loaded ("" = defaults only)

    // Load configuration. A missing file is not an error: defaults are used
    // and the reason is logged as a warning.
    static AdapterConfig load(const std::string& cfgFile);

    // Resolve a possibly-relative path against exeDir (absolute paths and
    // empty strings pass through unchanged).
    std::string resolvePath(const std::string& p) const;
};

// Directory of the current executable (no trailing separator). Empty on
// failure.
std::string getExeDir();

} // namespace EtherAdapter

#endif // ETHERADAPTER_ADAPTERCONFIG_H
