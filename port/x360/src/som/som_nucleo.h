/*
 * F6, etapa 3: o núcleo portátil da camada de áudio do 360 (plano/F6/03.md). As funções de fluxo da SDL3 que o
 * gs/snd_adx.c e o gs/snd_se.c chamam (a fila de cada fluxo, o ganho, a conversão de taxa e de canais para a saída e
 * a soma dos fluxos), em C89 puro: o cl.exe do XDK compila para o console e o gcc do WSL para o teste
 * (port/x360/tools/teste_som360.sh), sem nada de plataforma. Quem toca (o XAudio2 no 360, o teste no WSL) dá a trava
 * (som_travar/som_destravar, recursiva) e chama SomNucleo_Misturar com a trava pega.
 *
 * Os tipos têm o mesmo layout dos de port/x360/headless/SDL3/SDL.h, que o jogo vê; aqui "bool" é int, como o
 * stdbool.h do 360 (port/x360/shim) o define, e o jogo do 360 chama estas funções direto.
 */
#ifndef X360_SOM_NUCLEO_H
#define X360_SOM_NUCLEO_H
#ifdef __cplusplus
extern "C" {
#endif

#define SOM_TAXA 48000        /* a saída: 48 kHz estéreo, s16 na ordem nativa (decisão 25.3) */
#define SOM_GANHO_UM 65536    /* o ganho em Q16 (SDL_SetAudioStreamGain: 0 a 1, o que o jogo pede) */
#define SOM_AUDIO_S16 0x8010  /* SDL_AUDIO_S16 */

typedef unsigned SDL_AudioDeviceID;
typedef int SDL_AudioFormat;
typedef struct SDL_AudioSpec { SDL_AudioFormat format; int channels; int freq; } SDL_AudioSpec;
typedef struct SDL_AudioStream SDL_AudioStream;
typedef void (*SDL_AudioStreamCallback)(void *user, SDL_AudioStream *stream, int additional, int total);

/* as nove funções de fluxo (as cinco do dispositivo ficam com a plataforma: som360.cpp) */
SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *de, const SDL_AudioSpec *para);
void SDL_DestroyAudioStream(SDL_AudioStream *s);
int SDL_BindAudioStream(SDL_AudioDeviceID d, SDL_AudioStream *s);
int SDL_SetAudioStreamGetCallback(SDL_AudioStream *s, SDL_AudioStreamCallback cb, void *user);
int SDL_PutAudioStreamData(SDL_AudioStream *s, const void *dados, int bytes);
int SDL_ClearAudioStream(SDL_AudioStream *s);
int SDL_LockAudioStream(SDL_AudioStream *s);
int SDL_UnlockAudioStream(SDL_AudioStream *s);
int SDL_SetAudioStreamGain(SDL_AudioStream *s, float ganho);

/* a última mensagem de erro do núcleo ("" sem erro); a plataforma a devolve pelo SDL_GetError */
const char *SomNucleo_Erro(void);

/* Soma em saida[quadros * 2] (esquerdo, direito) todos os fluxos ligados, convertidos para 48 kHz estéreo, com o
   ganho de cada um e saturação em 16 bits. Chama o retorno de chamada de cada fluxo que não tem o bastante (como a
   SDL: additional = o que falta, em bytes da taxa do fluxo; total = o pedido inteiro). Com a trava pega. */
void SomNucleo_Misturar(short *saida, int quadros);

/* A Guide suspende os fluxos cujo user não é o handle BGM nem MAP do jogo. Sem consumir a fila, mudar ganho,
   fase ou pausa do próprio jogo. Handles nulos nunca identificam música (o sampler dos efeitos usa user NULL).
   Chamadas com a trava pega, inclusive a leitura do relógio. Fluxos criados com a Guide aberta também param. */
void SomNucleo_Guide(int aberta, void *bgm, void *map);
typedef struct SomRelogio {
    unsigned geracao; /* identidade da criação do fluxo, mesmo se malloc reutilizar o endereço */
    unsigned quadros; /* saídas de 48 kHz produzidas por este fluxo, incluindo as voltas do laço */
    unsigned curtos;  /* saídas sem dado; uma pausa da Guide não conta falta */
} SomRelogio;
int SomNucleo_Relogio(void *user, SomRelogio *r);

/* contadores desde o começo (lidos sem trava, para o log) */
typedef struct SomContadores {
    unsigned misturas;   /* chamadas de SomNucleo_Misturar */
    unsigned retornos;   /* retornos de chamada dos fluxos */
    unsigned curtos;     /* quadros de saída que um fluxo ligado não tinha (silêncio no lugar; um fluxo que acabou conta) */
    unsigned fluxos;     /* fluxos existentes agora */
    unsigned ligados;    /* fluxos ligados agora */
    unsigned saturados;  /* amostras de saída cortadas em 16 bits */
} SomContadores;
void SomNucleo_Contadores(SomContadores *c);

/* Etapa 7: posição do fluxo em quadros de saída de 48 kHz, com a trava pega. Conta só dados consumidos,
   inclusive PCM de valor zero; falta de dados não anda. user é o handle adxt, não a ordem dos fluxos. */
typedef struct SomFilmeRelogio {
    unsigned geracao, quadros;
} SomFilmeRelogio;
int SomNucleo_FilmeRelogio(void *user, SomFilmeRelogio *r);

/* da plataforma: uma trava recursiva única (a do XAudio2 no 360; no teste do WSL, nada) */
void som_travar(void);
void som_destravar(void);

#ifdef __cplusplus
}
#endif
#endif
