/* F5: fronteira de publicação, comum ao D3D e ao falso. q contém apenas desenhos
   fechados: o último ainda fundível fica fora. O consumidor nunca consulta a lista viva.
   Conservador: não cortar sequências DATE. Um Resolve adicional no meio delas pode
   atualizar a textura que date_area guardou por alias, mesmo sem corrida de CPU. */
#ifndef GS_LOTES_H
#define GS_LOTES_H
#define GS_LOTE_MIN 64u
static uint32_t gs_lotes_limite(const GsQuadro *q, uint32_t de, int (*usaZ)(const GsDraw *)) {
    uint32_t i = de, seguro = de, j, grupo = de;
    while (i < q->drawCount) {
        const GsDraw *d = &q->draws[i];
        if (d->native == 5) {
            for (j = i + 1; j < q->drawCount && q->draws[j].native != 5; j++) {}
            if (j == q->drawCount) break;
            /* A sequência inteira vai na mesma faixa. O marcador seguinte fica nela
               como consulta, mas será executado só junto de seus próprios leitores. */
            grupo = j;
        }
        if (!d->native && !usaZ(d)) {
            for (j = i + 1; j < q->drawCount; j++) {
                const GsDraw *e = &q->draws[j];
                if (!e->native && (e->target != d->target || usaZ(e))) break;
            }
            if (j == q->drawCount) break;
        }
        i++;
        if (i >= grupo) seguro = i;
    }
    return seguro - de >= GS_LOTE_MIN ? seguro : de;
}
#endif
