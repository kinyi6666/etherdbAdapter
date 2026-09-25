//////////////////////////////////////////////////////////////////////////////////
//�ļ���Timestamp.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������ʱ���
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Timestamp_H_
#define __EtherDB_Timestamp_H_

#include "EtherDBConfig.h"

namespace EtherDB
{
	class BASE_API Timestamp
	{
	public:
		Timestamp();
		explicit Timestamp(int64_t microSecondsSinceEpochArg);
		void swap(Timestamp& that);

		//ʱ���Ƿ���Ч
		bool valid() const { return _microSecondsSinceEpoch > 0; }
		//����΢����
		int64_t microSecondsSinceEpoch() const { return _microSecondsSinceEpoch; }
		//��������
		time_t secondsSinceEpoch() const { 	return static_cast<time_t>(_microSecondsSinceEpoch / kMicroSecondsPerSecond);}

		//���ع�ȥ��ʱ�䣨΢�룩
		int64_t elapsed() const;
		//ʱ���Ƿ��ѹ�
		bool isElapsed(int64_t interval) const;

		std::string toString() const;
		std::string toFormattedString(bool showMicroseconds = true) const;
		std::string toFormattedLocalString(bool showMicroseconds = true) const;
		std::string toFromattedHttp() const;

		//���ص�ǰʱ��
		static Timestamp now();
		//������Чʱ��
		static Timestamp invalid()
		{
			return Timestamp();
		}

		static Timestamp fromUnixTime(time_t t)
		{
			return fromUnixTime(t, 0);
		}

		static Timestamp fromUnixTime(time_t t, int microseconds)
		{
			return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
		}

		bool operator == (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch == ts._microSecondsSinceEpoch;
		}

		bool operator != (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch != ts._microSecondsSinceEpoch;
		}

		bool operator >  (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch > ts._microSecondsSinceEpoch;
		}

		bool operator >= (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch >= ts._microSecondsSinceEpoch;
		}

		bool operator <  (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch < ts._microSecondsSinceEpoch;
		}

		bool operator <= (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch <= ts._microSecondsSinceEpoch;
		}


		Timestamp  operator +  (int64_t d) const
		{
			return Timestamp(_microSecondsSinceEpoch + d);
		}

		int64_t  operator -  (const Timestamp& ts) const
		{
			return _microSecondsSinceEpoch - ts._microSecondsSinceEpoch;
		}

		Timestamp  operator -  (int64_t d) const
		{
			return Timestamp(_microSecondsSinceEpoch - d);
		}

		Timestamp& operator += (int64_t d)
		{
			_microSecondsSinceEpoch += d;
			return *this;
		}

		Timestamp& operator -= (int64_t d)
		{
			_microSecondsSinceEpoch -= d;
			return *this;
		}


		static const int kMicroSecondsPerSecond = 1000 * 1000;

	private:
		int64_t _microSecondsSinceEpoch;//1970-01-01 00:00:00 UTC  ΢����
	};

	//����ʱ���룩
	inline double timeDifference(Timestamp high, Timestamp low)
	{
		int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
		return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
	}

	//����ʱ�䣨�룩
	inline Timestamp addTime(Timestamp timestamp, double seconds)
	{
		int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
		return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
	}


}



#endif //!__EtherDB_Timestamp_H_

