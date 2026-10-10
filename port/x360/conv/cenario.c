/*
 * Arquivos de cenário da luta (pzs3us1, índices 368..445): o pacote de 25 entradas que a luta carrega em
 * BattleRes.stage. Especificação: docs/x360/formatos/cenario.md (as seções citadas abaixo).
 *
 * Estruturas apontadas por mais de um lugar (partes, malhas, cadeias VIF, malhas de colisão, listas de quadros,
 * listas de chaves, pontos de caminho...) se convertem uma vez só: antes de converter, ja() olha a marca do
 * primeiro byte.
 */
#include "conv.h"
#include "conv_pecas.h"

/* entrada n de uma tabela já lida; b == a quando vazia */
#define ENT(n) (base + ent[n]), (base + ent[(n) + 1])

#define N_ENTRADAS 25

/* um s32 do arquivo (lido como conv_u32) é positivo */
#define POSITIVO(v) ((v) != 0 && (v) < 0x80000000u)

static void vazia(ConvCtx *c, const conv_u32 *ent, conv_u32 n) {
    if (ent[n] != ent[n + 1]) {
        Conv_Erro(c, "entrada %lu: esperada vazia (sem leitor)", (unsigned long)n);
    }
}

/* 1 se a estrutura em p já foi convertida por outro caminho (o primeiro byte tem a marca de começo esperada);
   se p cai no meio de outra coisa, é erro (devolve 1 para o chamador não converter) */
static int ja(ConvCtx *c, conv_ofs p, conv_u8 marca, const char *oque) {
    conv_u8 m = Conv_Marca(c, p);
    if (c->erro) {
        return 1;
    }
    if (m == CONV_LIVRE) {
        return 0;
    }
    if (m != marca) {
        Conv_Erro(c, "%s em 0x%lX: o lugar já foi convertido como outra coisa (marca %u)", oque, (unsigned long)p,
                  (unsigned)m);
    }
    return 1;
}

/* n registros de tam bytes em [p, p + n x tam) cabem em [p, fim)? senão erro */
static int cabem(ConvCtx *c, conv_ofs p, conv_u32 n, conv_ofs tam, conv_ofs fim, const char *oque) {
    if (c->erro) {
        return 0;
    }
    if (p > fim || (tam != 0 && n > (fim - p) / tam)) {
        Conv_Erro(c, "%s em 0x%lX: %lu registros de 0x%lX bytes passam de 0x%lX", oque, (unsigned long)p,
                  (unsigned long)n, (unsigned long)tam, (unsigned long)fim);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ entrada 1 (§4) */

/* a entrada 1 em [ini, fim); todo deslocamento dela é em palavras a partir de ini (§4.1) */
typedef struct Ent1 {
    conv_ofs ini, fim;
    conv_ofs timers;  /* §4.4: os temporizadores, para conferir o `anim` dos materiais */
    conv_u32 ntimers;
} Ent1;

/* deslocamento em palavras -> posição de n registros de tam bytes dentro da entrada 1; ini com erro se não cabe */
static conv_ofs em1(ConvCtx *c, const Ent1 *e, conv_u32 desl, conv_u32 n, conv_ofs tam, const char *oque) {
    conv_ofs p;
    if (c->erro) {
        return e->ini;
    }
    if (desl > (e->fim - e->ini) / 4) {
        Conv_Erro(c, "%s: deslocamento 0x%lX palavras fora da entrada 1", oque, (unsigned long)desl);
        return e->ini;
    }
    p = e->ini + (conv_ofs)desl * 4;
    return cabem(c, p, n, tam, e->fim, oque) ? p : e->ini;
}

/* §4.5: cadeia VIF de uma malha (etiqueta ret, fluxo, etiqueta end): toda palavra u32 (decisão 12) */
static void cadeia(ConvCtx *c, const Ent1 *e, conv_u32 desl) {
    conv_ofs p;
    if (desl == 0) {
        Conv_Erro(c, "malha com cadeia VIF de deslocamento 0");
        return;
    }
    p = em1(c, e, desl, 1, 16, "cadeia VIF");
    if (!ja(c, p, 4, "cadeia VIF")) {
        Conv_PacoteVif(c, p, e->fim);
    }
}

/* §4.5: malha (StgMMesh, 0x10) */
static void malha(ConvCtx *c, const Ent1 *e, conv_u32 desl) {
    conv_ofs p;
    if (desl == 0) {
        Conv_Erro(c, "malha de deslocamento 0 (o cabeçalho)");
        return;
    }
    p = em1(c, e, desl, 1, 0x10, "malha");
    if (ja(c, p, 2, "malha")) {
        return;
    }
    Conv_U16(c, p);         /* visIdx */
    Conv_Bytes(c, p + 2, 6); /* unk2, prog, alpha, 3 sem leitor */
    cadeia(c, e, Conv_U32(c, p + 8));
    Conv_U32(c, p + 0x0C);  /* mtx: nó do destroço ou 0 (não seguido: os nós são convertidos em §4.3) */
}

/* §4.5: material (StgMMat, 0x10) */
static void material(ConvCtx *c, const Ent1 *e, conv_ofs p) {
    conv_u32 n, malhas, anim, i;
    conv_ofs l;
    if (ja(c, p, 2, "material")) {
        return;
    }
    Conv_U16(c, p); /* tex */
    n = Conv_U16(c, p + 2);
    malhas = Conv_U32(c, p + 4);
    anim = Conv_U32(c, p + 8);
    Conv_U16s(c, p + 0x0C, 2); /* cur (u16, como o leitor; §4.5), unkE */
    if (anim != 0 && !c->erro) {
        /* §4.4: o `anim` aponta para um dos temporizadores, já convertidos */
        conv_ofs t = e->ini + (conv_ofs)anim * 4;
        if (anim > (e->fim - e->ini) / 4 || t < e->timers || t >= e->timers + (conv_ofs)e->ntimers * 0x10 ||
            (t - e->timers) % 0x10 != 0) {
            Conv_Erro(c, "material em 0x%lX: anim 0x%lX não é um temporizador", (unsigned long)p, (unsigned long)anim);
            return;
        }
    }
    if (n == 0) {
        return;
    }
    l = em1(c, e, malhas, n, 0x10, "malhas do material");
    for (i = 0; i < n && !c->erro; i++) {
        malha(c, e, (conv_u32)((l - e->ini) / 4 + i * 4));
    }
}

/* §4.6: parte (StgMPart, 0x20) e a lista de malhas dela */
static void parte(ConvCtx *c, const Ent1 *e, conv_u32 desl) {
    conv_ofs p, l;
    conv_u32 n, malhas, i;
    if (desl == 0) {
        Conv_Erro(c, "parte de deslocamento 0 (o cabeçalho)");
        return;
    }
    p = em1(c, e, desl, 1, 0x20, "parte");
    if (ja(c, p, 2, "parte")) {
        return;
    }
    n = Conv_U16(c, p);
    Conv_U16(c, p + 2);       /* flags */
    malhas = Conv_U32(c, p + 4);
    Conv_U32s(c, p + 8, 6);   /* node (não seguido: §4.3), radius, centro, unk1C */
    if (n == 0) {
        return;
    }
    l = em1(c, e, malhas, n, 4, "lista de malhas da parte");
    if (ja(c, l, 4, "lista de malhas da parte")) {
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        malha(c, e, Conv_U32(c, l + i * 4));
    }
}

/* §4.7: célula da árvore de recorte (StgMCell, 0x40), recursiva */
static void celula(ConvCtx *c, const Ent1 *e, conv_u32 desl, int prof) {
    conv_ofs p, l;
    conv_u32 filhos[8], n, partes, i;
    if (prof > 64) {
        Conv_Erro(c, "árvore de recorte com mais de 64 níveis");
        return;
    }
    p = em1(c, e, desl, 1, 0x40, "célula");
    if (ja(c, p, 4, "célula")) {
        return;
    }
    Conv_U32(c, p); /* a mãe (não seguida) */
    for (i = 0; i < 8; i++) {
        filhos[i] = Conv_U32(c, p + 4 + i * 4);
    }
    Conv_U32s(c, p + 0x24, 5); /* vis, centro, depth */
    n = Conv_U32(c, p + 0x38);
    partes = Conv_U32(c, p + 0x3C);
    if (POSITIVO(n) && partes != 0) {
        l = em1(c, e, partes, n, 4, "lista de partes da célula");
        if (!ja(c, l, 4, "lista de partes da célula")) {
            for (i = 0; i < n && !c->erro; i++) {
                conv_u32 x = Conv_U32(c, l + i * 4);
                if (x != 0 && x != 0xFFFFFFFFu) { /* 0 e -1 = nenhuma */
                    parte(c, e, x);
                }
            }
        }
    }
    for (i = 0; i < 8 && !c->erro; i++) {
        if (filhos[i] != 0) {
            celula(c, e, filhos[i], prof + 1);
        }
    }
}

/* §4.9: malha de colisão (ColMesh: cabeçalho de 0x30, vértices, polígonos, nós) */
static void colisao(ConvCtx *c, const Ent1 *e, conv_u32 desl) {
    conv_ofs p, pv, pp, pn;
    conv_u32 nos, npol, ono, opol, ovtx, maior = 0, i, k;
    p = em1(c, e, desl, 1, 0x30, "malha de colisão");
    if (ja(c, p, 4, "malha de colisão")) {
        return;
    }
    nos = Conv_U32(c, p);
    npol = Conv_U32(c, p + 4);
    Conv_U32(c, p + 8);
    ono = Conv_U32(c, p + 0x0C);
    opol = Conv_U32(c, p + 0x10);
    ovtx = Conv_U32(c, p + 0x14);
    Conv_U32s(c, p + 0x18, 6); /* caixa (sem leitor) */
    if (!POSITIVO(nos)) {
        return; /* o código só testa nodeCount <= 0 e não anda */
    }
    if (npol >= 0x80000000u) {
        Conv_Erro(c, "malha de colisão em 0x%lX: %ld polígonos", (unsigned long)p, (long)(int)npol);
        return;
    }
    pp = em1(c, e, opol, npol, 0x20, "polígonos de colisão");
    pn = em1(c, e, ono, nos, 0x20, "nós de colisão");
    for (i = 0; i < npol && !c->erro; i++) {
        conv_ofs q = pp + i * 0x20;
        Conv_U32(c, q); /* flags */
        for (k = 0; k < 3; k++) {
            conv_u32 v = Conv_U32(c, q + 4 + k * 4);
            if (v >= 0x80000000u) {
                Conv_Erro(c, "polígono de colisão em 0x%lX: vértice %ld", (unsigned long)q, (long)(int)v);
            } else if (v + 1 > maior) {
                maior = v + 1; /* o arquivo não diz quantos vértices: o maior índice + 1 (§4.9) */
            }
        }
        Conv_U32s(c, q + 0x10, 4); /* plano */
    }
    pv = em1(c, e, ovtx, maior, 0x10, "vértices de colisão");
    Conv_U32s(c, pv, (conv_ofs)maior * 4);
    Conv_U32s(c, pn, (conv_ofs)nos * 8);
}

/* §4.8: zona (StgColZone, 0x40), vizinhas e registros */
static void zona(ConvCtx *c, const Ent1 *e, conv_ofs p) {
    conv_u32 mesh, nviz, viz, nreg, regs, i;
    conv_ofs l;
    Conv_U32(c, p); /* mark */
    mesh = Conv_U32(c, p + 4);
    nviz = Conv_U32(c, p + 8);
    viz = Conv_U32(c, p + 0x0C);
    Conv_U32(c, p + 0x10); /* unk10: 0 em todas, relocado e sem leitor */
    nreg = Conv_U32(c, p + 0x14);
    regs = Conv_U32(c, p + 0x18);
    Conv_U32s(c, p + 0x1C, 9); /* sem leitor, center, half */
    /* mesh 0 apontaria para o cabeçalho (§10, item 1): zona que o jogo não consulta, não se segue */
    if (mesh != 0) {
        colisao(c, e, mesh);
    }
    if (POSITIVO(nviz)) {
        l = em1(c, e, viz, nviz, 4, "vizinhas da zona");
        if (!ja(c, l, 4, "vizinhas da zona")) {
            Conv_U32s(c, l, nviz);
        }
    }
    if (POSITIVO(nreg)) {
        l = em1(c, e, regs, nreg, 0x10, "registros da zona");
        if (ja(c, l, 4, "registros da zona")) {
            return;
        }
        for (i = 0; i < nreg && !c->erro; i++) {
            conv_ofs r = l + i * 0x10;
            Conv_U32s(c, r, 2); /* broken, obj */
            mesh = Conv_U32(c, r + 8);
            Conv_U32(c, r + 0x0C);
            if (mesh == 0) {
                Conv_Erro(c, "registro de zona em 0x%lX: malha de colisão de deslocamento 0", (unsigned long)r);
                return;
            }
            colisao(c, e, mesh);
        }
    }
}

/* §4.10: objeto destrutível (StgObj, 0x50) e a lista de pedaços */
static void objeto(ConvCtx *c, const Ent1 *e, conv_ofs p) {
    conv_u32 partes[6], n, pedacos, i;
    conv_ofs l;
    Conv_U32s(c, p, 3); /* state, unk4, hp */
    for (i = 0; i < 6; i++) {
        partes[i] = Conv_U32(c, p + 0x0C + i * 4);
    }
    n = Conv_U32(c, p + 0x24);
    pedacos = Conv_U32(c, p + 0x28);
    Conv_U32s(c, p + 0x2C, 9); /* animEnd, frame, type, parent, unk3C, hitPos */
    /* parte 0 apontaria para o cabeçalho (§10, item 1): não se segue */
    for (i = 0; i < 6 && !c->erro; i++) {
        if (partes[i] != 0) {
            parte(c, e, partes[i]);
        }
    }
    if (n == 0) {
        return;
    }
    l = em1(c, e, pedacos, n, 4, "pedaços do objeto");
    if (ja(c, l, 4, "pedaços do objeto")) {
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        parte(c, e, Conv_U32(c, l + i * 4));
    }
}

static void entrada1(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Ent1 e;
    conv_u32 h[24], i, j;
    conv_ofs p, l;
    e.ini = a;
    e.fim = b;
    e.timers = a;
    e.ntimers = 0;
    if (b - a < 0x60) {
        Conv_Erro(c, "entrada 1 em 0x%lX: menor que o cabeçalho", (unsigned long)a);
        return;
    }
    /* §4.1: cabeçalho. A palavra 0 (unk0, sem leitor próprio) vai como dois u16: objetos com parte de
       deslocamento 0 apontam para o cabeçalho, e o jogo mexe no u16 em +0x02 (§10, item 1) */
    Conv_U16s(c, a, 2);
    h[0] = 0;
    for (i = 1; i < 24; i++) {
        h[i] = Conv_U32(c, a + i * 4);
    }
    if (c->erro) {
        return;
    }
    /* §4.4: temporizadores e listas de quadros */
    if (h[4] != 0) {
        e.timers = em1(c, &e, h[5], h[4], 0x10, "temporizadores");
        e.ntimers = h[4];
        for (i = 0; i < h[4] && !c->erro; i++) {
            conv_u32 n, q;
            p = e.timers + i * 0x10;
            Conv_U16(c, p); /* flags */
            n = Conv_U16(c, p + 2);
            q = Conv_U32(c, p + 4);
            Conv_U32s(c, p + 8, 2); /* time, period */
            if (n != 0) {
                l = em1(c, &e, q, n, 4, "quadros do temporizador");
                if (!ja(c, l, 4, "quadros do temporizador")) {
                    Conv_U32s(c, l, n);
                }
            }
        }
        Conv_Onde(c, "temporizadores", 0);
    }
    if (c->erro) {
        return;
    }
    /* §4.3: nós de transformação e chaves */
    if (h[12] != 0) {
        conv_ofs nos = em1(c, &e, h[13], h[12], 0x50, "nós");
        for (i = 0; i < h[12] && !c->erro; i++) {
            conv_u32 n, k;
            p = nos + i * 0x50;
            Conv_U32s(c, p, 16); /* mtx */
            n = Conv_U32(c, p + 0x40);
            k = Conv_U32(c, p + 0x44);
            Conv_U32s(c, p + 0x48, 2); /* endFrame, body */
            if (POSITIVO(n)) {
                l = em1(c, &e, k, n, 0x20, "chaves do nó");
                if (!ja(c, l, 4, "chaves do nó")) {
                    Conv_U32s(c, l, (conv_ofs)n * 8);
                }
            }
        }
        Conv_Onde(c, "nós", 0);
    }
    if (c->erro) {
        return;
    }
    /* §4.5: grupos, materiais, malhas, cadeias */
    if (h[6] != 0) {
        conv_ofs g = em1(c, &e, h[7], h[6], 8, "grupos");
        for (i = 0; i < h[6] && !c->erro; i++) {
            conv_u32 n, m;
            p = g + i * 8;
            Conv_U16(c, p); /* unk0 */
            n = Conv_U16(c, p + 2);
            m = Conv_U32(c, p + 4);
            if (n != 0) {
                l = em1(c, &e, m, n, 0x10, "materiais");
                for (j = 0; j < n && !c->erro; j++) {
                    material(c, &e, l + j * 0x10);
                }
            }
        }
        Conv_Onde(c, "grupos", 0);
    }
    if (c->erro) {
        return;
    }
    /* §4.6: tabela de visibilidade (bytes, reescrita a cada vista) */
    if (POSITIVO(h[10])) {
        Conv_Bytes(c, em1(c, &e, h[11], h[10], 1, "tabela de visibilidade"), h[10]);
    }
    /* §4.6: listas de partes (vetores de partes contíguas) */
    if (h[8] != 0) {
        conv_ofs lp = em1(c, &e, h[9], h[8], 8, "listas de partes");
        for (i = 0; i < h[8] && !c->erro; i++) {
            conv_u32 n, it;
            p = lp + i * 8;
            n = Conv_U32(c, p);
            it = Conv_U32(c, p + 4);
            if (n != 0) {
                l = em1(c, &e, it, n, 0x20, "partes da lista");
                for (j = 0; j < n && !c->erro; j++) {
                    parte(c, &e, (conv_u32)((l - a) / 4 + j * 8));
                }
            }
        }
        Conv_Onde(c, "listas de partes", 0);
    }
    if (c->erro) {
        return;
    }
    /* §4.7: árvore de recorte */
    if (h[20] == 0) {
        Conv_Erro(c, "árvore de recorte de deslocamento 0");
    }
    celula(c, &e, h[20], 0);
    Conv_Onde(c, "árvore", 0);
    if (c->erro) {
        return;
    }
    /* §4.8, §4.9: zonas e malhas de colisão */
    if (h[14] != 0) {
        conv_ofs z = em1(c, &e, h[15], h[14], 0x40, "zonas");
        for (i = 0; i < h[14] && !c->erro; i++) {
            zona(c, &e, z + i * 0x40);
            Conv_Onde(c, "zona %lu", (unsigned long)i);
        }
    }
    if (c->erro) {
        return;
    }
    /* §4.10: objetos e limites */
    if (h[17] != 0) {
        conv_ofs o = em1(c, &e, h[18], h[17], 0x50, "objetos");
        for (i = 0; i < h[17] && !c->erro; i++) {
            objeto(c, &e, o + i * 0x50);
            Conv_Onde(c, "objeto %lu", (unsigned long)i);
        }
    }
    if (c->erro) {
        return;
    }
    if (h[16] == 0) {
        Conv_Erro(c, "limites de deslocamento 0");
    }
    Conv_U32s(c, em1(c, &e, h[16], 1, 0xC, "limites"), 3);
    /* §4.11: o arquivo de textura, com os deslocamentos a partir do início da entrada 1. Por último: o
       Conv_Zeros dele vai até o fim da entrada */
    p = em1(c, &e, h[21], 1, 0x14, "texturas do cenário");
    if (!c->erro) {
        Conv_TexFileEm(c, a, p, b, 0);
        Conv_Onde(c, "texturas", 0);
    }
    if (c->erro) {
        return;
    }
    /* §4.12: todo byte que nenhuma parte cobre é zero */
    Conv_Zeros(c, a, b, "entrada 1");
}

/* ------------------------------------------------------------------ entrada 2 (§5) */

/* tamanho de registro de cada uma das 20 listas (sizeof da struct do leitor) */
static const conv_u8 sTamLista[20] = {
    0x40, 0x10, 0x20, 0x10, 0x20, 0x20, 0x20, 0x10, 0x0C, 0x0C,
    0x20, 0x18, 0x18, 0x04, 0x0C, 0x08, 0x04, 0x08, 0x04, 0x18,
};

static void registro2(ConvCtx *c, conv_u32 k, conv_ofs r) {
    switch (k) {
    case 0: Conv_U32s(c, r, 16); break;                  /* objDefs (0x10..0x1B sem leitor: palavras) */
    case 1: Conv_U32s(c, r, 4); break;                   /* env */
    case 2: Conv_U32s(c, r, 8); break;                   /* light */
    case 3: Conv_U32s(c, r, 4); break;                   /* points */
    case 4: case 5: case 6: Conv_U32s(c, r, 8); break;   /* starts, altStarts, places */
    case 7: Conv_U32(c, r); Conv_Bytes(c, r + 4, 10); break;            /* StgDepthTintParam */
    case 8: Conv_U32s(c, r, 2); Conv_Bytes(c, r + 8, 4); break;         /* StgGlareParam */
    case 9: Conv_U32(c, r); Conv_Bytes(c, r + 4, 6); break;             /* StgFogParams */
    case 10: Conv_U32s(c, r, 5); Conv_U16(c, r + 0x14); Conv_Bytes(c, r + 0x16, 9); break; /* StgSurfRec */
    case 11: Conv_U32s(c, r, 5); Conv_U16s(c, r + 0x14, 2); break;      /* StgWeatherRec */
    case 12: /* EftGndDustStage */
        Conv_U32(c, r);
        Conv_Bytes(c, r + 4, 8);
        Conv_U32s(c, r + 0x0C, 2);
        Conv_U16(c, r + 0x14);
        Conv_Bytes(c, r + 0x16, 2);
        break;
    case 13: case 18: Conv_U32(c, r); break;             /* flags (list70, list98) */
    case 14: Conv_Bytes(c, r, 12); break;                /* cores de luz */
    case 15: Conv_Bytes(c, r, 8); break;                 /* cores do escurecimento */
    case 16: Conv_Bytes(c, r, 4); break;                 /* luz ambiente */
    case 17: Conv_U32(c, r); Conv_Bytes(c, r + 4, 4); break;            /* GfxWaterStage */
    default: Conv_U32(c, r); Conv_Bytes(c, r + 4, 16); Conv_U32(c, r + 0x14); break; /* StgHazeParam */
    }
}

/* §5: StgDataA, cabeçalho de 0xA8 (unk0, unk4, 20 pares contagem / deslocamento em bytes a partir de a) */
static void entrada2(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n[20], o[20], k, i;
    if (b - a < 0xA8) {
        Conv_Erro(c, "entrada 2 em 0x%lX: menor que o cabeçalho", (unsigned long)a);
        return;
    }
    Conv_U32s(c, a, 2);
    for (k = 0; k < 20; k++) {
        n[k] = Conv_U32(c, a + 8 + k * 8);
        o[k] = Conv_U32(c, a + 12 + k * 8);
    }
    for (k = 0; k < 20 && !c->erro; k++) {
        if (n[k] == 0) {
            continue;
        }
        if (o[k] < 0xA8 || o[k] > b - a || !cabem(c, a + o[k], n[k], sTamLista[k], b, "lista da entrada 2")) {
            Conv_Erro(c, "entrada 2: lista do par 0x%lX com %lu registros em +0x%lX", (unsigned long)(8 + k * 8),
                      (unsigned long)n[k], (unsigned long)o[k]);
            return;
        }
        for (i = 0; i < n[k] && !c->erro; i++) {
            registro2(c, k, a + o[k] + i * sTamLista[k]);
        }
    }
    Conv_Zeros(c, a, b, "entrada 2");
}

/* ------------------------------------------------------------------ entradas de efeito (§7) */

/* §7.2: entrada 7, quatro s32 e EftMeshData (deslocamentos em bytes a partir do EftMeshData) */
static void entrada7(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs d = a + 0x10;
    conv_u32 ng, tris, vtx, g, nt = 0, nv = 0, i, k;
    if (b - a < 0x20) {
        Conv_Erro(c, "entrada 7 em 0x%lX: curta", (unsigned long)a);
        return;
    }
    Conv_U32s(c, a, 4); /* tamanho, unk2B8, velocidade, ângulo */
    Conv_U32(c, d);     /* unk0 */
    ng = Conv_U32(c, d + 4);
    tris = Conv_U32(c, d + 8);
    vtx = Conv_U32(c, d + 12);
    if (c->erro || !cabem(c, d + 0x10, ng, 0x10, b, "grupos da malha rolante") || tris > b - d || vtx > b - d) {
        Conv_Erro(c, "entrada 7: %lu grupos, triângulos em +0x%lX, vértices em +0x%lX", (unsigned long)ng,
                  (unsigned long)tris, (unsigned long)vtx);
        return;
    }
    for (g = 0; g < ng; g++) {
        conv_u32 num = Conv_U32(c, d + 0x10 + g * 16), primeiro;
        Conv_U32(c, d + 0x14 + g * 16);
        primeiro = Conv_U32(c, d + 0x18 + g * 16);
        Conv_U32(c, d + 0x1C + g * 16);
        if (num >= 0x10000000u || primeiro >= 0x10000000u) {
            Conv_Erro(c, "entrada 7: grupo %lu com %lu triângulos a partir de %lu", (unsigned long)g,
                      (unsigned long)num, (unsigned long)primeiro);
            return;
        }
        if (primeiro + num > nt) {
            nt = primeiro + num; /* a contagem é o maior firstTri + numTris */
        }
    }
    if (!cabem(c, d + tris, nt, 0x10, b, "triângulos da malha rolante")) {
        return;
    }
    for (i = 0; i < nt && !c->erro; i++) {
        for (k = 0; k < 3; k++) {
            conv_u32 v = Conv_U32(c, d + tris + i * 16 + k * 4);
            if (v >= 0x10000000u) {
                Conv_Erro(c, "entrada 7: triângulo %lu com vértice %lu", (unsigned long)i, (unsigned long)v);
                return;
            }
            if (v + 1 > nv) {
                nv = v + 1; /* o maior índice usado + 1 */
            }
        }
        Conv_U32(c, d + tris + i * 16 + 12); /* tex */
    }
    if (!cabem(c, d + vtx, nv, 0x30, b, "vértices da malha rolante")) {
        return;
    }
    Conv_U32s(c, d + vtx, (conv_ofs)nv * 12);
    Conv_Zeros(c, a, b, "entrada 7");
}

/* §7.3: entrada 13, superfície refletora (EftSurfParam + EftSurfFile; deslocamentos em bytes a partir do
   EftSurfFile) */
static void entrada13(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs f = a + 0x40;
    conv_u32 versao, nbloco, bloco, ntex, texs, nmalhas, malhas, i;
    if (b - a < 0x60) {
        Conv_Erro(c, "entrada 13 em 0x%lX: curta", (unsigned long)a);
        return;
    }
    Conv_U32s(c, a, 16);   /* EftSurfParam */
    Conv_Bytes(c, f, 4);   /* assinatura (sem leitor) */
    versao = Conv_U32(c, f + 4);
    nbloco = Conv_U32(c, f + 8);
    bloco = Conv_U32(c, f + 0x0C);
    ntex = Conv_U32(c, f + 0x10);
    texs = Conv_U32(c, f + 0x14);
    nmalhas = Conv_U32(c, f + 0x18);
    malhas = Conv_U32(c, f + 0x1C);
    if (c->erro) {
        return;
    }
    if (versao != 4) {
        Conv_Erro(c, "entrada 13: version %lu (o código exige 4)", (unsigned long)versao);
        return;
    }
    /* bloco unkC: só relocado, ninguém o lê; u32 por hipótese (§7.3, §11) */
    if (POSITIVO(nbloco)) {
        if (bloco > b - f || !cabem(c, f + bloco, nbloco, 0x100, b, "bloco unkC da entrada 13")) {
            Conv_Erro(c, "entrada 13: bloco unkC em +0x%lX", (unsigned long)bloco);
            return;
        }
        Conv_U32s(c, f + bloco, (conv_ofs)nbloco * 0x40);
    }
    if (ntex != 0) {
        if (texs > b - f || !cabem(c, f + texs, ntex, 0x40, b, "texturas da entrada 13")) {
            Conv_Erro(c, "entrada 13: texturas em +0x%lX", (unsigned long)texs);
            return;
        }
        for (i = 0; i < ntex && !c->erro; i++) {
            conv_ofs t = f + texs + i * 0x40;
            conv_u32 pix = Conv_U32(c, t), clut = Conv_U32(c, t + 4);
            conv_u32 ptam = Conv_U32(c, t + 8), ctam = Conv_U32(c, t + 12);
            Conv_U32s(c, t + 0x10, 6);
            Conv_U64(c, t + 0x28); /* tex0 */
            Conv_U64(c, t + 0x30); /* unk30, lido inteiro */
            Conv_U32s(c, t + 0x38, 2);
            if (pix != 0) {
                Conv_PacoteImagem(c, f + pix, ptam);
            }
            if (clut != 0) {
                Conv_PacoteImagem(c, f + clut, ctam);
            }
        }
    }
    if (nmalhas != 0) {
        if (malhas > b - f || !cabem(c, f + malhas, nmalhas, 0x60, b, "superfícies da entrada 13")) {
            Conv_Erro(c, "entrada 13: superfícies em +0x%lX", (unsigned long)malhas);
            return;
        }
        for (i = 0; i < nmalhas && !c->erro; i++) {
            conv_ofs m = f + malhas + i * 0x60;
            conv_u32 nt, tri;
            Conv_U32(c, m); /* tex */
            nt = Conv_U32(c, m + 4);
            tri = Conv_U32(c, m + 8);
            Conv_U32s(c, m + 0x0C, 0x15);
            if (POSITIVO(nt)) {
                if (tri > b - f || !cabem(c, f + tri, nt, 0x90, b, "triângulos da entrada 13")) {
                    Conv_Erro(c, "entrada 13: superfície %lu com triângulos em +0x%lX", (unsigned long)i,
                              (unsigned long)tri);
                    return;
                }
                if (!ja(c, f + tri, 4, "triângulos da entrada 13")) {
                    Conv_U32s(c, f + tri, (conv_ofs)nt * 36);
                }
            }
        }
    }
    Conv_Zeros(c, a, b, "entrada 13");
}

/* ------------------------------------------------------------------ entrada 9 (§8) */

/* fim do bloco que começa em x: o menor deslocamento da tabela maior que x, ou o fim da entrada */
static conv_ofs fim_bloco(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, conv_ofs x) {
    conv_ofs fim = b;
    conv_u32 i;
    for (i = 0; i < n; i++) {
        conv_ofs y = a + Conv_Ler32(c, a + 0x10 + i * 4);
        if (y > x && y < fim) {
            fim = y;
        }
    }
    return fim;
}

/* §8.2: contêiner de nós (cabeçalho de 0x10 e Vu1Node encadeados) e o bloco até as trilhas; devolve os nós */
static conv_u32 nos_vu1(ConvCtx *c, conv_ofs nd, conv_ofs tr) {
    conv_u32 prim, n = 0;
    conv_ofs o;
    if (nd + 0x10 > tr) {
        Conv_Erro(c, "nós em 0x%lX: contêiner curto", (unsigned long)nd);
        return 0;
    }
    prim = Conv_U32(c, nd);
    Conv_U32s(c, nd + 4, 3);
    if (prim < 0x10 || prim % 4 != 0 || prim > tr - nd) {
        Conv_Erro(c, "nós em 0x%lX: primeiro nó em +0x%lX", (unsigned long)nd, (unsigned long)prim);
        return 0;
    }
    for (o = nd + prim; !c->erro;) {
        conv_u32 tam, ultimo, malha;
        if (o + 0xF0 > tr) {
            Conv_Erro(c, "nó em 0x%lX passa das trilhas", (unsigned long)o);
            return 0;
        }
        n++;
        tam = Conv_U32(c, o);
        Conv_U32(c, o + 4); /* popCount */
        ultimo = Conv_U32(c, o + 8);
        malha = Conv_U32(c, o + 0x0C);
        Conv_U32s(c, o + 0x10, (0xF0 - 0x10) / 4); /* f32 */
        if (tam < 0xF0 || tam % 4 != 0 || tam > tr - o) {
            Conv_Erro(c, "nó em 0x%lX: size 0x%lX", (unsigned long)o, (unsigned long)tam);
            return 0;
        }
        /* com hasMesh, o pacote DMA/VIF em +0xF0 termina exatamente no fim do nó */
        if (malha != 0 && Conv_PacoteVif(c, o + 0xF0, o + tam) != o + tam && !c->erro) {
            Conv_Erro(c, "nó em 0x%lX: o pacote VIF não fecha com size 0x%lX", (unsigned long)o, (unsigned long)tam);
        }
        o += tam;
        if (ultimo != 0) {
            break;
        }
    }
    /* depois do último nó, até as trilhas: bloco que ninguém lê (u32 por hipótese, §8, §11) */
    if (!c->erro && (tr - o) % 4 != 0) {
        Conv_Erro(c, "nós em 0x%lX: o resto até as trilhas não é múltiplo de 4", (unsigned long)nd);
    }
    Conv_U32s(c, o, (tr - o) / 4);
    return n;
}

/* §8.3: trilhas (Vu1Track), uma por nó, em sequência */
static void trilhas(ConvCtx *c, conv_ofs t, conv_ofs fim, conv_u32 n) {
    conv_u32 i, k;
    for (i = 0; i < n && !c->erro; i++) {
        if (t + 0x10 > fim) {
            Conv_Erro(c, "trilha em 0x%lX passa do bloco", (unsigned long)t);
            return;
        }
        Conv_U32s(c, t, 2);  /* 0x00..0x07 sem leitor */
        Conv_U16(c, t + 8);  /* 0x08..0x09 sem leitor (zero) */
        k = Conv_U16(c, t + 0x0A); /* keyCount */
        Conv_U16s(c, t + 0x0C, 2); /* length (u16 de StgAnimTrack), zero */
        if (!cabem(c, t + 0x10, k, 0x20, fim, "chaves da trilha")) {
            return;
        }
        Conv_U32s(c, t + 0x10, (conv_ofs)k * 8);
        t += 0x10 + (conv_ofs)k * 0x20;
    }
}

/* §8: entrada 9, objetos animados (StgOfsTable + trincas TexFile, nós, trilhas; deslocamentos em bytes a partir
   da tabela) */
static void entrada9(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n, i, k;
    if (b - a < 0x10) {
        Conv_Erro(c, "entrada 9 em 0x%lX: curta", (unsigned long)a);
        return;
    }
    Conv_Bytes(c, a, 4); /* assinatura (sem leitor) */
    Conv_U32(c, a + 4);
    n = Conv_U32(c, a + 8);
    Conv_U32(c, a + 0x0C);
    if (c->erro || n % 3 != 0 || !cabem(c, a + 0x10, n, 4, b, "tabela da entrada 9")) {
        Conv_Erro(c, "entrada 9: count %lu", (unsigned long)n);
        return;
    }
    for (i = 0; i < n; i++) {
        conv_u32 x = Conv_Ler32(c, a + 0x10 + i * 4);
        if (x < 0x10 + n * 4 || x >= b - a) {
            Conv_Erro(c, "entrada 9: deslocamento %lu = 0x%lX fora da entrada", (unsigned long)i, (unsigned long)x);
            return;
        }
    }
    for (i = 0; i < n && !c->erro; i += 3) {
        conv_ofs nd = a + Conv_Ler32(c, a + 0x14 + i * 4);
        conv_ofs tr = a + Conv_Ler32(c, a + 0x18 + i * 4);
        if (tr <= nd) {
            Conv_Erro(c, "entrada 9: animação %lu com as trilhas antes dos nós", (unsigned long)(i / 3));
            return;
        }
        k = nos_vu1(c, nd, tr);
        trilhas(c, tr, fim_bloco(c, a, b, n, tr), k);
        Conv_Onde(c, "animação %lu", (unsigned long)(i / 3));
    }
    /* os TexFile (base no próprio TexFile, como 7.1) depois dos nós e trilhas: cada um vai até o bloco seguinte */
    for (i = 0; i < n && !c->erro; i += 3) {
        conv_ofs tx = a + Conv_Ler32(c, a + 0x10 + i * 4);
        Conv_TexFile(c, tx, fim_bloco(c, a, b, n, tx), 0);
        Conv_Onde(c, "texturas da animação %lu", (unsigned long)(i / 3));
    }
    Conv_U32s(c, a + 0x10, n); /* os deslocamentos, lidos acima sem trocar */
    Conv_Zeros(c, a, b, "entrada 9");
}

/* ------------------------------------------------------------------ navegação, MEF0, caminhos (§9) */

/* §9.1: entrada 16, StgNavData */
static void entrada16(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 versao, n, i;
    if (b - a < 0x10) {
        Conv_Erro(c, "entrada 16 em 0x%lX: curta", (unsigned long)a);
        return;
    }
    versao = Conv_U32(c, a);
    n = Conv_U32(c, a + 4);
    Conv_U32s(c, a + 8, 2); /* unk8, nodes (gravado) */
    if (c->erro || versao != 1 || !cabem(c, a + 0x10, n, 0x30, b, "nós de navegação")) {
        Conv_Erro(c, "entrada 16: version %lu, %lu nós", (unsigned long)versao, (unsigned long)n);
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        conv_ofs p = a + 0x10 + i * 0x30;
        Conv_U32s(c, p, 8);        /* pos, flags, obj, unk18 */
        Conv_U16s(c, p + 0x20, 8); /* link: s16 */
    }
    Conv_Zeros(c, a, b, "entrada 16");
}

/* §9.2: entrada 19, pontos de efeito MEF0 (deslocamentos em bytes a partir de a) */
static void entrada19(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 magic, n[3], o[3], k, i;
    if (b - a < 0x20) {
        Conv_Erro(c, "entrada 19 em 0x%lX: curta", (unsigned long)a);
        return;
    }
    magic = Conv_U32(c, a); /* comparado como u32 por BtlStage_BindMef */
    Conv_U32(c, a + 4);
    for (k = 0; k < 3; k++) {
        n[k] = Conv_U32(c, a + 8 + k * 8);
        o[k] = Conv_U32(c, a + 12 + k * 8);
    }
    if (!c->erro && magic != 0x3046454Du) {
        Conv_Erro(c, "entrada 19: magic 0x%08lX não é MEF0", (unsigned long)magic);
        return;
    }
    for (k = 0; k < 3 && !c->erro; k++) {
        if (n[k] == 0) {
            continue;
        }
        if (o[k] < 0x20 || o[k] > b - a || !cabem(c, a + o[k], n[k], 0x30, b, "lista da entrada 19")) {
            Conv_Erro(c, "entrada 19: lista %lu em +0x%lX", (unsigned long)k, (unsigned long)o[k]);
            return;
        }
        for (i = 0; i < n[k] && !c->erro; i++) {
            conv_ofs r = a + o[k] + i * 0x30;
            Conv_U32s(c, r, 6); /* f32 x6 */
            switch (k) {
            case 0: Conv_U16s(c, r + 0x18, 3); Conv_Bytes(c, r + 0x1E, 0x12); break; /* EftGeyserRec */
            case 1: Conv_U16s(c, r + 0x18, 2); Conv_Bytes(c, r + 0x1C, 0x14); break; /* EftSmokeSrc */
            default: Conv_U16(c, r + 0x18); Conv_Bytes(c, r + 0x1A, 0x16); break;    /* EftStageMarker */
            }
        }
    }
    Conv_Zeros(c, a, b, "entrada 19");
}

/* §9.3: entrada 24, StgPathTable (deslocamentos em bytes a partir de a) */
static void entrada24(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n, cam, i;
    if (b - a < 0x14) {
        Conv_Erro(c, "entrada 24 em 0x%lX: curta", (unsigned long)a);
        return;
    }
    Conv_Bytes(c, a, 4); /* assinatura (sem leitor) */
    Conv_U32(c, a + 4);
    n = Conv_U32(c, a + 8);
    cam = Conv_U32(c, a + 0x0C);
    Conv_U32(c, a + 0x10); /* relocated */
    if (c->erro) {
        return;
    }
    if (n != 0 && (cam > b - a || !cabem(c, a + cam, n, 0x10, b, "caminhos"))) {
        Conv_Erro(c, "entrada 24: %lu caminhos em +0x%lX", (unsigned long)n, (unsigned long)cam);
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        conv_ofs p = a + cam + i * 0x10;
        conv_u32 fim = Conv_U32(c, p), pts = Conv_U32(c, p + 4);
        Conv_U32s(c, p + 8, 2); /* first, unkC */
        if (!POSITIVO(fim)) {
            continue;
        }
        if (pts > b - a || !cabem(c, a + pts, fim, 0x30, b, "pontos do caminho")) {
            Conv_Erro(c, "entrada 24: caminho %lu com %lu pontos em +0x%lX", (unsigned long)i, (unsigned long)fim,
                      (unsigned long)pts);
            return;
        }
        if (!ja(c, a + pts, 4, "pontos do caminho")) {
            Conv_U32s(c, a + pts, (conv_ofs)fim * 12); /* f32 x, y, z e 0x24 bytes em palavras */
        }
    }
    Conv_Zeros(c, a, b, "entrada 24");
}

/* §7.4: entradas 22 (f32[4]) e 23 (EftBoundParam: f32 x14, s32 otZ) */
static void palavras(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 n, const char *oque) {
    if (b - a < n * 4) {
        Conv_Erro(c, "%s em 0x%lX: curta", oque, (unsigned long)a);
        return;
    }
    Conv_U32s(c, a, n);
    Conv_Zeros(c, a, b, oque);
}

/* ------------------------------------------------------------------ o arquivo (§2, §3) */

void Conv_Cenario(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[N_ENTRADAS + 2], n, i;
    n = Conv_Tabela(c, base, tam, ent, N_ENTRADAS);
    if (!c->erro && n != N_ENTRADAS) {
        Conv_Erro(c, "cenário com %lu entradas (esperado %d)", (unsigned long)n, N_ENTRADAS);
    }
    if (!c->erro && ent[1] != 0x80) {
        Conv_Erro(c, "cenário com a entrada 1 em 0x%lX (esperado 0x80)", (unsigned long)ent[1]);
    }
    if (c->erro) {
        return;
    }
    for (i = 1; i <= N_ENTRADAS && !c->erro; i++) {
        conv_ofs a = base + ent[i], b = base + ent[i + 1];
        if (a == b) {
            continue;
        }
        switch (i) {
        case 1: entrada1(c, a, b); break;
        case 2: entrada2(c, a, b); break;
        case 3: case 4: case 5: Conv_Camera(c, a, b); break; /* §6: DemoCamAnim */
        case 6: case 10: case 11: case 14: case 18: case 20: case 21: Conv_TexFile(c, a, b, 0); break; /* §7.1 */
        case 7: entrada7(c, a, b); break;
        case 9: entrada9(c, a, b); break;
        case 13: entrada13(c, a, b); break;
        case 16: entrada16(c, a, b); break;
        case 19: entrada19(c, a, b); break;
        case 22: palavras(c, a, b, 4, "entrada 22"); break;
        case 23: palavras(c, a, b, 15, "entrada 23"); break;
        case 24: entrada24(c, a, b); break;
        default: vazia(c, ent, i); break; /* 8, 12, 15, 17, 25: sem leitor (§11) */
        }
        Conv_Onde(c, "entrada %lu", (unsigned long)i);
    }
}
