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
static uint32_t sPipeKey[4] = {0x10400u, 0x11400u, 0x410400u, 0x10800u}; /* 0 sem z, 1 escreve z, 2 fx, 3 testa z */
#include "../headless/prefixos.h"
/* (seção 36) as regras reais: visitas com e sem z, um DATE curto, um longo e o último marcador, em ordem */
static void regras(void) {
    GsDraw d[80];
    GsQuadro q;
    unsigned rel[80], i;
    PxCenso c;
    const char *m = getenv("BT3_PREFIXOS_MUTANTE");
    memset(d, 0, sizeof(d)); memset(&q, 0, sizeof(q));
    for (i = 0; i < 80; i++) { d[i].count = 1; d[i].target = 0; d[i].scissor[2] = 64; d[i].scissor[3] = 64; }
    d[0].pipeline = 1;                         /* visita 0 com z no primeiro: decide em 0 */
    d[1].target = 1; d[2].target = 1;          /* visita do 1 sem z: decide na troca, o 3 */
    d[3].pipeline = 3;                         /* volta ao 0, testa z: decide em 3 */
    d[4].native = 5;                           /* DATE curto: o próximo marcador é o 10 */
    d[10].native = 5;                          /* DATE longo: o próximo é o 75 (65 adiante) */
    d[75].native = 5;                          /* o último: até o fim, 5 adiante, curto */
    d[76].target = 2; d[77].target = 2; d[78].target = 2; d[79].target = 2; /* visita final sem z: só no fim */
    if (m && !strcmp(m, "regra_z")) d[3].pipeline = 0;
    if (m && !strcmp(m, "regra_date")) d[10].native = 0;
    q.draws = d; q.drawCount = 80; q.targetCount = 3;
    px_regras(&q, sPipeKey, rel, &c);
    px_exige(rel[0] == 1 && rel[1] == 4 && rel[2] == 4 && rel[3] == 4, "regra: z da visita");
    px_exige(rel[4] == 10 && rel[9] == 10 && rel[10] == 11, "regra: DATE curto espera, longo não");
    px_exige(rel[75] == 80 && rel[76] == 80 && c.liberados == 75, "regra: o último marcador e a visita final");
    px_exige(c.dateLongos == 1 && c.zmax == 4, "regra: contagens");
    puts("prefixos2: regras de z, DATE curto/longo e ordem: tudo certo");
}
int main(void) {
    GsQuadro q;
    regras();
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
