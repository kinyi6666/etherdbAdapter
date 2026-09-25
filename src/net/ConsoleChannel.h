//////////////////////////////////////////////////////////////////////////////////
//�ļ���ConsoleChannel.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//����������̨
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_ConsoleChannel_H_
#define __EtherDB_Net_ConsoleChannel_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/AtomicInt.h>
#include "Channel.h"
#include "Buffer.h"
#include "NetEnvInit.h"

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API ConsoleChannel : Noncopyable
		{
		public:
			typedef std::function<void(Buffer *)> ReadCallBack;

			ConsoleChannel(EventLoop *loop);
			~ConsoleChannel();

			void setReadCallback(const ReadCallBack &cb)
			{
				_readCallback = cb;
			}

		private:
			void handleRead(Timestamp receiveTime);

			static void defaultHandle(Buffer *buf)
			{
				buf->retrieveAll();
			}

		private:
			EventLoop *_loop;
			int _fd;
			std::unique_ptr<Channel> _channel;
			ReadCallBack _readCallback;
			Buffer _readBuffer;

#ifdef INWINDOWS
			WSATransData *_rdata;
			bool _reading;
#endif // INWINDOWS
			static AtomicInt32 s_count;
		};
	}
}

#endif // !__EtherDB_Net_ConsoleChannel_H_


