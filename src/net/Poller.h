//////////////////////////////////////////////////////////////////////////////////
//�ļ���Poller.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Poller
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_Poller_H_
#define __EtherDB_Net_Poller_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/Timestamp.h>
#include <vector>
#include <map>

namespace EtherDB
{
	namespace Net
	{
		class EventLoop;
		class Channel;
		class AMNET_API Poller : Noncopyable
		{
		public:
			typedef std::vector<Channel*> ChannelList;

			Poller(EventLoop*);
			virtual ~Poller();

			virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
			virtual void wakeup() = 0;
			virtual void updateChannel(Channel* channel) = 0;
			virtual void removeChannel(Channel* channel) = 0;
			virtual bool hasChannel(Channel* channel) const;

			void assertInLoopThread() const;

			static Poller* newDefaultPoller(EventLoop* loop);

		protected:
			typedef std::map<int, Channel*> ChannelMap;
			ChannelMap _channels;
			EventLoop * _ownerLoop;
		};
	}
}


#endif // !__EtherDB_Net_Poller_H_


