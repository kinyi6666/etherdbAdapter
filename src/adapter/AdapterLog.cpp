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
