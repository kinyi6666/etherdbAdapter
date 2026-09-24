//////////////////////////////////////////////////////////////////////////////////
//??��HttpServer.h 
//���k��LSPZ
//??��2018-01-02
//��v��Http???
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_HttpServer_H_
#define __EtherDB_Net_HttpServer_H_

#include "../AmNetConfig.h"
#include "../IdelTcpServer.h"
#include "HttpSession.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace EtherDB
{
	namespace Net 
	{
		class AMNET_API HttpServer
		{
		public:
			typedef TcpServer::ThreadInitCallback ThreadInitCallback;

			HttpServer(EventLoop* loop,
				const InetAddress& listenAddr,
				const std::string& nameArg,
				TcpServer::Option option = TcpServer::kNoReusePort);
			~HttpServer();

			void start();

			void setTimeout(double t) { _server.setTimeout(t); }

			void setMaxConn(uint32_t c) { _server.setMaxConn(c); }
			uint32_t getConnCount() { return _server.getConnCount(); }

			void setThreadNum(int numThreads) { _server.setThreadNum(numThreads); }

			const std::string ipPort() const { return _server.ipPort(); }
			const std::string& name() const { return _server.name(); }

			EventLoop* getLoop() const { return _server.getLoop(); }
			std::shared_ptr<EventLoopThreadPool> threadPool() { return _server.threadPool(); }

			void setRootPath(std::string path) { _rootpath = path; }

			void setThreadInitCallback(const ThreadInitCallback& cb) { _server.setThreadInitCallback(cb); }

			void setSessionOut(int v) { _sessionout = v; }
			HttpSessionPtr getSession(std::string id);
			HttpSessionPtr addSession();
			void removeSession(std::string id);

			void handleRequest(HttpRequestPtr request, HttpResponsePtr response);
			
		private:
			void onConnection(const TcpConnectionPtr &conn);
			void doMessage(const TcpConnectionPtr &conn, Buffer* buf, Timestamp stamp);
			void onSessionTimeOut();


		private:
			//Session??
			class SessionEntry 
			{
			public:
				SessionEntry(HttpServer &server, std::string id) 
					:_server(server),
					_id(id)
				{
				}

				~SessionEntry()
				{
					_server.removeSession(_id);
				}

			private:
				HttpServer &_server;
				std::string _id;
			};

			typedef std::shared_ptr<SessionEntry> SessionEntryPtr;
			typedef std::weak_ptr<SessionEntry> SessionEntryWtr;
			typedef std::set<SessionEntryPtr> SessionBucket;
			typedef std::queue<SessionBucket> SessionBucketList;

		private:
			Mutex _mutex;
			int _sessionout;
			std::string _rootpath;
			SessionBucketList _sessionList;
			std::map<std::string, HttpSessionPtr> _sessionMap;

		private:
			IdelTcpServer _server;
		};
	}
}




#endif // !__EtherDB_Net_HttpServer_H_








