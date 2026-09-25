

#include "TcpSocket.h"
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
		TcpSocket::TcpSocket(int sockfd)
			:_sockfd(sockfd)
		{}

		TcpSocket::~TcpSocket()
		{
			::close(_sockfd);
		}

		void TcpSocket::bindAddress(const InetAddress& localaddr)
		{
			int ret = ::bind(_sockfd, localaddr.getSockAddr(), static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
			if (ret < 0)
			{
				LOG_SYSFATAL << "sockets::bindOrDie";
			}
		}

		void TcpSocket::listen()
		{
			int ret = ::listen(_sockfd, 10);
			if (ret < 0)
			{
				LOG_SYSFATAL << "sockets::listenOrDie";
			}
		}

		int TcpSocket::accept(InetAddress* peeraddr)
		{
			struct sockaddr_in6 addr;
			memset(&addr, 0, sizeof addr);
			socklen_t addrlen = static_cast<socklen_t>(sizeof addr);
			int connfd = ::accept4(_sockfd, (struct sockaddr*)&addr,
				&addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
			if (connfd >= 0)
			{
				peeraddr->setSockAddrInet6(addr);
			}
			return connfd;
		}

		void TcpSocket::shutdownWrite()
		{
			::shutdown(_sockfd, SHUT_WR);
		}

		int TcpSocket::read(char *buf, int len)
		{
			return ::recv(_sockfd, buf, len, 0);
		}

		int TcpSocket::write(char *buf, int len)
		{
			return ::send(_sockfd, buf, len, 0);
		}

		void TcpSocket::setTcpNoDelay(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void TcpSocket::setReuseAddr(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void TcpSocket::setReusePort(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void TcpSocket::setKeepAlive(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		SOCKET TcpSocket::createNonblockingOrDie(bool nonblock)
		{
			SOCKET sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sockfd <= 0)
			{
				LOG_SYSFATAL << "Sockets::createNonblockingOrDie";
			}
			if (nonblock)
			{
				int flags = ::fcntl(sockfd, F_GETFL, 0);
				flags |= O_NONBLOCK;
				int ret = ::fcntl(sockfd, F_SETFL, flags);

				flags = ::fcntl(sockfd, F_GETFD, 0);
				flags |= FD_CLOEXEC;
				ret = ::fcntl(sockfd, F_SETFD, flags);
				(void)ret;
			}
			return sockfd;
		}

	}
}


#else

namespace EtherDB
{
	namespace Net
	{
		TcpSocket::TcpSocket(int sockfd)
			:_sockfd(sockfd),
			_rdata(NULL),
			_wdata(NULL),
			_addr(new char[(sizeof(sockaddr) + 16) * 2])
		{
		}

		TcpSocket::~TcpSocket()
		{
			::closesocket(_sockfd);
			delete[] _addr;
		}

		void TcpSocket::bindAddress(const InetAddress& localaddr)
		{
			int ret = ::bind(_sockfd, localaddr.getSockAddr(), static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
			if (ret < 0)
			{
				LOG_SYSFATAL << "sockets::bindOrDie";
			}
		}

		void TcpSocket::listen()
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

		int TcpSocket::accept(InetAddress* peeraddr)
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



		bool TcpSocket::readRequest(char *buf, int len)
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

		int TcpSocket::readSize()
		{
			int len = _rdata->translen;;
			delete _rdata;
			_rdata = NULL;
			return len;
		}

		bool TcpSocket::writeRequest(char *buf, int len)
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

		int TcpSocket::writeSize()
		{
			int len = _wdata->translen;;
			delete _wdata;
			_wdata = NULL;
			return len;
		}

		void TcpSocket::shutdownWrite()
		{
			::shutdown(_sockfd, SD_SEND);
		}


		void TcpSocket::setTcpNoDelay(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void TcpSocket::setReuseAddr(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		void TcpSocket::setReusePort(bool on)
		{
		}

		void TcpSocket::setKeepAlive(bool on)
		{
			int optval = on ? 1 : 0;
			::setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE,
				(const char *)&optval, static_cast<socklen_t>(sizeof optval));
		}

		SOCKET TcpSocket::createNonblockingOrDie(bool nonblock)
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

