//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpServlet.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������HttpServlet
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_HttpServlet_H_
#define __EtherDB_Net_HttpServlet_H_

#include "../AmNetConfig.h"
#include "../TcpConnection.h"
#include "HttpRequest.h"
#include "HttpResponse.h"


namespace EtherDB
{
	namespace Net
	{
		class HttpServlet;
		typedef std::shared_ptr<HttpServlet> HttpServletPtr;
		typedef std::function<HttpServletPtr()> ServletFactory;

		class AMNET_API HttpServlet
		{
		public:
			void handRequest(const HttpRequestPtr &requst, const HttpResponsePtr &response)
			{
				if (requst->getMethod() == "GET")
				{
					get(requst, response);
				}
				else if (requst->getMethod() == "POST")
				{
					post(requst, response);
				}
				else
				{
					TcpConnectionPtr conn = requst->getConnection();
					if (conn)
					{
						conn->forceClose();
					}
				}
			}


			virtual void get(const HttpRequestPtr &requst, const HttpResponsePtr &response) {}
			virtual void post(const HttpRequestPtr &requst, const HttpResponsePtr &response) {}

			static void insert(std::string url, ServletFactory factory);
			static HttpServletPtr getServlet(std::string url);
		};

#define SERVLETFACTORY(cls) \
		HttpServletPtr create##cls() \
		{ return HttpServletPtr(new cls);}

#define ADDSERVLETFACTORY(url, cls) \
		class __Add_Servlet##cls{ \
		public: \
			__Add_Servlet##cls() { HttpServlet::insert(url, std::bind(create##cls));} \
		}; \
	    __Add_Servlet##cls _acls_servlet##cls;
	}
}


#endif // !__EtherDB_Net_HttpServlet_H_



