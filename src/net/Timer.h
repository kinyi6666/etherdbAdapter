//////////////////////////////////////////////////////////////////////////////////
//�ļ���Timer.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//��������ʱ��
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_Timer_H_
#define __EtherDB_Net_Timer_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/AtomicInt.h>
#include <base/Timestamp.h>
#include "Callbacks.h"

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API Timer : Noncopyable
		{
		public:
			Timer(const TimerCallback& cb, Timestamp when, double interval)
				:_callback(cb),
				_expiration(when),
				_interval(interval),
				_repeat(interval > 0.0),
				_sequence(_s_numCreated.incrementAndGet())
			{

			}

			~Timer()
			{

			}

			void run() const
			{
				_callback();
			}

			Timestamp expiration() const { return _expiration; }
			bool repeat() const { return _repeat; }
			int64_t sequence() const { return _sequence; }

			void restart(Timestamp now);
			static int64_t numCreated() { return _s_numCreated.get(); }

		private:
			const TimerCallback _callback;
			Timestamp _expiration;
			const double _interval;
			const bool _repeat;
			const int64_t _sequence;

			static AtomicInt64 _s_numCreated;
		};

	}
}




#endif // !__EtherDB_Net_Timer_H_



