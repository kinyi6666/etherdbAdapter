//////////////////////////////////////////////////////////////////////////////////
//�ļ���TcpServer.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������TCP����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_TcpServer_H_
#define __EtherDB_Net_TcpServer_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/AtomicInt.h>
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include <memory>
#include <map>

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API TcpServer : Noncopyable
		{
		public:
			typedef EventLoopThread::ThreadInitCallback ThreadInitCallback;

			enum Option
			{
				kNoReusePort,
				kReusePort,
			};

			TcpServer(EventLoop *loop,
				const InetAddress &listenAddr,
				const std::string &name,
				Option option = kNoReusePort);
			~TcpServer();

			void setThreadNum(int numThreads);
			void start();

			const std::string ipPort() const { return _localAddr.toIpPort(); }
			const std::string& name() const { return _name; }
			EventLoop* getLoop() const { return _loop; }
			std::shared_ptr<EventLoopThreadPool> threadPool() { return _threadPool; }

			void setThreadInitCallback(const ThreadInitCallback& cb) { _threadInitCallback = cb; }
			void setConnectionCallback(const ConnectionCallback& cb) { _connectionCallback = cb; }
			void setMessageCallback(const MessageCallback& cb) { _messageCallback = cb; }
			void setWriteCompleteCallback(const WriteCompleteCallback& cb) { _writeCompleteCallback = cb; }

		private:
			void newConnection(int sockfd, const InetAddress& peerAddr);
			void removeConnection(const TcpConnectionPtr& conn);
			void removeConnectionInLoop(const TcpConnectionPtr& conn);

		private:
			typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

			EventLoop *_loop;
			const InetAddress _localAddr;
			const std::string _name;
			std::unique_ptr<Acceptor> _acceptor;
			std::shared_ptr<EventLoopThreadPool> _threadPool;

			//�ص�����
			ConnectionCallback _connectionCallback;
			MessageCallback _messageCallback;
			WriteCompleteCallback _writeCompleteCallback;
			ThreadInitCallback _threadInitCallback;

			//����ά��
			AtomicInt32 _started;
			uint64_t _nextConnId;
			ConnectionMap _connections;
		};
	}
}


#endif // !__EtherDB_Net_TcpServer_H_



