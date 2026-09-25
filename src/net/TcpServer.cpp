//////////////////////////////////////////////////////////////////////////////////
//�ļ���TcpServer.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������TCP����
////////////////////////////////////////////////////////////////////////////////////

#include "TcpServer.h"
#include <base/Logging.h>
#include <assert.h>

namespace EtherDB
{
	namespace Net
	{
		TcpServer::TcpServer(EventLoop *loop,
			const InetAddress &listenAddr,
			const std::string &name,
			Option option)
			:_loop(loop),
			_localAddr(listenAddr),
			_name(name),
			_acceptor(new Acceptor(loop, listenAddr, option == kReusePort)),
			_threadPool(new EventLoopThreadPool(loop, _name)),
			_connectionCallback(defaultConnectionCallback),
			_messageCallback(defaultMessageCallback),
			_nextConnId(1)
		{
			_acceptor->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
		}

		TcpServer::~TcpServer()
		{
			_loop->assertInLoopThread();
			LOG_TRACE << "TcpServer::~TcpServer [" << _name << "] destructing";

			for (ConnectionMap::iterator it(_connections.begin());
				it != _connections.end(); ++it)
			{
				TcpConnectionPtr conn(it->second);
				it->second.reset();
				conn->getLoop()->runInLoop(
					std::bind(&TcpConnection::connectDestroyed, conn));
			}

		}

		void TcpServer::setThreadNum(int numThreads)
		{
			assert(0 <= numThreads);
			_threadPool->setThreadNum(numThreads);
		}

		void TcpServer::start()
		{
			if (_started.getAndSet(1) == 0)
			{
				_threadPool->start(_threadInitCallback);

				assert(!_acceptor->listenning());
				_loop->runInLoop(
					std::bind(&Acceptor::listen, _acceptor.get()));
			}
		}

		void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
		{
			_loop->assertInLoopThread();
			EventLoop* ioLoop = _threadPool->getNextLoop();
			char buf[64];
			snprintf(buf, sizeof buf, "-%s#%lld", _localAddr.toIpPort().c_str(), _nextConnId);
			++_nextConnId;
			std::string connName = _name + buf;

			LOG_TRACE << "TcpServer::newConnection [" << _name
				<< "] - new connection [" << connName
				<< "] from " << peerAddr.toIpPort();

			TcpConnectionPtr conn(new TcpConnection(ioLoop,
				connName,
				sockfd,
				_localAddr,
				peerAddr));
			// Disable Nagle on accepted sockets: request/response traffic
			// must not wait for delayed ACKs.
			conn->setTcpNoDelay(true);
			_connections[connName] = conn;
			conn->setConnectionCallback(_connectionCallback);
			conn->setMessageCallback(_messageCallback);
			conn->setWriteCompleteCallback(_writeCompleteCallback);
			conn->setCloseCallback(
				std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
			ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
		}

		void TcpServer::removeConnection(const TcpConnectionPtr& conn)
		{
			_loop->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
		}

		void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
		{
			_loop->assertInLoopThread();
			LOG_TRACE << "TcpServer::removeConnectionInLoop [" << _name
				<< "] - connection " << conn->name();
			size_t n = _connections.erase(conn->name());
			(void)n;
			assert(n == 1);
			EventLoop* ioLoop = conn->getLoop();
			ioLoop->queueInLoop(
				std::bind(&TcpConnection::connectDestroyed, conn));
		}

	}
}

