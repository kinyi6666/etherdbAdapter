////////////////////////////////////////////////////////////////////////////////
//�ļ���Logging.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������Ԫ����
////////////////////////////////////////////////////////////////////////////////////

#include "LogFile.h"
#include "Timestamp.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>

namespace EtherDB
{

	LogFile::AppendFile::AppendFile(std::string filename)
		:fp_(::fopen(filename.c_str(), "a")),
		writtenBytes_(0)
	{
		assert(fp_);
	}

	LogFile::AppendFile::~AppendFile()
	{
		::fclose(fp_);
	}


	void LogFile::AppendFile::append(const char* logline, const size_t len)
	{
		size_t n = write(logline, len);
		size_t remain = len - n;
		while (remain > 0)
		{
			size_t x = write(logline + n, remain);
			if (x == 0)
			{
				int err = ferror(fp_);
				if (err)
				{
					fprintf(stderr, "AppendFile::append() failed \n");
				}
				break;
			}
			n += x;
			remain = len - n; // remain -= x
		}

		writtenBytes_ += len;
	}

	void LogFile::AppendFile::flush()
	{
		::fflush(fp_);
	}

	size_t LogFile::AppendFile::write(const char* logline, size_t len)
	{
		return ::fwrite(logline, 1, len, fp_);
	}

	LogFile::LogFile(const std::string& basename,
		off_t rollSize,
		bool threadSafe,
		int flushInterval,
		int checkEveryN)
		:_basename(basename),
		_rollSize(rollSize),
		_flushInterval(flushInterval),
		_checkEveryN(checkEveryN),
		_count(0),
		_mutex(threadSafe ? new Mutex : NULL),
		_startOfPeriod(0),
		_lastRoll(0),
		_lastFlush(0)
	{
		rollFile();
	}

	LogFile::~LogFile()
	{
	}

	void LogFile::append(const char* logline, int len)
	{
		if (_mutex)
		{
			MutexLock lock(*_mutex);
			append_unlocked(logline, len);
		}
		else
		{
			append_unlocked(logline, len);
		}
	}

	void LogFile::flush()
	{
		if (_mutex)
		{
			MutexLock lock(*_mutex);
			_file->flush();
		}
		else
		{
			_file->flush();
		}
	}

	bool LogFile::rollFile()
	{
		time_t now = 0;
		std::string filename = getLogFileName(_basename, &now);
		time_t start = now / _kRollPerSeconds * _kRollPerSeconds;

		if (now > _lastRoll)
		{
			_lastRoll = now;
			_lastFlush = now;
			_startOfPeriod = start;

			_file.reset(new AppendFile(filename));
			return true;
		}

		return false;
	}

	void LogFile::append_unlocked(const char* logline, int len)
	{
		_file->append(logline, len);
		if (_file->writtenBytes() > _rollSize)
		{
			rollFile();
		}
		else
		{
			++_count;
			if (_count >= _checkEveryN)
			{
				_count = 0;
				time_t now = ::time(NULL);
				time_t thisPeriod_ = now / _kRollPerSeconds * _kRollPerSeconds;
				if (thisPeriod_ != _startOfPeriod)
				{
					rollFile();
				}
				else if (now - _lastFlush > _flushInterval)
				{
					_lastFlush = now;
					_file->flush();
				}
			}
		}
	}

	std::string LogFile::getLogFileName(const std::string& basename, time_t* now)
	{
		std::string filename;
		filename.reserve(basename.size() + 64);
		filename = basename;

		char timebuf[32];

		*now = time(NULL);
		struct tm tm = *gmtime(now); // FIXME: localtime_r ?
		strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S", &tm);
		filename += timebuf;
		filename += ".log";
		return filename;
	}

}
