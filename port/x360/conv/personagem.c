/*
 * Arquivos do slot de um lutador: file[0] (modelo, texturas, parâmetros, câmeras, texto, lábio) e file[1]
 * (animações em BPE). Especificação: docs/x360/formatos/personagem.md (as seções citadas abaixo).
 */
#include "conv.h"

/* ------------------------------------------------------------------ peças comuns */

/* §2: tabela de deslocamentos. Devolve N; ent[n] = início da entrada n (n = 1..N+1, o último é o fim). */
static conv_u32 tabela(ConvCtx *c, conv_ofs base, conv_ofs tam, conv_u32 *ent, conv_u32 max) {
    conv_u32 n = Conv_Ler32(c, base), i, ini;
    if (c->erro) {
        return 0;
    }
    if (n == 0 || n > max) {
        Conv_Erro(c, "tabela: %lu entradas (máximo %lu)", (unsigned long)n, (unsigned long)max);
        return 0;
    }
    for (i = 1; i <= n + 1; i++) {
        ent[i] = Conv_Ler32(c, base + i * 4);
        if (!c->erro && (ent[i] > tam || (i > 1 && ent[i] < ent[i - 1]))) {
            Conv_Erro(c, "tabela: entrada %lu em 0x%lX fora de ordem ou do arquivo", (unsigned long)i, (unsigned long)ent[i]);
        }
    }
    if (!c->erro && ent[n + 1] != tam) {
        Conv_Erro(c, "tabela: fim 0x%lX, arquivo 0x%lX", (unsigned long)ent[n + 1], (unsigned long)tam);
    }
    ini = ent[1];
    if (!c->erro && ini < (n + 2) * 4) {
        Conv_Erro(c, "tabela: entrada 1 em 0x%lX, dentro da tabela", (unsigned long)ini);
    }
    if (!c->erro) {
        Conv_U32s(c, base, ini / 4); /* a contagem, os deslocamentos, o fim e o enchimento: tudo u32 */
    }
    return c->erro ? 0 : n;
}

/* §4.2.1: pacote DMA/VIF (etiqueta ret, fluxo, etiqueta end de 16 bytes): toda palavra u32. Devolve o fim. */
static conv_ofs pacote_vif(ConvCtx *c, conv_ofs ofs, conv_ofs fim) {
    conv_u32 tag = Conv_Ler32(c, ofs), qwc;
    conv_ofs fimpac;
    if (c->erro) {
        return ofs;
    }
    qwc = tag & 0xFFFF;
    if (((tag >> 28) & 7) != 6) {
        Conv_Erro(c, "pacote VIF em 0x%lX: etiqueta DMA 0x%08lX não é ret", (unsigned long)ofs, (unsigned long)tag);
        return ofs;
    }
    fimpac = ofs + 16 + (conv_ofs)qwc * 16;
    if (fimpac + 16 > fim || Conv_Ler32(c, fimpac) != 0x70000000u) {
        Conv_Erro(c, "pacote VIF em 0x%lX: sem a etiqueta end em 0x%lX", (unsigned long)ofs, (unsigned long)fimpac);
        return ofs;
    }
    Conv_U32s(c, ofs, (fimpac + 16 - ofs) / 4);
    return fimpac + 16;
}

/* §4.4: pacote de pixels ou de paleta: cabeçalho de 0x60 e fim de 0x20 em palavras u32 (os valores de 64 bits
   ficam como duas palavras, a baixa primeiro, como no fluxo VIF: decisão 12), a imagem em bytes. */
static void pacote_imagem(ConvCtx *c, conv_ofs ofs, conv_ofs tam) {
    if (Conv_Marca(c, ofs) != CONV_LIVRE) {
        return; /* outro registro aponta para o mesmo pacote */
    }
    if (tam < 0x80 || tam % 16 != 0 || !Conv_Cabe(c, ofs, tam, "pacote de imagem")) {
        Conv_Erro(c, "pacote de imagem em 0x%lX: tamanho 0x%lX", (unsigned long)ofs, (unsigned long)tam);
        return;
    }
    if ((Conv_Ler32(c, ofs + 12) & 0xFFFF) != tam / 16 - 1) {
        Conv_Erro(c, "pacote de imagem em 0x%lX: DIRECT não cobre o pacote", (unsigned long)ofs);
        return;
    }
    Conv_U32s(c, ofs, 0x60 / 4);
    Conv_Bytes(c, ofs + 0x60, tam - 0x80);
    Conv_U32s(c, ofs + tam - 0x20, 0x20 / 4);
}

/* §4.4: arquivo de textura (TexFile + TexEntry[count] + pacotes) */
static void texfile(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n = Conv_U32(c, a), ent = Conv_U32(c, a + 4) * 4, i;
    Conv_U32s(c, a + 8, 3); /* 0x08 não determinado (2 x u32 provável), 0x10 ponteiro gravado na carga */
    if (c->erro || ent < 0x14 || n > 256 || a + ent + n * 0x40 > b) {
        Conv_Erro(c, "texturas em 0x%lX: %lu registros em +0x%lX", (unsigned long)a, (unsigned long)n, (unsigned long)ent);
        return;
    }
    for (i = 0; i < n && !c->erro; i++) {
        conv_ofs e = a + ent + i * 0x40;
        conv_u32 pix = Conv_U32(c, e), clut = Conv_U32(c, e + 4);
        conv_u32 ptam = Conv_U32(c, e + 8), ctam = Conv_U32(c, e + 12);
        Conv_U32s(c, e + 0x10, 6);
        Conv_Bytes(c, e + 0x28, 8);
        Conv_U64(c, e + 0x30); /* tex0: campo u64 de struct */
        Conv_U32s(c, e + 0x38, 2);
        if (pix != 0) {
            pacote_imagem(c, a + pix * 4, ptam);
        }
        if (clut != 0) {
            pacote_imagem(c, a + clut * 4, ctam);
        }
    }
    Conv_Zeros(c, a, b, "texturas");
}

/* o que sobrou sem marca em [a, b) fica como está (bytes), sem exigir zero: só para trechos que o código nunca lê */
static void resto_bytes(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs p;
    for (p = a; p < b && !c->erro; p++) {
        if (Conv_Marca(c, p) == CONV_LIVRE) {
            Conv_Bytes(c, p, 1);
        }
    }
}

/* ------------------------------------------------------------------ file[0] */

/* §4.1, §4.2: o modelo (cabeçalho, malhas com pacotes VIF, costura) */
static void modelo(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    conv_u32 malhas, costura;
    Conv_Bytes(c, a, 0xC);
    Conv_U32s(c, a + 0x0C, 7);  /* flags, fadeNear, fadeFar, size[4] */
    Conv_U32(c, a + 0x28);      /* alphaRow */
    Conv_Bytes(c, a + 0x2C, 0x2C); /* alpha por material, 0x3C não determinado, índices de textura (u8) */
    Conv_U32s(c, a + 0x58, 5);  /* tbp, cbp, cbp2, tbp2, unk68 */
    malhas = Conv_U32(c, a + 0x6C);
    costura = Conv_U32(c, a + 0x70);
    if (c->erro || malhas < 0x74 || a + malhas >= b) {
        Conv_Erro(c, "modelo em 0x%lX: malhas em +0x%lX", (unsigned long)a, (unsigned long)malhas);
        return;
    }
    Conv_Bytes(c, a + 0x74, malhas - 0x74);
    for (r = a + malhas; !c->erro;) {
        conv_u32 prox = Conv_U32(c, r), ultimo, ligado;
        Conv_U16(c, r + 4);
        ultimo = Conv_U16(c, r + 6);
        ligado = Conv_U16(c, r + 8);
        Conv_U16s(c, r + 0x0A, 3);
        Conv_U32s(c, r + 0x10, 12);
        if (ligado) {
            Conv_U32s(c, r + 0x40, 8);
            if (pacote_vif(c, r + 0x60, b) != r + prox) {
                Conv_Erro(c, "malha em 0x%lX: next 0x%lX não fecha com o pacote", (unsigned long)r, (unsigned long)prox);
            }
        } else if (prox != 0x40 && !ultimo) {
            Conv_Erro(c, "malha sem desenho em 0x%lX com next 0x%lX", (unsigned long)r, (unsigned long)prox);
        }
        if (ultimo) {
            /* depois do último registro, até o fim da entrada: alinhamento que o jogo não lê, com alguns bytes
               não nulos nos arquivos do disco */
            resto_bytes(c, r + 0x40, b);
            break;
        }
        r += prox;
        if (r + 0x40 > b) {
            Conv_Erro(c, "malhas passam do fim do modelo em 0x%lX", (unsigned long)r);
        }
    }
    if (costura != 0 && !c->erro) {
        /* §4.2.2: ObjSeamSec, ObjSeamVtx[vtxCount] (0x70), ObjSeamTri[triCount] (0x20); deslocamentos a partir de a */
        conv_ofs s = a + costura;
        conv_u32 nv = Conv_U32(c, s), nt = Conv_U32(c, s + 4), ov = Conv_U32(c, s + 8), ot = Conv_U32(c, s + 12), i;
        if (!c->erro && (a + ov + nv * 0x70 > b || a + ot + nt * 0x20 > b)) {
            Conv_Erro(c, "costura em 0x%lX fora do modelo", (unsigned long)s);
        }
        for (i = 0; i < nv && !c->erro; i++) {
            Conv_U32s(c, a + ov + i * 0x70, 0x70 / 4); /* f32, s32 e quadwords de 4 x 32 bits */
        }
        for (i = 0; i < nt && !c->erro; i++) {
            conv_ofs t = a + ot + i * 0x20;
            Conv_U32s(c, t, 4);
            Conv_U64(c, t + 0x10); /* TEX0 lido como u64 inteiro aqui (gfxm_d_c.c) */
            Conv_U64(c, t + 0x18);
        }
    }
    Conv_Zeros(c, a, b, "modelo");
}

/* §4.3: volumes do corpo */
static void corpo(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs p;
    Conv_U32s(c, a, 2);
    Conv_Bytes(c, a + 8, 8); /* não determinado */
    for (p = a + 0x10; !c->erro; p += 0x1D0) {
        conv_u32 fl;
        if (p + 0x1D0 > b) {
            Conv_Erro(c, "corpo em 0x%lX: parte em 0x%lX passa do fim", (unsigned long)a, (unsigned long)p);
            break;
        }
        fl = Conv_U32(c, p);
        Conv_U32s(c, p + 4, 12);          /* node, 2 ponteiros gravados, offset[4], center[4], raio */
        Conv_Bytes(c, p + 0x34, 12);      /* não determinado */
        Conv_U32s(c, p + 0x40, 0x190 / 4); /* caixa orientada: tudo f32 */
        if (fl & 1) {
            break;
        }
    }
    Conv_Zeros(c, a, b, "corpo");
}

/* §5.2 */
static void param(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_U16(c, a);
    Conv_Bytes(c, a + 2, 2);
    Conv_U32s(c, a + 0x04, 21);
    Conv_Bytes(c, a + 0x58, 4);
    Conv_U32s(c, a + 0x5C, 5);
    Conv_U16s(c, a + 0x70, 2);
    Conv_U32s(c, a + 0x74, 3);
    Conv_U16s(c, a + 0x80, 2);
    Conv_Bytes(c, a + 0x84, 0x44);
    Conv_U32(c, a + 0xC8);
    Conv_Zeros(c, a, b, "parâmetros");
}

/* §5.3: tamanho / 0x30 registros */
static void golpes(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    for (r = a; r + 0x30 <= b && !c->erro; r += 0x30) {
        Conv_U32(c, r);
        Conv_U16s(c, r + 4, 7);
        Conv_Bytes(c, r + 0x12, 0x1E);
    }
    Conv_Zeros(c, a, b, "golpes");
}

/* §5.4: 13 registros de 0x34 */
static void rajadas(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    if (a + 13 * 0x34 > b) {
        Conv_Erro(c, "rajadas em 0x%lX: entrada curta", (unsigned long)a);
        return;
    }
    for (r = a; r < a + 13 * 0x34 && !c->erro; r += 0x34) {
        Conv_U16(c, r);
        Conv_Bytes(c, r + 2, 2);
        Conv_U32s(c, r + 4, 8);
        Conv_Bytes(c, r + 0x24, 4);
        Conv_U32s(c, r + 0x28, 2);
        Conv_Bytes(c, r + 0x30, 4);
    }
    Conv_Zeros(c, a, b, "rajadas");
}

/* §5.7 */
static void cadeias(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_Bytes(c, a + 0x000, 0x10);
    Conv_U32s(c, a + 0x010, 48);
    Conv_Bytes(c, a + 0x0D0, 0x10);
    Conv_U16s(c, a + 0x0E0, 16);
    Conv_Bytes(c, a + 0x100, 0x10);
    Conv_U32s(c, a + 0x110, 32);
    Conv_Bytes(c, a + 0x190, 0x10);
    Conv_U16s(c, a + 0x1A0, 16);
    Conv_Bytes(c, a + 0x1C0, 0x10);
    Conv_U32s(c, a + 0x1D0, 32);
    Conv_Bytes(c, a + 0x250, 0x10);
    Conv_U32s(c, a + 0x260, 32);
    Conv_Bytes(c, a + 0x2E0, 0x20);
    Conv_U32s(c, a + 0x300, 16);
    Conv_Bytes(c, a + 0x340, 0x15);
    Conv_Zeros(c, a, b, "cadeias");
}

/* §5.8 */
static void tecnicas(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_U32s(c, a + 0x000, 6);
    Conv_U16s(c, a + 0x018, 6);
    Conv_U32s(c, a + 0x024, 27);
    Conv_Bytes(c, a + 0x090, 0x44);
    Conv_U32s(c, a + 0x0D4, 24);
    Conv_Bytes(c, a + 0x134, 0x38);
    Conv_U32s(c, a + 0x16C, 39);
    Conv_U16s(c, a + 0x208, 6);
    Conv_U32s(c, a + 0x214, 3);
    Conv_Bytes(c, a + 0x220, 0x0C);
    Conv_U32s(c, a + 0x22C, 6);
    Conv_Zeros(c, a, b, "técnicas");
}

/* §5.9 */
static void habilidades(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_U32s(c, a + 0x00, 4);
    Conv_U16s(c, a + 0x10, 4);
    Conv_U32s(c, a + 0x18, 8);
    Conv_Bytes(c, a + 0x38, 0x3C);
    Conv_U32s(c, a + 0x74, 8);
    Conv_Bytes(c, a + 0x94, 0x0C);
    Conv_U32s(c, a + 0xA0, 10);
    Conv_Zeros(c, a, b, "habilidades");
}

/* §5.11: tamanho / 0x24 registros */
static void cortes(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs r;
    for (r = a; r + 0x24 <= b && !c->erro; r += 0x24) {
        Conv_Bytes(c, r, 4);
        Conv_U32s(c, r + 4, 7);
        Conv_U16s(c, r + 0x20, 2);
    }
    Conv_Zeros(c, a, b, "cortes de câmera");
}

/* §5.12, §6.5: animação de câmera (deslocamentos a partir de a) */
static void camera(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 canais, n, pose, i;
    Conv_Bytes(c, a, 4); /* assinatura */
    Conv_U32(c, a + 4);
    canais = Conv_U32(c, a + 8);
    n = Conv_U32(c, a + 12);
    Conv_U32s(c, a + 0x10, 2);
    pose = Conv_U32(c, a + 0x18);
    Conv_U32s(c, a + 0x1C, 2);
    if (c->erro || n > 64 || a + canais + n * 0x10 > b || a + pose + 0x20 > b) {
        Conv_Erro(c, "câmera em 0x%lX: %lu canais em +0x%lX", (unsigned long)a, (unsigned long)n, (unsigned long)canais);
        return;
    }
    Conv_U32s(c, a + pose, 8);
    for (i = 0; i < n && !c->erro; i++) {
        conv_ofs ch = a + canais + i * 0x10;
        conv_u32 chaves, k;
        Conv_U32(c, ch);
        chaves = Conv_U32(c, ch + 4);
        k = Conv_U32(c, ch + 8);
        Conv_U32(c, ch + 12);
        if (k > 4096 || a + chaves + k * 0x20 > b) {
            Conv_Erro(c, "câmera em 0x%lX: canal %lu com %lu chaves", (unsigned long)a, (unsigned long)i, (unsigned long)k);
            break;
        }
        if (k != 0 && Conv_Marca(c, a + chaves) == CONV_LIVRE) {
            Conv_U32s(c, a + chaves, k * 8); /* flags, tempo, valor e 0x14 bytes de palavras não lidas */
        }
    }
    resto_bytes(c, a, b); /* o bloco unk1C (nunca lido; version 0) e o alinhamento ficam como estão */
}

/* §5.13: texto u16 */
static void texto(ConvCtx *c, conv_ofs a, conv_ofs b) {
    if ((b - a) % 2 != 0) {
        Conv_Erro(c, "texto em 0x%lX: tamanho ímpar", (unsigned long)a);
        return;
    }
    Conv_U16s(c, a, (b - a) / 2);
}

/* §6.6: faixa de lábio */
static void labio(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 n;
    Conv_Bytes(c, a, 8);
    n = Conv_U16(c, a + 8);
    Conv_Bytes(c, a + 0x0A, 6);
    if (!c->erro && a + 0x10 + n * 4 > b) {
        Conv_Erro(c, "lábio em 0x%lX: %lu chaves", (unsigned long)a, (unsigned long)n);
        return;
    }
    Conv_U16s(c, a + 0x10, n * 2);
    Conv_Zeros(c, a, b, "lábio");
}

/* §6.3: animação (no arquivo, entradas 29..36, ou descomprimida); deslocamentos em palavras a partir de a */
static void animacao(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 eventos, i;
    conv_u16 trilha[71];
    if (b - a < 0x94) {
        Conv_Erro(c, "animação em 0x%lX: menor que o cabeçalho", (unsigned long)a);
        return;
    }
    Conv_Bytes(c, a, 2);
    eventos = c->buf[a + 1];
    Conv_U16s(c, a + 2, 2);
    for (i = 0; i < 71; i++) {
        trilha[i] = Conv_U16(c, a + 6 + i * 2);
    }
    if (a + 0x94 + eventos * 0x10 > b) {
        Conv_Erro(c, "animação em 0x%lX: %lu eventos", (unsigned long)a, (unsigned long)eventos);
        return;
    }
    for (i = 0; i < eventos && !c->erro; i++) {
        conv_ofs e = a + 0x94 + i * 0x10;
        conv_u32 attr = Conv_U32(c, e), volume;
        Conv_U32(c, e + 4); /* o atributo de 64 bits é montado de duas leituras u32 */
        Conv_U16(c, e + 8);
        volume = Conv_U16(c, e + 10);
        Conv_U32(c, e + 12);
        if ((attr & 1) && Conv_Marca(c, a + volume * 4) == CONV_LIVRE) {
            if (a + volume * 4 + 0x20 > b) {
                Conv_Erro(c, "animação em 0x%lX: volume em +0x%lX", (unsigned long)a, (unsigned long)(volume * 4));
                return;
            }
            Conv_U32s(c, a + volume * 4, 8); /* f32; a última palavra não é lida */
        }
    }
    for (i = 0; i < 71 && !c->erro; i++) {
        conv_ofs t = a + (conv_ofs)trilha[i] * 4;
        conv_u32 fl, n;
        if (trilha[i] == 0 || Conv_Marca(c, t) != CONV_LIVRE) {
            continue;
        }
        fl = Conv_U16(c, t);
        n = Conv_U16(c, t + 2);
        if (fl & 1) {
            if (t + 4 + n * 10 > b) {
                Conv_Erro(c, "animação em 0x%lX: trilha de rotação em +0x%lX passa do fim", (unsigned long)a,
                          (unsigned long)(t - a));
                return;
            }
            Conv_U32s(c, t + 4, n * 2);       /* quaternions empacotados: lo, hi */
            Conv_U16s(c, t + 4 + n * 8, n);   /* quadros */
        } else {
            if (t + 4 + n * 0x18 > b) {
                Conv_Erro(c, "animação em 0x%lX: trilha completa em +0x%lX passa do fim", (unsigned long)a,
                          (unsigned long)(t - a));
                return;
            }
            Conv_U32s(c, t + 4, n * 6);       /* x, y, z, frame, rot lo, rot hi */
        }
    }
    Conv_Zeros(c, a, b, "animação");
}

static void malha_boca(ConvCtx *c, conv_ofs a, conv_ofs b) {
    pacote_vif(c, a, b);
    Conv_Zeros(c, a, b, "boca");
}

static void bytes(ConvCtx *c, conv_ofs a, conv_ofs b) {
    Conv_Bytes(c, a, b - a);
}

static void u32s(ConvCtx *c, conv_ofs a, conv_ofs b) {
    if ((b - a) % 4 != 0) {
        Conv_Erro(c, "entrada em 0x%lX: tamanho não múltiplo de 4", (unsigned long)a);
        return;
    }
    Conv_U32s(c, a, (b - a) / 4);
}

/* §3.1: o formato de cada entrada do file[0] */
static ConvFn formato_f0(conv_u32 n) {
    if (n == 1 || n == 12 || n == 13 || (n >= 14 && n <= 17)) {
        return texfile;
    }
    if (n == 2) {
        return corpo;
    }
    if (n == 3) {
        return modelo;
    }
    if (n >= 4 && n <= 11) {
        return malha_boca;
    }
    switch (n) {
    case 0x12: return param;
    case 0x13: return golpes;
    case 0x14: return rajadas;
    case 0x15: return u32s;     /* §5.5: só números de 4 bytes */
    case 0x16: return bytes;    /* §5.6 */
    case 0x17: return cadeias;
    case 0x18: return tecnicas;
    case 0x19: return habilidades;
    case 0x1B: return bytes;    /* §5.10 */
    case 0x1C: return cortes;
    default: break;
    }
    if (n >= 0x1D && n <= 0x24) {
        return animacao;
    }
    if (n >= 0x25 && n <= 0x2B) {
        return camera;
    }
    if (n >= 0x2C && n <= 0x34) {
        return texto;
    }
    if (n >= 0x35 && n <= 0xFC) {
        return labio;
    }
    return 0; /* 0x1A: sem leitor (vazia nos arquivos conhecidos) */
}

void Conv_Personagem(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[0x100 + 2], n, i;
    n = tabela(c, base, tam, ent, 0xFC);
    if (!c->erro && n != 0xFC) {
        Conv_Erro(c, "file[0] com %lu entradas (esperado 0xFC)", (unsigned long)n);
    }
    for (i = 1; i <= n && !c->erro; i++) {
        ConvFn f;
        if (ent[i] == ent[i + 1]) {
            continue;
        }
        f = formato_f0(i);
        if (f == 0) {
            Conv_Erro(c, "entrada %lu (0x%lX..0x%lX): formato não determinado", (unsigned long)i, (unsigned long)ent[i],
                      (unsigned long)ent[i + 1]);
            break;
        }
        f(c, base + ent[i], base + ent[i + 1]);
    }
}

/* ------------------------------------------------------------------ file[1] */

/* §6.1, §6.2: só a tabela e o cabeçalho de cada contêiner BPE; o fluxo comprimido é todo byte, e a animação
   se converte depois de descomprimida (Conv_Animacao, chamada por BtlObjAnim_Load). */
void Conv_PersonagemAnim(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static conv_u32 ent[0x200 + 2];
    conv_u32 n, i;
    n = tabela(c, base, tam, ent, 0x200);
    for (i = 1; i <= n && !c->erro; i++) {
        conv_ofs a = base + ent[i], b = base + ent[i + 1];
        conv_u32 pack;
        if (a == b) {
            continue;
        }
        Conv_U32(c, a);
        pack = Conv_U32(c, a + 4);
        if (!c->erro && 8 + pack > b - a) {
            Conv_Erro(c, "BPE da entrada %lu: fluxo de 0x%lX passa do fim", (unsigned long)i, (unsigned long)pack);
            break;
        }
        /* o fluxo e o resto da entrada: bytes (há bytes não nulos depois do fluxo, que o decodificador não lê) */
        Conv_Bytes(c, a + 8, b - a - 8);
    }
}

/* uma animação descomprimida inteira */
void Conv_BobjAnim(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    animacao(c, base, base + tam);
}
