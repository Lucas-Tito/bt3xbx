/* Incluído uma vez pelo gs_core.c, depois do relógio das famílias. Não mede a espera entre listas.
   Duas marcas por chamada só nos quadros pedidos. Não usar este perfil para comparar o ritmo sem marcas. */
int gGsCortesAtivo;
static int sCortesInit, sCortesLigado, sCortesPilha[32], sCortesTopo;
static unsigned sCortesA, sCortesB, sCortesQuadro, sCortesChamadas[COR_N];
static uint64_t sCortesDesde, sCortesInicio, sCortesT[COR_N];
static void cortes_marca(int etapa) {
    uint64_t t = GS_FAM_TIQUE();
    sCortesT[etapa] += t - sCortesDesde;
    sCortesDesde = t;
}
void Gs_CortesEntra(int etapa) {
    cortes_marca(sCortesPilha[sCortesTopo]);
    if (++sCortesTopo >= 32) abort();
    sCortesPilha[sCortesTopo] = etapa;
    sCortesChamadas[etapa]++;
}
void Gs_CortesSai(void) {
    cortes_marca(sCortesPilha[sCortesTopo]);
    if (--sCortesTopo < 0) abort();
}
void Gs_CortesInicio(unsigned quadro) {
    if (!sCortesInit) {
        const char *e = getenv("BT3_CORTES");
        sCortesInit = 1;
        sCortesLigado = e != NULL && atoi(e) > 0;
        sCortesA = 6900; sCortesB = 7025;
        if (e != NULL) sscanf(e, "%u,%u", &sCortesA, &sCortesB);
        if (e != NULL && atoi(e) == 1) sCortesA = 6900;
    }
    gGsCortesAtivo = sCortesLigado && quadro >= sCortesA && quadro <= sCortesB;
    if (!gGsCortesAtivo) return;
    memset(sCortesT, 0, sizeof(sCortesT));
    memset(sCortesChamadas, 0, sizeof(sCortesChamadas));
    sCortesTopo = 0; sCortesPilha[0] = COR_DMA; sCortesQuadro = quadro;
    sCortesDesde = sCortesInicio = GS_FAM_TIQUE();
}
void Gs_CortesFim(void) {
    static const char *nomes[COR_N] = {"dma", "vif", "gif", "cpu", "vu1", "unpack", "envio", "entrega", "hash", "constantes", "fecho"};
    char linha[1000];
    int i, n;
    uint64_t total;
    if (!gGsCortesAtivo) return;
    if (sCortesTopo != 0) abort();
    cortes_marca(COR_DMA);
    total = sCortesDesde - sCortesInicio;
    gGsCortesAtivo = 0;
    n = snprintf(linha, sizeof(linha), "cortes: gravacao quadro %u total %.6f", sCortesQuadro, Gs_FamMs(total));
    for (i = 0; i < COR_N; i++) n += snprintf(linha + n, sizeof(linha) - n, " %s %.6f/%u", nomes[i], Gs_FamMs(sCortesT[i]), sCortesChamadas[i]);
#ifdef PORT_X360
    { extern void Diario_Escrever(const char *formato, ...); Diario_Escrever("%s", linha); }
#else
    fprintf(stderr, "%s\n", linha);
#endif
}
