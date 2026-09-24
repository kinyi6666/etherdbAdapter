//////////////////////////////////////////////////////////////////////////////////
//�ļ���InetAddress.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�����������ַ
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_InetAddress_H_
#define __EtherDB_Net_InetAddress_H_

#include "AmNetConfig.h"

#ifndef INWINDOWS
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <base/AmWindows.h>
#endif // !INWINDOWS

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API InetAddress
		{
		public:
			explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);
			InetAddress(std::string ip, uint16_t port, bool ipv6 = false);
			explicit InetAddress(const struct sockaddr_in& addr);
			explicit InetAddress(const struct sockaddr_in6& addr);

			ADDRESS_FAMILY family() const { return _addr.addr4.sin_family; }
			std::string toIp() const;
			std::string toIpPort() const;
			uint16_t toPort() const;

			const struct sockaddr* getSockAddr() const { return static_cast<const struct sockaddr*>(static_cast<const void*>(&_addr.addr6)); }
			void setSockAddrInet6(const struct sockaddr_in6& addr6) { _addr.addr6 = addr6; }

			uint32_t ipNetEndian() const;
			uint16_t portNetEndian() const { return _addr.addr4.sin_port; }

			//static bool resolve(std::string hostname, InetAddress* result);

		private:
			union Addr
			{
				struct sockaddr_in addr4;
				struct sockaddr_in6 addr6;
			};

			Addr _addr;
		};
	}
}

#endif // !__EtherDB_Net_InetAddress_H_


