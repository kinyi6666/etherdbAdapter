//////////////////////////////////////////////////////////////////////////////////
//�ļ���IdelTcpServer.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������
////////////////////////////////////////////////////////////////////////////////////

#include "IdelTcpServer.h"

namespace EtherDB
{
	namespace Net
	{
		IdelTcpServer::IdelTcpServer(EventLoop* loop,
			const InetAddress& listenAddr,
			const std::string& nameArg,
			TcpServer::Option option)
			:_server(loop, listenAddr, nameArg, option),
			_timeout(0),
			_timeHz(0),
			_maxConn(0),
			_connectionCallback(defaultConnectionCallback),
			_messageCallback(defaultMessageCallback)
		{
			_server.setConnectionCallback(std::bind(&IdelTcpServer::onConnection, this, std::placeholders::_1));
			_server.setMessageCallback(std::bind(&IdelTcpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			_connectionBuckets.push(Bucket());
		}

		IdelTcpServer::~IdelTcpServer()
		{

		}

		void IdelTcpServer::start()
		{
			_server.start();
			if (_timeout > 0)
			{
				if (_timeout > 10)
				{
					_timeHz = _timeout / 10;
				}
				else
				{
					_timeHz = 1;
				}

				getLoop()->runEvery(_timeHz, std::bind(&IdelTcpServer::onTimer, this));
			}
		}

		void IdelTcpServer::onConnection(const TcpConnectionPtr &conn)
		{
			if (conn->connected())
			{
				_connected.incrementAndGet();
				if (_timeout > 0)
				{
					MutexLock lock(_mutex);
					EntryPtr entry(new Entry(conn, std::bind(&IdelTcpServer::ontimeoutCallback, this, std::placeholders::_1)));
					_connectionBuckets.back().insert(entry);
					WeakEntryPtr weakEntry(entry);
					conn->setContext("IdelTcpServer", weakEntry);
				}
			}
			else
			{
				_connected.decrement();
			}

			if ((_maxConn > 0) && (_connected.get() > _maxConn))
			{
				conn->forceClose();
			}
			else
			{
				_connectionCallback(conn);
			}
		}

		void IdelTcpServer::onMessage(const TcpConnectionPtr &conn, Buffer* buf, Timestamp stamp)
		{
			_messageCallback(conn, buf, stamp);

			if (_timeout > 0)
			{
				MutexLock lock(_mutex);
				WeakEntryPtr weakEntry(AnyCast<WeakEntryPtr>(conn->getContext("IdelTcpServer")));

				EntryPtr entry = weakEntry.lock();
				if (entry)
				{
					_connectionBuckets.back().insert(entry);
				}
			}
		}

		void IdelTcpServer::ontimeoutCallback(const TcpConnectionPtr &conn)
		{
			if (_timerCallback)
			{
				_timerCallback(conn);
				EntryPtr entry(new Entry(conn, std::bind(&IdelTcpServer::ontimeoutCallback, this, std::placeholders::_1)));
				_connectionBuckets.back().insert(entry);
				WeakEntryPtr weakEntry(entry);
				conn->setContext("IdelTcpServer", weakEntry);
			}
			else
			{
				conn->forceClose();
			}
		}

		void IdelTcpServer::onTimer()
		{
			MutexLock lock(_mutex);
			_connectionBuckets.push(Bucket());
			while (_timeHz*_connectionBuckets.size()  > _timeout)
			{
				_connectionBuckets.pop();
			}
		}

	}
}

