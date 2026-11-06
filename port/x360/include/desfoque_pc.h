/* F8/14: política do PC, separada da escala física do backend. Todas as opções
   começam desligadas para conservar a imagem da 1.0.2. */
#ifndef DESFOQUE_PC_H
#define DESFOQUE_PC_H
typedef struct DesfoquePc {
    int sharp, distancia, media, blocos, brilho;
    int cadeia; /* 0 fora, 1 brilho, 2 distância */
} DesfoquePc;

static int desfoque_pc_baixo(const DesfoquePc *p, unsigned pagina) {
    int cadeia = (p->cadeia == 1 && p->brilho) || (p->cadeia == 2 && p->distancia);
    return cadeia && (pagina == 0x150 || pagina == 0x170 || pagina == 0x1F6 || pagina == 0x1F8);
}

static int desfoque_pc_bloco(const DesfoquePc *p, int linear, int fonte_quadro,
                            int destino_quadro, int fonte_baixa, int destino_baixo) {
    if (!linear || fonte_baixa) return 0;
    if (fonte_quadro && destino_baixo) {
        return (p->cadeia == 2 && p->media) || p->blocos;
    }
    return p->blocos && !fonte_quadro && !destino_quadro && !(p->sharp && !p->cadeia);
}
#endif
