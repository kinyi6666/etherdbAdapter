//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpSession.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Http�Ự
////////////////////////////////////////////////////////////////////////////////////

#include "HttpSession.h"
#include "HttpServer.h"
#define GUID_LEN 64
#ifdef INWINDOWS
#include <windows.h>
#include <objbase.h>   // CoCreateGuid
#endif // INWINDOWS

namespace EtherDB
{
	namespace Net
	{
		HttpSession::HttpSession()
			:_isNew(true)
		{
#ifdef INWINDOWS
			char buffer[GUID_LEN] = { 0 };
			GUID guid;
			if (!CoCreateGuid(&guid))
			{
				snprintf(buffer, sizeof(buffer),
					"%08X%04X%04x%02X%02X%02X%02X%02X%02X%02X%02X",
					guid.Data1, guid.Data2, guid.Data3,
					guid.Data4[0], guid.Data4[1], guid.Data4[2],
					guid.Data4[3], guid.Data4[4], guid.Data4[5],
					guid.Data4[6], guid.Data4[7]);
			}
			_id = toLower(std::string(buffer));
#else

#endif // INWINDOWS
		}

		Cookie HttpSession::createSeesionCookie()
		{
			return Cookie("amid", _id);
		}


		Any HttpSession::getAttribute(std::string key)
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

		void HttpSession::setAttribute(std::string key, Any value)
		{
			MutexLock lock(_mutex);
			_attributeMap[key] = value;
		}

		void HttpSession::removeAttribute(std::string key)
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

		bool HttpSession::hasAttribute(std::string key)
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

		std::map<std::string, Any> &HttpSession::getAttributs()
		{
			MutexLock lock(_mutex);
			return _attributeMap;
		}

	}
}


