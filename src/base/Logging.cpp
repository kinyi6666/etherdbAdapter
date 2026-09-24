////////////////////////////////////////////////////////////////////////////////
//�ļ���Logging.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������Ԫ����
////////////////////////////////////////////////////////////////////////////////////

#include "Logging.h"
#include "Thread.h"
#include <errno.h>
#include <sstream>

namespace EtherDB
{
	Logger::LogLevel initLogLevel()
	{
		if (::getenv("AM_LOG_TRACE"))
			return Logger::LTRACE;
		else if (::getenv("AM_LOG_DEBUG"))
			return Logger::LDEBUG;
		else
			return Logger::LINFO;
	}

	const char* LogLevelName[Logger::LNUM_LOG_LEVELS] =
	{
		"TRACE",
		"DEBUG",
		"INFO",
		"WARN",
		"ERROR",
		"FATAL",
	};

	void defaultOutput(const char* msg, int len)
	{
		size_t n = ::fwrite(msg, 1, len, stdout);
		(void)n;
	}

	void defaultFlush()
	{
		::fflush(stdout);
	}


	Logger::LogLevel g_logLevel = initLogLevel();
	Logger::OutputFunc g_output = defaultOutput;
	Logger::FlushFunc g_flush = defaultFlush;


	Logger::Logger(const char * file, int line)
		:_time(Timestamp::now()),
		_basename(sourceFileName(file)),
		_level(LINFO),
		_line(line)
	{
		_stream << _time.toFormattedString() << "-" << LogLevelName[_level] << ": ";
	}

	Logger::Logger(const char * file, int line, LogLevel level)
		:_time(Timestamp::now()),
		_basename(sourceFileName(file)),
		_level(level),
		_line(line)
	{
		_stream << _time.toFormattedString() << "-" << LogLevelName[_level] << ": ";
	}

	Logger::Logger(const char * file, int line, LogLevel level, const char* func)
		:_time(Timestamp::now()),
		_basename(sourceFileName(file)),
		_level(level),
		_line(line)
	{
		_stream << _time.toFormattedString() << "-" << LogLevelName[_level] << "-" << func << ": ";
	}

	Logger::Logger(const char * file, int line, bool toAbort)
		:_time(Timestamp::now()),
		_basename(sourceFileName(file)),
		_level(toAbort ? LFATAL : LERROR),
		_line(line)
	{
		_stream << _time.toFormattedString() << "-" << LogLevelName[_level] << ": ";

	}

	Logger::~Logger()
	{
		_stream << " - " << _basename << ':' << _line << '\n';
		const LogStream::Buffer& buf(stream().buffer());
		g_output(buf.data(), buf.length());
		if (_level == LFATAL)
		{
			g_flush();
			abort();
		}
	}

	const char* Logger::sourceFileName(const char *path)
	{
#ifdef INWINDOWS
		const char* slash = ::strrchr(path, '\\');
#else
		const char* slash = ::strrchr(path, '/');
#endif // INWINDOWS

		if (slash)
		{
			return slash + 1;
		}
		else
		{
			return path;
		}
	}

	Logger::LogLevel Logger::logLevel()
	{
		return g_logLevel;
	}

	void Logger::setLogLevel(LogLevel level)
	{
		g_logLevel = level;
	}


	void Logger::setOutput(OutputFunc out)
	{
		g_output = out;
	}

	void Logger::setFlush(FlushFunc flush)
	{
		g_flush = flush;
	}


}

