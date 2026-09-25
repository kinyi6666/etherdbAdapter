//////////////////////////////////////////////////////////////////////////////////
//�ļ���IOCPPoller.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_IOCPPoller_H_
#define __EtherDB_IOCPPoller_H_

#include "AmNetConfig.h"

#ifdef INWINDOWS

#include "Poller.h"
#include <base/AmWindows.h>

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API IOCPPoller : public Poller
		{
		public:
			IOCPPoller(EventLoop *loop);
			virtual ~IOCPPoller();

			virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels);
			virtual void wakeup();
			virtual void updateChannel(Channel* channel);
			virtual void removeChannel(Channel* channel);

		private:
			HANDLE _comPort;
		};
	}
}


#endif // INWINDOWS



#endif // !__EtherDB_IOCPPoller_H_


