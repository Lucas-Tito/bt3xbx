/* F3, etapa 7: o <stdint.h> que o cl do XDK não tem, só para o backend D3D9 (gs_d3d.cpp) incluir os headers do GS do
   port (gs_draw.h, gs_internal.h). Os mesmos tamanhos do shim com que o c89.py prepara o gs_draw.c
   (port/x360/shim/stdint.h): as structs que os dois lados trocam (GsDraw, Vtx, GsTarget) têm o mesmo layout. */
#ifndef X360_GS_CL_STDINT_H
#define X360_GS_CL_STDINT_H
#include <crtdefs.h> /* uintptr_t e intptr_t vêm de lá */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif
