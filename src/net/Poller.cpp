//////////////////////////////////////////////////////////////////////////////////
//�ļ���Poller.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Poller
////////////////////////////////////////////////////////////////////////////////////

#include "Poller.h"
#include "EventLoop.h"
#include "Channel.h"
#include "IOCPPoller.h"
#include "EPollPoller.h"

namespace EtherDB
{
	namespace Net
	{
		Poller::Poller(EventLoop* loop)
			:_ownerLoop(loop)
		{}

		Poller::~Poller()
		{}

		bool Poller::hasChannel(Channel* channel) const
		{
			assertInLoopThread();
			ChannelMap::const_iterator it = _channels.find(channel->fd());
			return it != _channels.end() && it->second == channel;
		}

		void Poller::assertInLoopThread() const
		{
			_ownerLoop->assertInLoopThread();
		}

		Poller* Poller::newDefaultPoller(EventLoop* loop)
		{
#ifndef INWINDOWS
			return new EPollPoller(loop);
#else
			return new IOCPPoller(loop);
#endif // !INWINDOWS
		}

	}
}


