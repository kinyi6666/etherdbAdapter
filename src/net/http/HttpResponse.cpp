//////////////////////////////////////////////////////////////////////////////////
//�ļ���HttpResponse.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������HttpResponse
////////////////////////////////////////////////////////////////////////////////////

#include "../TcpConnection.h"
#include "HttpResponse.h"
#include "HttpServer.h"

namespace EtherDB
{
	namespace Net
	{
		const std::string HttpResponse::HTTP_REASON_CONTINUE = "Continue";
		const std::string HttpResponse::HTTP_REASON_SWITCHING_PROTOCOLS = "Switching Protocols";
		const std::string HttpResponse::HTTP_REASON_OK = "OK";
		const std::string HttpResponse::HTTP_REASON_CREATED = "Created";
		const std::string HttpResponse::HTTP_REASON_ACCEPTED = "Accepted";
		const std::string HttpResponse::HTTP_REASON_NONAUTHORITATIVE = "Non-Authoritative Information";
		const std::string HttpResponse::HTTP_REASON_NO_CONTENT = "No Content";
		const std::string HttpResponse::HTTP_REASON_RESET_CONTENT = "Reset Content";
		const std::string HttpResponse::HTTP_REASON_PARTIAL_CONTENT = "Partial Content";
		const std::string HttpResponse::HTTP_REASON_MULTIPLE_CHOICES = "Multiple Choices";
		const std::string HttpResponse::HTTP_REASON_MOVED_PERMANENTLY = "Moved Permanently";
		const std::string HttpResponse::HTTP_REASON_FOUND = "Found";
		const std::string HttpResponse::HTTP_REASON_SEE_OTHER = "See Other";
		const std::string HttpResponse::HTTP_REASON_NOT_MODIFIED = "Not Modified";
		const std::string HttpResponse::HTTP_REASON_USEPROXY = "Use Proxy";
		const std::string HttpResponse::HTTP_REASON_TEMPORARY_REDIRECT = "Temporary Redirect";
		const std::string HttpResponse::HTTP_REASON_BAD_REQUEST = "Bad Request";
		const std::string HttpResponse::HTTP_REASON_UNAUTHORIZED = "Unauthorized";
		const std::string HttpResponse::HTTP_REASON_PAYMENT_REQUIRED = "Payment Required";
		const std::string HttpResponse::HTTP_REASON_FORBIDDEN = "Forbidden";
		const std::string HttpResponse::HTTP_REASON_NOT_FOUND = "Not Found";
		const std::string HttpResponse::HTTP_REASON_METHOD_NOT_ALLOWED = "Method Not Allowed";
		const std::string HttpResponse::HTTP_REASON_NOT_ACCEPTABLE = "Not Acceptable";
		const std::string HttpResponse::HTTP_REASON_PROXY_AUTHENTICATION_REQUIRED = "Proxy Authentication Required";
		const std::string HttpResponse::HTTP_REASON_REQUEST_TIMEOUT = "Request Time-out";
		const std::string HttpResponse::HTTP_REASON_CONFLICT = "Conflict";
		const std::string HttpResponse::HTTP_REASON_GONE = "Gone";
		const std::string HttpResponse::HTTP_REASON_LENGTH_REQUIRED = "Length Required";
		const std::string HttpResponse::HTTP_REASON_PRECONDITION_FAILED = "Precondition Failed";
		const std::string HttpResponse::HTTP_REASON_REQUESTENTITYTOOLARGE = "Request Entity Too Large";
		const std::string HttpResponse::HTTP_REASON_REQUESTURITOOLONG = "Request-URI Too Large";
		const std::string HttpResponse::HTTP_REASON_UNSUPPORTEDMEDIATYPE = "Unsupported Media Type";
		const std::string HttpResponse::HTTP_REASON_REQUESTED_RANGE_NOT_SATISFIABLE = "Requested Range Not Satisfiable";
		const std::string HttpResponse::HTTP_REASON_EXPECTATION_FAILED = "Expectation Failed";
		const std::string HttpResponse::HTTP_REASON_INTERNAL_SERVER_ERROR = "Internal Server Error";
		const std::string HttpResponse::HTTP_REASON_NOT_IMPLEMENTED = "Not Implemented";
		const std::string HttpResponse::HTTP_REASON_BAD_GATEWAY = "Bad Gateway";
		const std::string HttpResponse::HTTP_REASON_SERVICE_UNAVAILABLE = "Service Unavailable";
		const std::string HttpResponse::HTTP_REASON_GATEWAY_TIMEOUT = "Gateway Time-out";
		const std::string HttpResponse::HTTP_REASON_VERSION_NOT_SUPPORTED = "HTTP Version not supported";
		const std::string HttpResponse::HTTP_REASON_UNKNOWN = "???";
		const std::string HttpResponse::DATE = "Date";
		const std::string HttpResponse::SET_COOKIE = "Set-Cookie";


		HttpResponse::HttpResponse()
			:_status(200)
		{

		}

		void HttpResponse::setIntHeader(std::string name, int value)
		{
			char buf[24] = { 0 };
			snprintf(buf, 23, "%d", value);
			_heads[name] = buf;
		}

		void HttpResponse::setDateHeader(std::string name, Timestamp date)
		{
			setHeader(name, date.toFromattedHttp());
		}

		void HttpResponse::setContentType(std::string type)
		{
			_heads["Content-Type"] = type;
		}

		void HttpResponse::setContentLength(int len)
		{
			setIntHeader("Content-Length", len);
		}

		void HttpResponse::setCharacterEncoding(std::string charset)
		{

		}

		void HttpResponse::sendRedirect(std::string location)
		{

		}

		void HttpResponse::reset()
		{
			_status = 200;
			_heads.clear();
			_cookis.clear();
			_body = "";
		}

		void HttpResponse::flush()
		{
			if (_flushCb)
			{
				_flushCb(this);
			}
			else
			{
				defaultflush();
			}
		}

		void HttpResponse::defaultflush()
		{
			do 
			{
				TcpConnectionPtr conn = _conn.lock();
				if(!conn)
					break;

				if (_request->getHeader("connection") == "keep-alive")
				{
					setHeader("Connection", "Keep-Alive");
				}
				setContentLength(_body.size());
				char status[8] = { 0 };
				sprintf(status, "%d", _status);

				std::string html;

				html.append(_request->getProtocol() + " " + status + " "+ getReasonForStatus(_status) +"\r\n");

				
				for (auto it = _heads.begin(); it != _heads.end(); it++)
				{
					html.append(it->first + ":" + it->second + "\r\n");
				}

				HttpSessionPtr session = _request->getSession();
				if (session)
				{
					_cookis.push_back(session->createSeesionCookie());
				}

				for (auto it = _cookis.begin(); it != _cookis.end(); it++)
				{
					html.append("Set-Cookie:" + it->toString() + "\r\n");
				}

				html.append("\r\n");
				html.append(_body);
	
				conn->send(html);
				

			} while (0);
		}

		const std::string& HttpResponse::getReasonForStatus(int status)
		{
			switch (status)
			{
			case HTTP_CONTINUE:
				return HTTP_REASON_CONTINUE;
			case HTTP_SWITCHING_PROTOCOLS:
				return HTTP_REASON_SWITCHING_PROTOCOLS;
			case HTTP_OK:
				return HTTP_REASON_OK;
			case HTTP_CREATED:
				return HTTP_REASON_CREATED;
			case HTTP_ACCEPTED:
				return HTTP_REASON_ACCEPTED;
			case HTTP_NONAUTHORITATIVE:
				return HTTP_REASON_NONAUTHORITATIVE;
			case HTTP_NO_CONTENT:
				return HTTP_REASON_NO_CONTENT;
			case HTTP_RESET_CONTENT:
				return HTTP_REASON_RESET_CONTENT;
			case HTTP_PARTIAL_CONTENT:
				return HTTP_REASON_PARTIAL_CONTENT;
			case HTTP_MULTIPLE_CHOICES:
				return HTTP_REASON_MULTIPLE_CHOICES;
			case HTTP_MOVED_PERMANENTLY:
				return HTTP_REASON_MOVED_PERMANENTLY;
			case HTTP_FOUND:
				return HTTP_REASON_FOUND;
			case HTTP_SEE_OTHER:
				return HTTP_REASON_SEE_OTHER;
			case HTTP_NOT_MODIFIED:
				return HTTP_REASON_NOT_MODIFIED;
			case HTTP_USEPROXY:
				return HTTP_REASON_USEPROXY;
			case HTTP_TEMPORARY_REDIRECT:
				return HTTP_REASON_TEMPORARY_REDIRECT;
			case HTTP_BAD_REQUEST:
				return HTTP_REASON_BAD_REQUEST;
			case HTTP_UNAUTHORIZED:
				return HTTP_REASON_UNAUTHORIZED;
			case HTTP_PAYMENT_REQUIRED:
				return HTTP_REASON_PAYMENT_REQUIRED;
			case HTTP_FORBIDDEN:
				return HTTP_REASON_FORBIDDEN;
			case HTTP_NOT_FOUND:
				return HTTP_REASON_NOT_FOUND;
			case HTTP_METHOD_NOT_ALLOWED:
				return HTTP_REASON_METHOD_NOT_ALLOWED;
			case HTTP_NOT_ACCEPTABLE:
				return HTTP_REASON_NOT_ACCEPTABLE;
			case HTTP_PROXY_AUTHENTICATION_REQUIRED:
				return HTTP_REASON_PROXY_AUTHENTICATION_REQUIRED;
			case HTTP_REQUEST_TIMEOUT:
				return HTTP_REASON_REQUEST_TIMEOUT;
			case HTTP_CONFLICT:
				return HTTP_REASON_CONFLICT;
			case HTTP_GONE:
				return HTTP_REASON_GONE;
			case HTTP_LENGTH_REQUIRED:
				return HTTP_REASON_LENGTH_REQUIRED;
			case HTTP_PRECONDITION_FAILED:
				return HTTP_REASON_PRECONDITION_FAILED;
			case HTTP_REQUESTENTITYTOOLARGE:
				return HTTP_REASON_REQUESTENTITYTOOLARGE;
			case HTTP_REQUESTURITOOLONG:
				return HTTP_REASON_REQUESTURITOOLONG;
			case HTTP_UNSUPPORTEDMEDIATYPE:
				return HTTP_REASON_UNSUPPORTEDMEDIATYPE;
			case HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
				return HTTP_REASON_REQUESTED_RANGE_NOT_SATISFIABLE;
			case HTTP_EXPECTATION_FAILED:
				return HTTP_REASON_EXPECTATION_FAILED;
			case HTTP_INTERNAL_SERVER_ERROR:
				return HTTP_REASON_INTERNAL_SERVER_ERROR;
			case HTTP_NOT_IMPLEMENTED:
				return HTTP_REASON_NOT_IMPLEMENTED;
			case HTTP_BAD_GATEWAY:
				return HTTP_REASON_BAD_GATEWAY;
			case HTTP_SERVICE_UNAVAILABLE:
				return HTTP_REASON_SERVICE_UNAVAILABLE;
			case HTTP_GATEWAY_TIMEOUT:
				return HTTP_REASON_GATEWAY_TIMEOUT;
			case HTTP_VERSION_NOT_SUPPORTED:
				return HTTP_REASON_VERSION_NOT_SUPPORTED;
			default:
				return HTTP_REASON_UNKNOWN;
			}
		}

		void HttpResponse::forward(std::string url)
		{
			//reset();
			_body = "";
			_flushCb = ResponseFlushCallback();
			_request->_url = url;
			_server->handleRequest(_request, shared_from_this());
		}
	}
}

