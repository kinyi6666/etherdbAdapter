//////////////////////////////////////////////////////////////////////////////////
//�ļ���EPollPoller.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_EPollPoller_H_
#define __EtherDB_Net_EPollPoller_H_

#include "AmNetConfig.h"

#ifndef INWINDOWS

#include "Poller.h"
#include <vector>
#include <memory>

struct epoll_event;

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API EPollPoller : public Poller
		{
		public:
			EPollPoller(EventLoop* loop);
			virtual ~EPollPoller();

			virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels);
			virtual void wakeup();
			virtual void updateChannel(Channel* channel);
			virtual void removeChannel(Channel* channel);

		private:
			void fillActiveChannels(int numEvents,
				ChannelList* activeChannels) const;
			void update(int operation, Channel* channel);
			void registerWakeFd();
			void handleRead();

			static const char* operationToString(int op);

		private:
			typedef std::vector<struct epoll_event> EventList;

			int _epollfd;
			EventList _events;
			bool _registerWk;
			int _wakeupFd;
			std::unique_ptr<Channel> _wakeupChannel;

			static const int kInitEventListSize = 16;

		};
	}
}

#endif // !INWINDOWS

#endif // !__EtherDB_Net_EPollPoller_H_


