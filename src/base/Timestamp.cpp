//////////////////////////////////////////////////////////////////////////////////
//�ļ���Timestamp.cpp
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������ʱ���
////////////////////////////////////////////////////////////////////////////////////

#include "Timestamp.h"
#include "AmWindows.h"
#include <time.h>


#ifndef INWINDOWS

#include <sys/time.h>

#else


int
gettimeofday(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return (0);
}


#endif // !INWINDOWS

namespace EtherDB
{

	const std::string WEEKDAY_NAMES[] =
	{
		"Sun",
		"Mon",
		"Tue",
		"Wed",
		"Thu",
		"Fri",
		"Sat"
	};


	const std::string MONTH_NAMES[] =
	{
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec"
	};

	Timestamp::Timestamp()
		:_microSecondsSinceEpoch(0)
	{

	}

	Timestamp::Timestamp(int64_t microSecondsSinceEpochArg)
		: _microSecondsSinceEpoch(microSecondsSinceEpochArg)
	{

	}

	void Timestamp::swap(Timestamp& that)
	{
		std::swap(_microSecondsSinceEpoch, that._microSecondsSinceEpoch);
	}

	int64_t Timestamp::elapsed() const
	{
		Timestamp now(Timestamp::now());
		return now - *this;
	}

	bool Timestamp::isElapsed(int64_t interval) const
	{
		Timestamp now;
		int64_t diff = now - *this;
		return diff >= interval;
	}

	std::string Timestamp::toString() const
	{
		char buf[32] = { 0 };
		int64_t seconds = _microSecondsSinceEpoch / kMicroSecondsPerSecond;
		int64_t microseconds = _microSecondsSinceEpoch % kMicroSecondsPerSecond;
		snprintf(buf, sizeof(buf) - 1, "%lld.%lld06", seconds, microseconds);
		return buf;
	}

	std::string Timestamp::toFormattedString(bool showMicroseconds) const
	{
		char buf[32] = { 0 };
		time_t seconds = static_cast<time_t>(_microSecondsSinceEpoch / kMicroSecondsPerSecond);
		struct tm tm_time;

#ifdef INWINDOWS
		gmtime_s(&tm_time, &seconds);
#else
		gmtime_r(&seconds, &tm_time);
#endif // INWINDOWS
		if (showMicroseconds)
		{
			int microseconds = static_cast<int>(_microSecondsSinceEpoch % kMicroSecondsPerSecond);
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06d",
				tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
				tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
				microseconds);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
				tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
				tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
		}
		return buf;
	}

	std::string Timestamp::toFormattedLocalString(bool showMicroseconds) const
	{
		char buf[32] = { 0 };
		time_t seconds = static_cast<time_t>(_microSecondsSinceEpoch / kMicroSecondsPerSecond);
		struct tm tm_time;

#ifdef INWINDOWS
		gmtime_s(&tm_time, &seconds);
#else
		gmtime_r(&seconds, &tm_time);
#endif // INWINDOWS

		tm_time.tm_mday += (tm_time.tm_hour + 8) / 24;
		tm_time.tm_hour = (tm_time.tm_hour + 8) % 24;

		if (showMicroseconds)
		{
			int microseconds = static_cast<int>(_microSecondsSinceEpoch % kMicroSecondsPerSecond);
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06d",
				tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
				tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
				microseconds);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
				tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
				tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
		}
		return buf;
	}

	std::string Timestamp::toFromattedHttp() const
	{
		char buf[48] = { 0 };
		time_t seconds = static_cast<time_t>(_microSecondsSinceEpoch / kMicroSecondsPerSecond);
		struct tm tm_time;

#ifdef INWINDOWS
		gmtime_s(&tm_time, &seconds);
#else
		gmtime_r(&seconds, &tm_time);
#endif // INWINDOWS

		
		snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
			WEEKDAY_NAMES[tm_time.tm_wday].c_str(), tm_time.tm_mday, MONTH_NAMES[tm_time.tm_mon].c_str(), tm_time.tm_year + 1900,
			tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
		return buf;
	}

	Timestamp Timestamp::now()
	{
		struct timeval tv;
		::gettimeofday(&tv, NULL);
		int64_t seconds = tv.tv_sec;
		return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
	}

}