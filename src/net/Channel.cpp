//////////////////////////////////////////////////////////////////////////////////
//�ļ���Channel.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Channel
////////////////////////////////////////////////////////////////////////////////////

#include "Channel.h"
#include "EventLoop.h"
#include "NetEnvInit.h"
#include <base/Logging.h>
#include <sstream>
#include <assert.h>

#ifndef INWINDOWS
#include <poll.h>
#endif // !INWINDOWS

namespace EtherDB
{
	namespace Net
	{
#ifndef INWINDOWS
		const int Channel::kNoneEvent = 0;
		const int Channel::kReadEvent = POLLIN | POLLPRI;
		const int Channel::kWriteEvent = POLLOUT;
#else
		const int Channel::kNoneEvent = 0;
		const int Channel::kReadEvent = 1;
		const int Channel::kWriteEvent = 2;
#endif // !INWINDOWS
		AtomicInt64 Channel::s_Count;

		Channel::Channel(EventLoop* loop, int fd)
			:_loop(loop),
			_fd(fd),
			_amfd(s_Count.incrementAndGet()),
			_events(0),
			_revents(0),
			_index(-1),
			_logHup(true),
			_tied(false),
			_eventHandling(false),
			_addedToLoop(false)
		{

		}

		Channel::~Channel()
		{
			assert(!_eventHandling);
			assert(!_addedToLoop);
			if (_loop->isInLoopThread())
			{
				assert(!_loop->hasChannel(this));
			}
		}

		void Channel::handleEvent(Timestamp receiveTime)
		{
			std::shared_ptr<void> guard;
			if (_tied)
			{
				guard = _tie.lock();
				if (guard)
				{
					handleEventWithGuard(receiveTime);
				}
			}
			else
			{
				handleEventWithGuard(receiveTime);
			}
		}

		void Channel::tie(const std::shared_ptr<void>& obj)
		{
			_tie = obj;
			_tied = true;
		}

		void Channel::remove()
		{
			assert(isNoneEvent());
			_addedToLoop = false;
			_loop->removeChannel(this);
		}


		std::string Channel::reventsToString() const
		{
			return eventsToString(_fd, _revents);
		}

		std::string Channel::eventsToString() const
		{
			return eventsToString(_fd, _events);
		}


		void Channel::update()
		{
			_addedToLoop = true;
			_loop->updateChannel(this);
		}

		void Channel::handleEventWithGuard(Timestamp receiveTime)
		{
			_eventHandling = true;
			LOG_TRACE << reventsToString();

#ifndef INWINDOWS
			if ((_revents & POLLHUP) && !(_revents & POLLIN))
			{
				if (_logHup)
				{
					LOG_WARN << "fd = " << _fd << " Channel::handle_event() POLLHUP";
				}
				if (_closeCallback) _closeCallback();
			}

			if (_revents & POLLNVAL)
			{
				LOG_WARN << "fd = " << _fd << " Channel::handle_event() POLLNVAL";
			}

			if (_revents & (POLLERR | POLLNVAL))
			{
				if (_errorCallback) _errorCallback();
			}
			if (_revents & (POLLIN | POLLPRI | POLLRDHUP))
			{
				if (_readCallback) _readCallback(receiveTime);
			}
			if (_revents & POLLOUT)
			{
				if (_writeCallback) _writeCallback();
			}
#else
			if (_revents & IOCPREAD)
			{
				if (_readCallback) _readCallback(receiveTime);
			}

			if (_revents & IOCPWRITE)
			{
				if (_writeCallback) _writeCallback();
			}

			if (_revents & IOCPERROE)
			{
				if (_errorCallback) _errorCallback();
			}
#endif // !INWINDOWS
			_eventHandling = false;
		}

		std::string Channel::eventsToString(int fd, int ev)
		{
			std::ostringstream oss;
#ifndef INWINDOWS
			oss << fd << ": ";
			if (ev & POLLIN)
				oss << "IN ";
			if (ev & POLLPRI)
				oss << "PRI ";
			if (ev & POLLOUT)
				oss << "OUT ";
			if (ev & POLLHUP)
				oss << "HUP ";
			if (ev & POLLRDHUP)
				oss << "RDHUP ";
			if (ev & POLLERR)
				oss << "ERR ";
			if (ev & POLLNVAL)
				oss << "NVAL ";
#else
			if (ev & kReadEvent)
				oss << "READ ";
			if (ev & kWriteEvent)
				oss << "Write ";
#endif // !INWINDOWS
			return oss.str().c_str();
		}

	}
}

