/*
 * F6: o pedaço da API do 360 que o port/x360/src/iso360.c usa, sobre POSIX, só para o teste do WSL
 * (port/x360/src/som/teste/teste_fluxo.c, pelo port/x360/tools/teste_som360.sh, com -I desta pasta). Não entra em
 * nenhuma build do jogo.
 *
 * O ReadFile tem uma leitora lenta: uma leitura que toca [gTesteLentaIni, gTesteLentaFim) da imagem dorme
 * gTesteLentaUs microssegundos antes de ler (o disco lento do console, dentro da trava de quem lê), e gTesteDentro
 * conta as leituras dormindo agora. As variáveis são do teste.
 */
#ifndef TESTE_XTL_H
#define TESTE_XTL_H
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>
#include <sys/syscall.h>

#define __int64 long long
typedef int BOOL;
typedef unsigned int DWORD;
typedef void *HANDLE;
typedef union { long long QuadPart; } LARGE_INTEGER;
typedef pthread_mutex_t CRITICAL_SECTION;
typedef struct { char cFileName[260]; } WIN32_FIND_DATA;

#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define GENERIC_READ 1
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_BEGIN 0
#define _snprintf snprintf
#define _stricmp strcasecmp
#define __lwsync() __sync_synchronize()

extern long long gTesteLentaIni, gTesteLentaFim;
extern long gTesteLentaUs;
extern volatile int gTesteDentro;

typedef struct TesteArq { int fd; long long pos; } TesteArq;

static inline HANDLE CreateFile(const char *c, int a, int s, void *x, int d, int f, void *y) {
    int fd = open(c, O_RDONLY);
    TesteArq *h;
    (void)a; (void)s; (void)x; (void)d; (void)f; (void)y;
    if (fd < 0) {
        return INVALID_HANDLE_VALUE;
    }
    h = (TesteArq *)calloc(1, sizeof(*h));
    h->fd = fd;
    return h;
}
static inline BOOL CloseHandle(HANDLE h) {
    close(((TesteArq *)h)->fd);
    free(h);
    return 1;
}
static inline BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER p, void *n, int origem) {
    (void)n; (void)origem;
    ((TesteArq *)h)->pos = p.QuadPart;
    return 1;
}
static inline BOOL ReadFile(HANDLE h, void *buf, DWORD n, DWORD *lidos, void *ov) {
    TesteArq *a = (TesteArq *)h;
    ssize_t r;
    (void)ov;
    if (gTesteLentaUs > 0 && a->pos < gTesteLentaFim && a->pos + (long long)n > gTesteLentaIni) {
        struct timespec t;
        t.tv_sec = gTesteLentaUs / 1000000;
        t.tv_nsec = (gTesteLentaUs % 1000000) * 1000;
        __sync_fetch_and_add(&gTesteDentro, 1);
        nanosleep(&t, NULL);
        __sync_fetch_and_sub(&gTesteDentro, 1);
    }
    r = pread(a->fd, buf, n, a->pos);
    if (r < 0) {
        return 0;
    }
    a->pos += r;
    *lidos = (DWORD)r;
    return 1;
}
static inline void QueryPerformanceCounter(LARGE_INTEGER *t) {
    struct timespec s;
    clock_gettime(CLOCK_MONOTONIC, &s);
    t->QuadPart = (long long)s.tv_sec * 1000000000LL + s.tv_nsec;
}
static inline void QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000000LL; }
static inline void InitializeCriticalSection(CRITICAL_SECTION *c) {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(c, &a);
    pthread_mutexattr_destroy(&a);
}
static inline void EnterCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_lock(c); }
static inline void LeaveCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_unlock(c); }
static inline DWORD GetCurrentThreadId(void) { return (DWORD)syscall(SYS_gettid); }
static inline DWORD GetLastError(void) { return (DWORD)errno; }
static inline HANDLE FindFirstFile(const char *c, WIN32_FIND_DATA *d) {
    (void)c; (void)d;
    return INVALID_HANDLE_VALUE;
}
static inline BOOL FindClose(HANDLE h) {
    (void)h;
    return 1;
}
#endif
