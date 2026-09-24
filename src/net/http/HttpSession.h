//////////////////////////////////////////////////////////////////////////////////
//���GHttpSession.h 
//�@�̡GLSPZ
//??�G2018-01-02
//�y�z�GHttp??
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_HttpSession_H_
#define __EtherDB_Net_HttpSession_H_

#include "../AmNetConfig.h"
#include <memory>
#include <base/Any.h>
#include <base/Mutex.h>
#include "Cookie.h"
#include <map>

namespace EtherDB
{
	namespace Net
	{
		class HttpServer;
		class AMNET_API HttpSession
		{
		public:
			HttpSession();
			Cookie createSeesionCookie();

			std::string getId() { return _id; }
			bool isNew() { return _isNew; }

			Any getAttribute(std::string key);
			void setAttribute(std::string key, Any value);
			void removeAttribute(std::string key);
			bool hasAttribute(std::string key);
			std::map<std::string, Any> &getAttributs();

			void setPrivateData(Any data) { _pridata = data; }
			Any &getPrivateData() { return _pridata; }

		private:
			std::string _id;
			bool _isNew;
			Any _pridata;
			HttpServer *_server;

			Mutex _mutex;
			std::map<std::string, Any> _attributeMap;

			friend HttpServer;
		};
		typedef std::shared_ptr<HttpSession> HttpSessionPtr;
		typedef std::weak_ptr<HttpSession> HttpSessionWtr;
	}
}


#endif // !__EtherDB_Net_HttpSession_H_



