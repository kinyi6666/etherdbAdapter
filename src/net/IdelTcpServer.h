//////////////////////////////////////////////////////////////////////////////////
//�ļ���IdelTcpServer.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_IdelTcpServer_H_
#define __EtherDB_Net_IdelTcpServer_H_

#include "AmNetConfig.h"
#include "TcpServer.h"
#include <queue>
#include <unordered_set>
#include <set>

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API IdelTcpServer
		{
		public:
			//typedef typename TcpServer::Option Option;
			typedef TcpServer::ThreadInitCallback ThreadInitCallback;
			typedef std::function<void(const TcpConnectionPtr&)> TimeoutCallback;

			IdelTcpServer(EventLoop* loop,
				const InetAddress& listenAddr,
				const std::string& nameArg,
				TcpServer::Option option = TcpServer::kNoReusePort);
			~IdelTcpServer();

			void start();

			void setTimeout(double t) { _timeout = t; }
			void setMaxConn(uint32_t c) { _maxConn = c; }
			uint32_t getConnCount() { return _connected.get(); }

			void setThreadNum(int numThreads) { _server.setThreadNum(numThreads); }
			const std::string ipPort() const { return _server.ipPort(); }
			const std::string& name() const { return _server.name(); }
			EventLoop* getLoop() const { return _server.getLoop(); }
			std::shared_ptr<EventLoopThreadPool> threadPool() { return _server.threadPool(); }

			void setThreadInitCallback(const ThreadInitCallback& cb) { _server.setThreadInitCallback(cb); }
			void setConnectionCallback(const ConnectionCallback& cb) { _connectionCallback = cb; }
			void setMessageCallback(const MessageCallback& cb) { _messageCallback = cb; }
			void setWriteCompleteCallback(const WriteCompleteCallback& cb) { _server.setWriteCompleteCallback(cb); }
			void setTimeOutCallback(const TimeoutCallback &cb) { _timerCallback = cb; }

		private:
			void onConnection(const TcpConnectionPtr &conn);
			void onMessage(const TcpConnectionPtr &conn, Buffer* buf, Timestamp stamp);
			void ontimeoutCallback(const TcpConnectionPtr &conn);
			void onTimer();

		private:
			class Entry
			{
			public:
				explicit Entry(const TcpConnectionWtr &conn, const TimeoutCallback &cb = TimeoutCallback())
					:_conn(conn),
					_cb(cb)
				{

				}

				~Entry()
				{
					TcpConnectionPtr conn = _conn.lock();
					if (conn)
					{
						if (_cb)
						{
							_cb(conn);
						}
						else
						{
							conn->forceClose();
						}
					}

				}

				TcpConnectionWtr _conn;
				TimeoutCallback _cb;
			};

			typedef std::shared_ptr<Entry> EntryPtr;
			typedef std::weak_ptr<Entry> WeakEntryPtr;
			//typedef std::unordered_set<EntryPtr> Bucket;
			typedef std::set<EntryPtr> Bucket;
			typedef std::queue<Bucket> WeakConnectionList;

		private:
			TcpServer _server;
			ConnectionCallback _connectionCallback;
			MessageCallback _messageCallback;
			TimeoutCallback _timerCallback;

			double _timeout;
			double _timeHz;

			uint32_t _maxConn;
			AtomicInt32 _connected;

			Mutex _mutex;
			WeakConnectionList _connectionBuckets;
		};
	}
}

#endif // !__EtherDB_Net_IdelTcpServer_H_

