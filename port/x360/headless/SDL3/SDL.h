/* Só o pedaço da SDL3 que os arquivos sem janela do port usam, para a build PowerPC da F1, que não
   tem SDL: o relógio (state.c) e o áudio (snd_adx.c, snd_se.c), que aqui nunca abre dispositivo. Com
   o áudio fechado, o estado que o jogo vê do som segue as mesmas regras da build de PC sem janela
   (BT3_SOUND_TICKS=1 conta a duração das músicas em quadros, igual nas duas). */
#ifndef X360_PPC_SDL_H
#define X360_PPC_SDL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define SDLCALL
#define SDL_INIT_AUDIO 0x10u
#define SDL_AUDIO_S16 0x8010u
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)

typedef uint32_t SDL_AudioDeviceID;
typedef int SDL_AudioFormat;
typedef struct SDL_AudioSpec { SDL_AudioFormat format; int channels; int freq; } SDL_AudioSpec;
typedef struct SDL_AudioStream SDL_AudioStream;
typedef void (SDLCALL *SDL_AudioStreamCallback)(void *user, SDL_AudioStream *stream, int additional, int total);

static inline uint64_t SDL_GetTicksNS(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

static inline const char *SDL_GetError(void) { return "sem SDL (build PowerPC da F1)"; }
static inline bool SDL_InitSubSystem(uint32_t flags) { (void)flags; return false; }
static inline const char *SDL_GetCurrentAudioDriver(void) { return NULL; }
static inline SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID d, const SDL_AudioSpec *s) { (void)d; (void)s; return 0; }
static inline bool SDL_ResumeAudioDevice(SDL_AudioDeviceID d) { (void)d; return false; }
static inline SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *a, const SDL_AudioSpec *b) { (void)a; (void)b; return NULL; }
static inline void SDL_DestroyAudioStream(SDL_AudioStream *s) { (void)s; }
static inline bool SDL_BindAudioStream(SDL_AudioDeviceID d, SDL_AudioStream *s) { (void)d; (void)s; return false; }
static inline bool SDL_SetAudioStreamGetCallback(SDL_AudioStream *s, SDL_AudioStreamCallback cb, void *u) { (void)s; (void)cb; (void)u; return false; }
static inline bool SDL_PutAudioStreamData(SDL_AudioStream *s, const void *b, int n) { (void)s; (void)b; (void)n; return false; }
static inline bool SDL_ClearAudioStream(SDL_AudioStream *s) { (void)s; return false; }
static inline bool SDL_LockAudioStream(SDL_AudioStream *s) { (void)s; return false; }
static inline bool SDL_UnlockAudioStream(SDL_AudioStream *s) { (void)s; return false; }
static inline bool SDL_SetAudioStreamGain(SDL_AudioStream *s, float g) { (void)s; (void)g; return false; }

#endif
