/*
 * EtherDB — Windows POSIX compatibility shims (MSVC / VS2022 only).
 *
 * This header is force-included (/FI) into every translation unit of the
 * Windows build so that:
 *   1. NOMINMAX / WIN32_LEAN_AND_MEAN and <winsock2.h> are in place BEFORE any
 *      <windows.h>, avoiding both the classic winsock/windows.h conflict and
 *      the Windows min/max macros clobbering std::min/std::max.
 *   2. A small set of POSIX names MSVC does not provide (S_ISDIR, ssize_t,
 *      fsync, mkdir(path,mode), sleep, strcasecmp, __builtin_ctz, ...) become
 *      available everywhere.
 *
 * Compiled ONLY on Windows (see CMakeLists). Linux never includes this file.
 */

#ifndef ETHERDB_WINCOMPAT_POSIX_COMPAT_H
#define ETHERDB_WINCOMPAT_POSIX_COMPAT_H

#if !defined(_WIN32) && !defined(_WIN64)
#error "posix_compat.h is for the Windows (MSVC) build only"
#endif

/* ---- MUST precede every <windows.h> include ---- */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN_WIN10
#define WIN32_LEAN_AND_MEAN_WIN10 1
#endif

/* ---- winsock2 before windows.h ---- */
#include <winsock2.h>
#include <ws2tcpip.h>

#include <direct.h>   /* _mkdir */
#include <io.h>       /* _read/_write/_open/_close/_fileno ... */
#include <process.h>  /* _getpid / getpid */
#include <fcntl.h>    /* _O_* flags */
#include <sys/stat.h> /* struct _stat / stat */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <intrin.h>   /* _BitScanForward/_BitScanReverse */

/* ---- MSVC does not support GCC's __attribute__.
 * NOTE: This mapping is only a safety net for GCC-isms in bundled code
 * (e.g. lz4's internal unaligned-read union). Any struct that defines an
 * ON-DISK / ON-WIRE format must NOT rely on __attribute__((packed)) — those
 * use portable #pragma pack(push,1)/pop or fixed-width byte encoding so their
 * layout is identical on Linux (GCC) and Windows (MSVC). ---- */
#ifndef __attribute__
#define __attribute__(x)
#endif

/* ---- ssize_t ---- */
#ifndef _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

/* ---- socklen_t ---- */
#ifndef _SOCKLEN_T_DEFINED
typedef int socklen_t;
#define _SOCKLEN_T_DEFINED
#endif

/* ---- mode-test macros missing from MSVC <sys/stat.h> ---- */
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m)  (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)  (((m) & _S_IFMT) == _S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(m)  (0)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(m) (0)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m)  (0)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(m) (0)
#endif

/* ---- fileno / fsync / unlink / access / isatty ---- */
#ifndef fileno
#define fileno _fileno
#endif
#ifndef fsync
#define fsync _commit
#endif
#ifndef unlink
#define unlink _unlink
#endif
#ifndef access
#define access _access
#endif
#ifndef isatty
#define isatty _isatty
#endif

/* ---- case-insensitive compare / strdup ---- */
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strdup
#define strdup _strdup
#endif

/* ---- access modes (F_OK/R_OK/W_OK/X_OK) ---- */
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif
#ifndef F_OK
#define F_OK 0
#endif

/* ---- O_ACCMODE (MSVC <fcntl.h> does not define it) ---- */
#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

/* ---- mkdir(path, mode) -> _mkdir(path) (POSIX 2-arg form) ---- */
#ifdef mkdir
#undef mkdir
#endif
static __inline int etdbWinMkdir(const char* path, unsigned int mode) {
    (void)mode;  /* POSIX permissions have no Win32 meaning */
    return _mkdir(path);
}
#define mkdir(path, mode) etdbWinMkdir((path), (mode))

/* ---- lstat(): Windows has no symlinks, so lstat() behaves as stat(). ---- */
#ifndef lstat
#define lstat stat
#endif

/* ---- sleep / usleep / srandom / random as FUNCTIONS (not macros) so that
 *      member functions named sleep() in existing headers are not clobbered.
 *      MSVC provides no global sleep(); these fill the gap. ---- */
static __inline int etdbWinSleep(unsigned int sec) {
    Sleep(sec * 1000u);
    return 0;
}
static __inline int etdbWinUsleep(unsigned int usec) {
    if (usec != 0) Sleep((usec + 999) / 1000);
    return 0;
}
static __inline void etdbWinSrandom(unsigned int seed) { srand(seed); }
static __inline long etdbWinRandom(void) { return rand(); }

/* Plain POSIX names. Declared as file-local functions to avoid colliding with
 * any class member named sleep/random (a macro would clobber those). */
static __inline int sleep(unsigned int sec) { return etdbWinSleep(sec); }
static __inline int usleep(unsigned int usec) { return etdbWinUsleep(usec); }
static __inline void srandom(unsigned int seed) { etdbWinSrandom(seed); }
static __inline long random(void) { return etdbWinRandom(); }

/* ---- GCC/Clang builtins used by the code -> MSVC intrinsics ---- */
static __inline int etdbCtz32(unsigned int x) {
    unsigned long idx = 0;
    _BitScanForward(&idx, x);
    return (int)idx;
}
static __inline int etdbClz32(unsigned int x) {
    unsigned long idx = 0;
    _BitScanReverse(&idx, x);
    return (int)(31 - idx);
}
static __inline int etdbCtz64(unsigned __int64 x) {
    unsigned long idx = 0;
    _BitScanForward64(&idx, x);
    return (int)idx;
}
static __inline int etdbClz64(unsigned __int64 x) {
    unsigned long idx = 0;
    _BitScanReverse64(&idx, x);
    return (int)(63 - idx);
}
static __inline int etdbPopcnt32(unsigned int x) { return (int)__popcnt(x); }
static __inline int etdbPopcnt64(unsigned __int64 x) { return (int)__popcnt64(x); }

#ifdef __cplusplus
/* Overloaded helpers mirroring the GCC __builtin_* names. */
static __inline int __builtin_ctz(unsigned int x)         { return etdbCtz32(x); }
static __inline int __builtin_ctz(unsigned long x)        { return etdbCtz64((unsigned __int64)x); }
static __inline int __builtin_ctz(unsigned long long x)   { return etdbCtz64((unsigned __int64)x); }
static __inline int __builtin_clz(unsigned int x)         { return etdbClz32(x); }
static __inline int __builtin_clz(unsigned long x)        { return etdbClz64((unsigned __int64)x); }
static __inline int __builtin_clz(unsigned long long x)   { return etdbClz64((unsigned __int64)x); }
static __inline int __builtin_popcount(unsigned int x)    { return etdbPopcnt32(x); }
static __inline int __builtin_popcountll(unsigned long long x) { return etdbPopcnt64((unsigned __int64)x); }
#endif /* __cplusplus */

/* ---- errno aliases (Winsock error codes share names with POSIX errno) ---- */
#ifndef EINTR
#define EINTR WSAEINTR
#endif
#ifndef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef EAGAIN
#define EAGAIN WSAEWOULDBLOCK
#endif
#ifndef ENOTCONN
#define ENOTCONN WSAENOTCONN
#endif
#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

#endif /* ETHERDB_WINCOMPAT_POSIX_COMPAT_H */
