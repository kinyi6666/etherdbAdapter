//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpParser.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������HttpЭ�������
////////////////////////////////////////////////////////////////////////////////////

#include "HttpParser.h"
#include "../Buffer.h"
#include <base/String.h>
namespace EtherDB
{
	namespace Net
	{
		HttpParser::HttpParser()
			:_parser(new http_parser),
			_seting(new http_parser_settings),
			_parserStatus(PARSERING),
			_fild(0),
			_contextlength(0)
		{
			http_parser_init(_parser, HTTP_REQUEST);
			_parser->data = this;


			_seting->on_body = OnBodyCallback;
			_seting->on_chunk_complete = NULL;
			_seting->on_chunk_header = NULL;
			_seting->on_header_field = OnHeaderFieldCallback;
			_seting->on_header_value = OnHeaderValueCallback;
			_seting->on_headers_complete = OnHeadersCompleteCallback;
			_seting->on_message_begin = OnMessageBeginCallback;
			_seting->on_message_complete = OnMessageCompleteCallback;
			_seting->on_status = onStatus;
			_seting->on_url = OnUrlCallback;
		}

		HttpParser::~HttpParser()
		{
			delete _seting;
			delete _parser;
		}

		int HttpParser::parser(Buffer *buf)
		{
			int len = http_parser_execute(_parser, _seting, buf->peek(), buf->readableBytes());
			if (_parser->http_errno != 0)
			{
				return PARSERERROR;
			}
			buf->retrieve(len);
			return _parserStatus;
		}

		int HttpParser::onStatus(http_parser *parser, const char *at, size_t length)
		{
			return 0;
		}

		int HttpParser::OnMessageBeginCallback(http_parser *parser)
		{
			return 0;
		}

		int HttpParser::OnUrlCallback(http_parser *parser, const char *at, size_t length)
		{
			HttpParser* requestparser = (HttpParser*)(parser->data);
			std::string line(at, length);

			do 
			{
				std::vector<std::string> lines = split(line, std::string("?"));
				if ((lines.size() == 0) || (lines.size() > 2))
				{
					requestparser->_parserStatus = PARSERERROR;
					break;
				}

				requestparser->_url = UrlDecode(lines[0]);

				if (lines.size() == 2) 
				{
					std::vector<std::string> params = split(lines[1], std::string("&"));
					for (auto it = params.begin(); it != params.end(); it++)
					{
						std::vector<std::string> param = split(*it, std::string("="));

						if ((param.empty()) || (param.size() > 2))
						{
							requestparser->_parserStatus = PARSERERROR;
							break;
						}


						std::string key = toLower(trim(UrlDecode(param[0])));
						std::string value = "";

						if (param.size() == 2)
						{
							value = trim(UrlDecode(param[1]));// toLower(trim(UrlDecode(param[1])));
						}
						requestparser->_params[key] = value;
					}
				}
			} while (0);
			return 0;
		}

		int HttpParser::OnHeaderFieldCallback(http_parser *parser, const char *at, size_t length)
		{
			HttpParser* requestparser = (HttpParser*)(parser->data);
			if (requestparser->_fild == 1)
			{
				requestparser->_parserStatus = PARSERERROR;
			}
			else
			{
				requestparser->_fild = 1;
				requestparser->_head = std::string(at, length);
			}
			return 0;
		}

		int HttpParser::OnHeaderValueCallback(http_parser *parser, const char *at, size_t length)
		{
			HttpParser* requestparser = (HttpParser*)(parser->data);
			if (requestparser->_fild != 1)
			{
				requestparser->_parserStatus = PARSERERROR;
			}
			else
			{
				requestparser->_fild = 2;
				std::string key = toLower(trim(requestparser->_head));
				std::string value = toLower(trim(std::string(at, length)));
				requestparser->_heads[key] = value;
				if (key == "cookie")
				{
					std::vector<std::string> lines = split(value, std::string(";"));
					for (auto it = lines.begin(); it != lines.end(); it++)
					{
						std::vector<std::string> param = split(*it, std::string("="));
						if(param.size() == 0)
							continue;

						std::string cookiekey = toLower(trim(param[0]));
						std::string cookievalue = "";
						if (param.size() >= 2)
						{
							cookievalue = toLower(trim(param[1]));
							if (cookiekey == "amid")
							{
								requestparser->_sessionid = cookievalue;
							}
						}
						requestparser->_cookie.push_back(Cookie(cookiekey, cookievalue));
					}
				}
				else if(key == "content-length" )
				{
					requestparser->_contextlength = atoi(value.c_str());
				}
			}
			return 0;
		}

		int HttpParser::OnHeadersCompleteCallback(http_parser *parser)
		{
			HttpParser* requestparser = (HttpParser*)(parser->data);
			requestparser->_method = method_strings[parser->method];
			char buf[12] = { 0 };
			snprintf(buf, 11, "HTTP/%d.%d", parser->http_major, parser->http_minor);
			requestparser->_version = buf;
			return 0;
		}

		int HttpParser::OnBodyCallback(http_parser *parser, const char *at, size_t length)
		{
			HttpParser* requestparser = (HttpParser*)(parser->data);
			requestparser->_body.append(at, length);

			return 0;
		}

		int HttpParser::OnMessageCompleteCallback(http_parser *parser)
		{
			HttpParser* requestparser = (HttpParser*)(parser->data);
			if (requestparser->_parserStatus != PARSERERROR)
			{
				if (requestparser->_body.size() >= requestparser->_contextlength)
				{
					requestparser->_parserStatus = PARSERED;

					if (requestparser->_method == "POST") 
					{
						std::vector<std::string> params = split(requestparser->_body, std::string("&"));
						for (auto it = params.begin(); it != params.end(); it++)
						{
							std::vector<std::string> param = split(*it, std::string("="));
							if ((param.size() == 0) || (param.size() > 2))
							{
								requestparser->_parserStatus = PARSERERROR;
								break;
							}

							std::string key = toLower(trim(UrlDecode(param[0])));
							std::string value = "";

							if (param.size() == 2)
							{
								value = trim(UrlDecode(param[1]));//toLower(trim(UrlDecode(param[1])));
							}
							requestparser->_params[key] = value;
						}
					}
				}	
			}

			return 0;
		}


		namespace
		{

			unsigned char ToHex(unsigned char x)
			{
				return  x > 9 ? x + 55 : x + 48;
			}

			unsigned char FromHex(unsigned char x)
			{
				unsigned char y;
				if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
				else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
				else if (x >= '0' && x <= '9') y = x - '0';
				else assert(0);
				return y;
			}
		}

		std::string HttpParser::UrlEncode(const std::string& str)
		{
			std::string strTemp = "";
			size_t length = str.length();
			for (size_t i = 0; i < length; i++)
			{
				if (isalnum((unsigned char)str[i]) ||
					(str[i] == '-') ||
					(str[i] == '_') ||
					(str[i] == '.') ||
					(str[i] == '~'))
				{
					strTemp += str[i];
				}
				else if (str[i] == ' ')
				{
					strTemp += "+";
				}
				else
				{
					strTemp += '%';
					strTemp += ToHex((unsigned char)str[i] >> 4);
					strTemp += ToHex((unsigned char)str[i] % 16);
				}
			}
			return strTemp;
		}

		std::string HttpParser::UrlDecode(const std::string& str)
		{
			std::string strTemp = "";
			size_t length = str.length();
			for (size_t i = 0; i < length; i++)
			{

				if (str[i] == '+') strTemp += ' ';
				else if (str[i] == '%')
				{
					assert(i + 2 < length);
					unsigned char high = FromHex((unsigned char)str[++i]);
					unsigned char low = FromHex((unsigned char)str[++i]);
					strTemp += high * 16 + low;
				}
				else strTemp += str[i];
			}
			return strTemp;
		}
	}
}

