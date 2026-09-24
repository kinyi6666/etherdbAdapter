/*
 * EtherDB — POSIX <arpa/inet.h> for MSVC.
 * inet_pton / inet_ntop / inet_addr / htonl / ntohl ... come from
 * <winsock2.h> / <ws2tcpip.h> (force-included via posix_compat.h).
 */

#ifndef ETHERDB_WINCOMPAT_ARPA_INET_H
#define ETHERDB_WINCOMPAT_ARPA_INET_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "posix_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* inet_aton(): set address to IPv4 a.b.c.d string. Returns 1 on success. */
static inline int inet_aton(const char* cp, struct in_addr* inp) {
    if (!cp || !inp) return 0;
    unsigned long a = inet_addr(cp);
    if (a == INADDR_NONE) return 0;  /* also true for 255.255.255.255, acceptable */
    inp->s_addr = a;
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERDB_WINCOMPAT_ARPA_INET_H */
