//////////////////////////////////////////////////////////////////////////////////
//�ļ���InetAddress.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�����������ַ
////////////////////////////////////////////////////////////////////////////////////

#include "InetAddress.h"
#include <base/Logging.h>

namespace EtherDB
{
	namespace Net
	{
		static const uint32_t kInaddrAny = INADDR_ANY;
		static const uint32_t kInaddrLoopback = INADDR_LOOPBACK;

		InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6)
		{
			memset(&_addr, 0, sizeof _addr);
			if (ipv6)
			{
				_addr.addr6.sin6_family = AF_INET6;
				in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
				_addr.addr6.sin6_addr = ip;
				_addr.addr6.sin6_port = ::htons(port);
			}
			else
			{
				_addr.addr4.sin_family = AF_INET;
				uint32_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
				_addr.addr4.sin_addr.s_addr = htonl(ip);
				_addr.addr4.sin_port = htons(port);
			}
		}

		InetAddress::InetAddress(std::string ip, uint16_t port, bool ipv6)
		{
			memset(&_addr, 0, sizeof _addr);
			if (ipv6)
			{
				_addr.addr6.sin6_family = AF_INET6;
				_addr.addr6.sin6_port = ::htons(port);
				if (::inet_pton(AF_INET6, ip.c_str(), &_addr.addr6.sin6_addr) <= 0)
				{
					LOG_SYSERR << "sockets::fromIpPort";
				}
			}
			else
			{
				_addr.addr4.sin_family = AF_INET;
				_addr.addr4.sin_port = ::htons(port);
				if (::inet_pton(AF_INET, ip.c_str(), &_addr.addr4.sin_addr) <= 0)
				{
					LOG_SYSERR << "sockets::fromIpPort";
				}
			}

		}

		InetAddress::InetAddress(const struct sockaddr_in& addr)
		{
			_addr.addr4 = addr;
		}

		InetAddress::InetAddress(const struct sockaddr_in6& addr)
		{
			_addr.addr6 = addr;
		}

		std::string InetAddress::toIp() const
		{
			char buf[64] = { 0 };
			if (_addr.addr4.sin_family == AF_INET)
			{
				::inet_ntop(AF_INET, (void*)&_addr.addr4.sin_addr, buf, 64);
			}
			else if (_addr.addr4.sin_family == AF_INET6)
			{
				::inet_ntop(AF_INET6, (void*)&_addr.addr6.sin6_addr, buf, 64);
			}
			return buf;
		}

		std::string InetAddress::toIpPort() const
		{
			char buf[64] = { 0 };

			std::string ip = toIp();
			memcpy(buf, ip.c_str(), ip.size());

			uint16_t port = ::ntohs(_addr.addr4.sin_port);
			snprintf(buf + ip.size(), 64 - ip.size(), ":%u", port);
			return buf;
		}

		uint16_t InetAddress::toPort() const
		{
			return ::ntohs(portNetEndian());
		}

		uint32_t InetAddress::ipNetEndian() const
		{
			assert(family() == AF_INET);
			return _addr.addr4.sin_addr.s_addr;
		}


	}
}



