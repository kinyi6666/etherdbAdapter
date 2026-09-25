//////////////////////////////////////////////////////////////////////////////////
//�ļ���EPollPoller.cpp  
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#include "EPollPoller.h"

#ifndef INWINDOWS
#include <base/Logging.h>
#include <base/Timestamp.h>
#include "Channel.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace EtherDB
{
	namespace Net
	{
		namespace
		{
			const int kNew = -1;
			const int kAdded = 1;
			const int kDeleted = 2;

			int createEventfd()
			{
				int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
				if (evtfd < 0)
				{
					abort();
				}
				return evtfd;
			}
		}

		EPollPoller::EPollPoller(EventLoop* loop)
			:Poller(loop),
			_epollfd(::epoll_create1(EPOLL_CLOEXEC)),
			_events(kInitEventListSize),
			_registerWk(false),
			_wakeupFd(createEventfd())
		{
			if (_epollfd < 0)
			{
				LOG_FATAL << "create epollfd fail...";
			}
		}

		EPollPoller::~EPollPoller()
		{
			if (_wakeupChannel)
			{
				_wakeupChannel->disableAll();
				_wakeupChannel->remove();
			}
			::close(_wakeupFd);
			::close(_epollfd);
		}

		Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
		{
			if (!_registerWk)
			{
				registerWakeFd();
			}

			int numEvents = ::epoll_wait(_epollfd,
				&*_events.begin(),
				static_cast<int>(_events.size()),
				timeoutMs);
			int savedErrno = errno;
			Timestamp now(Timestamp::now());
			if (numEvents > 0)
			{
				LOG_TRACE << numEvents << " events happened";
				fillActiveChannels(numEvents, activeChannels);
				if (static_cast<size_t>(numEvents) == _events.size())
				{
					_events.resize(_events.size() * 2);
				}
			}
			else if (numEvents == 0)
			{
				LOG_TRACE << "nothing happened";
			}
			else
			{
				if (savedErrno != EINTR)
				{
					errno = savedErrno;
					LOG_SYSERR << "EPollPoller::poll()";
				}
			}
			return now;
		}

		void EPollPoller::wakeup()
		{
			uint64_t one = 1;
			size_t n = ::write(_wakeupFd, &one, sizeof one);
			if (n != sizeof one)
			{
				LOG_FATAL << "write wakefd fail";
			}
		}

		void EPollPoller::updateChannel(Channel* channel)
		{
			Poller::assertInLoopThread();
			const int index = channel->index();
			LOG_TRACE << "fd = " << channel->fd()
				<< " events = " << channel->events() << " index = " << index;
			if (index == kNew || index == kDeleted)
			{
				int fd = channel->fd();
				if (index == kNew)
				{
					assert(_channels.find(fd) == _channels.end());
					_channels[fd] = channel;
				}
				else
				{
					assert(_channels.find(fd) != _channels.end());
					assert(_channels[fd] == channel);
				}

				channel->set_index(kAdded);
				update(EPOLL_CTL_ADD, channel);
			}
			else
			{
				// update existing one with EPOLL_CTL_MOD/DEL
				int fd = channel->fd();
				(void)fd;
				assert(_channels.find(fd) != _channels.end());
				assert(_channels[fd] == channel);
				assert(index == kAdded);
				if (channel->isNoneEvent())
				{
					update(EPOLL_CTL_DEL, channel);
					channel->set_index(kDeleted);
				}
				else
				{
					update(EPOLL_CTL_MOD, channel);
				}
			}
		}

		void EPollPoller::removeChannel(Channel* channel)
		{
			Poller::assertInLoopThread();
			int fd = channel->fd();
			LOG_TRACE << "fd = " << fd;
			assert(_channels.find(fd) != _channels.end());
			assert(_channels[fd] == channel);
			assert(channel->isNoneEvent());
			int index = channel->index();
			assert(index == kAdded || index == kDeleted);
			size_t n = _channels.erase(fd);
			(void)n;
			assert(n == 1);

			if (index == kAdded)
			{
				update(EPOLL_CTL_DEL, channel);
			}
			channel->set_index(kNew);
		}


		void EPollPoller::fillActiveChannels(int numEvents,
			ChannelList* activeChannels) const
		{
			assert(static_cast<size_t>(numEvents) <= _events.size());
			for (int i = 0; i < numEvents; ++i)
			{
				Channel* channel = static_cast<Channel*>(_events[i].data.ptr);
				int fd = channel->fd();
				ChannelMap::const_iterator it = _channels.find(fd);
				assert(it != _channels.end());
				assert(it->second == channel);
				channel->set_revents(_events[i].events);
				activeChannels->push_back(channel);
			}
		}

		void EPollPoller::update(int operation, Channel* channel)
		{
			struct epoll_event event;
			bzero(&event, sizeof event);
			event.events = channel->events();
			event.data.ptr = channel;
			int fd = channel->fd();
			LOG_TRACE << "epoll_ctl op = " << operationToString(operation)
				<< " fd = " << fd << " event = { " << channel->eventsToString() << " }";
			if (::epoll_ctl(_epollfd, operation, fd, &event) < 0)
			{
				if (operation == EPOLL_CTL_DEL)
				{
					LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
				}
				else
				{
					LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
				}
			}
		}

		void EPollPoller::registerWakeFd()
		{
			_wakeupChannel.reset(new Channel(_ownerLoop, _wakeupFd));
			_wakeupChannel->setReadCallback(
				std::bind(&EPollPoller::handleRead, this));
			_wakeupChannel->enableReading();
			_registerWk = true;
		}

		void EPollPoller::handleRead()
		{
			uint64_t one = 1;
			size_t n = ::read(_wakeupFd, &one, sizeof one);
			if (n != sizeof one)
			{
				LOG_FATAL << "read wakefd fail";
			}
		}


		const char* EPollPoller::operationToString(int op)
		{
			switch (op)
			{
			case EPOLL_CTL_ADD:
				return "ADD";
			case EPOLL_CTL_DEL:
				return "DEL";
			case EPOLL_CTL_MOD:
				return "MOD";
			default:
				assert(false && "ERROR op");
				return "Unknown Operation";
			}
		}

	}
}

#endif // !INWINDOWS



