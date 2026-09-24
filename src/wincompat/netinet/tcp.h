/*
 * EtherDB — POSIX <netinet/tcp.h> for MSVC.
 * TCP_NODELAY / IPPROTO_TCP are defined by Winsock2.
 */

#ifndef ETHERDB_WINCOMPAT_NETINET_TCP_H
#define ETHERDB_WINCOMPAT_NETINET_TCP_H

#include <winsock2.h>
#include "posix_compat.h"

#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#endif /* ETHERDB_WINCOMPAT_NETINET_TCP_H */
