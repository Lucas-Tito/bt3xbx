/*
 * F2: o pedaço de POSIX e de biblioteca C que o port usa e a CRT do Xbox 360 não tem, sobre a API do console. As
 * declarações que o código do jogo e do port vê estão em port/x360/shim (o c89.py pré-processa com elas).
 *   - ambiente: getenv lê uma tabela (padrões da build sem janela + game:\ambiente.txt, linhas NOME=VALOR);
 *   - arquivos: caminhos com '/' viram '\' e caminho relativo vai para game:\ (a pasta do .xex);
 *   - saída de texto (stdout, stderr, printf): game:\saida.txt, com buffer e fflush por printf;
 *   - tempo, pastas, threads (pthread sobre threads e seções críticas) e os atômicos do GCC.
 */
#include <xtl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "diario.h"

/* ------------------------------------------------------------------ ambiente */

#define MAX_VARS 64
static char sNome[MAX_VARS][48], sValor[MAX_VARS][160];
static int sVars;

static void definir(const char *nome, const char *valor) {
    int i;
    for (i = 0; i < sVars; i++) {
        if (strcmp(sNome[i], nome) == 0) {
            break;
        }
    }
    if (i == MAX_VARS) {
        return;
    }
    strncpy(sNome[i], nome, sizeof(sNome[i]) - 1);
    strncpy(sValor[i], valor, sizeof(sValor[i]) - 1);
    if (i == sVars) {
        sVars++;
    }
}

/* Padrões da luta de demonstração sem janela (os do ciclo da F1, port/x360/tools/f1.sh), e por cima o arquivo. */
void Ambiente_Carregar(const char *caminho) {
    FILE *f;
    char linha[256];
    definir("BT3_DEMO", "1");
    definir("BT3_GS", "none");
    definir("BT3_SOUND_TICKS", "1");
    definir("BT3_SETTINGS", "");
    definir("BT3_DATA", "game:\\gamedata");
    definir("BT3_SAVES", "game:\\saves");
    definir("BT3_HASH", "game:\\hash.txt");
    f = fopen(caminho, "r");
    if (f == NULL) {
        Diario_Escrever("ambiente: sem %s, só os padrões", caminho);
        return;
    }
    while (fgets(linha, sizeof(linha), f) != NULL) {
        char *igual = strchr(linha, '=');
        size_t n = strlen(linha);
        while (n > 0 && (linha[n - 1] == '\n' || linha[n - 1] == '\r')) {
            linha[--n] = '\0';
        }
        if (igual != NULL && linha[0] != '#') {
            *igual = '\0';
            definir(linha, igual + 1);
            Diario_Escrever("ambiente: %s=%s", linha, igual + 1);
        }
    }
    fclose(f);
}

char *getenv(const char *nome) {
    int i;
    for (i = 0; i < sVars; i++) {
        if (strcmp(sNome[i], nome) == 0) {
            return sValor[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ arquivos */

static void caminho360(const char *de, char *para, size_t n) {
    size_t i, k = 0;
    if (strchr(de, ':') == NULL) {
        strncpy(para, "game:\\", n);
        k = strlen(para);
    }
    for (i = 0; de[i] != '\0' && k + 1 < n; i++) {
        para[k++] = de[i] == '/' ? '\\' : de[i];
    }
    para[k] = '\0';
}

/* iso360.c: os dados do jogo de dentro da ISO (decisão 18) */
void *Iso_Abrir(const char *rel);
int Iso_Eh(const void *f);
size_t Iso_Ler(void *buf, size_t tam, size_t n, void *f);
int Iso_Posicionar(void *f, long desl, int origem);
long Iso_Posicao(void *f);
int Iso_Fechar(void *f);
int Iso_Caractere(void *f);

/* Leitura sob a raiz de dados (BT3_DATA): da ISO, se houver uma; o resto (e tudo sem ISO) é arquivo de verdade. */
FILE *Port_X360Fopen(const char *caminho, const char *modo) {
    char c[512];
    const char *raiz = getenv("BT3_DATA");
    size_t n = raiz != NULL ? strlen(raiz) : 0;
    if (n > 0 && strchr(modo, 'w') == NULL && strchr(modo, 'a') == NULL && strchr(modo, '+') == NULL &&
        strncmp(caminho, raiz, n) == 0 && (caminho[n] == '/' || caminho[n] == '\\')) {
        char rel[256];
        size_t i;
        void *v;
        for (i = 0; caminho[n + 1 + i] != '\0' && i + 1 < sizeof(rel); i++) {
            rel[i] = caminho[n + 1 + i] == '\\' ? '/' : caminho[n + 1 + i];
        }
        rel[i] = '\0';
        v = Iso_Abrir(rel);
        if (v != (void *)-1) {
            return (FILE *)v;
        }
    }
    caminho360(caminho, c, sizeof(c));
    return fopen(c, modo);
}

/* as leituras do shim: arquivo da ISO ou da CRT */
size_t Port_X360Fread(void *buf, size_t tam, size_t n, FILE *f) {
    return Iso_Eh(f) ? Iso_Ler(buf, tam, n, f) : fread(buf, tam, n, f);
}

int Port_X360Fseek(FILE *f, long desl, int origem) {
    return Iso_Eh(f) ? Iso_Posicionar(f, desl, origem) : fseek(f, desl, origem);
}

long Port_X360Ftell(FILE *f) {
    return Iso_Eh(f) ? Iso_Posicao(f) : ftell(f);
}

int Port_X360Fclose(FILE *f) {
    return Iso_Eh(f) ? Iso_Fechar(f) : fclose(f);
}

int Port_X360Fgetc(FILE *f) {
    return Iso_Eh(f) ? Iso_Caractere(f) : fgetc(f);
}

char *Port_X360Fgets(char *s, int n, FILE *f) {
    int i = 0, c = 0;
    if (!Iso_Eh(f)) {
        return fgets(s, n, f);
    }
    while (i + 1 < n && (c = Iso_Caractere(f)) != EOF) {
        s[i++] = (char)c;
        if (c == '\n') {
            break;
        }
    }
    s[i] = '\0';
    return i > 0 ? s : NULL;
}

static FILE *sSaida;

FILE *Port_X360Saida(void) {
    if (sSaida == NULL) {
        /* nunca _IONBF: sem buffer, o _flsbuf da CRT do XDK faz _write(fh, &ch, 1) com ch int
           (Source/crt/_flsbuf.c:189), e no PowerPC sai o byte alto, 0, de cada caractere. "c": fflush e
           _flushall chamam FlushFileBuffers (_commit), como o diario.c */
        sSaida = fopen("game:\\saida.txt", "wc");
    }
    return sSaida;
}

/* Na saída do jogo (Port_Sair): o que estiver no buffer vai para o arquivo. */
void Port_X360FecharSaida(void) {
    if (sSaida != NULL) {
        fclose(sSaida);
        sSaida = NULL;
    }
}

int Port_Printf(const char *formato, ...) {
    va_list ap;
    int r;
    FILE *f = Port_X360Saida();
    if (f == NULL) {
        return 0;
    }
    va_start(ap, formato);
    r = vfprintf(f, formato, ap);
    va_end(ap);
    fflush(f);
    return r;
}

struct stat { long st_size; int st_mode; long st_mtime; };

int stat(const char *caminho, struct stat *st) {
    char c[512];
    WIN32_FILE_ATTRIBUTE_DATA d;
    caminho360(caminho, c, sizeof(c));
    if (!GetFileAttributesEx(c, GetFileExInfoStandard, &d)) {
        return -1;
    }
    st->st_size = (long)d.nFileSizeLow;
    st->st_mode = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0x4000 : 0x8000;
    st->st_mtime = 0;
    return 0;
}

int mkdir(const char *caminho, int modo) {
    char c[512];
    (void)modo;
    caminho360(caminho, c, sizeof(c));
    return CreateDirectory(c, NULL) ? 0 : -1;
}

int chdir(const char *caminho) {
    (void)caminho;
    return 0; /* tudo é relativo a game:\ (caminho360) */
}

struct dirent { char d_name[256]; };
typedef struct x360_dir { HANDLE h; WIN32_FIND_DATA d; int primeiro; struct dirent e; } DIR;

DIR *opendir(const char *caminho) {
    char c[512];
    DIR *d = (DIR *)calloc(1, sizeof(DIR));
    caminho360(caminho, c, sizeof(c) - 3);
    strcat(c, "\\*");
    if (d == NULL) {
        return NULL;
    }
    d->h = FindFirstFile(c, &d->d);
    if (d->h == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->primeiro = 1;
    return d;
}

struct dirent *readdir(DIR *d) {
    if (!d->primeiro && !FindNextFile(d->h, &d->d)) {
        return NULL;
    }
    d->primeiro = 0;
    strncpy(d->e.d_name, d->d.cFileName, sizeof(d->e.d_name) - 1);
    return &d->e;
}

int closedir(DIR *d) {
    FindClose(d->h);
    free(d);
    return 0;
}

/* ------------------------------------------------------------------ tempo */

struct timespec { long tv_sec; long tv_nsec; };

int clock_gettime(int relogio, struct timespec *ts) {
    LARGE_INTEGER c, f;
    unsigned __int64 n, hz;
    (void)relogio;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    n = (unsigned __int64)c.QuadPart;
    hz = (unsigned __int64)f.QuadPart;
    /* segundos e resto separados: contador * 1e9 passaria de 64 bits uns 6 minutos depois do boot (50 MHz) */
    ts->tv_sec = (long)(n / hz);
    ts->tv_nsec = (long)((n % hz) * 1000000000ull / hz);
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)rem;
    Sleep((DWORD)(req->tv_sec * 1000 + req->tv_nsec / 1000000));
    return 0;
}

int usleep(unsigned int us) {
    Sleep(us / 1000);
    return 0;
}

/* ------------------------------------------------------------------ threads */

typedef struct { size_t stack; } pthread_attr_t;
typedef struct { void *cs; } pthread_mutex_t;
typedef struct { void *ev; } pthread_cond_t;
typedef struct { volatile long feito; } pthread_once_t;
typedef struct { void *(*fn)(void *); void *arg; } Partida;

static DWORD WINAPI partida(LPVOID p) {
    Partida s = *(Partida *)p;
    free(p);
    _controlfp(_RC_CHOP, _MCW_RC); /* toda thread do port com o mesmo modelo de float */
    s.fn(s.arg);
    return 0;
}

int pthread_create(void **t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
    Partida *p = (Partida *)malloc(sizeof(Partida));
    p->fn = fn;
    p->arg = arg;
    *t = CreateThread(NULL, attr != NULL && attr->stack ? (DWORD)attr->stack : 1024 * 1024, partida, p, 0, NULL);
    return *t != NULL ? 0 : -1;
}

int pthread_join(void *t, void **r) {
    (void)r;
    WaitForSingleObject((HANDLE)t, INFINITE);
    CloseHandle((HANDLE)t);
    return 0;
}

int pthread_detach(void *t) {
    CloseHandle((HANDLE)t);
    return 0;
}

int pthread_attr_init(pthread_attr_t *a) {
    a->stack = 0;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *a, size_t n) {
    a->stack = n;
    return 0;
}

static CRITICAL_SECTION *secao(pthread_mutex_t *m) {
    if (m->cs == NULL) {
        CRITICAL_SECTION *c = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
        InitializeCriticalSection(c);
        __lwsync(); /* a seção inicializada antes de o ponteiro aparecer para outro núcleo */
        if (InterlockedCompareExchangePointer(&m->cs, c, NULL) != NULL) {
            DeleteCriticalSection(c); /* outra thread criou primeiro */
            free(c);
        }
    }
    return (CRITICAL_SECTION *)m->cs;
}

int pthread_mutex_init(pthread_mutex_t *m, const void *a) {
    (void)a;
    m->cs = NULL;
    secao(m);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m) {
    EnterCriticalSection(secao(m));
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
    LeaveCriticalSection(secao(m));
    return 0;
}

/* condição simples: um evento de reinício automático (quem espera solta a trava, espera, pega de novo) */
static HANDLE evento(pthread_cond_t *c) {
    if (c->ev == NULL) {
        HANDLE e = CreateEvent(NULL, FALSE, FALSE, NULL);
        __lwsync();
        if (InterlockedCompareExchangePointer(&c->ev, e, NULL) != NULL) {
            CloseHandle(e);
        }
    }
    return (HANDLE)c->ev;
}

int pthread_cond_init(pthread_cond_t *c, const void *a) {
    (void)a;
    c->ev = NULL;
    evento(c);
    return 0;
}

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    HANDLE e = evento(c);
    LeaveCriticalSection(secao(m));
    WaitForSingleObject(e, INFINITE);
    EnterCriticalSection(secao(m));
    return 0;
}

int pthread_cond_signal(pthread_cond_t *c) {
    SetEvent(evento(c));
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *c) {
    SetEvent(evento(c));
    return 0;
}

int pthread_once(pthread_once_t *o, void (*fn)(void)) {
    if (InterlockedCompareExchangeAcquire(&o->feito, 1, 0) == 0) {
        fn();
        __lwsync(); /* o que fn() gravou antes da marca */
        o->feito = 2;
    }
    while (o->feito != 2) {
        Sleep(0);
    }
    __lwsync();
    return 0;
}

/* ------------------------------------------------------------------ atômicos do GCC
   No 360 as Interlocked* sem Acquire/Release não têm barreira (winbase.h do XDK) e MemoryBarrier é só lwsync: a
   barreira completa que o GCC promete é __sync(). */

int __sync_lock_test_and_set(volatile int *p, int v) {
    int r = (int)InterlockedExchange((volatile LONG *)p, v);
    __lwsync(); /* acquire: nada da seção crítica sobe para antes da trava (lwsync ordena leitura->tudo) */
    return r;
}

void __sync_lock_release(volatile int *p) {
    __lwsync(); /* release */
    *p = 0;
}

int __sync_bool_compare_and_swap(volatile int *p, int velho, int novo) {
    int r;
    __sync();
    r = InterlockedCompareExchange((volatile LONG *)p, novo, velho) == velho;
    __lwsync();
    return r;
}

void __sync_synchronize(void) {
    __sync();
}
