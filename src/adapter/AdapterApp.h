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
// etherAdapter — application assembly
//
//   [sqlite config] -> ConfigDB -> device registry + frame specs
//
//   muduo TCP servers ──RawChunk──> ingest queue ──> ParserWorker (1 thread)
//        │                                              │
//        │                                   RowBatch ──┤──> WriteQueue ──> EtherDBWriter
//        │                                              └──> PublishQueue -> ZMQ PUB (stub)
// ============================================================================
#ifndef ETHERADAPTER_ADAPTERAPP_H
#define ETHERADAPTER_ADAPTERAPP_H

#include "AdapterConfig.h"
#include "ConfigDB.h"
#include "EtherDBWriter.h"
#include "HttpIngest.h"
#include "IngestServer.h"
#include "IngestStubs.h"
#include "ModbusPoller.h"
#include "ParserWorker.h"
#include "Pipeline.h"
#include "PublishQueue.h"

#include <net/EventLoop.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace EtherAdapter {

class AdapterApp {
public:
    AdapterApp();
    ~AdapterApp();

    AdapterApp(const AdapterApp&) = delete;
    AdapterApp& operator=(const AdapterApp&) = delete;

    // Load the configuration, build the pipeline and run the event loop
    // until stop() is called (Ctrl+C). Returns the process exit code.
    int run(const std::string& cfgFile);

    // Ask the event loop to exit. Safe to call from any thread / signal handler.
    void stop();

private:
    bool initLogging();
    void installSignalHandlers();
    void statsLoop();
    void logConfigSummary() const;

    AdapterConfig _cfg;

    std::unique_ptr<ConfigDB>     _configDb;
    std::unique_ptr<IngestQueue>  _ingestQueue;
    std::unique_ptr<WriteQueue>   _writeQueue;
    std::unique_ptr<AdapterStats> _stats;

    std::unique_ptr<PublishQueue>  _publish;
    std::unique_ptr<ParserWorker>  _parser;
    std::unique_ptr<EtherDBWriter> _writer;
    std::unique_ptr<IngestServer>  _server;

    std::unique_ptr<HttpIngest>    _http;
    std::unique_ptr<MqttIngest>    _mqtt;
    std::unique_ptr<ModbusPoller>  _modbusPoller;

    EtherDB::Net::EventLoop _loop;

    std::thread       _statsThread;
    std::atomic<bool> _statsRunning{false};
};

} // namespace EtherAdapter

#endif // ETHERADAPTER_ADAPTERAPP_H
