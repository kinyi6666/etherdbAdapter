//////////////////////////////////////////////////////////////////////////////////
//�ļ���TcpSocket.h  
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Tcp�׽���
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_TcpSocket_H_
#define __EtherDB_Net_TcpSocket_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include "InetAddress.h"
#include "NetEnvInit.h"

#ifndef INWINDOWS

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API TcpSocket : Noncopyable
		{
		public:
			explicit TcpSocket(int sockfd);
			~TcpSocket();

			int fd() const { return _sockfd; }
			void bindAddress(const InetAddress& localaddr);
			void listen();
			int accept(InetAddress* peeraddr);
			void shutdownWrite();

			int read(char *buf, int len);
			int write(char *buf, int len);

			void setTcpNoDelay(bool on);
			void setReuseAddr(bool on);
			void setReusePort(bool on);
			void setKeepAlive(bool on);

			static SOCKET createNonblockingOrDie(bool nonblock = true);

		private:
			const SOCKET _sockfd;
		};
	}
}

#else

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API TcpSocket : Noncopyable
		{
		public:
			explicit TcpSocket(int sockfd);
			~TcpSocket();

			int fd() const { return (int)_sockfd; }
			void bindAddress(const InetAddress& localaddr);
			void listen();
			int accept(InetAddress* peeraddr);
			void shutdownWrite();

			bool readRequest(char *buf, int len);
			int readSize();
			bool writeRequest(char *buf, int len);
			int writeSize();
			void setTcpNoDelay(bool on);
			void setReuseAddr(bool on);
			void setReusePort(bool on);
			void setKeepAlive(bool on);

			static SOCKET createNonblockingOrDie(bool nonblock = true);
		private:
			const SOCKET _sockfd;
			WSATransData *_rdata;
			WSATransData *_wdata;
			char *_addr;
		};

	}
}

#endif // !INWINDOWS



#endif // !__EtherDB_Net_TcpSocket_H_


