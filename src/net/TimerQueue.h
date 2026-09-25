//////////////////////////////////////////////////////////////////////////////////
//�ļ���TimerQueue.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//��������ʱ������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_TimerQueue_H_
#define __EtherDB_Net_TimerQueue_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/Mutex.h>
#include <base/Timestamp.h>
#include "Callbacks.h"
#include "Channel.h"
#include <set>
#include <vector>

namespace EtherDB
{
	namespace Net
	{
		class EventLoop;
		class Timer;
		class TimerId;

		class AMNET_API TimerQueue : Noncopyable
		{
		public:
			explicit TimerQueue(EventLoop* loop);
			~TimerQueue();

			TimerId addTimer(const TimerCallback& cb,
				Timestamp when,
				double interval);

			void cancel(TimerId timerId);

#ifdef INWINDOWS
			void handle();
#endif // INWINDOWS

		private:
			typedef std::pair<Timestamp, Timer*> Entry;
			typedef std::set<Entry> TimerList;
			typedef std::pair<Timer*, int64_t> ActiveTimer;
			typedef std::set<ActiveTimer> ActiveTimerSet;

			void addTimerInLoop(Timer* timer);
			void cancelInLoop(TimerId timerId);


			void handleInLoop();
			std::vector<Entry> getExpired(Timestamp now);
			void reset(const std::vector<Entry>& expired, Timestamp now);
			bool insert(Timer* timer);

		private:
			EventLoop * _loop;
			const int _timerfd;
#ifndef INWINDOWS
			Channel _timerfdChannel;
#endif // !INWINDOWS
			TimerList _timers;
			ActiveTimerSet _activeTimers;
			bool _callingExpiredTimers;
			ActiveTimerSet _cancelingTimers;
		};
	}
}


#endif // !__EtherDB_Net_TimerQueue_H_


