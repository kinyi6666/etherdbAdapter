/*
 * EtherDB — minimal POSIX <unistd.h> for MSVC (VS2022).
 * read/write/close/open/lseek/access/... are provided by the CRT headers
 * below (MSVC maps them to their _underscore equivalents automatically when
 * _CRT_NONSTDC_NO_DEPRECATE is NOT defined).
 */

#ifndef ETHERDB_WINCOMPAT_UNISTD_H
#define ETHERDB_WINCOMPAT_UNISTD_H

#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdint.h>

#include "posix_compat.h" /* ssize_t, sleep, usleep, fsync, fileno, mkdir, ... */

/* Standard descriptor numbers */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* lseek whence values are shared with SEEK_SET/... from <stdio.h> */

#endif /* ETHERDB_WINCOMPAT_UNISTD_H */
