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
// [parse]
// ---------------------------------------------------------------------------
struct ParseConfig {
    // How data_header_table.frame_len is interpreted:
    //   "auto"    - probe at runtime per protocol (default)
    //   "total"   - frame_len counts the whole frame including flags
    //   "payload" - frame_len counts the payload only (flags excluded)
    std::string frameLenMode = "auto";
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
    ParseConfig   parse;
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
