/*
 * F2: as funções de float da biblioteca C (fabsf, ceilf e powf já vêm da d3d9.lib do XDK, das versões C++ da
 * crt/math.h, e não se repetem aqui). Na CRT do Xbox em C elas são macros sobre as de double (crt/math.h),
 * sem símbolo; o port as declara e chama como funções (port/x360/shim/math.h), então elas existem aqui, do mesmo
 * jeito que as macros da CRT. As que o jogo usa da biblioteca do PS2 (newlib, Port_*) não passam por aqui.
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

float sqrtf(float x) { return (float)sqrt((double)x); }
float floorf(float x) { return (float)floor((double)x); }
float expf(float x) { return (float)exp((double)x); }
float logf(float x) { return (float)log((double)x); }
float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
float sinf(float x) { return (float)sin((double)x); }
float cosf(float x) { return (float)cos((double)x); }
float tanf(float x) { return (float)tan((double)x); }
float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
float atanf(float x) { return (float)atan((double)x); }
float asinf(float x) { return (float)asin((double)x); }
float acosf(float x) { return (float)acos((double)x); }
