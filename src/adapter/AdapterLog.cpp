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
// etherAdapter — lightweight process-local logging (see AdapterLog.h)
// ============================================================================
#include "AdapterLog.h"

namespace EtherAdapter {
namespace Log {

namespace {
std::atomic<Sink> g_sink{nullptr};
std::atomic<int>  g_level{(int)Level::Info};
} // namespace

void setSink(Sink s) {
    g_sink.store(s, std::memory_order_release);
}

Sink sink() {
    return g_sink.load(std::memory_order_acquire);
}

void setLevel(Level lv) {
    g_level.store((int)lv, std::memory_order_release);
}

Level level() {
    return (Level)g_level.load(std::memory_order_acquire);
}

const char* levelName(Level lv) {
    switch (lv) {
    case Level::Trace: return "trace";
    case Level::Debug: return "debug";
    case Level::Info:  return "info";
    case Level::Warn:  return "warn";
    case Level::Error: return "error";
    case Level::Fatal: return "fatal";
    }
    return "?";
}

} // namespace Log
} // namespace EtherAdapter
