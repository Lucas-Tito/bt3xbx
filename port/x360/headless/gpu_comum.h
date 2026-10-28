/* Diagnóstico de listas, só no backend falso. A saída contém dados do jogo: sempre fora do git.
   BT3_GPU_COMUM=pasta existente; BT3_GPU_COMUM_QUADROS=lista separada por vírgulas.
   Não altera listas nem hashes. Posições só das primitivas comuns, antes dos testes por pixel. */
static void comum_lista(const Vista *q) {
    const char *pasta = getenv("BT3_GPU_COMUM"), *p = getenv("BT3_GPU_COMUM_QUADROS");
    char nome[1024];
    FILE *f;
    unsigned i, j;
    int selecionado = 0;
    if (!pasta || !p) return;
    while (*p) {
        char *fim;
        unsigned n = (unsigned)strtoul(p, &fim, 10);
        if (fim == p) return;
        if (n == q->quadro) selecionado = 1;
        p = fim;
        if (*p == ',') p++;
        else if (*p) return;
    }
    if (!selecionado) return;
    snprintf(nome, sizeof(nome), "%s/%u.jsonl", pasta, q->quadro);
    f = fopen(nome, "w");
    if (!f) { perror(nome); exit(1); }
    fprintf(f, "{\"quadro\":%u,\"desenhos\":%u,\"vertices\":%u,\"fbp\":%d,\"escala\":%d,\"alvos\":[", q->quadro, q->drawCount, q->vertCount, q->fbp, gsScale);
    for (i = 0; i < (unsigned)q->targetCount; i++)
        fprintf(f, "%s%u", i ? "," : "", q->targets[i].fbp);
    fprintf(f, "]}\n");
    for (i = 0; i < q->drawCount; i++) {
        const GsDraw *d = &q->draws[i];
        fprintf(f, "{\"n\":%u,\"vu\":%d,\"native\":%d,\"src\":%d,\"alvo\":%d,\"tex\":%u,\"tex_alvo\":%d,\"sampler\":%d,\"pipe\":%u,\"count\":%u,\"modo\":[%d,%d,%d,%d],\"misc\":[%.9g,%.9g,%.9g,%.9g],\"tesoura\":[%d,%d,%d,%d],\"blendc\":%.9g,\"rect\":[%.9g,%.9g,%.9g,%.9g],\"xy\":[",
                i, d->vu, d->native, d->src, d->target, (unsigned)d->tex, d->tex_is_target, d->sampler, sPipeKey[d->pipeline], d->count,
                d->mode[0], d->mode[1], d->mode[2], d->mode[3], d->misc[0], d->misc[1], d->misc[2], d->misc[3],
                d->scissor[0], d->scissor[1], d->scissor[2], d->scissor[3], d->blendc, d->rect[0], d->rect[1], d->rect[2], d->rect[3]);
        if (!d->vu && !d->native) for (j = 0; j < d->count; j++) {
            const Vtx *v = &q->verts[d->first + j];
            fprintf(f, "%s[%.9g,%.9g]", j ? "," : "", v->x, v->y);
        }
        fprintf(f, "]}\n");
    }
    if (fclose(f)) { perror(nome); exit(1); }
}
