//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpRequest.cpp
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Http����
////////////////////////////////////////////////////////////////////////////////////

#include "HttpRequest.h"
#include "HttpServer.h"

namespace EtherDB
{
	namespace Net
	{
		Cookis &HttpRequest::getCookies()
		{
			return _cookis;
		}

		bool HttpRequest::hasCookie(std::string key)
		{
			key = toLower(trim(key));
			do 
			{
				if(key.empty())
					break;
				for (int i = 0; i < _cookis.size(); i++)
				{
					if (key == _cookis[i].getName())
					{
						return true;
					}
				}
			} while (0);

			return false;
		}

		Cookie HttpRequest::getCookie(std::string key)
		{
			key = toLower(trim(key));
			do 
			{
				if (key.empty())
					break;
				for (int i = 0; i < _cookis.size(); i++)
				{
					if (key == _cookis[i].getName())
					{
						return _cookis[i];
					}
				}
			} while (0);
			return Cookie();
		}

		Any HttpRequest::getAttribute(std::string key)
		{
			key = toLower(trim(key));

			do
			{
				if (key.empty())
					break;
				MutexLock lock(_mutex);
				auto it = _attributeMap.find(key);
				if (it != _attributeMap.end())
				{
					return it->second;
				}
			} while (0);
			return Any();
		}

		void HttpRequest::setAttribute(std::string key, Any value)
		{
			MutexLock lock(_mutex);
			_attributeMap[key] = value;
		}

		void HttpRequest::removeAttribute(std::string key)
		{
			key = toLower(trim(key));
			if (key.empty())
				return;

			MutexLock lock(_mutex);
			auto it = _attributeMap.find(key);
			if (it != _attributeMap.end())
			{
				_attributeMap.erase(it);
			}
		}

		bool HttpRequest::hasAttribute(std::string key)
		{
			key = toLower(trim(key));

			do
			{
				if (key.empty())
					break;
				MutexLock lock(_mutex);
				auto it = _attributeMap.find(key);
				if (it != _attributeMap.end())
				{
					return true;
				}
			} while (0);
			return false;
		}

		HttpRequest::RequestAttribute &HttpRequest::getAttributs()
		{
			MutexLock lock(_mutex);
			return _attributeMap;
		}

		std::string HttpRequest::getHeader(std::string key)
		{
			key = toLower(trim(key));
			do
			{
				if (key.empty())
					break;
				auto it = _headers.find(key);
				if (it != _headers.end())
				{
					return it->second;
				}
			} while (0);
			return "";
		}

		bool HttpRequest::hasHeader(std::string key)
		{
			key = toLower(trim(key));
			do
			{
				if (key.empty())
					break;
				auto it = _headers.find(key);
				if (it != _headers.end())
				{
					return true;
				}
			} while (0);
			return false;
		}

		HttpRequest::RequestHead &HttpRequest::getHeaders()
		{
			return _headers;
		}

		std::string HttpRequest::getParam(std::string key)
		{
			key = toLower(trim(key));
			do
			{
				if (key.empty())
					break;
				auto it = _params.find(key);
				if (it != _params.end())
				{
					return it->second;
				}
			} while (0);
			return "";
		}

		bool HttpRequest::hasParam(std::string key)
		{
			key = toLower(trim(key));
			do
			{
				if (key.empty())
					break;
				auto it = _params.find(key);
				if (it != _params.end())
				{
					return true;
				}
			} while (0);
			return false;
		}

		HttpRequest::RequestParam &HttpRequest::getParams()
		{
			return _params;
		}

		HttpSessionPtr HttpRequest::getSession()
		{
			return _session;
		}

		HttpSessionPtr HttpRequest::getSession(bool create)
		{
			if (!_session)
			{
				_session = _server->addSession();
				_sessionid = _session->getId();
			}
			return _session;
		}

		std::string HttpRequest::getLocale()
		{
			return "";
		}

		std::string HttpRequest::getCharacterEncoding()
		{
			return "";
		}

		std::string HttpRequest::getContentType()
		{
			return "";
		}

		int HttpRequest::getContentLength()
		{
			return _contextlength;
		}

		std::string HttpRequest::getMethod()
		{
			return _method;
		}

		std::string HttpRequest::getRequestURI()
		{
			return _url;
		}

		std::string HttpRequest::getProtocol()
		{
			return _version;
		}

		std::string HttpRequest::getRemoteAddr()
		{
			return "";
		}

		std::string HttpRequest::getRequestedSessionId()
		{
			return _sessionid;
		}

		int HttpRequest::getIntHeader(std::string name)
		{
			return 0;
		}

	}
}






