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
// etherAdapter — application assembly (see AdapterApp.h)
// ============================================================================
#include "AdapterApp.h"

#include <base/AsyncLogging.h>
#include "AdapterLog.h"

#include "FrameCodec.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace EtherAdapter {

// ============================================================================
// Logging (async file logging, same setup as the EtherDB dserver)
// ============================================================================
namespace {

EtherDB::AsyncLogging* g_asyncLog = nullptr;

void asyncLogOutput(const char* msg, int len) {
    if (g_asyncLog) g_asyncLog->append(msg, len);
    else fwrite(msg, 1, (size_t)len, stdout);
}

bool ensureDirRecursive(const std::string& dir) {
    if (dir.empty()) return false;
    std::string path;
    for (size_t i = 0; i <= dir.size(); ++i) {
        if (i == dir.size() || dir[i] == '/' || dir[i] == '\\') {
            if (!path.empty() && !(path.size() == 2 && path[1] == ':')) {
#ifdef _WIN32
                _mkdir(path.c_str());
#else
                mkdir(path.c_str(), 0755);
#endif
            }
        }
        if (i < dir.size()) path.push_back(dir[i]);
    }
    return true;
}

// Flush + tear down the async log on every exit path of run().
struct AsyncLogGuard {
    ~AsyncLogGuard() {
        if (g_asyncLog) {
            g_asyncLog->stop();
            delete g_asyncLog;
            g_asyncLog = nullptr;
        }
        Log::setSink(nullptr);
    }
};

Log::Level levelFromString(const std::string& s) {
    if (s == "trace") return Log::Level::Trace;
    if (s == "debug") return Log::Level::Debug;
    if (s == "warn")  return Log::Level::Warn;
    if (s == "error") return Log::Level::Error;
    return Log::Level::Info;
}

// ---------------------------------------------------------------------------
// Ctrl+C / SIGTERM handling: ask the event loop to exit. EventLoop::quit() is
// thread safe (flag + wakeup pipe), so calling it from the console handler
// thread or a POSIX signal handler is safe.
// ---------------------------------------------------------------------------
EtherDB::Net::EventLoop* g_signalLoop = nullptr;

#ifdef _WIN32
BOOL WINAPI consoleCtrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        if (g_signalLoop) g_signalLoop->quit();
        return TRUE;
    }
    return FALSE;
}
#else
void posixSignalHandler(int) {
    if (g_signalLoop) g_signalLoop->quit();
}
#endif

} // namespace

// ============================================================================
// AdapterApp
// ============================================================================
AdapterApp::AdapterApp() = default;

AdapterApp::~AdapterApp() = default;

bool AdapterApp::initLogging() {
    Log::setLevel(levelFromString(_cfg.log.level));

    const std::string dir = _cfg.resolvePath(_cfg.log.dir);
    if (!ensureDirRecursive(dir)) {
        fprintf(stderr, "WARNING: cannot create log directory: %s\n", dir.c_str());
        return false;
    }

    g_asyncLog = new EtherDB::AsyncLogging(dir + "/etherAdapter", 64 * 1024 * 1024, 1000);
    g_asyncLog->start();
    Log::setSink(asyncLogOutput);
    return true;
}

void AdapterApp::installSignalHandlers() {
    g_signalLoop = &_loop;
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#else
    signal(SIGINT,  posixSignalHandler);
    signal(SIGTERM, posixSignalHandler);
#endif
}

void AdapterApp::stop() {
    _loop.quit();
}

void AdapterApp::logConfigSummary() const {
    EA_LOG_INFO << "config summary:";
    EA_LOG_INFO << "  sqlite    : " << _cfg.resolvePath(_cfg.sqlite.path);
    EA_LOG_INFO << "  etherdb   : " << _cfg.etherdb.host << ":" << (unsigned)_cfg.etherdb.port
             << " db=" << _cfg.etherdb.db
             << " batchRows=" << _cfg.etherdb.batchRows
             << " flush=" << _cfg.etherdb.flushIntervalMs << "ms";
    EA_LOG_INFO << "  parse     : custom_data header 6B (frameType/flag/frameLen/sequenceId)";
    EA_LOG_INFO << "  http      : port=" << (_cfg.http.port ? std::to_string(_cfg.http.port)
                                                          : std::string("(from device_table)"));
    EA_LOG_INFO << "  modbus    : poll every " << _cfg.modbus.pollIntervalMs << " ms";
    EA_LOG_INFO << "  zmq       : " << (_cfg.zmq.enabled ? "enabled (stub)" : "disabled")
             << " endpoint=" << _cfg.zmq.pubEndpoint;
    EA_LOG_INFO << "  log       : " << _cfg.resolvePath(_cfg.log.dir)
             << " level=" << _cfg.log.level;
}

void AdapterApp::statsLoop() {
    uint64_t pc = 0, pb = 0, pf = 0, pr = 0, pw = 0, pwe = 0, pmb = 0, pht = 0, pev = 0;

    while (_statsRunning.load(std::memory_order_relaxed)) {
        for (int i = 0; i < 50 && _statsRunning.load(std::memory_order_relaxed); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!_statsRunning.load(std::memory_order_relaxed)) break;

        const uint64_t c  = _stats->chunks.load(std::memory_order_relaxed);
        const uint64_t b  = _stats->bytesIn.load(std::memory_order_relaxed);
        const uint64_t f  = _stats->frames.load(std::memory_order_relaxed);
        const uint64_t r  = _stats->rowsParsed.load(std::memory_order_relaxed);
        const uint64_t w  = _stats->rowsWritten.load(std::memory_order_relaxed);
        const uint64_t we = _stats->writeErrors.load(std::memory_order_relaxed);
        const uint64_t un = _stats->unmatched.load(std::memory_order_relaxed);
        const uint64_t mb = _stats->modbusRequests.load(std::memory_order_relaxed);
        const uint64_t ht = _stats->httpRequests.load(std::memory_order_relaxed);
        const uint64_t ev = _stats->eventRows.load(std::memory_order_relaxed);

        EA_LOG_INFO << "[stats] chunks +" << (c - pc)
                 << " (" << (b - pb) / 1024 << " KB) frames +" << (f - pf)
                 << " rows +" << (r - pr) << " (event +" << (ev - pev) << ")"
                 << " written +" << (w - pw)
                 << " modbus +" << (mb - pmb) << " http +" << (ht - pht)
                 << " drop " << un << " err " << we
                 << " | queue ingest " << _ingestQueue->size_approx()
                 << " write " << _writeQueue->size_approx();
        pc = c; pb = b; pf = f; pr = r; pw = w; pwe = we; pmb = mb; pht = ht; pev = ev;
    }
    (void)pwe;
}

// ============================================================================
// run
// ============================================================================
int AdapterApp::run(const std::string& cfgFile) {
    _cfg = AdapterConfig::load(cfgFile);
    AsyncLogGuard logGuard;   // flush the async log on every return path
    initLogging();   // failure is non-fatal (logs go to stdout)

    EA_LOG_INFO << "-----------------------------------------------------------------";
    EA_LOG_INFO << "etherAdapter starting (exe dir: " << _cfg.exeDir << ")";
    logConfigSummary();

    std::string err;

    // ── 1. configuration database (device / header / field tables) ──
    _configDb.reset(new ConfigDB());
    const std::string dbPath = _cfg.resolvePath(_cfg.sqlite.path);
    if (!_configDb->open(dbPath, &err) || !_configDb->loadAll(&err)) {
        EA_LOG_FATAL << "configuration error: " << err;
        return 2;
    }
    if (_configDb->devices().empty()) {
        EA_LOG_FATAL << "no devices in " << dbPath << " — nothing to ingest";
        return 2;
    }

    // ── 2. queues + stats ──
    _ingestQueue.reset(new IngestQueue());
    _writeQueue.reset(new WriteQueue());
    _stats.reset(new AdapterStats());

    // ── 3. EtherDB writer (connect + schema + thread) ──
    _writer.reset(new EtherDBWriter(_cfg.etherdb, *_configDb, _writeQueue.get(), _stats.get()));
    if (!_writer->start(&err)) {
        EA_LOG_FATAL << err;
        return 3;
    }

    // ── 4. publish path (reserved / stub) ──
    _publish.reset(new PublishQueue(_cfg.zmq, _stats.get()));
    _publish->start();

    // ── 5. parser thread (single thread for ALL protocol parsing) ──
    _parser.reset(new ParserWorker(_ingestQueue.get(), _writeQueue.get(), _publish.get(),
                                   _cfg.etherdb.batchRows, _cfg.etherdb.flushIntervalMs,
                                   _stats.get()));
    _parser->start();

    // ── 6. modbus polling (register-read requests) + mqtt stub ──
    _modbusPoller.reset(new ModbusPoller(_cfg, *_configDb, _stats.get()));
    _modbusPoller->start();

    _mqtt.reset(new MqttIngest(_cfg));
    _mqtt->start(&err);

    // ── 7. listeners: unified TCP ingest (custom_data/modbus) + ONE http server ──
    _server.reset(new IngestServer(&_loop, *_configDb, _cfg.server,
                                   _ingestQueue.get(), _stats.get()));
    if (!_server->start(&err)) {
        EA_LOG_FATAL << err;
        _modbusPoller->stop();
        _parser->stop();
        _publish->stop();
        _writer->stop();
        return 4;
    }
    _http.reset(new HttpIngest(&_loop, *_configDb, _cfg, _writeQueue.get(), _stats.get()));
    if (!_http->start(&err)) {
        EA_LOG_FATAL << err;
        _server->stop();
        _modbusPoller->stop();
        _parser->stop();
        _publish->stop();
        _writer->stop();
        return 5;
    }

    // ── 8. run ──
    installSignalHandlers();
    _statsRunning.store(true);
    _statsThread = std::thread(&AdapterApp::statsLoop, this);

    EA_LOG_INFO << "etherAdapter is running (press Ctrl+C to stop)";
    _loop.loop();

    // ── 9. orderly shutdown ──
    EA_LOG_INFO << "shutting down...";
    _statsRunning.store(false);
    if (_statsThread.joinable()) _statsThread.join();

    _server->stop();        // stop accepting new data
    _http->stop();          // stop the HTTP server
    _modbusPoller->stop();  // stop register polling
    _parser->stop();        // parse queued data, flush partial batches
    _publish->stop();
    _writer->stop();        // drain write queue, close EtherDB connection
    _mqtt->stop();

    EA_LOG_INFO << "etherAdapter stopped: "
             << _stats->frames.load() << " frame(s), "
             << _stats->rowsParsed.load() << " row(s) parsed ("
             << _stats->eventRows.load() << " event), "
             << _stats->rowsWritten.load() << " row(s) written, "
             << _stats->modbusRequests.load() << " modbus request(s), "
             << _stats->httpRequests.load() << " http request(s), "
             << _stats->writeErrors.load() << " write error(s)";
    return 0;
}

} // namespace EtherAdapter
