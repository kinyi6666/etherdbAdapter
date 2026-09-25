/*
 * EtherDB — POSIX <sys/socket.h> for MSVC.
 * Backed by Winsock2 (already force-included via posix_compat.h).
 */

#ifndef ETHERDB_WINCOMPAT_SYS_SOCKET_H
#define ETHERDB_WINCOMPAT_SYS_SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "posix_compat.h"  /* socklen_t */

#endif /* ETHERDB_WINCOMPAT_SYS_SOCKET_H */
