/* API FATX simulada para executar o adaptador real no WSL, sem emulador. */
#ifndef TESTE_INSTALAR_XTL_H
#define TESTE_INSTALAR_XTL_H
#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define __int64 long long
typedef unsigned DWORD;
typedef int BOOL;
typedef int LONG;
typedef int HANDLE;
typedef union {
    long long QuadPart;
} LARGE_INTEGER;
typedef union {
    unsigned long long QuadPart;
} ULARGE_INTEGER;
#define INVALID_HANDLE_VALUE (-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD) - 1)
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define CREATE_NEW 1
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0
#define FILE_BEGIN 0
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_PATH_NOT_FOUND ENOTDIR
static int falha(const char *s) {
    const char *p = getenv("TESTE_FALHA");
    return p && !strcmp(p, s);
}
static const char *caminho(const char *p, char *buf) {
    if (!strncmp(p, "game:\\", 6)) {
        snprintf(buf, 1024, "%s/%s", getenv("TESTE_GAME"), p + 6);
        return buf;
    }
    return p;
}
static HANDLE CreateFile(const char *p, int a, int share, void *x, int modo, int attr, void *y) {
    char buf[1024];
    (void)share;
    (void)x;
    (void)attr;
    (void)y;
    if (falha("reabrir") && modo == OPEN_EXISTING && strstr(p, ".tmp"))
        return -1;
    return open(caminho(p, buf),
                (a & GENERIC_WRITE ? O_RDWR : O_RDONLY) |
                    (modo == CREATE_NEW ? O_CREAT | O_EXCL | O_NOFOLLOW : 0),
                0600);
}
static BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER *n) {
    struct stat s;
    if (fstat(h, &s))
        return 0;
    n->QuadPart = s.st_size;
    return 1;
}
static BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER p, void *a, int b) {
    (void)a;
    (void)b;
    return lseek(h, p.QuadPart, SEEK_SET) == p.QuadPart;
}
static BOOL ReadFile(HANDLE h, void *p, unsigned n, DWORD *l, void *a) {
    ssize_t r;
    (void)a;
    if (falha("leitura")) {
        *l = 0;
        return 1;
    }
    r = read(h, p, n);
    *l = r < 0 ? 0 : (DWORD)r;
    return r >= 0;
}
static BOOL WriteFile(HANDLE h, const void *p, unsigned n, DWORD *l, void *a) {
    static unsigned total;
    ssize_t r;
    (void)a;
    if (falha("parar") && total >= 16 * 1024 * 1024)
        _exit(77);
    if (falha("escrever")) {
        *l = 0;
        return 1;
    }
    r = write(h, p, n);
    *l = r < 0 ? 0 : (DWORD)r;
    total += *l;
    return r >= 0;
}
static BOOL CloseHandle(HANDLE h) {
    int escrita = fcntl(h, F_GETFL) & O_RDWR;
    int r = close(h);
    return !r && !(falha("fechar") && escrita);
}
static BOOL FlushFileBuffers(HANDLE h) {
    const char *p = getenv("TESTE_CORROMPER");
    if (falha("sincronizar"))
        return 0;
    if (falha("truncar") && ftruncate(h, 3020816383LL))
        return 0;
    if (p) {
        unsigned char b;
        off_t pos = (off_t)strtoull(p, NULL, 0);
        if (pread(h, &b, 1, pos) != 1)
            return 0;
        b ^= 1;
        if (pwrite(h, &b, 1, pos) != 1)
            return 0;
    }
    return fsync(h) == 0;
}
static BOOL GetDiskFreeSpaceEx(const char *p, ULARGE_INTEGER *a, ULARGE_INTEGER *b, ULARGE_INTEGER *c) {
    struct statvfs s;
    char buf[1024];
    if (statvfs(caminho(p, buf), &s))
        return 0;
    a->QuadPart = falha("espaco") ? 3221225471ULL : (unsigned long long)s.f_bavail * s.f_frsize;
    b->QuadPart = (unsigned long long)s.f_blocks * s.f_frsize;
    c->QuadPart = a->QuadPart;
    return 1;
}
static DWORD GetFileAttributes(const char *p) {
    struct stat s;
    char b[1024];
    return lstat(caminho(p, b), &s) ? INVALID_FILE_ATTRIBUTES : 0;
}
static BOOL DeleteFile(const char *p) {
    char b[1024];
    return unlink(caminho(p, b)) == 0;
}
static BOOL MoveFile(const char *a, const char *b) {
    char x[1024], y[1024];
    const char *p = caminho(a, x), *q = caminho(b, y);
    if (falha("publicar")) {
        int fd = open(q, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            if (write(fd, "preservado", 10) != 10)
                abort();
            close(fd);
        }
    }
    return syscall(SYS_renameat2, AT_FDCWD, p, AT_FDCWD, q, 1) == 0;
}
static DWORD GetLastError(void) {
    return (DWORD)errno;
}
static DWORD GetTickCount(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (DWORD)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
static LONG InterlockedExchange(volatile LONG *p, LONG v) {
    return __sync_lock_test_and_set(p, v);
}
static LONG InterlockedCompareExchange(volatile LONG *p, LONG v, LONG c) {
    return __sync_val_compare_and_swap(p, c, v);
}
#endif
