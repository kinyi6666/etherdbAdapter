//////////////////////////////////////////////////////////////////////////////////
//�ļ���UdpSocket.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Tcp�׽���
////////////////////////////////////////////////////////////////////////////////////

#include "UdpSocket.h"
#include <base/Logging.h>

#ifndef INWINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace EtherDB
{
	namespace Net
	{
		UdpSocket::UdpSocket(int sockfd)
			:_sockfd(sockfd)
		{}

		UdpSocket::~UdpSocket()
		{
			::close(_sockfd);
		}

		void UdpSocket::bindAddress(const InetAddress& localaddr)
		{
			int ret = ::bind(_sockfd, localaddr.getSockAddr(), static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
			if (ret < 0)
			{
				LOG_SYSFATAL << "sockets::bindOrDie";
			}
		
		}

		

		void UdpSocket::shutdownWrite()
		{
			::shutdown(_sockfd, SHUT_WR);
		}

		int UdpSocket::read(char *buf, int len)
		{
			struct sockaddr_in  fMsgAddr;
			#if INWINDOWS 
					int addrLen = sizeof(fMsgAddr);
			#else
				socklen_t addrLen = sizeof(fMsgAddr);
			#endif
			int iRet =  ::recvfrom(_sockfd, (signed char*)buf, len, 0, (struct sockaddr*)&fMsgAddr, &addrLen);
			_remoteAddr = ntohl(fMsgAddr.sin_addr.s_addr);//static_cast<uint32_t>(fMsgAddr.sin_addr.s_addr);//ntohl(fMsgAddr.sin_addr.s_addr);
			_remotePort = ntohs(fMsgAddr.sin_port);

			return iRet;
		}

		int UdpSocket::write(char *buf, int len)
		{
			int theErr = ::sendto(_sockfd, (signed char*)buf, len, 0, (struct sockaddr*)&saddr, sizeof(_remotePort));
			return theErr;
		}

		void UdpSocket::setDestAddr(const InetAddress& localaddr){
		
			const struct sockaddr* ptr = localaddr.getSockAddr();

			// 获取实际地址长度（这里需要知道是 IPv4 还是 IPv6）
			socklen_t addr_len = 0;
			if (ptr->sa_family == AF_INET) {
				addr_len = sizeof(struct sockaddr_in);
			} else if (ptr->sa_family == AF_INET6) {
				addr_len = sizeof(struct sockaddr_in6);
			}

			// 安全复制：限制长度不超过 saved_addr 的大小
			if (addr_len > 0 && addr_len <= sizeof(sockaddr_in6)) {
				memcpy(&saddr, ptr, addr_len);
			}
		}
		void UdpSocket::setReuseAddr(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void UdpSocket::setReusePort(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		
		SOCKET UdpSocket::createUdpSocket()
		{
			SOCKET sockfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			return sockfd;
		}

	}
}


#else

namespace EtherDB
{
	namespace Net
	{
		UdpSocket::UdpSocket(int sockfd)
			:_sockfd(sockfd),
			_rdata(NULL),
			_wdata(NULL),
			_addr(new char[(sizeof(sockaddr) + 16) * 2])
		{
		}

		UdpSocket::~UdpSocket()
		{
			::closesocket(_sockfd);
			delete[] _addr;
		}

		void UdpSocket::bindAddress(const InetAddress& localaddr)
		{
			int ret = ::bind(_sockfd, localaddr.getSockAddr(), static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
			if (ret < 0)
			{
				LOG_SYSFATAL << "sockets::bindOrDie";
			}
		}

		void UdpSocket::listen()
		{
			int ret = ::listen(_sockfd, SOMAXCONN);
			if (ret < 0)
			{
				LOG_SYSFATAL << "sockets::listenOrDie";
			}
			LPFN_ACCEPTEX lpfnAcceptEx = NULL;
			GUID guidAcceptEx = WSAID_ACCEPTEX;
			DWORD dwBytes = 0;

			if (WSAIoctl(_sockfd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&guidAcceptEx, sizeof(guidAcceptEx), &lpfnAcceptEx, sizeof(lpfnAcceptEx),
				&dwBytes, NULL, NULL) != 0)
			{
				//TODO
				throw;
			}

			_rdata = new WSATransData;
			memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
			SOCKET s = createNonblockingOrDie(true);
			_rdata->databuff.len = (sizeof(sockaddr) + 16) * 2;
			_rdata->databuff.buf = _addr;
			_rdata->event = IOCPREAD;
			_rdata->fd = _sockfd;
			_rdata->translen = 0;
			_rdata->ptr = s;
			memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
			lpfnAcceptEx(_sockfd, s, _rdata->databuff.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, 0, (LPOVERLAPPED)_rdata);

		}

		int UdpSocket::accept(InetAddress* peeraddr)
		{
			LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs = NULL;
			GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
			DWORD dwBytes;
			WSAIoctl(_sockfd, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidGetAcceptExSockAddrs,
				sizeof(GuidGetAcceptExSockAddrs), &lpfnGetAcceptExSockAddrs, sizeof(lpfnGetAcceptExSockAddrs),
				&dwBytes, NULL, NULL);

			SOCKADDR_IN* remote = NULL;
			SOCKADDR_IN* local = NULL;
			int remoteLen = sizeof(SOCKADDR_IN);
			int localLen = sizeof(SOCKADDR_IN);
			lpfnGetAcceptExSockAddrs(_rdata->databuff.buf, _rdata->databuff.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, (sockaddr **)&local, &localLen, (sockaddr**)&remote, &remoteLen);
			*peeraddr = InetAddress(*remote);
			SOCKET acceptsocket = _rdata->ptr;

			LPFN_ACCEPTEX lpfnAcceptEx = NULL;
			GUID guidAcceptEx = WSAID_ACCEPTEX;
			dwBytes = 0;

			if (WSAIoctl(_sockfd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&guidAcceptEx, sizeof(guidAcceptEx), &lpfnAcceptEx, sizeof(lpfnAcceptEx),
				&dwBytes, NULL, NULL) != 0)
			{
				//TODO
				throw;
			}
			delete _rdata;
			_rdata = new WSATransData;
			memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
			SOCKET s = createNonblockingOrDie(true);
			_rdata->databuff.len = (sizeof(sockaddr) + 16) * 2;
			_rdata->databuff.buf = _addr;
			_rdata->event = IOCPREAD;
			_rdata->fd = _sockfd;
			_rdata->translen = 0;
			_rdata->ptr = s;
			memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
			lpfnAcceptEx(_sockfd, s, _rdata->databuff.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, 0, (LPOVERLAPPED)_rdata);

			return acceptsocket;
	}



		bool UdpSocket::readRequest(char *buf, int len)
		{
			_rdata = new WSATransData;
			memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
			_rdata->databuff.len = len;
			_rdata->databuff.buf = buf;
			_rdata->event = IOCPREAD;
			_rdata->fd = _sockfd;
			_rdata->translen = 0;
			_rdata->ptr = 0;
			DWORD RecvBytes = 0, Flags = 0;
			int ret = WSARecv(_sockfd, &(_rdata->databuff), 1, &RecvBytes, &Flags, &(_rdata->overlapped), NULL);
			if (ret == 0 || WSAGetLastError() == WSA_IO_PENDING)
			{
				return true;
			}
			else
			{
				delete _rdata;
				_rdata = NULL;
				return false;
			}

		}

		int UdpSocket::readSize()
		{
			int len = _rdata->translen;;
			delete _rdata;
			_rdata = NULL;
			return len;
		}

		bool UdpSocket::writeRequest(char *buf, int len)
		{
			_wdata = new WSATransData;
			memset(&(_wdata->overlapped), 0, sizeof(OVERLAPPED));
			_wdata->databuff.len = len;
			_wdata->databuff.buf = buf;
			_wdata->event = IOCPWRITE;
			_wdata->fd = _sockfd;
			_wdata->translen = 0;
			_wdata->ptr = 0;

			DWORD RecvBytes = 0, Flags = 0;
			int ret = WSASend(_sockfd, &(_wdata->databuff), 1,
				&RecvBytes, Flags, &(_wdata->overlapped), NULL);
			if (ret == 0 || WSAGetLastError() == WSA_IO_PENDING)
			{
				return true;
			}
			else
			{
				delete _wdata;
				_wdata = NULL;
				return false;
			}
		}

		int UdpSocket::writeSize()
		{
			int len = _wdata->translen;;
			delete _wdata;
			_wdata = NULL;
			return len;
		}

		void UdpSocket::shutdownWrite()
		{
			::shutdown(_sockfd, SD_SEND);
		}


		void UdpSocket::setTcpNoDelay(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void UdpSocket::setReuseAddr(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void UdpSocket::setReusePort(bool on)
		{
		}

		void UdpSocket::setKeepAlive(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		SOCKET UdpSocket::createNonblockingOrDie(bool nonblock)
		{
			if (!nonblock)//����
			{
				SOCKET sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (sockfd <= 0)
				{
					LOG_SYSFATAL << "Sockets::createNonblockingOrDie";
				}
				return sockfd;
			}
			else
			{
				SOCKET sockfd = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
				if (sockfd <= 0)
				{
					LOG_SYSFATAL << "Sockets::createNonblockingOrDie";
				}
				LINGER linger = { 1, 0 };
				setsockopt(sockfd, SOL_SOCKET, SO_LINGER,
					(char *)&linger, sizeof(linger));
				return sockfd;
			}
		}

	}
}


#endif //!INWINDOWS

