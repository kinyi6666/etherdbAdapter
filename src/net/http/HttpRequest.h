//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpRequest.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Http����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_HttpRequest_H_
#define __EtherDB_Net_HttpRequest_H_

#include "../AmNetConfig.h"
#include "../Callbacks.h"
#include <base/String.h>
#include <base/Mutex.h>
#include "HttpSession.h"
#include "Cookie.h"
#include <memory>
#include <vector>
#include <map>


namespace EtherDB
{
	namespace Net
	{
		class HttpServer;
		class HttpResponse;
		class AMNET_API HttpRequest
		{
		public:
			typedef std::map<std::string, Any> RequestAttribute;
			typedef std::map<std::string, std::string> RequestHead;
			typedef std::map<std::string, std::string> RequestParam;

			Cookis &getCookies();
			bool hasCookie(std::string key);
			Cookie getCookie(std::string key);
			
			Any getAttribute(std::string key);
			void setAttribute(std::string key, Any value);
			void removeAttribute(std::string key);
			bool hasAttribute(std::string key);
			RequestAttribute &getAttributs();

			std::string getHeader(std::string key);
			bool hasHeader(std::string key);
			RequestHead &getHeaders();

			std::string getParam(std::string key);
			bool hasParam(std::string key);
			RequestParam &getParams();

			HttpSessionPtr getSession();
			HttpSessionPtr getSession(bool create);

			std::string getLocale();
			std::string getCharacterEncoding();
			std::string getContentType();
			int getContentLength();

			std::string getMethod();
			std::string getRequestURI();
			std::string getProtocol();

			std::string getRemoteAddr();
			std::string getRequestedSessionId();
			int getIntHeader(std::string name);

			
			TcpConnectionPtr getConnection() { return _conn.lock(); }
		private:
			Mutex _mutex;
		
			std::string _method;
			std::string _version;
			std::string _url;
			std::string _body;
			std::string _sessionid;
			int _contextlength;

			RequestHead _headers;
			RequestParam _params;
			Cookis _cookis;
			TcpConnectionWtr _conn;
			HttpSessionPtr _session;
			RequestAttribute _attributeMap;
			HttpServer *_server;

			friend HttpServer;
			friend HttpResponse;
		};

		typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
		typedef std::weak_ptr<HttpRequest> HttpRequestWtr;
	}
}


#endif // !__EtherDB_Net_HttpRequest_H_


