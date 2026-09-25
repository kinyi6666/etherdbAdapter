//////////////////////////////////////////////////////////////////////////////////
//�ļ���Acceptor.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Acceptor
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_Acceptor_H_
#define __EtherDB_Net_Acceptor_H_

#include "AmNetConfig.h"
#include "base/Noncopyable.h"
#include "Channel.h"
#include "InetAddress.h"
#include "EventLoop.h"
#include "TcpSocket.h"
#include <functional>


namespace EtherDB
{
	namespace Net
	{
		class AMNET_API Acceptor : Noncopyable
		{
		public:
			typedef std::function<void(int sockfd, const InetAddress&)> NewConnectionCallback;

			Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport = false);
			~Acceptor();

			void setNewConnectionCallback(const NewConnectionCallback& cb)
			{
				_newConnectionCallback = cb;
			}

			bool listenning() const { return _listenning; }
			void listen();

		private:
			void handleRead();

		private:
			EventLoop * _loop;
			TcpSocket _acceptSocket;
			Channel _acceptChannel;
			NewConnectionCallback _newConnectionCallback;
			bool _listenning;
		};
	}
}



#endif // !__EtherDB_Net_Acceptor_H_

