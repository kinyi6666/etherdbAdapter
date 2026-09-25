////////////////////////////////////////////////////////////////////////////////
//�ļ���Logging.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������Ԫ����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Logging_H_
#define __EtherDB_Logging_H_

#include "EtherDBConfig.h"
#include "Timestamp.h"
#include "LogStream.h"

namespace EtherDB
{
	class BASE_API Logger
	{
	public:
		enum LogLevel
		{
			LTRACE,
			LDEBUG,
			LINFO,
			LWARN,
			LERROR,
			LFATAL,
			LNUM_LOG_LEVELS,
		};

		Logger(const char * file, int line);
		Logger(const char * file, int line, LogLevel level);
		Logger(const char * file, int line, LogLevel level, const char* func);
		Logger(const char * file, int line, bool toAbort);
		~Logger();

		LogStream& stream() { return _stream; }

		static LogLevel logLevel();
		static void setLogLevel(LogLevel level);

		typedef void(*OutputFunc)(const char* msg, int len);
		typedef void(*FlushFunc)();
		static void setOutput(OutputFunc);
		static void setFlush(FlushFunc);

	private:
		const char* sourceFileName(const char *path);

	private:
		Timestamp _time;
		LogStream _stream;
		std::string _basename;
		LogLevel _level;
		int _line;
	};

#define LOG_TRACE if (EtherDB::Logger::logLevel() <= EtherDB::Logger::LTRACE) \
  EtherDB::Logger(__FILE__, __LINE__, EtherDB::Logger::LTRACE).stream()
#define LOG_DEBUG if (EtherDB::Logger::logLevel() <= EtherDB::Logger::LDEBUG) \
  EtherDB::Logger(__FILE__, __LINE__, EtherDB::Logger::LDEBUG).stream()
#define LOG_INFO if (EtherDB::Logger::logLevel() <= EtherDB::Logger::LINFO) \
  EtherDB::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN EtherDB::Logger(__FILE__, __LINE__, EtherDB::Logger::LWARN).stream()
#define LOG_ERROR EtherDB::Logger(__FILE__, __LINE__, EtherDB::Logger::LERROR).stream()
#define LOG_FATAL EtherDB::Logger(__FILE__, __LINE__, EtherDB::Logger::LFATAL).stream()
#define LOG_SYSERR EtherDB::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL EtherDB::Logger(__FILE__, __LINE__, true).stream()

}
#endif // !__EtherDB_Logging_H_




