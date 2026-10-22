/*
 * F6, etapa 2, rede 4: o dispositivo gravador (plano/F6/00-escopo.md, seção 2). Na build headless.py --gravador, as
 * funções de áudio da SDL que gs/snd_adx.c e gs/snd_se.c chamam (as 14 da seção 4 do plano) saem daqui em vez dos
 * stubs que falham de port/x360/headless/SDL3/SDL.h: o dispositivo abre, cada fluxo guarda o que o retorno de chamada
 * põe, e um relógio de teste (Gravador_Vblank, chamado por som.c no fim de cada vblank) puxa de cada fluxo ligado as
 * amostras de um vblank na taxa dele: na k-ésima vez, floor((k+1) * taxa * 1001 / 60000) - floor(k * taxa * 1001 /
 * 60000) quadros estéreo (59,94 vblanks por segundo). Tudo na thread do jogo: a ordem é a do jogo, igual nas duas builds.
 *
 * BT3_GRAVADOR=arquivo grava um registro por fluxo e vblank (little-endian, nas duas builds):
 *   u8 'G', u8 tipo (0 ADX, 1 a mistura dos efeitos, o fluxo único do snd_se.c), u16 fluxo (pela ordem de criação),
 *   u32 vblank, u32 taxa, u16 quadros, u16 quadros que faltaram (o fluxo não tinha), u16 vozes de efeito ativas depois
 *   do bloco (só na mistura), u16 bandeiras (1: os dados vêm a seguir), u32 ganho (os bits do float do
 *   SDL_SetAudioStreamGain), u64 FNV-1a dos bytes do bloco (little-endian, com os que faltaram em zero);
 *   e, na mistura dos efeitos quando não é só silêncio, os quadros * 4 bytes (s16 esquerdo e direito).
 * O PCM do ADX vai só pelo hash (a comparação é exata); o dos efeitos vai inteiro, porque a comparação admite 1 LSB (o
 * mixer usa pow, cos e sin da libm, que a PPC implementa por conta própria).
 *
 * A mistura final dos fluxos (a conversão de taxa e a soma que a SDL faz) não está aqui: no 360 ela é a da camada da
 * etapa 3, que se liga a este mesmo relógio quando existir.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned gPortVBlanks;                 /* plat_stub.c (o teste_som não tem: Gravador_Vblank não roda lá) */
extern SDL_AudioStream *Som_FluxoEfeitos(void); /* grava_se.c */
extern int Som_VozesAtivas(void);              /* grava_se.c */

struct SDL_AudioStream {
    int id, rate;
    SDL_AudioStreamCallback cb;
    void *user;
    int bound;
    float gain;
    uint8_t *buf;
    size_t len, cap;
    uint64_t k; /* vblanks puxados */
    struct SDL_AudioStream *next;
};

static SDL_AudioStream *sFluxos;
static int sProximo;
static FILE *sArq;
static int sTentou;

const char *SDL_GetError(void) { return "gravador"; }
bool SDL_InitSubSystem(uint32_t flags) { (void)flags; return true; }
const char *SDL_GetCurrentAudioDriver(void) { return "gravador (F6, etapa 2)"; }
SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID d, const SDL_AudioSpec *s) { (void)d; (void)s; return 1; }
bool SDL_ResumeAudioDevice(SDL_AudioDeviceID d) { (void)d; return true; }

SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *a, const SDL_AudioSpec *b) {
    SDL_AudioStream *s = calloc(1, sizeof(*s));
    (void)b;
    if (s == NULL || a == NULL || a->channels != 2 || a->format != SDL_AUDIO_S16) {
        free(s);
        return NULL;
    }
    s->id = sProximo++;
    s->rate = a->freq;
    s->gain = 1.0f;
    s->next = sFluxos;
    sFluxos = s;
    return s;
}

void SDL_DestroyAudioStream(SDL_AudioStream *s) {
    SDL_AudioStream **p;
    for (p = &sFluxos; *p != NULL; p = &(*p)->next) {
        if (*p == s) {
            *p = s->next;
            free(s->buf);
            free(s);
            return;
        }
    }
}

bool SDL_BindAudioStream(SDL_AudioDeviceID d, SDL_AudioStream *s) { (void)d; if (s != NULL) { s->bound = 1; } return s != NULL; }

bool SDL_SetAudioStreamGetCallback(SDL_AudioStream *s, SDL_AudioStreamCallback cb, void *u) {
    if (s == NULL) {
        return false;
    }
    s->cb = cb;
    s->user = u;
    return true;
}

bool SDL_PutAudioStreamData(SDL_AudioStream *s, const void *b, int n) {
    if (s == NULL || n < 0) {
        return false;
    }
    if (s->len + (size_t)n > s->cap) {
        size_t c = s->cap ? s->cap : 4096;
        uint8_t *nb;
        while (c < s->len + (size_t)n) {
            c *= 2;
        }
        nb = realloc(s->buf, c);
        if (nb == NULL) {
            return false;
        }
        s->buf = nb;
        s->cap = c;
    }
    memcpy(s->buf + s->len, b, (size_t)n);
    s->len += (size_t)n;
    return true;
}

bool SDL_ClearAudioStream(SDL_AudioStream *s) { if (s != NULL) { s->len = 0; } return s != NULL; }
bool SDL_LockAudioStream(SDL_AudioStream *s) { return s != NULL; }
bool SDL_UnlockAudioStream(SDL_AudioStream *s) { return s != NULL; }
bool SDL_SetAudioStreamGain(SDL_AudioStream *s, float g) { if (s != NULL) { s->gain = g; } return s != NULL; }

/* ----------------------------------------------------------------------------------------------- o relógio */

static void le16(uint8_t *o, unsigned v) { o[0] = (uint8_t)v; o[1] = (uint8_t)(v >> 8); }
static void le32(uint8_t *o, uint32_t v) { le16(o, v & 0xFFFF); le16(o + 2, v >> 16); }

/* um vblank: puxa cada fluxo ligado (na ordem de criação, a do jogo) e grava o bloco */
void Gravador_Vblank(void) {
    SDL_AudioStream *s, *ordem[64];
    int n = 0, i;
    if (!sTentou) {
        const char *a = getenv("BT3_GRAVADOR");
        sTentou = 1;
        sArq = a != NULL && a[0] != '\0' ? fopen(a, "wb") : NULL;
    }
    for (s = sFluxos; s != NULL && n < 64; s = s->next) {
        ordem[n++] = s;
    }
    for (i = n - 1; i >= 0; i--) { /* (a lista é da mais nova para a mais velha) */
        uint64_t ini, fim, h = 0xcbf29ce484222325ull;
        unsigned quadros, falta, j, vozes = 0, tem = 0;
        size_t bytes, pega;
        uint8_t cab[36], *pcm;
        int efeitos;
        s = ordem[i];
        if (!s->bound || s->cb == NULL) {
            continue;
        }
        ini = s->k * (uint64_t)s->rate * 1001u / 60000u;
        fim = (s->k + 1) * (uint64_t)s->rate * 1001u / 60000u;
        s->k++;
        quadros = (unsigned)(fim - ini);
        bytes = (size_t)quadros * 4;
        if (s->len < bytes) {
            s->cb(s->user, s, (int)(bytes - s->len), (int)bytes);
        }
        pega = s->len < bytes ? s->len : bytes;
        falta = (unsigned)((bytes - pega) / 4);
        efeitos = s == Som_FluxoEfeitos();
        if (sArq != NULL) {
            pcm = calloc(bytes ? bytes : 1, 1);
            if (pcm == NULL) {
                return;
            }
            for (j = 0; j < pega / 2; j++) {
                int16_t v;
                memcpy(&v, s->buf + j * 2, 2);
                le16(pcm + j * 2, (uint16_t)v);
                tem |= v != 0;
            }
            for (j = 0; j < bytes; j++) {
                h = (h ^ pcm[j]) * 0x100000001b3ull;
            }
            if (efeitos) {
                vozes = (unsigned)Som_VozesAtivas();
            }
            cab[0] = 'G';
            cab[1] = (uint8_t)efeitos;
            le16(cab + 2, (unsigned)s->id);
            le32(cab + 4, gPortVBlanks);
            le32(cab + 8, (uint32_t)s->rate);
            le16(cab + 12, quadros);
            le16(cab + 14, falta);
            le16(cab + 16, vozes);
            le16(cab + 18, efeitos && tem ? 1u : 0u);
            {
                uint32_t g;
                memcpy(&g, &s->gain, 4);
                le32(cab + 20, g);
            }
            le32(cab + 24, (uint32_t)h);
            le32(cab + 28, (uint32_t)(h >> 32));
            fwrite(cab, 1, 32, sArq);
            if (efeitos && tem) {
                fwrite(pcm, 1, bytes, sArq);
            }
            free(pcm);
        }
        memmove(s->buf, s->buf + pega, s->len - pega);
        s->len -= pega;
    }
    if (sArq != NULL) {
        fflush(sArq);
    }
}
