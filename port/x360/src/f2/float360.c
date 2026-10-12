/*
 * F2: o modelo de float do 360 (decisões 4 e 5): a FPU em truncamento faz soma e multiplicação como o PS2. Aqui o
 * que no PC vem do float por software (port/src/softfloat_ps2.c, plat_fastvec.c, plat_libm.c), na mesma
 * interface, pela FPU:
 *   - RefVu0_AddBits / MulBits / DivBits: a aritmética do código vetorial, em padrões de bits;
 *   - Port_FastAdd / Mul / MtxApply: os caminhos rápidos do PC (o de matriz diz "não tratado": vai pelo comum);
 *   - Port_ceilf, expf, logf, fmodf: as funções que a biblioteca do PS2 não fornece (o resto vem da newlib).
 * O que difere do PS2 (bit de guarda da soma, denormais, estouro, divisão ao mais próximo) fica medido na decisão
 * 5; para jogar sozinho basta, e a F2 mede o efeito comparando com a build de referência.
 */
#include <math.h>
#include <string.h>

typedef unsigned int u32;

static float f(u32 u) {
    float x;
    memcpy(&x, &u, 4);
    return x;
}

static u32 b(float x) {
    u32 u;
    memcpy(&u, &x, 4);
    return u;
}

u32 RefVu0_AddBits(u32 x, u32 y) { return b(f(x) + f(y)); }
u32 RefVu0_MulBits(u32 x, u32 y) { return b(f(x) * f(y)); }

u32 RefVu0_DivBits(u32 x, u32 y) {
    if ((y & 0x7FFFFFFFu) == 0) {
        return ((x ^ y) & 0x80000000u) | 0x7F7FFFFFu; /* divisão por zero: o maior número, como o PS2 */
    }
    return b(f(x) / f(y));
}

u32 Port_FastAdd(u32 x, u32 y) { return RefVu0_AddBits(x, y); }
u32 Port_FastMul(u32 x, u32 y) { return RefVu0_MulBits(x, y); }

int Port_FastMtxApply(const u32 *r, const u32 *v, u32 *out) {
    (void)r;
    (void)v;
    (void)out;
    return 0;
}

#undef ceilf
#undef expf
#undef logf
#undef fmodf
float Port_ceilf(float x) { return (float)ceil((double)x); }
float Port_expf(float x) { return (float)exp((double)x); }
float Port_logf(float x) { return (float)log((double)x); }
float Port_fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }

/* o gancho do código vetorial para o atan2f do jogo (o port_libm.h o liga à biblioteca do PS2 no código do jogo) */
float Port_atan2f(float y, float x);
float Ref_atan2f(float y, float x) { return Port_atan2f(y, x); }
