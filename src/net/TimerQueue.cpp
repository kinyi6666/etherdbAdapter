//////////////////////////////////////////////////////////////////////////////////
//???TimerQueue.cpp 
//???LSPZ
//???2018-01-02
//????????
////////////////////////////////////////////////////////////////////////////////////

#include "TimerQueue.h"
#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"
#include "base/Logging.h"
#include <functional>
#include <assert.h>

#ifndef INWINDOWS
#include <sys/timerfd.h>
#include <unistd.h>

namespace EtherDB
{
	namespace Net
	{
		namespace detail
		{

			int createTimerfd()
			{
				int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
					TFD_NONBLOCK | TFD_CLOEXEC);
				if (timerfd < 0)
				{
					LOG_SYSFATAL << "Failed in timerfd_create";
				}
				return timerfd;
			}

			struct timespec howMuchTimeFromNow(Timestamp when)
			{
				int64_t microseconds = when.microSecondsSinceEpoch()
					- Timestamp::now().microSecondsSinceEpoch();
				if (microseconds < 100)
				{
					microseconds = 100;
				}
				struct timespec ts;
				ts.tv_sec = static_cast<time_t>(
					microseconds / Timestamp::kMicroSecondsPerSecond);
				ts.tv_nsec = static_cast<long>(
					(microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
				return ts;
			}

			void readTimerfd(int timerfd, Timestamp now)
			{
				uint64_t howmany;
				ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
				LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
				if (n != sizeof howmany)
				{
					LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
				}
			}

			void resetTimerfd(int timerfd, Timestamp expiration)
			{
				// wake up loop by timerfd_settime()
				struct itimerspec newValue;
				struct itimerspec oldValue;
				bzero(&newValue, sizeof newValue);
				bzero(&oldValue, sizeof oldValue);
				newValue.it_value = howMuchTimeFromNow(expiration);
				int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
				if (ret)
				{
					LOG_SYSERR << "timerfd_settime()";
				}
			}
		}

		TimerQueue::TimerQueue(EventLoop* loop)
			:_loop(loop),
			_timerfd(detail::createTimerfd()),
			_timerfdChannel(loop, _timerfd),
			_timers(),
			_callingExpiredTimers(false)
		{
			_timerfdChannel.setReadCallback(
				std::bind(&TimerQueue::handleInLoop, this));
			_timerfdChannel.enableReading();
		}

		TimerQueue::~TimerQueue()
		{
			_timerfdChannel.disableAll();
			_timerfdChannel.remove();
			::close(_timerfd);
			for (TimerList::iterator it = _timers.begin();
				it != _timers.end(); ++it)
			{
				delete it->second;
			}
		}


		TimerId TimerQueue::addTimer(const TimerCallback& cb,
			Timestamp when,
			double interval)
		{
			Timer* timer = new Timer(cb, when, interval);
			_loop->runInLoop(
				std::bind(&TimerQueue::addTimerInLoop, this, timer));
			return TimerId(timer, timer->sequence());
		}


		void TimerQueue::cancel(TimerId timerId)
		{
			_loop->runInLoop(
				std::bind(&TimerQueue::cancelInLoop, this, timerId));
		}


		void TimerQueue::addTimerInLoop(Timer* timer)
		{
			_loop->assertInLoopThread();
			bool earliestChanged = insert(timer);
			if (earliestChanged)
			{
				detail::resetTimerfd(_timerfd, timer->expiration());
			}
		}

		void TimerQueue::cancelInLoop(TimerId timerId)
		{
			_loop->assertInLoopThread();
			assert(_timers.size() == _activeTimers.size());
			ActiveTimer timer(timerId._timer, timerId._sequence);
			ActiveTimerSet::iterator it = _activeTimers.find(timer);
			if (it != _activeTimers.end())
			{
				size_t n = _timers.erase(Entry(it->first->expiration(), it->first));
				assert(n == 1); (void)n;
				delete it->first; // FIXME: no delete please
				_activeTimers.erase(it);
			}
			else if (_callingExpiredTimers)
			{
				_cancelingTimers.insert(timer);
			}
		}



		void TimerQueue::handleInLoop()
		{
			_loop->assertInLoopThread();
			Timestamp now(Timestamp::now());

			std::vector<Entry> expired = getExpired(now);
			_callingExpiredTimers = true;
			_cancelingTimers.clear();
			for (std::vector<Entry>::iterator it = expired.begin();
				it != expired.end(); ++it)
			{
				it->second->run();
			}
			_callingExpiredTimers = false;

			reset(expired, now);
		}

		std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
		{
			assert(_timers.size() == _activeTimers.size());
			std::vector<Entry> expired;
			Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
			TimerList::iterator end = _timers.lower_bound(sentry);
			assert(end == _timers.end() || now < end->first);

			for (auto it = _timers.begin(); it != end; it++)
			{
				expired.push_back(*it);
			}
			//std::copy(_timers.begin(), end, expired.begin());//back_inserter
			_timers.erase(_timers.begin(), end);

			for (std::vector<Entry>::iterator it = expired.begin();
				it != expired.end(); ++it)
			{
				ActiveTimer timer(it->second, it->second->sequence());
				size_t n = _activeTimers.erase(timer);
				assert(n == 1); (void)n;
			}

			assert(_timers.size() == _activeTimers.size());
			return expired;
		}

		void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
		{
			Timestamp nextExpire;
			for (std::vector<Entry>::const_iterator it = expired.begin();
				it != expired.end(); ++it)
			{
				ActiveTimer timer(it->second, it->second->sequence());
				if (it->second->repeat()
					&& _cancelingTimers.find(timer) == _cancelingTimers.end())
				{
					it->second->restart(now);
					insert(it->second);
				}
				else
				{
					delete it->second;
				}
			}

			if (!_timers.empty())
			{
				nextExpire = _timers.begin()->second->expiration();
			}

			if (nextExpire.valid())
			{
				detail::resetTimerfd(_timerfd, nextExpire);
			}
		}

		bool TimerQueue::insert(Timer* timer)
		{
			_loop->assertInLoopThread();
			assert(_timers.size() == _activeTimers.size());
			bool earliestChanged = false;
			Timestamp when = timer->expiration();
			TimerList::iterator it = _timers.begin();
			if (it == _timers.end() || when < it->first)
			{
				earliestChanged = true;
			}
			{
				std::pair<TimerList::iterator, bool> result
					= _timers.insert(Entry(when, timer));
				assert(result.second); (void)result;
			}
			{
				std::pair<ActiveTimerSet::iterator, bool> result
					= _activeTimers.insert(ActiveTimer(timer, timer->sequence()));
				assert(result.second); (void)result;
			}

			assert(_timers.size() == _activeTimers.size());
			return earliestChanged;
		}

	}
}


#else

#include "base/AmWindows.h"
#include <unordered_map>
#include <mutex>

namespace EtherDB
{
	namespace Net
	{
		namespace detail
		{

			struct TimerInfo
			{
				HANDLE handle;
				void* pContext;
			};

			class TimerHandleMap
			{
			public:
				int add(HANDLE h, void* ctx)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					int id = ++m_nextId;
					m_map[id] = { h, ctx };
					return id;
				}

				TimerInfo get(int id)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					auto it = m_map.find(id);
					if (it != m_map.end())
						return it->second;
					return { nullptr, nullptr };
				}

				void updateHandle(int id, HANDLE newHandle)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					auto it = m_map.find(id);
					if (it != m_map.end())
					{
						it->second.handle = newHandle;
					}
				}

				void remove(int id)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					m_map.erase(id);
				}

			private:
				std::unordered_map<int, TimerInfo> m_map;
				int m_nextId = 0;
				std::mutex m_mutex;
			};

			static TimerHandleMap g_timerMap;

			HANDLE g_timerQueue = CreateTimerQueue();
			const DWORD delaytime = 24 * 60 * 60 * 1000;

			VOID CALLBACK timerFunc(PVOID pContext, BOOLEAN bTimeOrWait)
			{
				TimerQueue *queue = (TimerQueue*)pContext;
				queue->handle();
			}

			int createTimerfd(void *pContext)
			{
				HANDLE f;
				::CreateTimerQueueTimer(&f, g_timerQueue, WAITORTIMERCALLBACK(timerFunc), pContext, delaytime, delaytime, NULL);
				//return (int)(intptr_t)f;
				return g_timerMap.add(f, pContext);
			}

			void closeTimerfd(int f)
			{
				TimerInfo info = g_timerMap.get(f);
				if (info.handle)
				{
					::DeleteTimerQueueTimer(g_timerQueue, info.handle, NULL);
					g_timerMap.remove(f);
				}
				//::DeleteTimerQueueTimer(g_timerQuue, (HANDLE)f, 0);
			}

			void resetTimerfd(int timerfd, Timestamp expiration)
			{
				if (expiration.valid())
				{
					Timestamp now = Timestamp::now();
					int diff = (int)(timeDifference(expiration, now));
					if (diff < 100)
					{
						diff = 100;
					}
				
					TimerInfo info = g_timerMap.get(timerfd);
					if (!info.handle)
						return;

					::DeleteTimerQueueTimer(g_timerQueue, info.handle, NULL);

					HANDLE newHandle = nullptr;
					::CreateTimerQueueTimer(&newHandle, g_timerQueue,
						WAITORTIMERCALLBACK(timerFunc),
						info.pContext, diff, delaytime, NULL);

					if (newHandle)
					{
						g_timerMap.updateHandle(timerfd, newHandle);
					}
					//::ChangeTimerQueueTimer(g_timerQueue, (HANDLE)timerfd, diff, delaytime);
				}
			}
		}

		using namespace detail;

		TimerQueue::TimerQueue(EventLoop* loop)
			:_loop(loop),
			_timerfd(detail::createTimerfd(this)),
			_callingExpiredTimers(false)
		{

		}

		TimerQueue::~TimerQueue()
		{
			detail::closeTimerfd(_timerfd);
			for (TimerList::iterator it = _timers.begin();
				it != _timers.end(); ++it)
			{
				delete it->second;
			}
		}


		TimerId TimerQueue::addTimer(const TimerCallback& cb,
			Timestamp when,
			double interval)
		{
			Timer* timer = new Timer(cb, when, interval);
			_loop->runInLoop(
				std::bind(&TimerQueue::addTimerInLoop, this, timer));
			return TimerId(timer, timer->sequence());
		}

		void TimerQueue::addTimerInLoop(Timer* timer)
		{
			_loop->assertInLoopThread();
			bool earliestChanged = insert(timer);
			if (earliestChanged)
			{
				detail::resetTimerfd(_timerfd, timer->expiration());
			}
		}

		void TimerQueue::cancel(TimerId timerId)
		{
			_loop->runInLoop(
				std::bind(&TimerQueue::cancelInLoop, this, timerId));
		}

		void TimerQueue::cancelInLoop(TimerId timerId)
		{
			_loop->assertInLoopThread();
			assert(_timers.size() == _activeTimers.size());
			ActiveTimer timer(timerId._timer, timerId._sequence);
			ActiveTimerSet::iterator it = _activeTimers.find(timer);
			if (it != _activeTimers.end())
			{
				size_t n = _timers.erase(Entry(it->first->expiration(), it->first));
				assert(n == 1); (void)n;
				delete it->first; // FIXME: no delete please
				_activeTimers.erase(it);
			}
			else if (_callingExpiredTimers)
			{
				_cancelingTimers.insert(timer);
			}
		}

		void TimerQueue::handle()
		{
			_loop->runInLoop(
				std::bind(&TimerQueue::handleInLoop, this));
		}

		void TimerQueue::handleInLoop()
		{
			_loop->assertInLoopThread();
			Timestamp now(Timestamp::now());

			std::vector<Entry> expired = getExpired(now);
			_callingExpiredTimers = true;
			_cancelingTimers.clear();
			for (std::vector<Entry>::iterator it = expired.begin();
				it != expired.end(); ++it)
			{
				it->second->run();
			}
			_callingExpiredTimers = false;

			reset(expired, now);
		}

		std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
		{
			assert(_timers.size() == _activeTimers.size());
			std::vector<Entry> expired;
			Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
			TimerList::iterator end = _timers.lower_bound(sentry);
			assert(end == _timers.end() || now < end->first);

			for (auto it = _timers.begin(); it != end; it++)
			{
				expired.push_back(*it);
			}
			//std::copy(_timers.begin(), end, expired.begin());//back_inserter
			_timers.erase(_timers.begin(), end);

			for (std::vector<Entry>::iterator it = expired.begin();
				it != expired.end(); ++it)
			{
				ActiveTimer timer(it->second, it->second->sequence());
				size_t n = _activeTimers.erase(timer);
				assert(n == 1); (void)n;
			}

			assert(_timers.size() == _activeTimers.size());
			return expired;
		}

		void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
		{
			Timestamp nextExpire;
			for (std::vector<Entry>::const_iterator it = expired.begin();
				it != expired.end(); ++it)
			{
				ActiveTimer timer(it->second, it->second->sequence());
				if (it->second->repeat()
					&& _cancelingTimers.find(timer) == _cancelingTimers.end())
				{
					it->second->restart(now);
					insert(it->second);
				}
				else
				{
					delete it->second;
				}
			}

			if (!_timers.empty())
			{
				nextExpire = _timers.begin()->second->expiration();
			}

			if (nextExpire.valid())
			{
				detail::resetTimerfd(_timerfd, nextExpire);
			}
		}

		bool TimerQueue::insert(Timer* timer)
		{
			_loop->assertInLoopThread();
			assert(_timers.size() == _activeTimers.size());
			bool earliestChanged = false;
			Timestamp when = timer->expiration();
			TimerList::iterator it = _timers.begin();
			if (it == _timers.end() || when < it->first)
			{
				earliestChanged = true;
			}
			{
				std::pair<TimerList::iterator, bool> result
					= _timers.insert(Entry(when, timer));
				assert(result.second); (void)result;
			}
			{
				std::pair<ActiveTimerSet::iterator, bool> result
					= _activeTimers.insert(ActiveTimer(timer, timer->sequence()));
				assert(result.second); (void)result;
			}

			assert(_timers.size() == _activeTimers.size());
			return earliestChanged;
		}

	}
}


#endif // !INWINDOWS




