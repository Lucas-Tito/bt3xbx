/*
 * F6, etapa 3: o núcleo portátil da camada de áudio do 360 (som_nucleo.h, plano/F6/03.md).
 *
 * Cada fluxo guarda numa fila circular os quadros que o jogo pôs, na taxa e nos canais dele (s16, 1 ou 2 canais).
 * A conversão para 48 kHz é linear e inteira, igual em qualquer máquina: a posição entre o quadro a e o b é f/48000,
 * e cada quadro de saída soma a taxa do fluxo a f; quando f passa de 48000, o fluxo anda um quadro (24 kHz dá o dobro
 * exato: a, (a+b)/2, b...). Um fluxo começa com f = 2 x 48000, então os dois primeiros quadros entram antes da primeira
 * saída, e a 48 kHz a saída é o próprio fluxo, quadro a quadro. Mono vai igual aos dois lados. O ganho é Q16 (0 a 1,
 * arredondado), a soma em int e o corte em 16 bits no fim. Tudo em int de 32 bits (o long do cl do 360 tem 32 e o do
 * gcc do WSL 64): o teste do WSL e o console dão as mesmas amostras.
 *
 * O que a SDL3 fazia e fica de fora: outros formatos que não s16 (o jogo só usa s16), ganho acima de 1 (o jogo nunca
 * pede), o fluxo de saída diferente de 48 kHz estéreo.
 */
#include <stdlib.h>
#include <string.h>
#include "som_nucleo.h"

#define CAP_INICIAL 4096 /* quadros: o maior pedido de um bloco a 48 kHz cabe com folga */
#define PEDACO 1024      /* quadros de saída por volta da mistura */

struct SDL_AudioStream {
    int taxa, canais;
    SDL_AudioStreamCallback cb;
    void *user;
    int ligado;
    int ganho;           /* Q16 */
    short *fila;         /* cap quadros de canais amostras */
    int cap, ini, n;     /* capacidade, primeiro e quantos, em quadros */
    unsigned char resto[4];
    int nresto;          /* bytes de um quadro incompleto (a SDL aceita qualquer tamanho) */
    int f;               /* a fase: a posição entre a e b, em 1/SOM_TAXA de quadro do fluxo */
    short a[2], b[2];
    struct SDL_AudioStream *prox;
};

static SDL_AudioStream *sFluxos;
static const char *sErro = "";
static SomContadores sCont;
static int sAcc[PEDACO * 2];

const char *SomNucleo_Erro(void) {
    return sErro;
}

static void zera_fase(SDL_AudioStream *s) {
    s->f = 2 * SOM_TAXA;
    s->a[0] = s->a[1] = s->b[0] = s->b[1] = 0;
}

SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *de, const SDL_AudioSpec *para) {
    SDL_AudioStream *s;
    (void)para; /* a saída é sempre a do dispositivo: 48 kHz estéreo */
    if (de == NULL || de->format != SOM_AUDIO_S16 || de->channels < 1 || de->channels > 2 || de->freq < 1000 ||
        de->freq > 192000) {
        sErro = "som: formato que o núcleo não converte (só s16, 1 ou 2 canais, 1 a 192 kHz)";
        return NULL;
    }
    s = (SDL_AudioStream *)calloc(1, sizeof(*s));
    if (s == NULL) {
        sErro = "som: sem memória para o fluxo";
        return NULL;
    }
    s->fila = (short *)malloc((size_t)CAP_INICIAL * (size_t)de->channels * sizeof(short));
    if (s->fila == NULL) {
        free(s);
        sErro = "som: sem memória para a fila";
        return NULL;
    }
    s->taxa = de->freq;
    s->canais = de->channels;
    s->cap = CAP_INICIAL;
    s->ganho = SOM_GANHO_UM;
    zera_fase(s);
    som_travar();
    s->prox = sFluxos;
    sFluxos = s;
    sCont.fluxos++;
    som_destravar();
    return s;
}

/* com a trava pega a mistura não está no meio de um retorno deste fluxo: soltar é seguro (o stop() do snd_adx.c conta) */
void SDL_DestroyAudioStream(SDL_AudioStream *s) {
    SDL_AudioStream **p;
    if (s == NULL) {
        return;
    }
    som_travar();
    for (p = &sFluxos; *p != NULL; p = &(*p)->prox) {
        if (*p == s) {
            *p = s->prox;
            sCont.fluxos--;
            if (s->ligado) {
                sCont.ligados--;
            }
            break;
        }
    }
    som_destravar();
    free(s->fila);
    free(s);
}

int SDL_BindAudioStream(SDL_AudioDeviceID d, SDL_AudioStream *s) {
    if (d == 0 || s == NULL) {
        sErro = "som: dispositivo ou fluxo nulo";
        return 0;
    }
    som_travar();
    if (!s->ligado) {
        s->ligado = 1;
        sCont.ligados++;
    }
    som_destravar();
    return 1;
}

int SDL_SetAudioStreamGetCallback(SDL_AudioStream *s, SDL_AudioStreamCallback cb, void *user) {
    if (s == NULL) {
        return 0;
    }
    som_travar();
    s->cb = cb;
    s->user = user;
    som_destravar();
    return 1;
}

/* a fila cresce se precisar (com a trava pega) */
static int cabe(SDL_AudioStream *s, int quadros) {
    short *nova;
    int cap, i;
    if (s->n + quadros <= s->cap) {
        return 1;
    }
    cap = s->cap * 2;
    while (cap < s->n + quadros) {
        cap *= 2;
    }
    nova = (short *)malloc((size_t)cap * (size_t)s->canais * sizeof(short));
    if (nova == NULL) {
        return 0;
    }
    for (i = 0; i < s->n; i++) {
        memcpy(nova + i * s->canais, s->fila + ((s->ini + i) % s->cap) * s->canais, (size_t)s->canais * sizeof(short));
    }
    free(s->fila);
    s->fila = nova;
    s->cap = cap;
    s->ini = 0;
    return 1;
}

int SDL_PutAudioStreamData(SDL_AudioStream *s, const void *dados, int bytes) {
    const unsigned char *p = (const unsigned char *)dados;
    int tam, quadros, fim, i;
    if (s == NULL || bytes < 0 || (dados == NULL && bytes > 0)) {
        sErro = "som: dados inválidos";
        return 0;
    }
    som_travar();
    tam = s->canais * 2;
    quadros = (s->nresto + bytes) / tam;
    if (!cabe(s, quadros)) {
        som_destravar();
        sErro = "som: sem memória para a fila";
        return 0;
    }
    while (bytes > 0) {
        if (s->nresto > 0 || bytes < tam) { /* (um quadro que veio partido) */
            int k = tam - s->nresto;
            if (k > bytes) {
                k = bytes;
            }
            memcpy(s->resto + s->nresto, p, (size_t)k);
            s->nresto += k;
            p += k;
            bytes -= k;
            if (s->nresto < tam) {
                break;
            }
            fim = (s->ini + s->n) % s->cap;
            memcpy(s->fila + fim * s->canais, s->resto, (size_t)tam);
            s->n++;
            s->nresto = 0;
            continue;
        }
        quadros = bytes / tam;
        for (i = 0; i < quadros; i++) {
            fim = (s->ini + s->n) % s->cap;
            memcpy(s->fila + fim * s->canais, p, (size_t)tam);
            s->n++;
            p += tam;
        }
        bytes -= quadros * tam;
    }
    som_destravar();
    return 1;
}

int SDL_ClearAudioStream(SDL_AudioStream *s) {
    if (s == NULL) {
        return 0;
    }
    som_travar();
    s->n = 0;
    s->ini = 0;
    s->nresto = 0;
    zera_fase(s);
    som_destravar();
    return 1;
}

int SDL_LockAudioStream(SDL_AudioStream *s) {
    if (s == NULL) {
        return 0;
    }
    som_travar();
    return 1;
}

int SDL_UnlockAudioStream(SDL_AudioStream *s) {
    if (s == NULL) {
        return 0;
    }
    som_destravar();
    return 1;
}

int SDL_SetAudioStreamGain(SDL_AudioStream *s, float ganho) {
    int g;
    if (s == NULL) {
        return 0;
    }
    if (!(ganho > 0.0f)) { /* (também NaN) */
        g = 0;
    } else if (ganho >= 1.0f) {
        g = SOM_GANHO_UM;
    } else {
        g = (int)((double)ganho * 65536.0 + 0.5);
    }
    som_travar();
    s->ganho = g;
    som_destravar();
    return 1;
}

/* ------------------------------------------------------------------------------------------------- a mistura */

static void anda(SDL_AudioStream *s) {
    const short *q = s->fila + s->ini * s->canais;
    s->a[0] = s->b[0];
    s->a[1] = s->b[1];
    s->b[0] = q[0];
    s->b[1] = s->canais == 2 ? q[1] : q[0];
    s->ini = (s->ini + 1) % s->cap;
    s->n--;
}

static int com_ganho(int v, int g) {
    return g >= SOM_GANHO_UM ? v : (v * g + 32768) >> 16;
}

/* soma n quadros de saída do fluxo em sAcc */
static void um_fluxo(SDL_AudioStream *s, int n) {
    int precisa;
    int i, c;
    /* quantos quadros o fluxo anda em n saídas: a fase de agora mais n - 1 passos */
    precisa = (s->f + (n - 1) * s->taxa) / SOM_TAXA;
    if (s->cb != NULL && precisa > s->n) {
        int tam = s->canais * 2;
        s->cb(s->user, s, (precisa - s->n) * tam, precisa * tam);
        sCont.retornos++;
    }
    for (i = 0; i < n; i++) {
        int frac;
        while (s->f >= SOM_TAXA) {
            if (s->n == 0) {
                sCont.curtos += (unsigned)(n - i);
                return;
            }
            anda(s);
            s->f -= SOM_TAXA;
        }
        frac = (s->f << 15) / SOM_TAXA;
        for (c = 0; c < 2; c++) {
            int v = s->a[c] + (((s->b[c] - s->a[c]) * frac) >> 15);
            sAcc[i * 2 + c] += com_ganho(v, s->ganho);
        }
        s->f += s->taxa;
    }
}

void SomNucleo_Misturar(short *saida, int quadros) {
    SDL_AudioStream *s, *ordem[64];
    int feitos = 0;
    sCont.misturas++;
    while (feitos < quadros) {
        int n = quadros - feitos > PEDACO ? PEDACO : quadros - feitos, i, k = 0;
        memset(sAcc, 0, sizeof(int) * (size_t)n * 2);
        /* na ordem de criação (a lista é da mais nova para a mais velha): o retorno de um fluxo pode criar ou soltar
           outro? Não no jogo (o feed e o mix só põem dados); a cópia da lista protege a volta assim mesmo */
        for (s = sFluxos; s != NULL && k < 64; s = s->prox) {
            ordem[k++] = s;
        }
        for (i = k - 1; i >= 0; i--) {
            if (ordem[i]->ligado) {
                um_fluxo(ordem[i], n);
            }
        }
        for (i = 0; i < n * 2; i++) {
            int v = sAcc[i];
            if (v > 32767) {
                v = 32767;
                sCont.saturados++;
            } else if (v < -32768) {
                v = -32768;
                sCont.saturados++;
            }
            saida[(feitos + i / 2) * 2 + (i & 1)] = (short)v;
        }
        feitos += n;
    }
}

void SomNucleo_Contadores(SomContadores *c) {
    *c = sCont;
}
