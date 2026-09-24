//////////////////////////////////////////////////////////////////////////////////
//�ļ���Acceptor.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Acceptor
////////////////////////////////////////////////////////////////////////////////////

#include "Acceptor.h"
#include <base/Logging.h>

namespace EtherDB
{
	namespace Net
	{
		Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
			:_loop(loop),
			_acceptSocket(TcpSocket::createNonblockingOrDie(true)),
			_acceptChannel(loop, _acceptSocket.fd()),
			_listenning(false)
		{
			_acceptSocket.setReuseAddr(true);
			_acceptSocket.setReusePort(reuseport);
			_acceptSocket.bindAddress(listenAddr);
			_acceptChannel.setReadCallback(
				std::bind(&Acceptor::handleRead, this));
		}

		Acceptor::~Acceptor()
		{
			_loop->assertInLoopThread();
			_acceptChannel.disableAll();
			_acceptChannel.remove();
		}

		void Acceptor::listen()
		{
			_loop->assertInLoopThread();
			_listenning = true;
			_acceptChannel.enableReading();
			_acceptSocket.listen();
		}

		void Acceptor::handleRead()
		{
			_loop->assertInLoopThread();
			InetAddress peerAddr;
			int connfd = _acceptSocket.accept(&peerAddr);
			if (connfd > 0)
			{
				if (_newConnectionCallback)
				{
					_newConnectionCallback(connfd, peerAddr);
				}
				else
				{
#ifndef INWINDOWS
					::close(connfd);
#else
					::closesocket(connfd);
#endif // !INWINDOWS
				}
			}
			else
			{
				LOG_SYSERR << "Acceptor::handleRead Error";
			}
		}

	}
}




