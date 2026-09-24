// ============================================================================
// etherAdapter — lightweight process-local logging
//
// The EtherDB client SDK (EtDBLog.h) also defines EtherDB::Logger, so adapter
// translation units that include SDK headers cannot use base/Logging.h.
// This tiny logger is used everywhere in the adapter instead; the stream
// syntax mirrors the usual EA_LOG_INFO << ... style via EA_LOG_* macros.
//
// AdapterApp::initLogging() installs the async-file sink; lines go to stdout
// until then.
// ============================================================================
#ifndef ETHERADAPTER_ADAPTERLOG_H
#define ETHERADAPTER_ADAPTERLOG_H

#include <atomic>
#include <cstdio>
#include <sstream>
#include <string>

namespace EtherAdapter {
namespace Log {

enum class Level : int { Trace = 0, Debug, Info, Warn, Error, Fatal };

using Sink = void (*)(const char* msg, int len);

// Install/replace the output sink (pass nullptr to fall back to stdout).
void  setSink(Sink sink);
Sink  sink();

void  setLevel(Level lv);
Level level();

inline bool enabled(Level lv) {
    return (int)lv >= (int)level();
}

const char* levelName(Level lv);

// One log line: `file(line) [level] message\n` written to the sink on
// destruction.
class Line {
public:
    Line(const char* file, int line, Level lv) : _file(file), _line(line), _lv(lv) {}

    ~Line() {
        std::string msg;
        msg.reserve(96);
        msg.append(_file ? _file : "?");
        msg += '(';
        msg += std::to_string(_line);
        msg += ") [";
        msg += levelName(_lv);
        msg += "] ";
        msg += _stream.str();
        msg += '\n';

        if (Sink s = sink()) s(msg.data(), (int)msg.size());
        else fwrite(msg.data(), 1, msg.size(), stdout);
    }

    std::ostringstream& stream() { return _stream; }

private:
    const char* _file;
    int         _line;
    Level       _lv;
    std::ostringstream _stream;
};

} // namespace Log
} // namespace EtherAdapter

// ---------------------------------------------------------------------------
// EA_LOG_* macros (glog-style: safe in if/else without braces)
// ---------------------------------------------------------------------------
#define EA_LOG_TRACE \
    if (!EtherAdapter::Log::enabled(EtherAdapter::Log::Level::Trace)) ; \
    else EtherAdapter::Log::Line(__FILE__, __LINE__, EtherAdapter::Log::Level::Trace).stream()
#define EA_LOG_DEBUG \
    if (!EtherAdapter::Log::enabled(EtherAdapter::Log::Level::Debug)) ; \
    else EtherAdapter::Log::Line(__FILE__, __LINE__, EtherAdapter::Log::Level::Debug).stream()
#define EA_LOG_INFO \
    if (!EtherAdapter::Log::enabled(EtherAdapter::Log::Level::Info)) ; \
    else EtherAdapter::Log::Line(__FILE__, __LINE__, EtherAdapter::Log::Level::Info).stream()
#define EA_LOG_WARN \
    if (!EtherAdapter::Log::enabled(EtherAdapter::Log::Level::Warn)) ; \
    else EtherAdapter::Log::Line(__FILE__, __LINE__, EtherAdapter::Log::Level::Warn).stream()
#define EA_LOG_ERROR \
    if (!EtherAdapter::Log::enabled(EtherAdapter::Log::Level::Error)) ; \
    else EtherAdapter::Log::Line(__FILE__, __LINE__, EtherAdapter::Log::Level::Error).stream()
#define EA_LOG_FATAL \
    EtherAdapter::Log::Line(__FILE__, __LINE__, EtherAdapter::Log::Level::Fatal).stream()

#endif // ETHERADAPTER_ADAPTERLOG_H
