/*
 * Os arquivos da luta de um episódio da História (F4, etapa 9a): o roteiro de evento (GSCF, src/sys/gsc.c), os textos
 * das falas e as faixas de boca das falas. Especificação: docs/x360/formatos/historia-luta.md (as seções citadas
 * abaixo). Os três se leem inteiros na carga da luta (File_Request), então se convertem como arquivo (CONV_ARQ).
 */
#include "conv.h"
#include "conv_pecas.h"

/* ------------------------------------------------------------------ §2: o roteiro (GSCF) */

#define GSC_MAX_VALORES 4096 /* o maior GSDT do disco tem 892 valores */
#define TIPO_NENHUM 0xFF     /* valor sem operando que o aponte */

/* o tipo de cada valor do GSDT pelos operandos que o apontam (bits 4..7 do operando: 0 int, 1 float, 2 cadeia) */
static conv_u8 sTipo[GSC_MAX_VALORES];

/* §2.1: as quatro letras da etiqueta como estão no arquivo (o jogo lê a palavra e a compara depois de GSC_SWAP) */
static int etiqueta(ConvCtx *c, conv_ofs o, const char *esperada) {
    int i;
    if (!Conv_Cabe(c, o, 4, "etiqueta")) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (c->buf[o + i] != (conv_u8)esperada[i]) {
            Conv_Erro(c, "roteiro: etiqueta em 0x%lX não é %s", (unsigned long)o, esperada);
            return 0;
        }
    }
    return 1;
}

/* §2.1: cabeçalho de bloco {etiqueta, headSize = 0x10, dataSize, arg}, as quatro palavras u32 (a etiqueta trocada
   como número: o GSC_SWAP do jogo dá o mesmo valor nas duas ordens). Devolve o dataSize; o fim do bloco é
   o + 0x10 + dataSize, e lá vem o EOFC */
static conv_u32 bloco(ConvCtx *c, conv_ofs o, const char *esperada, conv_ofs limite) {
    conv_u32 hs, ds;
    if (!etiqueta(c, o, esperada)) {
        return 0;
    }
    Conv_U32(c, o);
    hs = Conv_U32(c, o + 4);
    ds = Conv_U32(c, o + 8);
    Conv_U32(c, o + 12);
    if (!c->erro && hs != 0x10) {
        Conv_Erro(c, "roteiro: bloco %s em 0x%lX com headSize 0x%lX", esperada, (unsigned long)o, (unsigned long)hs);
    }
    if (!c->erro && (ds % 4 != 0 || o + 0x10 + (conv_ofs)ds + 0x10 > limite)) {
        Conv_Erro(c, "roteiro: bloco %s em 0x%lX com dataSize 0x%lX (limite 0x%lX)", esperada, (unsigned long)o,
                  (unsigned long)ds, (unsigned long)limite);
    }
    return c->erro ? 0 : ds;
}

/* §2.1: o EOFC que fecha um bloco: {"EOFC", 0x10, 0, 0} */
static void eofc(ConvCtx *c, conv_ofs o) {
    conv_u32 ds = bloco(c, o, "EOFC", o + 0x20);
    if (!c->erro && (ds != 0 || Conv_Ler32(c, o + 12) != 0)) {
        /* (já trocado: a leitura sem troca de um zero é zero nas duas ordens) */
        Conv_Erro(c, "roteiro: EOFC em 0x%lX com conteúdo", (unsigned long)o);
    }
}

/* §2.3: um operando (tipo 10): o índice do valor nos bits 8..31 e o tipo nos 4..7 */
static void operando(ConvCtx *c, conv_ofs o, conv_u32 nval) {
    conv_u32 w = Conv_U32(c, o), idx = w >> 8, tipo = (w >> 4) & 0xF;
    if (c->erro) {
        return;
    }
    if ((w & 0xF) != 10) {
        Conv_Erro(c, "roteiro: palavra 0x%lX em 0x%lX, esperado um operando", (unsigned long)(w & 0xF),
                  (unsigned long)o);
        return;
    }
    if (idx >= nval || tipo > 2) {
        Conv_Erro(c, "roteiro: operando em 0x%lX com valor %lu de %lu, tipo %lu", (unsigned long)o,
                  (unsigned long)idx, (unsigned long)nval, (unsigned long)tipo);
        return;
    }
    if (sTipo[idx] != TIPO_NENHUM && sTipo[idx] != tipo) {
        Conv_Erro(c, "roteiro: valor %lu apontado com os tipos %u e %lu", (unsigned long)idx, (unsigned)sTipo[idx],
                  (unsigned long)tipo);
        return;
    }
    sTipo[idx] = (conv_u8)tipo;
}

/* §2.3: as palavras de uma ação [a, b), pelo mesmo caminho do Gsc_StepTask e do Gsc_NextCommand: comando (1), com
   n operandos simples e depois as opções (8) e operandos (10) de bit 3 ligado; chamada (2, 3); fim (0). Toda palavra
   é u32 (as leituras de byte e de u16 do jogo pedem a correção do código, §2.5). Depois do fim, zeros */
static void acao(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 nval) {
    conv_ofs o = a;
    conv_u32 w, n, i;
    while (!c->erro) {
        if (o + 4 > b) {
            Conv_Erro(c, "roteiro: ação em 0x%lX sem a palavra de fim", (unsigned long)a);
            return;
        }
        w = Conv_U32(c, o);
        o += 4;
        switch (w & 0xF) {
        case 0:
            Conv_Zeros(c, o, b, "depois do fim da ação");
            return;
        case 2:
        case 3:
            break;
        case 1:
            n = (w >> 8) & 0xFF;
            for (i = 0; i < n && !c->erro; i++, o += 4) {
                if (o + 4 > b) {
                    Conv_Erro(c, "roteiro: comando em 0x%lX passa da ação", (unsigned long)(o - 4));
                    return;
                }
                operando(c, o, nval);
            }
            /* opções e operandos avulsos: toda palavra com o bit 3 (Gsc_NextCommand) */
            while (!c->erro && o + 4 <= b && (Conv_Ler32(c, o) & 8)) {
                w = Conv_Ler32(c, o);
                if ((w & 0xF) == 8) {
                    Conv_U32(c, o);
                    n = (w >> 16) & 0xFF;
                    o += 4;
                    for (i = 0; i < n && !c->erro; i++, o += 4) {
                        if (o + 4 > b) {
                            Conv_Erro(c, "roteiro: opção passa da ação em 0x%lX", (unsigned long)o);
                            return;
                        }
                        operando(c, o, nval);
                    }
                } else {
                    operando(c, o, nval);
                    o += 4;
                }
            }
            break;
        default:
            Conv_Erro(c, "roteiro: palavra de tipo %lu em 0x%lX fora de um comando", (unsigned long)(w & 0xF),
                      (unsigned long)(o - 4));
            return;
        }
    }
}

/* §2.4: os valores do GSDT [a, a + 4 * nval): int e float u32; cadeia (tipo 2, que nenhum comando lê) em bytes até
   o NUL, arredondada a 4; o que nenhum operando aponta tem de ser zero */
static void valores(ConvCtx *c, conv_ofs a, conv_u32 nval) {
    conv_u32 i, k;
    conv_ofs o, fim;
    for (i = 0; i < nval && !c->erro; i++) {
        o = a + (conv_ofs)i * 4;
        if (sTipo[i] == 0 || sTipo[i] == 1) {
            Conv_U32(c, o);
        } else if (sTipo[i] == 2) {
            fim = o;
            while (fim < a + (conv_ofs)nval * 4 && c->buf[fim] != 0) {
                fim++;
            }
            if (fim >= a + (conv_ofs)nval * 4) {
                Conv_Erro(c, "roteiro: cadeia do valor %lu sem NUL", (unsigned long)i);
                return;
            }
            k = (conv_u32)((fim - o) / 4 + 1); /* palavras da cadeia, com o NUL */
            Conv_Bytes(c, o, (conv_ofs)k * 4);
            for (k = i + 1; k < i + (conv_u32)((fim - o) / 4 + 1); k++) {
                if (sTipo[k] != TIPO_NENHUM) {
                    Conv_Erro(c, "roteiro: valor %lu apontado dentro da cadeia do %lu", (unsigned long)k,
                              (unsigned long)i);
                    return;
                }
            }
            i += (conv_u32)((fim - o) / 4);
        } else {
            Conv_Zeros(c, o, o + 4, "valor sem operando");
        }
    }
}

/* §2: o arquivo inteiro: GSCF { GSHD, GSCD { GSAC... }, GSDT }, cada bloco com o EOFC depois */
void Conv_RoteiroLuta(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_ofs fim = base + tam, oh, oc, od, o, fimc;
    conv_u32 dsf, dsh, dsc, dsd, nval, i, nac = 0;
    dsf = bloco(c, base, "GSCF", fim);
    if (!c->erro && base + 0x10 + (conv_ofs)dsf + 0x10 != fim) {
        Conv_Erro(c, "roteiro: GSCF de 0x%lX bytes num arquivo de 0x%lX", (unsigned long)dsf, (unsigned long)tam);
    }
    if (c->erro) {
        return;
    }
    /* os três blocos na ordem que o Gsc_LoadFile supõe (head, code, data), fechando o GSCF */
    oh = base + 0x10;
    dsh = bloco(c, oh, "GSHD", fim - 0x10);
    Conv_Bytes(c, oh + 0x10, dsh); /* §2.2: ninguém lê os dados do GSHD */
    eofc(c, oh + 0x10 + dsh);
    oc = oh + 0x20 + dsh;
    dsc = bloco(c, oc, "GSCD", fim - 0x10);
    od = oc + 0x20 + dsc;
    dsd = bloco(c, od, "GSDT", fim - 0x10);
    if (!c->erro && od + 0x20 + dsd != fim - 0x10) {
        Conv_Erro(c, "roteiro: os blocos terminam em 0x%lX, o GSCF em 0x%lX", (unsigned long)(od + 0x20 + dsd),
                  (unsigned long)(fim - 0x10));
    }
    nval = dsd / 4;
    if (!c->erro && nval > GSC_MAX_VALORES) {
        Conv_Erro(c, "roteiro: %lu valores (máximo %lu)", (unsigned long)nval, (unsigned long)GSC_MAX_VALORES);
    }
    if (c->erro) {
        return;
    }
    for (i = 0; i < nval; i++) {
        sTipo[i] = TIPO_NENHUM;
    }
    /* §2.3: as ações, até o fim dos dados do GSCD (onde o jogo lê a etiqueta do EOFC do GSCD e para) */
    o = oc + 0x10;
    fimc = oc + 0x10 + dsc;
    while (o < fimc && !c->erro) {
        conv_u32 ds = bloco(c, o, "GSAC", fimc);
        acao(c, o + 0x10, o + 0x10 + ds, nval);
        Conv_Onde(c, "ação %lu", (unsigned long)nac);
        eofc(c, o + 0x10 + ds);
        o += 0x20 + ds;
        nac++;
    }
    if (!c->erro && o != fimc) {
        Conv_Erro(c, "roteiro: as ações terminam em 0x%lX, o GSCD em 0x%lX", (unsigned long)o, (unsigned long)fimc);
    }
    if (!c->erro && nac == 0) {
        Conv_Erro(c, "roteiro: GSCD sem ações");
    }
    eofc(c, fimc);
    valores(c, od + 0x10, nval);
    eofc(c, od + 0x10 + dsd);
    eofc(c, fim - 0x10);
}

/* ------------------------------------------------------------------ §3 e §4: textos e faixas de boca */

#define FALAS 300 /* BtlScriptTalk.speakers[300]: uma entrada por fala (BtlScript_GetText, BtlScript_GetFile8Entry) */

/* §3: textos das falas (BtlScript_GetText): a tabela de deslocamentos com 300 entradas e um texto u16 em cada */
void Conv_TextosLuta(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static conv_u32 ent[FALAS + 2];
    conv_u32 n, i;
    n = Conv_Tabela(c, base, tam, ent, FALAS);
    if (!c->erro && n != FALAS) {
        Conv_Erro(c, "textos da luta com %lu entradas (esperado %d)", (unsigned long)n, FALAS);
    }
    for (i = 1; i <= n && !c->erro; i++) {
        if (ent[i] % 4 != 0) {
            Conv_Erro(c, "texto %lu em 0x%lX: o jogo arredonda o deslocamento a 4", (unsigned long)i,
                      (unsigned long)ent[i]);
            break;
        }
        Conv_Texto(c, base + ent[i], base + ent[i + 1]);
        Conv_Onde(c, "texto %lu", (unsigned long)i);
    }
}

/* §4: faixas de boca das falas (BtlScript_GetFile8Entry, BtlObjFace BOBJ_MOUTH_LIP_PTR): a mesma tabela, cada
   entrada vazia (fala sem boca) ou um BObjLipData (personagem.md 6.6) */
void Conv_LabioLuta(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static conv_u32 ent[FALAS + 2];
    conv_u32 n, i;
    n = Conv_Tabela(c, base, tam, ent, FALAS);
    if (!c->erro && n != FALAS) {
        Conv_Erro(c, "bocas da luta com %lu entradas (esperado %d)", (unsigned long)n, FALAS);
    }
    for (i = 1; i <= n && !c->erro; i++) {
        if (ent[i] == ent[i + 1]) {
            continue;
        }
        if (ent[i] % 4 != 0 || ent[i + 1] - ent[i] < 0x10) {
            Conv_Erro(c, "boca %lu em 0x%lX com 0x%lX bytes", (unsigned long)i, (unsigned long)ent[i],
                      (unsigned long)(ent[i + 1] - ent[i]));
            break;
        }
        Conv_Labio(c, base + ent[i], base + ent[i + 1]);
        Conv_Onde(c, "boca %lu", (unsigned long)i);
    }
}
