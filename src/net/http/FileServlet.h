//////////////////////////////////////////////////////////////////////////////////
//���GFileServlet.h 
//�@�̡GLSPZ
//??�G2018-01-02
//�y�z�G���
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_FileServlet_H_
#define __EtherDB_Net_FileServlet_H_

#include "../AmNetConfig.h"
#include "HttpServlet.h"


namespace EtherDB
{
	namespace Net
	{
		class AMNET_API FileServlet : public HttpServlet
		{
		public:
			void get(const HttpRequestPtr &requst, const HttpResponsePtr &response)
			{
				std::string path = checkPath(requst);
				if (path == "")
				{
					send404(requst, response);
				}
				else
				{
					sendFile(path, requst, response);
				}
			}

			void post(const HttpRequestPtr &requst, const HttpResponsePtr &response)
			{
				get(requst, response);
			}

			static std::string getHtmlRootPath();
			static std::string checkPath(const HttpRequestPtr &requst);
			static void setHtmlRootPath(std::string path) { _htmlroot = path; }

		private:
			void sendFile(std::string path, const HttpRequestPtr &requst, const HttpResponsePtr &response);
			void send404(const HttpRequestPtr &requst, const HttpResponsePtr &response);

			static std::string _htmlroot;
		};

	}
}

#endif //!__EtherDB_Net_FileServlet_H_








