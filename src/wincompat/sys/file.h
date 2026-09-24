/*
 * EtherDB — POSIX <sys/file.h> for MSVC.
 * flock() advisory locking has no Win32 equivalent used by the server;
 * provide the constants + a no-op implementation so the code compiles.
 */

#ifndef ETHERDB_WINCOMPAT_SYS_FILE_H
#define ETHERDB_WINCOMPAT_SYS_FILE_H

#ifndef LOCK_SH
#define LOCK_SH 1
#endif
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
#ifndef LOCK_UN
#define LOCK_UN 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Not supported on Windows: reported as success (advisory, best-effort). */
static inline int flock(int /*fd*/, int /*operation*/) {
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERDB_WINCOMPAT_SYS_FILE_H */
