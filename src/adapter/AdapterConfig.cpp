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
// etherAdapter — configuration loading (see AdapterConfig.h)
// ============================================================================
#include "AdapterConfig.h"

#include <base/ConfigParser.h>
#include "AdapterLog.h"

#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace EtherAdapter {

std::string getExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();
    std::string p(buf, n);
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = '\0';
    std::string p(buf, (size_t)n);
#endif
    size_t pos = p.find_last_of("\\/");
    if (pos == std::string::npos) return std::string();
    p.resize(pos);
    if (p.empty()) p = ".";
    return p;
}

static bool isAbsolutePath(const std::string& p) {
    if (p.empty()) return false;
#ifdef _WIN32
    if (p.size() >= 2 && p[1] == ':') return true;      // C:\...
    if (p.size() >= 2 && (p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/'))
        return true;                                     // \\server\share
    return p[0] == '\\' || p[0] == '/';
#else
    return p[0] == '/';
#endif
}

std::string AdapterConfig::resolvePath(const std::string& p) const {
    if (p.empty() || isAbsolutePath(p)) return p;
    if (exeDir.empty()) return p;
    return exeDir + "/" + p;
}

static std::vector<uint16_t> parsePortList(const std::string& text) {
    std::vector<uint16_t> out;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // trim
        size_t b = item.find_first_not_of(" \t");
        size_t e = item.find_last_not_of(" \t");
        if (b == std::string::npos) continue;
        int v = std::atoi(item.substr(b, e - b + 1).c_str());
        if (v > 0 && v <= 65535) out.push_back((uint16_t)v);
        else if (v != 0) EA_LOG_WARN << "etherAdapter.cfg: ignoring invalid port '" << item << "'";
    }
    return out;
}

AdapterConfig AdapterConfig::load(const std::string& cfgFile) {
    AdapterConfig cfg;
    cfg.exeDir     = getExeDir();
    cfg.configFile = cfgFile;

    EtherDB::ConfigParser ini;
    if (!ini.load(cfgFile)) {
        EA_LOG_WARN << "config file not found: " << cfgFile << " (using built-in defaults)";
        return cfg;
    }
    EA_LOG_INFO << "loaded config: " << cfgFile;

    // [server]
    cfg.server.extraListenPorts = parsePortList(ini.get("server", "extraListenPorts", ""));
    cfg.server.maxConnections   = ini.getInt("server", "maxConnections", cfg.server.maxConnections);

    // [sqlite]
    cfg.sqlite.path = ini.get("sqlite", "path", cfg.sqlite.path);

    // [etherdb]
    cfg.etherdb.host            = ini.get("etherdb", "host", cfg.etherdb.host);
    cfg.etherdb.port            = (uint16_t)ini.getInt("etherdb", "port", cfg.etherdb.port);
    cfg.etherdb.user            = ini.get("etherdb", "user", cfg.etherdb.user);
    cfg.etherdb.password        = ini.get("etherdb", "password", cfg.etherdb.password);
    cfg.etherdb.db              = ini.get("etherdb", "db", cfg.etherdb.db);
    cfg.etherdb.createDatabase  = ini.getBool("etherdb", "createDatabase", cfg.etherdb.createDatabase);
    cfg.etherdb.createTables    = ini.getBool("etherdb", "createTables", cfg.etherdb.createTables);
    cfg.etherdb.batchRows       = ini.getInt("etherdb", "batchRows", cfg.etherdb.batchRows);
    cfg.etherdb.flushIntervalMs = ini.getInt("etherdb", "flushIntervalMs", cfg.etherdb.flushIntervalMs);
    if (cfg.etherdb.batchRows < 1)   cfg.etherdb.batchRows = 1;
    if (cfg.etherdb.flushIntervalMs < 1) cfg.etherdb.flushIntervalMs = 1;

    // [parse]
    cfg.parse.frameLenMode = ini.get("parse", "frameLenMode", cfg.parse.frameLenMode);

    // [http]
    cfg.http.port = (uint16_t)ini.getInt("http", "port", cfg.http.port);

    // [modbus]
    cfg.modbus.pollIntervalMs = ini.getInt("modbus", "pollIntervalMs", cfg.modbus.pollIntervalMs);
    cfg.modbus.connectTimeoutMs = ini.getInt("modbus", "connectTimeoutMs", cfg.modbus.connectTimeoutMs);
    if (cfg.modbus.pollIntervalMs < 50) cfg.modbus.pollIntervalMs = 50;
    if (cfg.modbus.connectTimeoutMs < 100) cfg.modbus.connectTimeoutMs = 100;

    // [log]
    cfg.log.dir   = ini.get("log", "dir", cfg.log.dir);
    cfg.log.level = ini.get("log", "level", cfg.log.level);

    // [zmq]
    cfg.zmq.enabled     = ini.getBool("zmq", "enabled", cfg.zmq.enabled);
    cfg.zmq.pubEndpoint = ini.get("zmq", "pubEndpoint", cfg.zmq.pubEndpoint);

    return cfg;
}

} // namespace EtherAdapter
