#include "HttpServlet.h"
#include <base/Singleton.h>

namespace EtherDB
{
	namespace Net
	{
		class ServletFactoryMap
		{
		public:
			void insert(std::string url, ServletFactory factory)
			{
				_factoryMap[url] = factory;
			}

			HttpServletPtr get(std::string url)
			{
				auto it = _factoryMap.find(url);

				if (it != _factoryMap.end())
				{
					return it->second();
				}
				else
				{
					return HttpServletPtr();
				}
			}

		private:
			std::map<std::string, ServletFactory> _factoryMap;
		};


		void HttpServlet::insert(std::string url, ServletFactory factory)
		{
			Singleton<ServletFactoryMap>::instance().insert(url, factory);
		}

		HttpServletPtr HttpServlet::getServlet(std::string url)
		{
			return Singleton<ServletFactoryMap>::instance().get(url);
		}
	}
}
