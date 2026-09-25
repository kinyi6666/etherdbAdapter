//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpParser.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������HttpЭ�������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_HttpParser_H_
#define __EtherDB_Net_HttpParser_H_

#include "../AmNetConfig.h"
#include "../Buffer.h"
#include "http_parser.h"
#include "Cookie.h"
#include <memory>
#include <map>

namespace EtherDB
{
	namespace Net
	{
		class HttpServer;
		class AMNET_API HttpParser 
		{
		public:
			enum//����״̬
			{
				PARSERING,
				PARSERED,
				PARSERERROR
			};

			HttpParser();
			~HttpParser();

			int parser(Buffer *buf);

			static int onStatus(http_parser *parser, const char *at, size_t length);
			static int OnMessageBeginCallback(http_parser *parser);
			static int OnUrlCallback(http_parser *parser, const char *at, size_t length);
			static int OnHeaderFieldCallback(http_parser *parser, const char *at, size_t length);
			static int OnHeaderValueCallback(http_parser *parser, const char *at, size_t length);
			static int OnHeadersCompleteCallback(http_parser *parser);
			static int OnBodyCallback(http_parser *parser, const char *at, size_t length);
			static int OnMessageCompleteCallback(http_parser *parser);

			static std::string UrlEncode(const std::string& str);
			static std::string UrlDecode(const std::string& str);

		private:
			struct http_parser *_parser;
			struct http_parser_settings *_seting;
			int _parserStatus;

			int _fild;
			std::string _head;

			std::string _method;
			std::string _version;
			std::string _url;
			std::string _body;
			std::string _sessionid;
			int _contextlength;

			std::map<std::string, std::string> _params;
			std::map<std::string, std::string> _heads;
			Cookis _cookie;


			friend HttpServer;
		};
		typedef std::shared_ptr<HttpParser> HttpParserPtr;
		typedef std::weak_ptr<HttpParser> HttpParserWtr;
	}
}

#endif // !__EtherDB_Net_HttpParser_H_










