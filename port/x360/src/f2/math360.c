/*
 * F2: as funções de float da biblioteca C (fabsf, ceilf e powf já vêm da d3d9.lib do XDK, das versões C++ da
 * crt/math.h, e não se repetem aqui). Na CRT do Xbox em C elas são macros sobre as de double (crt/math.h),
 * sem símbolo; o port as declara e chama como funções (port/x360/shim/math.h), então elas existem aqui, do mesmo
 * jeito que as macros da CRT. As que o jogo usa da biblioteca do PS2 (newlib, Port_*) não passam por aqui.
 * F6, etapa 3: a xaudio2.lib traz as versões C++ da crt/math.h (inline: COMDAT "qualquer uma") de várias destas, e uma
 * definição comum aqui dava LNK2005. Com __inline o cl as emite também como COMDAT "qualquer uma" (a tabela abaixo
 * pega o endereço para que saiam), e o link fica com a primeira que vê: a deste objeto, que vem antes das bibliotecas
 * (o .map mostra math360.obj em cada uma). O corpo é o mesmo da crt/math.h.
 */
#include <math.h>

#undef fabsf
#undef sqrtf
#undef floorf
#undef ceilf
#undef expf
#undef logf
#undef powf
#undef fmodf
#undef sinf
#undef cosf
#undef tanf
#undef atan2f
#undef atanf
#undef asinf
#undef acosf

__inline float sqrtf(float x) { return (float)sqrt((double)x); }
__inline float floorf(float x) { return (float)floor((double)x); }
__inline float expf(float x) { return (float)exp((double)x); }
__inline float logf(float x) { return (float)log((double)x); }
__inline float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
__inline float sinf(float x) { return (float)sin((double)x); }
__inline float cosf(float x) { return (float)cos((double)x); }
__inline float tanf(float x) { return (float)tan((double)x); }
__inline float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
__inline float atanf(float x) { return (float)atan((double)x); }
__inline float asinf(float x) { return (float)asin((double)x); }
__inline float acosf(float x) { return (float)acos((double)x); }

/* (os endereços: sem eles o cl não emite as __inline que ninguém chama neste objeto) */
const void *const gMath360Funcoes[] = {
    (const void *)sqrtf, (const void *)floorf, (const void *)expf, (const void *)logf,
    (const void *)fmodf, (const void *)sinf, (const void *)cosf, (const void *)tanf,
    (const void *)atan2f, (const void *)atanf, (const void *)asinf, (const void *)acosf,
};
