/* Incluído pelo gs_draw.c, só na variante de compilação. A CPU da gravação
   aguarda a CPU do desenho em cada faixa, mas não a GPU. Isso conserva os donos
   de todos os recursos da entrega e permite à GPU executar antes do fim de G.
   Uma vaga, sem vblank adicional e sem espera por produtor na dona do D3D. */
static GsQuadro sLote;
static GsTarget sLoteAlvos[MAX_TARGETS];
static uint32_t sLoteAte, sLoteConsulta;
static void gs_lotes_passo(void) {
    uint32_t ate;
    if (!sDividido || !sVuDir || !sBackend->lotesProntos || gsDrawCount < sLoteAte + 1 + GS_LOTE_MIN) return;
    if (gsDrawCount < sLoteConsulta + 32) return;
    sLoteConsulta = gsDrawCount;
    pthread_mutex_lock(&sDesLock);
#ifdef PORT_X360
    while (sDesTem && !sDesParado) {
#else
    while (sDesTem) {
#endif
        pthread_cond_wait(&sDesFeito, &sDesLock);
    }
#ifdef PORT_X360
    if (sDesParado) { pthread_mutex_unlock(&sDesLock); return; }
    sEntregando = 1; /* impede a repetição enquanto o preparo mexe no backend */
#endif
    /* A decisão e o preparo precisam da mesma exclusão que entrega. */
    memset(&sLote, 0, sizeof(sLote));
    sLote.draws = gsDraws; sLote.drawCount = gsDrawCount - 1;
    sLote.verts = gsVerts; sLote.vertCount = gsVertCount;
    sLote.vuVerts = gsVuVerts; sLote.vuVertCount = gsVuVertCount;
    sLote.vuIdx = gsVuIdx; sLote.vuIdxCount = gsVuIdxCount;
    sLote.vuUni = gsVuUni; sLote.vuUniCount = gsVuUniCount;
    sLote.vuFaixa = sVuFaixa;
#ifdef GS_ALFA
    sLote.alfa = sAlfa[sJogo];
#endif
    memcpy(sLoteAlvos, gsTargets, sizeof(sLoteAlvos));
    sLote.targets = sLoteAlvos; sLote.targetCount = gsTargetCount;
    sLote.quadro = gGsFrame; sLote.dividido = 1;
    sLote.fbp = GS_FBP_QUADRO; sLote.anchor = gsAnchor;
    sLote.frame[0] = gGs.frame[0]; sLote.frame[1] = gGs.frame[1];
    sLote.texCount = gsTexCount; sLote.skipped = gsSkipped; sLote.native = gsNative;
    sLote.vuJogo = sVdJogo; sLote.vuSeq = sVdSeqDe[sVdJogo];
    ate = sBackend->lotesProntos(&sLote, sLoteAte);
    if (ate == sLoteAte) {
#ifdef PORT_X360
        sEntregando = 0;
#endif
        pthread_mutex_unlock(&sDesLock);
        return;
    }
    pthread_mutex_unlock(&sDesLock);
    sBackend->lotesPrepara(&sLote);
    /* No 360, a ponte de posix360.c emite sync, não lwsync. A escrita
       combinada da VU1 precisa sair do núcleo antes da publicação. */
    __sync_synchronize();
    pthread_mutex_lock(&sDesLock);
    sLoteAte = ate;
#ifdef PORT_X360
    sEntregando = 0;
#endif
    sDesTem = 4;
    pthread_cond_signal(&sDesAcorda);
#ifdef PORT_X360
    while (sDesTem && !sDesParado) {
#else
    while (sDesTem) {
#endif
        pthread_cond_wait(&sDesFeito, &sDesLock);
    }
    pthread_mutex_unlock(&sDesLock);
}
