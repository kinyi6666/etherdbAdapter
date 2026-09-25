/*
 * EtherDB — POSIX <dirent.h> emulation for MSVC (VS2022).
 *
 * Minimal but functional directory reading over the Win32 FindFirstFile/
 * FindNextFile APIs. Emulates POSIX behaviour: readdir() first yields "."
 * then "..", then the directory entries in the order returned by the FS.
 *
 * Windows-only. Linux uses the real <dirent.h>.
 */

#ifndef ETHERDB_WINCOMPAT_DIRENT_H
#define ETHERDB_WINCOMPAT_DIRENT_H

#if !defined(_WIN32) && !defined(_WIN64)
#error "dirent.h (wincompat) is for Windows builds only"
#endif

#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* d_type values (subset of POSIX DT_*) */
#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif

#define _WINCOMPAT_NAME_MAX 260

struct dirent {
    long          d_ino;    /* not meaningful on Windows */
    unsigned char d_type;   /* DT_DIR / DT_REG / DT_UNKNOWN */
    char          d_name[_WINCOMPAT_NAME_MAX];
};

typedef struct DIR {
    HANDLE             hFind;
    WIN32_FIND_DATAA   fd;
    int                dotsLeft;  /* 2, 1, 0 -> yield "." and ".." first */
    int                eof;       /* no more real entries */
    struct dirent      ent;
} DIR;

static inline DIR* opendir(const char* dirname) {
    if (!dirname || !*dirname) return NULL;

    size_t len = strlen(dirname);
    if (len == 0 || len >= _WINCOMPAT_NAME_MAX - 2) return NULL;

    char pattern[_WINCOMPAT_NAME_MAX];
    memcpy(pattern, dirname, len);
    /* Trim a single trailing separator before appending "\\*". */
    while (len > 0 && (pattern[len - 1] == '\\' || pattern[len - 1] == '/')) {
        pattern[len - 1] = '\0';
        len--;
    }
    if (len == 0) return NULL;
    pattern[len] = '\\';
    pattern[len + 1] = '*';
    pattern[len + 2] = '\0';

    DIR* dir = (DIR*)calloc(1, sizeof(DIR));
    if (!dir) return NULL;

    dir->hFind = FindFirstFileA(pattern, &dir->fd);
    dir->dotsLeft = 2;
    dir->eof = 0;
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
            /* Empty (or non-existent) dir -> only "." and ".." will be seen. */
            dir->hFind = INVALID_HANDLE_VALUE;
            dir->eof = 1;
        } else {
            free(dir);
            return NULL;
        }
    }
    return dir;
}

static inline struct dirent* readdir(DIR* dir) {
    if (!dir) return NULL;

    if (dir->dotsLeft > 0) {
        dir->dotsLeft--;
        const char* nm = (dir->dotsLeft == 1) ? ".." : ".";
        strncpy(dir->ent.d_name, nm, sizeof(dir->ent.d_name) - 1);
        dir->ent.d_name[sizeof(dir->ent.d_name) - 1] = '\0';
        dir->ent.d_ino = 0;
        dir->ent.d_type = DT_DIR;
        return &dir->ent;
    }

    if (dir->eof) return NULL;

    if (!FindNextFileA(dir->hFind, &dir->fd)) {
        dir->eof = 1;
        return NULL;
    }

    dir->ent.d_ino = 0;
    if (dir->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        dir->ent.d_type = DT_DIR;
    } else {
        dir->ent.d_type = DT_REG;
    }
    strncpy(dir->ent.d_name, dir->fd.cFileName, sizeof(dir->ent.d_name) - 1);
    dir->ent.d_name[sizeof(dir->ent.d_name) - 1] = '\0';
    return &dir->ent;
}

static inline int closedir(DIR* dir) {
    if (!dir) return 0;
    if (dir->hFind != INVALID_HANDLE_VALUE) {
        FindClose(dir->hFind);
    }
    free(dir);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERDB_WINCOMPAT_DIRENT_H */
