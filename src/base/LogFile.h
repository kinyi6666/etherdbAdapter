////////////////////////////////////////////////////////////////////////////////
//�ļ���LogFile.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������Ԫ����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_LogFile_H_
#define __EtherDB_LogFile_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"
#include "Mutex.h"
#include <memory>

namespace EtherDB
{

	class BASE_API LogFile :Noncopyable
	{
	public:
		LogFile(const std::string& basename,
			off_t rollSize,
			bool threadSafe = true,
			int flushInterval = 3,
			int checkEveryN = 1024);
		~LogFile();

		void append(const char* logline, int len);
		void flush();
		bool rollFile();

	private:
		void append_unlocked(const char* logline, int len);
		static std::string getLogFileName(const std::string& basename, time_t* now);

	private:
		class AppendFile
		{
		public:
			explicit AppendFile(std::string filename);
			~AppendFile();

			void append(const char* logline, const size_t len);
			void flush();
			off_t writtenBytes() const { return writtenBytes_; }

		private:
			size_t write(const char* logline, size_t len);

			FILE* fp_;
			char buffer_[64 * 1024];
			off_t writtenBytes_;
		};

		const std::string _basename;
		const off_t _rollSize;
		const int _flushInterval;
		const int _checkEveryN;

		int _count;
		std::unique_ptr<Mutex> _mutex;
		time_t _startOfPeriod;
		time_t _lastRoll;
		time_t _lastFlush;
		std::unique_ptr<AppendFile> _file;

		const static int _kRollPerSeconds = 60 * 60 * 24;
	};
}

#endif // !__EtherDB_LogFile_H_

