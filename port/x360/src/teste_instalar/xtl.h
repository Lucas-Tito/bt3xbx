/* API FATX simulada para executar o adaptador real no WSL, sem emulador. */
#ifndef TESTE_INSTALAR_XTL_H
#define TESTE_INSTALAR_XTL_H
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#define __int64 long long
typedef unsigned DWORD;
typedef int BOOL;
typedef int LONG;
typedef intptr_t HANDLE;
typedef void *LPVOID;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
#define WINAPI
#define FALSE 0
#define TRUE 1
#define INFINITE 0xffffffffU
#define WAIT_OBJECT_0 0
#define CREATE_SUSPENDED 4
typedef union {
    long long QuadPart;
} LARGE_INTEGER;
typedef union {
    unsigned long long QuadPart;
} ULARGE_INTEGER;
#define INVALID_HANDLE_VALUE (-1)
/* O XDK não define INVALID_FILE_ATTRIBUTES; não oferecer a constante desktop
 * aqui. */
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
/* Handles de arquivo são descritores; os demais apontam a objetos pthread.
   Eventos auto reset têm mutex/condição, sem esperas por consulta periódica. */
typedef struct TesteObjeto {
    int tipo, sinal, manual, hw, etiqueta;
    pthread_mutex_t trava;
    pthread_cond_t cond;
    pthread_t thread;
    LPTHREAD_START_ROUTINE fn;
    LPVOID arg;
} TesteObjeto;
static __thread int teste_hw = 2;
static unsigned teste_leituras, teste_escritas, teste_relidos, teste_duplicados, teste_gravacoes;
static HANDLE teste_origem = -1, teste_tmp = -1;
static unsigned teste_nfaixas;
static volatile unsigned teste_pendente[2], teste_sobrepostas;
static struct {
    unsigned pos, n;
} teste_faixas[16000];
static TesteObjeto *objeto(HANDLE h) { return (TesteObjeto *)(intptr_t)h; }
static HANDLE CreateEvent(void *a, BOOL manual, BOOL sinal, void *b) {
    static unsigned quantos;
    TesteObjeto *o;
    char caso[32];
    (void)a;
    (void)b;
    sprintf(caso, "evento%u", ++quantos);
    if (falha(caso))
        return 0;
    o = (TesteObjeto *)calloc(1, sizeof(*o));
    assert(o);
    o->tipo = 1;
    o->etiqueta = (int)quantos;
    o->manual = manual;
    o->sinal = sinal;
    pthread_mutex_init(&o->trava, NULL);
    pthread_cond_init(&o->cond, NULL);
    return (HANDLE)(intptr_t)o;
}
static BOOL SetEvent(HANDLE h) {
    TesteObjeto *o = objeto(h);
    pthread_mutex_lock(&o->trava);
    if (o->etiqueta == 1 || o->etiqueta == 3) {
        unsigned qual = o->etiqueta == 1 ? 0 : 1;
        __sync_add_and_fetch(&teste_pendente[qual], 1);
        if (__sync_fetch_and_add(&teste_pendente[1 - qual], 0))
            __sync_add_and_fetch(&teste_sobrepostas, 1);
    } else if (o->etiqueta == 2 || o->etiqueta == 4) {
        __sync_sub_and_fetch(&teste_pendente[o->etiqueta == 2 ? 0 : 1], 1);
    }
    o->sinal = 1;
    pthread_cond_signal(&o->cond);
    pthread_mutex_unlock(&o->trava);
    return 1;
}
static DWORD WaitForSingleObject(HANDLE h, DWORD ms) {
    TesteObjeto *o = objeto(h);
    assert(ms == INFINITE && teste_hw != 0 && teste_hw != 4 && teste_hw != 5);
    if (o->tipo == 2) {
        pthread_join(o->thread, NULL);
        return WAIT_OBJECT_0;
    }
    pthread_mutex_lock(&o->trava);
    while (!o->sinal)
        pthread_cond_wait(&o->cond, &o->trava);
    if (!o->manual)
        o->sinal = 0;
    pthread_mutex_unlock(&o->trava);
    return WAIT_OBJECT_0;
}
static void *teste_entrada(void *p) {
    TesteObjeto *o = (TesteObjeto *)p;
    teste_hw = o->hw;
    o->fn(o->arg);
    __sync_lock_test_and_set(&teste_pendente[o->hw == 3 ? 0 : 1], 0);
    return NULL;
}
static HANDLE CreateThread(void *a, unsigned pilha, LPTHREAD_START_ROUTINE fn, void *arg, DWORD flags, void *b) {
    static unsigned quantos;
    char caso[32];
    TesteObjeto *o;
    (void)a;
    (void)b;
    (void)pilha;
    assert(flags == CREATE_SUSPENDED);
    sprintf(caso, "thread%u", ++quantos);
    if (falha(caso))
        return 0;
    o = (TesteObjeto *)calloc(1, sizeof(*o));
    assert(o);
    o->tipo = 2;
    o->hw = -1;
    o->fn = fn;
    o->arg = arg;
    return (HANDLE)(intptr_t)o;
}
static void XSetThreadProcessor(HANDLE h, DWORD hw) {
    assert(hw == 1 || hw == 3);
    objeto(h)->hw = (int)hw;
}
static DWORD ResumeThread(HANDLE h) {
    TesteObjeto *o = objeto(h);
    assert(o->hw == 1 || o->hw == 3);
    assert(!pthread_create(&o->thread, NULL, teste_entrada, o));
    return 1;
}
static void *_aligned_malloc(size_t n, size_t a) {
    void *p = NULL;
    if (falha("alocar"))
        return NULL;
    return posix_memalign(&p, a, n) ? NULL : p;
}
static void _aligned_free(void *p) { free(p); }
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
    {
        HANDLE h = open(
            caminho(p, buf),
            (a & GENERIC_WRITE ? O_RDWR : O_RDONLY) | (modo == CREATE_NEW ? O_CREAT | O_EXCL | O_NOFOLLOW : 0), 0600);
        if (h >= 0) {
            if (strncmp(p, "game:\\", 6))
                teste_origem = h;
            else if (strstr(p, ".tmp"))
                teste_tmp = h;
        }
        return h;
    }
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
    assert(teste_hw != 0 && teste_hw != 4 && teste_hw != 5);
    if (h == teste_origem) {
        off_t pos = lseek(h, 0, SEEK_CUR);
        unsigned i;
        for (i = 0; i < teste_nfaixas; i++)
            if ((unsigned long long)pos < (unsigned long long)teste_faixas[i].pos + teste_faixas[i].n &&
                (unsigned long long)pos + n > teste_faixas[i].pos)
                teste_duplicados++;
        assert(teste_nfaixas < 16000);
        teste_faixas[teste_nfaixas].pos = (unsigned)pos;
        teste_faixas[teste_nfaixas++].n = n;
        teste_leituras += n;
    } else {
        teste_relidos += n;
    }
    r = read(h, p, n);
    *l = r < 0 ? 0 : (DWORD)r;
    if (falha("origem") && h == teste_origem && r > 0) {
        off_t fim = lseek(h, 0, SEEK_CUR), inicio = fim - r;
        if (inicio <= 2097152 && fim > 2097152)
            ((unsigned char *)p)[2097152 - inicio] ^= 1;
    }
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
    assert(teste_hw == 1 && ((uintptr_t)p & 127) == 0);
    teste_escritas += n;
    teste_gravacoes++;
    r = write(h, p, n);
    *l = r < 0 ? 0 : (DWORD)r;
    total += *l;
    return r >= 0;
}
static BOOL CloseHandle(HANDLE h) {
    int escrita, r;
    if (h > INT_MAX) {
        TesteObjeto *o = objeto(h);
        if (o->tipo == 1) {
            pthread_mutex_destroy(&o->trava);
            pthread_cond_destroy(&o->cond);
        }
        free(o);
        return 1;
    }
    if (h == teste_origem) {
        printf("E/S: origem=%u escrita=%u relidos=%u duplicados=%u\n", teste_leituras, teste_escritas, teste_relidos,
               teste_duplicados);
        printf("pedidos sobrepostos=%u gravações=%u\n", (unsigned)teste_sobrepostas, teste_gravacoes);
        teste_origem = -1;
    }
    escrita = fcntl(h, F_GETFL) & O_RDWR;
    r = close(h);
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
    return lstat(caminho(p, b), &s) ? (DWORD)-1 : 0;
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
static DWORD GetLastError(void) { return (DWORD)errno; }
static DWORD GetTickCount(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (DWORD)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
/* O ensaio observa cada publicação do progresso, não amostras por tempo. */
void Teste_Progresso(void);
static LONG InterlockedExchange(volatile LONG *p, LONG v) {
    LONG r = __sync_lock_test_and_set(p, v);
    Teste_Progresso();
    return r;
}
static LONG InterlockedCompareExchange(volatile LONG *p, LONG v, LONG c) {
    return __sync_val_compare_and_swap(p, c, v);
}
#endif
