/*
 * (F6, etapa 6) A leitura em blocos com o seguinte adiantado: ver blocos.h.
 *
 * Dois buffers de um bloco. Cada um guarda [ini, ini + n) do arquivo e está vazio, pedido (a thread de leitura vai
 * enchê-lo), pronto ou com erro. Quem lê copia do buffer que tem a posição; na primeira leitura de um bloco, o bloco
 * seguinte é pedido no outro buffer (que então já não serve a ninguém: a leitura é para a frente). Uma posição fora
 * dos dois buffers (o começo, um salto) é uma falta: o bloco dela é pedido na hora e esperado. A thread lê um pedido
 * por vez, em pedaços de 64 KB (cada pedaço uma chamada de `ler`, que no 360 pega a trava da ISO: as outras leituras
 * esperam no máximo um pedaço, não o bloco inteiro).
 */
#ifndef _XBOX
#define _POSIX_C_SOURCE 200112L /* (o clock_gettime no teste do WSL, em C89 estrito) */
#endif
#include <stdlib.h>
#include <string.h>
#include "blocos.h"

#ifdef _XBOX
#include <xtl.h>
#else
#include <pthread.h>
#include <time.h>
#endif

#define PEDACO 65536L

enum { VAZIO, PEDIDO, PRONTO, ERRO };

struct Blocos {
    BlocosLer ler;
    void *ctx;
    long tam, bloco;
    unsigned char *buf[2];
    long ini[2], n[2];
    int estado[2];
    int usado;  /* o buffer da última leitura (-1: nenhum ainda) */
    int pedido; /* o buffer que a thread tem de encher (-1: nada pedido) */
    int sair, comThread, primitivas;
    BlocosConta conta;
#ifdef _XBOX
    CRITICAL_SECTION trava;
    HANDLE ev[2]; /* 0: acorda a thread de leitura; 1: acorda quem lê */
    HANDLE thread;
#else
    pthread_mutex_t trava;
    pthread_cond_t ev[2];
    pthread_t thread;
#endif
};

/* ---- o relógio e a thread, do 360 ou do pthreads ---- */
#ifdef _XBOX
static double agora_ms(void) {
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
}
static void trava(Blocos *b) { EnterCriticalSection(&b->trava); }
static void destrava(Blocos *b) { LeaveCriticalSection(&b->trava); }
/* (eventos de volta automática: um SetEvent antes da espera não se perde; quem dorme confere o estado de novo) */
static void acorda(Blocos *b, int qual) { SetEvent(b->ev[qual]); }
static void dorme(Blocos *b, int qual) {
    LeaveCriticalSection(&b->trava);
    WaitForSingleObject(b->ev[qual], INFINITE);
    EnterCriticalSection(&b->trava);
}
#else
static double agora_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}
static void trava(Blocos *b) { pthread_mutex_lock(&b->trava); }
static void destrava(Blocos *b) { pthread_mutex_unlock(&b->trava); }
static void acorda(Blocos *b, int qual) { pthread_cond_signal(&b->ev[qual]); }
static void dorme(Blocos *b, int qual) { pthread_cond_wait(&b->ev[qual], &b->trava); }
#endif

/* um bloco inteiro, em pedaços; 1 se leu */
static int ler_bloco(Blocos *b, long pos, unsigned char *buf, long n) {
    long feito = 0;
    while (feito < n) {
        long p = n - feito > PEDACO ? PEDACO : n - feito;
        if (!b->ler(b->ctx, pos + feito, buf + feito, p)) {
            return 0;
        }
        feito += p;
    }
    return 1;
}

/* conta uma leitura de bloco (com a trava, ou sem thread) */
static void conta_leitura(Blocos *b, double ms) {
    b->conta.blocos++;
    b->conta.msLeitura += ms;
    if (ms > b->conta.msLeituraPior) {
        b->conta.msLeituraPior = ms;
    }
}

#ifdef _XBOX
static DWORD WINAPI leitor(LPVOID arg)
#else
static void *leitor(void *arg)
#endif
{
    Blocos *b = (Blocos *)arg;
    trava(b);
    for (;;) {
        int k, ok;
        long pos, n;
        double t0;
        while (!b->sair && b->pedido < 0) {
            dorme(b, 0);
        }
        if (b->sair) {
            break;
        }
        k = b->pedido;
        pos = b->ini[k];
        n = b->n[k];
        destrava(b);
        t0 = agora_ms();
        ok = ler_bloco(b, pos, b->buf[k], n);
        t0 = agora_ms() - t0;
        trava(b);
        b->estado[k] = ok ? PRONTO : ERRO;
        b->pedido = -1;
        conta_leitura(b, t0);
        acorda(b, 1);
    }
    destrava(b);
    return 0;
}

Blocos *Blocos_Abrir(BlocosLer ler, void *ctx, long tam, long bloco, int hw) {
    Blocos *b;
    if (bloco <= 0 || tam < 0) {
        return NULL;
    }
    b = (Blocos *)calloc(1, sizeof(Blocos));
    if (b == NULL) {
        return NULL;
    }
    b->ler = ler;
    b->ctx = ctx;
    b->tam = tam;
    b->bloco = bloco;
    b->usado = -1;
    b->pedido = -1;
    b->buf[0] = (unsigned char *)malloc((size_t)bloco);
    b->buf[1] = (unsigned char *)malloc((size_t)bloco);
    if (b->buf[0] == NULL || b->buf[1] == NULL) {
        free(b->buf[0]);
        free(b->buf[1]);
        free(b);
        return NULL;
    }
    if (hw >= 0) {
        b->primitivas = 1;
#ifdef _XBOX
        InitializeCriticalSection(&b->trava);
        b->ev[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
        b->ev[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
        b->thread = CreateThread(NULL, 64 * 1024, leitor, b, CREATE_SUSPENDED, NULL);
        if (b->thread != NULL) {
            XSetThreadProcessor(b->thread, (DWORD)hw);
            SetThreadPriority(b->thread, THREAD_PRIORITY_ABOVE_NORMAL); /* (passa quase todo o tempo esperando o disco) */
            ResumeThread(b->thread);
            b->comThread = 1;
        }
#else
        pthread_mutex_init(&b->trava, NULL);
        pthread_cond_init(&b->ev[0], NULL);
        pthread_cond_init(&b->ev[1], NULL);
        b->comThread = pthread_create(&b->thread, NULL, leitor, b) == 0;
#endif
    }
    return b;
}

/* o buffer que tem pos (pronto ou pedido), ou -1 */
static int achar(const Blocos *b, long pos) {
    int k;
    for (k = 0; k < 2; k++) {
        if ((b->estado[k] == PRONTO || b->estado[k] == PEDIDO) && pos >= b->ini[k] && pos < b->ini[k] + b->n[k]) {
            return k;
        }
    }
    return -1;
}

/* pede o bloco que começa em ini no buffer k (com a trava, se há thread); sem thread, lê na hora */
static void pedir(Blocos *b, int k, long ini) {
    b->ini[k] = ini;
    b->n[k] = b->tam - ini < b->bloco ? b->tam - ini : b->bloco;
    if (b->comThread) {
        b->estado[k] = PEDIDO;
        b->pedido = k;
        acorda(b, 0);
    } else {
        double t0 = agora_ms();
        b->estado[k] = ler_bloco(b, ini, b->buf[k], b->n[k]) ? PRONTO : ERRO;
        conta_leitura(b, agora_ms() - t0);
    }
}

/* espera o buffer k sair de pedido (com a trava) */
static void esperar(Blocos *b, int k) {
    double t0, ms;
    if (b->estado[k] != PEDIDO) {
        return;
    }
    t0 = agora_ms();
    while (b->estado[k] == PEDIDO) {
        dorme(b, 1);
    }
    ms = agora_ms() - t0;
    b->conta.esperas++;
    b->conta.msEspera += ms;
    if (ms > b->conta.msEsperaPior) {
        b->conta.msEsperaPior = ms;
    }
}

long Blocos_Ler(Blocos *b, long pos, void *dst, long n) {
    unsigned char *d = (unsigned char *)dst;
    long feito = 0;
    if (pos < 0 || n <= 0 || pos >= b->tam) {
        return 0;
    }
    if (n > b->tam - pos) {
        n = b->tam - pos;
    }
    if (b->comThread) {
        trava(b);
    }
    while (feito < n) {
        long p, q;
        int k = achar(b, pos);
        if (k < 0) {
            /* a falta: o bloco de pos, num buffer que não está sendo lido (um pedido em curso termina antes) */
            int j;
            if (b->pedido >= 0) {
                esperar(b, b->pedido);
            }
            j = b->usado == 0 ? 1 : 0;
            b->conta.faltas++;
            pedir(b, j, pos - pos % b->bloco);
            k = j;
        }
        esperar(b, k);
        if (b->estado[k] != PRONTO) {
            b->estado[k] = VAZIO; /* (a próxima leitura tenta de novo) */
            if (b->comThread) {
                destrava(b);
            }
            return -1;
        }
        if (k != b->usado) {
            /* o primeiro uso deste bloco: o seguinte vai para o outro buffer, que já não serve (a leitura é para a frente) */
            long prox = b->ini[k] + b->n[k];
            int j = 1 - k;
            b->usado = k;
            if (prox < b->tam && !(b->estado[j] != VAZIO && b->ini[j] == prox) && b->pedido < 0) {
                if (b->comThread) { /* (sem thread não há adiantamento: o seguinte é uma falta, lida na hora) */
                    pedir(b, j, prox);
                }
            }
        }
        q = b->ini[k] + b->n[k] - pos;
        p = n - feito < q ? n - feito : q;
        memcpy(d + feito, b->buf[k] + (pos - b->ini[k]), (size_t)p);
        feito += p;
        pos += p;
    }
    if (b->comThread) {
        destrava(b);
    }
    return feito;
}

void Blocos_Conta(Blocos *b, BlocosConta *c) {
    if (b->comThread) {
        trava(b);
    }
    *c = b->conta;
    if (b->comThread) {
        destrava(b);
    }
}

void Blocos_Fechar(Blocos *b) {
    if (b == NULL) {
        return;
    }
    if (b->comThread) {
        trava(b);
        b->sair = 1;
        acorda(b, 0);
        destrava(b);
#ifdef _XBOX
        WaitForSingleObject(b->thread, INFINITE);
        CloseHandle(b->thread);
#else
        pthread_join(b->thread, NULL);
#endif
    }
    if (b->primitivas) {
#ifdef _XBOX
        DeleteCriticalSection(&b->trava);
        CloseHandle(b->ev[0]);
        CloseHandle(b->ev[1]);
#else
        pthread_mutex_destroy(&b->trava);
        pthread_cond_destroy(&b->ev[0]);
        pthread_cond_destroy(&b->ev[1]);
#endif
    }
    free(b->buf[0]);
    free(b->buf[1]);
    free(b);
}
