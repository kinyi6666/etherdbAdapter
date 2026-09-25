//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpServer.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Http������
////////////////////////////////////////////////////////////////////////////////////

#include "HttpServer.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpServlet.h"
#include "FileServlet.h"

namespace EtherDB
{
	namespace Net 
	{
		HttpServer::HttpServer(EventLoop* loop,
			const InetAddress& listenAddr,
			const std::string& nameArg,
			TcpServer::Option option)
			:_server(loop, listenAddr, nameArg, option),
			_sessionout(60)
		{
			_server.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
			_server.setMessageCallback(std::bind(&HttpServer::doMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			_sessionList.push(SessionBucket());
		}

		HttpServer::~HttpServer()
		{

		}

		void HttpServer::onConnection(const TcpConnectionPtr &conn)
		{
			if(conn->connected())
			{
				printf("%s\n", conn->peerAddress().toIpPort().c_str());
			}
		}

		void HttpServer::doMessage(const TcpConnectionPtr &conn, Buffer* buf, Timestamp stamp)
		{
			HttpParserPtr parser;
			//��ȡ��������
			if (conn->getContext("parse").empty()) {
				parser = HttpParserPtr(new HttpParser);
				conn->setContext("parse", parser);
			}
			else {
				parser = AnyCast<HttpParserPtr>(conn->getContext("parse"));
			}

			//��������
			int parserstatus = parser->parser(buf);
			if (parserstatus == HttpParser::PARSERING) {//���ݽ���δ���
				return;
			}
			else if (parserstatus == HttpParser::PARSERERROR) {
				conn->forceClose();
				return;
			}
			else //���ݽ������
			{
				//�����session cookieId ����ά��
				std::string cookieid = parser->_sessionid;
				HttpSessionPtr session;
				if (!cookieid.empty())
				{
					MutexLock lock(_mutex);
					auto it = _sessionMap.find(cookieid);
					if (it != _sessionMap.end())
					{
						session = it->second;
						session->_isNew = false;

						SessionEntryWtr weakEntry(AnyCast<SessionEntryWtr>(session->getPrivateData()));
						SessionEntryPtr entry = weakEntry.lock();
						if (entry)
						{
							_sessionList.back().insert(entry);
						}
					}
					else
					{
						parser->_sessionid = "";
					}
				}

				HttpRequestPtr request(new HttpRequest);
				
				request->_method = parser->_method;
				request->_version = parser->_version;
				request->_url = parser->_url;
				request->_body = parser->_body;
				request->_sessionid = parser->_sessionid;
				request->_contextlength = parser->_contextlength;

				printf("%s\n", request->_url.c_str());

				request->_headers = parser->_heads;
				request->_params = parser->_params;
				request->_cookis = parser->_cookie;
				request->_conn = conn;
				request->_session = session;
				request->_server = this;

				HttpResponsePtr response(new HttpResponse);
				response->_status = 200;
				response->_conn = conn;
				response->_request = request;
				response->_server = this;
				
				handleRequest(request, response);
				
				conn->setContext("parse", Any());
			}
		}

		void HttpServer::handleRequest(HttpRequestPtr request, HttpResponsePtr response)
		{
			HttpServletPtr servlet = HttpServlet::getServlet(request->_url);
			if (servlet)
			{
				servlet->handRequest(request, response);
			}
			else
			{
				if (request->_url == "/")
				{
					request->_url = "/index.htm";
				}
				FileServlet file;
				file.handRequest(request, response);
			}
		}

		void HttpServer::onSessionTimeOut()
		{
			MutexLock lock(_mutex);
			_sessionList.push(SessionBucket());

			while (_sessionList.size() > 10)
			{
				_sessionList.pop();
			}
		}

		void HttpServer::start()
		{
			_server.start();

			int hz = 0;
			if (_sessionout > 10)
			{
				hz = _sessionout / 10;
			}
			else
			{
				hz = 1;
			}

			getLoop()->runEvery(hz, std::bind(&HttpServer::onSessionTimeOut, this));
		}

		HttpSessionPtr HttpServer::getSession(std::string id)
		{
			MutexLock lock(_mutex);
			auto it = _sessionMap.find(id);
			if (it != _sessionMap.end())
			{
				return it->second;
			}
			else
			{
				return HttpSessionPtr();
			}
		}

		HttpSessionPtr HttpServer::addSession()
		{
			HttpSessionPtr session(new HttpSession());
			session->_server = this;
			SessionEntryPtr entry(new SessionEntry(*this, session->_id));
			session->_pridata = SessionEntryWtr(entry);

			MutexLock lock(_mutex);
			_sessionMap[session->_id] = session;
			_sessionList.back().insert(entry);

			return session;
		}

		void HttpServer::removeSession(std::string id)
		{
			MutexLock lock(_mutex);
			auto it = _sessionMap.find(id);
			if (it != _sessionMap.end())
			{
				_sessionMap.erase(it);
			}
		}
	}
}

