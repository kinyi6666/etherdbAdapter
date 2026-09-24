/*
 * EtherDB — POSIX <netinet/in.h> for MSVC.
 * sockaddr_in / in_addr / htons / htonl / ntohs / ntohl come from Winsock2.
 */

#ifndef ETHERDB_WINCOMPAT_NETINET_IN_H
#define ETHERDB_WINCOMPAT_NETINET_IN_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "posix_compat.h"  /* socklen_t */

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#endif /* ETHERDB_WINCOMPAT_NETINET_IN_H */
