/* Sequência sintética com fila cheia, estados fechados e leituras de versões. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/gs/gs_draw.h"
unsigned gGsFrame=7;
GsDraw *gsDraws;
uint32_t gsDrawCount,gsVertCount,gsVuVertCount,gsVuIdxCount,gsVuUniCount;
Vtx *gsVerts;
float *gsVuVerts;
uint32_t *gsVuIdx;
Vu0Uniform *gsVuUni;
int gsTargetCount;
#include "../headless/prefixos.h"
int main(void) {
    GsQuadro q;
    GsDraw d[33];
    Vtx v[33];
    unsigned i;
    memset(&q,0,sizeof(q)); memset(d,0,sizeof(d)); memset(v,0,sizeof(v));
    gsDraws=d; gsVerts=v; gsTargetCount=1;
    for(i=0;i<33;i++) {
        d[i].target=0; d[i].first=i; d[i].count=1; v[i].a=(uint8_t)i;
        if(i&1) { d[i].tex_is_target=1; d[i].tex=0x1000; }
        gsDrawCount=gsVertCount=i+1;
        px_progresso();
    }
    q.draws=d; q.drawCount=33; q.verts=v; q.vertCount=33; q.targetCount=1; q.quadro=7;
    px_final(&q);
    px_exige(sPx.ligado && sPx.publicado==0 && sPx.consumido==0,"fila drenada");
    puts("prefixos: tudo certo");
    return 0;
}
