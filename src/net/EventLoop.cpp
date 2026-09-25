//////////////////////////////////////////////////////////////////////////////////
//�ļ���EventLoop.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������ʱ��ѭ��
////////////////////////////////////////////////////////////////////////////////////

#include "EventLoop.h"
#include <base/Logging.h>
#include <base/Thread.h>
#include "Channel.h"
#include "Poller.h"
#include "TimerQueue.h"
#include <assert.h>

namespace EtherDB
{
	namespace Net
	{
		__thread EventLoop* t_loopInThisThread = 0;
		const int kPollTimeMs = 10000;

		EventLoop::EventLoop()
			:_looping(false),
			_quit(false),
			_eventHandling(false),
			_callingPendingFunctors(false),
			_iteration(0),
			_threadId(Thread::currentTid()),
			_poller(Poller::newDefaultPoller(this)),
			_timerQueue(new TimerQueue(this)),
			_currentActiveChannel(NULL)
		{
			LOG_TRACE << "EventLoop created " << this << " in thread " << _threadId;
			if (t_loopInThisThread)
			{
				LOG_FATAL << "Another EventLoop " << t_loopInThisThread
					<< " exists in this thread " << _threadId;
			}
			else
			{
				t_loopInThisThread = this;
			}
		}

		EventLoop::~EventLoop()
		{
			LOG_TRACE << "EventLoop " << this << " of thread " << _threadId
				<< " destructs in thread " << Thread::currentTid();
			t_loopInThisThread = NULL;
		}

		void EventLoop::loop()
		{
			assert(!_looping);
			assertInLoopThread();
			_looping = true;
			_quit = false;
			LOG_TRACE << "EventLoop " << this << " start looping";
			
			while (!_quit)
			{
				_activeChannels.clear();
				_pollReturnTime = _poller->poll(kPollTimeMs, &_activeChannels);
				++_iteration;
				if (Logger::logLevel() <= Logger::LTRACE)
				{
					printActiveChannels();
				}
				_eventHandling = true;
				for (ChannelList::iterator it = _activeChannels.begin();
					it != _activeChannels.end(); ++it)
				{
					_currentActiveChannel = *it;
					_currentActiveChannel->handleEvent(_pollReturnTime);
				}
				_currentActiveChannel = NULL;
				_eventHandling = false;
				doPendingFunctors();
			}
			LOG_TRACE << "EventLoop " << this << " stop looping";
			_looping = false;
		}

		void EventLoop::quit()
		{
			_quit = true;
			if (!isInLoopThread())
			{
				wakeup();
			}
		}

		Timestamp EventLoop::pollReturnTime() const
		{
			return _pollReturnTime;
		}

		int64_t EventLoop::iteration() const
		{
			return _iteration;
		}

		void EventLoop::runInLoop(const Functor& cb)
		{
			if (isInLoopThread())
			{
				cb();
			}
			else
			{
				queueInLoop(cb);
			}
		}

		void EventLoop::queueInLoop(const Functor& cb)
		{
			{
				MutexLock lock(_mutex);
				_pendingFunctors.push_back(cb);
			}
			if (!isInLoopThread() || _callingPendingFunctors)
			{
				wakeup();
			}
		}

		size_t EventLoop::queueSize() const
		{
			MutexLock lock(_mutex);
			return _pendingFunctors.size();
		}

		TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
		{
			return _timerQueue->addTimer(cb, time, 0.0);
		}

		TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
		{
			Timestamp time(addTime(Timestamp::now(), delay));
			return runAt(time, cb);
		}

		TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
		{
			Timestamp time(addTime(Timestamp::now(), interval));
			return _timerQueue->addTimer(cb, time, interval);
		}

		void EventLoop::cancel(TimerId timerId)
		{
			return _timerQueue->cancel(timerId);
		}

		void EventLoop::wakeup()
		{
			_poller->wakeup();
		}

		void EventLoop::updateChannel(Channel* channel)
		{
			assert(channel->ownerLoop() == this);
			assertInLoopThread();
			_poller->updateChannel(channel);
		}

		void EventLoop::removeChannel(Channel* channel)
		{
			assert(channel->ownerLoop() == this);
			assertInLoopThread();
			if (_eventHandling)
			{
				assert(_currentActiveChannel == channel ||
					std::find(_activeChannels.begin(), _activeChannels.end(), channel) == _activeChannels.end());
			}
			_poller->removeChannel(channel);
		}

		bool EventLoop::hasChannel(Channel* channel)
		{
			assert(channel->ownerLoop() == this);
			assertInLoopThread();
			return _poller->hasChannel(channel);
		}

		void EventLoop::assertInLoopThread()
		{
			if (!isInLoopThread())
			{
				abortNotInLoopThread();
			}
		}

		bool EventLoop::isInLoopThread() const
		{
			return _threadId == Thread::currentTid();
		}

		bool EventLoop::eventHandling() const
		{
			return _eventHandling;
		}

		void EventLoop::setContext(std::string key, const Any& value)
		{
			_context[key] = value;
		}

		const Any& EventLoop::getContext(std::string key) const
		{
			if (_context.find(key) == _context.end())
			{
				_context[key] = Any();
			}
			return _context[key];
		}

		void EventLoop::abortNotInLoopThread()
		{
			LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
				<< " was created in threadId_ = " << _threadId
				<< ", current thread id = " << Thread::currentTid();
		}

		void EventLoop::doPendingFunctors()
		{
			std::vector<Functor> functors;
			_callingPendingFunctors = true;

			{
				MutexLock lock(_mutex);
				functors.swap(_pendingFunctors);
			}

			for (size_t i = 0; i < functors.size(); ++i)
			{
				functors[i]();
			}
			_callingPendingFunctors = false;
		}

		void EventLoop::printActiveChannels() const
		{
			for (ChannelList::const_iterator it = _activeChannels.begin();
				it != _activeChannels.end(); ++it)
			{
				const Channel* ch = *it;
				LOG_TRACE << "{" << ch->reventsToString() << "} ";
			}
		}

		EventLoop* EventLoop::getEventLoopOfCurrentThread()
		{
			return t_loopInThisThread;
		}

	}
}

