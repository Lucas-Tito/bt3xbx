/* F3, etapa 7: o pedaço da SDL3 que a gravação do quadro (port/src/gs/gs_draw.c) e o ui.h citam, para o gs_draw.c
   compilar no 360 sem mudar. No 360 não há janela, painel de configurações nem pacote de texturas: a janela comum dos
   backends do PC (GsDraw_WindowCreate) sempre falha aqui, e o backend do 360 (port/x360/src/gs/gs_d3d.cpp) nunca a
   pede. O relógio e o áudio vêm do shim da build sem janela (port/x360/headless/SDL3/SDL.h). */
#ifndef X360_GS_SDL_H
#define X360_GS_SDL_H
#include "../../../../headless/SDL3/SDL.h"

#define SDL_INIT_VIDEO 0x20u
#define SDL_WINDOWPOS_CENTERED_DISPLAY(d) ((int)(0x2FFF0000u | (d)))
#define SDL_PROP_WINDOW_CREATE_TITLE_STRING "title"
#define SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER "width"
#define SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER "height"
#define SDL_PROP_WINDOW_CREATE_X_NUMBER "x"
#define SDL_PROP_WINDOW_CREATE_Y_NUMBER "y"
#define SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN "resizable"
#define SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN "opengl"
#define SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN "fullscreen"

typedef struct SDL_Window SDL_Window;
typedef struct SDL_GPUDevice SDL_GPUDevice;
typedef struct SDL_GPUCommandBuffer SDL_GPUCommandBuffer;
typedef struct SDL_GPUTexture SDL_GPUTexture;
typedef union SDL_Event { uint32_t type; uint8_t padding[128]; } SDL_Event;
typedef int SDL_GamepadButton;
typedef uint32_t SDL_DisplayID;
typedef uint32_t SDL_PropertiesID;
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;

static inline bool SDL_Init(uint32_t flags) { (void)flags; return false; }
static inline SDL_DisplayID *SDL_GetDisplays(int *count) { *count = 0; return NULL; }
static inline bool SDL_GetDisplayBounds(SDL_DisplayID d, SDL_Rect *r) { (void)d; r->x = r->y = r->w = r->h = 0; return false; }
static inline const char *SDL_GetDisplayName(SDL_DisplayID d) { (void)d; return ""; }
static inline SDL_PropertiesID SDL_CreateProperties(void) { return 0; }
static inline void SDL_DestroyProperties(SDL_PropertiesID p) { (void)p; }
static inline bool SDL_SetStringProperty(SDL_PropertiesID p, const char *n, const char *v) { (void)p; (void)n; (void)v; return false; }
static inline bool SDL_SetNumberProperty(SDL_PropertiesID p, const char *n, long long v) { (void)p; (void)n; (void)v; return false; }
static inline bool SDL_SetBooleanProperty(SDL_PropertiesID p, const char *n, bool v) { (void)p; (void)n; (void)v; return false; }
static inline SDL_Window *SDL_CreateWindowWithProperties(SDL_PropertiesID p) { (void)p; return NULL; }
static inline bool SDL_SetWindowSize(SDL_Window *w, int x, int y) { (void)w; (void)x; (void)y; return false; }
static inline bool SDL_GetWindowSize(SDL_Window *w, int *x, int *y) { (void)w; *x = *y = 0; return false; }
static inline bool SDL_SetWindowFullscreen(SDL_Window *w, bool on) { (void)w; (void)on; return false; }
static inline void SDL_free(void *p) { (void)p; }
/* (só as imagens intermediárias, BT3_INTERP, esperam por aqui; desligadas no 360) */
static inline void SDL_DelayPrecise(uint64_t ns) {
    struct timespec t;
    t.tv_sec = (long)(ns / 1000000000ull);
    t.tv_nsec = (long)(ns % 1000000000ull);
    nanosleep(&t, NULL);
}

#endif
