/* Sem bytes do jogo: fronteiras, índices referidos, uniforme e fusões. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/gs/gs_draw.h"
#include "../include/gs_alfa.h"
int main(int argc, char **argv) {
    GsQuadro q;
    GsDraw d[2];
    Vtx v[3];
    Vu0Uniform u;
    float vv[36];
    uint32_t idx[3] = {0, 1, 2};
    unsigned char alfa[2] = {0, 0};
    unsigned a;
    (void)argv;
    memset(&q, 0, sizeof(q)); memset(d, 0, sizeof(d)); memset(v, 0, sizeof(v));
    memset(&u, 0, sizeof(u)); memset(vv, 0, sizeof(vv));
    q.draws=d; q.drawCount=1; q.verts=v; q.vertCount=3;
    q.vuVerts=vv; q.vuVertCount=3; q.vuIdx=idx; q.vuIdxCount=3; q.vuUni=&u; q.vuUniCount=1; q.alfa=alfa;
    d[0].count=3;
    for (a=0; a<256; a++) {
        v[2].a=(unsigned char)a;
        gs_alfa_junta(alfa, 0, a>128, 0);
        assert(gs_alfa_metadado(&q,0,8)==gs_alfa_referencia(&q,d,8));
    }
    v[2].a=0; gs_alfa_junta(alfa,0,0,0); gs_alfa_junta(alfa,0,1,1);
    gs_alfa_junta(alfa,0,0,1);
    v[2].a=255;
    assert(gs_alfa_metadado(&q,0,8)==1);
    assert(gs_alfa_metadado(&q,0,0)==-1);
    d[0].native=5; assert(gs_alfa_metadado(&q,0,8)==-1); d[0].native=0;
    d[0].count=0; assert(gs_alfa_metadado(&q,0,8)==-1); d[0].count=3;
    d[0].vu=1;
    for (a=0; a<256; a++) {
        u.misc[3]=3; u.color0[3]=0; vv[31]=(float)a;
        gs_alfa_junta(alfa,0,a>128,0);
        assert(gs_alfa_metadado(&q,0,8)==gs_alfa_referencia(&q,d,8));
    }
    vv[31]=255; idx[2]=3; gs_alfa_junta(alfa,0,0,0);
    assert(gs_alfa_referencia(&q,d,8)==0); /* fora do limite não participa */
    idx[2]=2; u.misc[3]=4;
    assert(gs_alfa_referencia(&q,d,8)==0); /* alfa do vértice ignorado */
    assert(gs_alfa_vu(&u,gs_alfa_triangulo(vv,0),3)==0);
    u.misc[3]=3;
    assert(gs_alfa_vu(&u,gs_alfa_triangulo(vv,0),3)==1);
    assert(gs_alfa_vu(&u,1,0)==0);
    u.misc[3]=0;
    u.color0[3]=129; gs_alfa_junta(alfa,0,1,0);
    assert(gs_alfa_referencia(&q,d,8)==1);
    assert(gs_alfa_vu(&u,0,3)==1);
    assert(gs_alfa_metadado(&q,0,8)==gs_alfa_referencia(&q,d,8));
    if (argc>1) { /* cópia inconsistente recusada pelo mesmo oráculo por desenho */
        alfa[0]=0;
        assert(gs_alfa_metadado(&q,0,8)==gs_alfa_referencia(&q,d,8));
    }
    puts("alfa: 512 fronteiras, máscara, nativa, vazio, índice, misc, uniforme e fusão: tudo certo");
    return 0;
}
