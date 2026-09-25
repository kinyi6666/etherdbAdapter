//////////////////////////////////////////////////////////////////////////////////
//�ļ���FileServlet.cpp
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�������ļ�
////////////////////////////////////////////////////////////////////////////////////

#include "FileServlet.h"
#include <base/String.h>
#include <stdio.h>
#ifndef INWINDOWS
	#include <unistd.h>
	#define _access access	
#else
#include <io.h>
#endif

namespace EtherDB
{
	namespace Net
	{
		std::string FileServlet::_htmlroot = "";

		std::string FileServlet::getHtmlRootPath()
		{
			if (_htmlroot.empty())
			{
				char buf[256] = { 0 };
#ifndef INWINDOWS
				getcwd(buf, 256);
#else
				GetCurrentDirectory(MAX_PATH, buf);
#endif
				_htmlroot = buf;
			}
			return _htmlroot;
		}

		std::string FileServlet::checkPath(const HttpRequestPtr &requst)
		{
#ifndef INWINDOWS
		std::string filepath;
		std::string rootpath;
		std::string path;
#else
			std::string path = requst->getRequestURI();
			translateInPlace(path, "/", "\\");

			//�ж��ļ��Ƿ����
			std::string rootpath = getHtmlRootPath();
			std::string filepath;
			if (rootpath[rootpath.size() - 1] == '\\')
			{
				rootpath = std::string(rootpath.c_str(), rootpath.size() - 1);
			}

#endif // !INWINDOWS
			filepath = trim(rootpath + path);

			if ((_access(filepath.c_str(), 0)) != -1)
			{
				if ((_access(filepath.c_str(), 2)) != -1)
				{
					return filepath;
				}
			}
			return "";
		}

		void FileServlet::sendFile(std::string path, const HttpRequestPtr &requst, const HttpResponsePtr &response)
		{
			std::string suffix = trim(path.substr(path.find_last_of(".") + 1));
			std::string contenttype;
			if (suffix == "htm" || suffix == "html")
			{
				contenttype = "text/html; charset=utf-8";
			}
			else if (suffix == "xml")
			{
				contenttype = "application/xml";
			}
			else if (suffix == "js")
			{
				contenttype = "application/x-javascript; charset=utf-8";
			}
			else if (suffix == "css")
			{
				contenttype = "text/css; charset=utf-8";
			}
			else if (suffix == "txt")
			{
				contenttype = "text/plain; charset=utf-8";
			}
			else if (suffix == "pdf")
			{
				contenttype = "application/pdf";
			}
			else if (suffix == "bmp")
			{
				contenttype = "image/bmp";
			}
			else if (suffix == "gif")
			{
				contenttype = "image/gif";
			}
			else if (suffix == "png" || suffix == "ico")
			{
				contenttype = "image/x-png";
			}
			else if (suffix == "jpe" || suffix == "jpg" || suffix == "jpeg")
			{
				contenttype = "image/jpeg";
			}
			else
			{
				contenttype = "application/octet-stream";
			}

			response->setContentType(contenttype);
			response->setHeader("Access-Control-Allow-Origin", "*");
			response->setHeader("Elapsed", "0ms");
			response->setDateHeader("Date", Timestamp::now());
			response->setHeader("Server", "JarvisCloud/20170822");

			FILE *fp = fopen(path.c_str(), "rb");
			if (fp)
			{
				char buf[1024 * 4] = { 0 };
				int len = 0;
				do
				{
					len = fread(buf, 1, 1024 * 4, fp);
					if (len > 0)
					{
						response->append(std::string(buf, len));
					}

				} while (len == 1024 * 4);
				fclose(fp);
			}

			response->flush();
		}

		void FileServlet::send404(const HttpRequestPtr &requst, const HttpResponsePtr &response)
		{
			response->setStatus(404);
			response->append("Not Found");

			response->flush();
		}

	}
}



