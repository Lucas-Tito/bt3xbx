/*
 * 360 (F2): a aritmética de float do PS2 pela FPU. Achado antes de port/src/softfloat_ps2_inl.h pela ordem dos
 * includes (port/x360/tools/c89.py), com a mesma interface: o código vetorial do port (src/port/vu0_a.c, vu0_b.c)
 * faz soma e multiplicação por aqui. A FPU do 360 roda em truncamento (_controlfp(_RC_CHOP), port/x360/src), o
 * modelo do PS2 para soma e multiplicação; o que difere (bit de guarda da soma, denormais, estouro) está na
 * decisão 5. Sem os caminhos rápidos do PC: mode() diz "sem modo especial".
 */
#ifndef PORT_SOFTFLOAT_PS2_INL_H
#define PORT_SOFTFLOAT_PS2_INL_H
#include <stdint.h>
#include <string.h>

#define SIGN 0x80000000u
#define FMAX 0x7F7FFFFFu
#define EXP(x) (((x) >> 23) & 0xFF)
#define MANT(x) (((x) & 0x7FFFFF) | 0x800000)

static inline float sf_float(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static inline uint32_t sf_bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static inline int mode(int which) {
    (void)which;
    return 0;
}

static inline uint32_t Sf_AddBits(uint32_t a, uint32_t b) { return sf_bits(sf_float(a) + sf_float(b)); }
static inline uint32_t Sf_MulBits(uint32_t a, uint32_t b) { return sf_bits(sf_float(a) * sf_float(b)); }

#endif
