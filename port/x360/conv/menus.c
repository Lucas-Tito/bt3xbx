/*
 * Arquivos de menu do caminho do boot até a tela de título: o pacote externo de 00449 e 00473 (tabela de contêineres
 * BPE), 00477 (um contêiner BPE inteiro) e, em tempo de jogo, os pacotes de tela que o jogo descomprime deles (cartão,
 * logos, recursos do cartão, título), com as peças de dentro: arquivo de diálogo, filme Flash ("FOD") e tabela de
 * textos. Especificação: docs/x360/formatos/menus.md (as seções citadas abaixo).
 */
#include "conv.h"
#include "conv_pecas.h"
#include "conv_menus.h"

/* entrada n de uma tabela já lida; b == a quando vazia */
#define ENT(n) (base + ent[n]), (base + ent[(n) + 1])

void Conv_Vazia(ConvCtx *c, const conv_u32 *ent, conv_u32 n) {
    if (!c->erro && ent[n] != ent[n + 1]) {
        Conv_Erro(c, "seção %lu: esperada vazia (sem leitor)", (unsigned long)n);
    }
}

/* tabela de seções com exatamente `n` entradas (§2.1, a de personagem.md 2) */
int Conv_Secoes(ConvCtx *c, conv_ofs base, conv_ofs tam, conv_u32 *ent, conv_u32 n, const char *oque) {
    conv_u32 lidas = Conv_Tabela(c, base, tam, ent, n);
    if (!c->erro && lidas != n) {
        Conv_Erro(c, "%s com %lu seções (esperado %lu)", oque, (unsigned long)lidas, (unsigned long)n);
    }
    Conv_Onde(c, oque, 0); /* o nome do pacote vai junto com o erro (oque não tem formato) */
    return !c->erro;
}

/* ------------------------------------------------------------------ no arquivo (§2, §3) */

/* §2.2: um contêiner BPE em [a, b): rawSize e packSize (s32), o fluxo e a cauda em bytes (a cauda tem bytes não nulos
   que o decodificador não lê, como em som-e-sprites.md 2.2) */
static void contem_bpe(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 bruto, pack;
    bruto = Conv_U32(c, a);
    pack = Conv_U32(c, a + 4);
    if (!c->erro && (bruto == 0 || 8 + (conv_ofs)pack > b - a)) {
        Conv_Erro(c, "BPE em 0x%lX: 0x%lX bytes em 0x%lX de fluxo, contêiner de 0x%lX", (unsigned long)a,
                  (unsigned long)bruto, (unsigned long)pack, (unsigned long)(b - a));
        return;
    }
    Conv_Bytes(c, a + 8, b - a - 8);
}

/* §2: arquivo de menu (00449, 00473): tabela de seções, cada uma um contêiner BPE (MPACK_AT) */
void Conv_MenuPacote(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[0x40 + 2], n, i;
    n = Conv_Tabela(c, base, tam, ent, 0x40);
    for (i = 1; i <= n && !c->erro; i++) {
        if (ent[i] != ent[i + 1]) {
            contem_bpe(c, ENT(i));
            Conv_Onde(c, "seção %lu", (unsigned long)i);
        }
    }
}

/* §3: 00477, o arquivo inteiro num contêiner BPE */
void Conv_MenuBpe(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    if (tam < 8) {
        Conv_Erro(c, "BPE de 0x%lX bytes", (unsigned long)tam);
        return;
    }
    contem_bpe(c, base, base + tam);
}

/* ------------------------------------------------------------------ filme Flash (§7) */

/* um byte em o, dentro de [.., fim); -1 (com erro) se passa */
static int byte(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    if (c->erro) {
        return -1;
    }
    if (o >= fim) {
        Conv_Erro(c, "Flash: leitura em 0x%lX passa do bloco (0x%lX)", (unsigned long)o, (unsigned long)fim);
        return -1;
    }
    Conv_Bytes(c, o, 1);
    return c->erro ? -1 : c->buf[o];
}

/* u16 sem alinhamento em o, dentro de [.., fim) (o jogo lê por memcpy) */
static conv_u32 u16(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    if (!c->erro && o + 2 > fim) {
        Conv_Erro(c, "Flash: u16 em 0x%lX passa do bloco (0x%lX)", (unsigned long)o, (unsigned long)fim);
    }
    return c->erro ? 0 : Conv_U16(c, o);
}

/* §7.8: `n` bits a partir do bit `pos` de o, o mais alto primeiro (Flash_ReadBits); só para medir o campo */
static conv_u32 bits(ConvCtx *c, conv_ofs o, conv_u32 pos, conv_u32 n, conv_ofs fim) {
    conv_u32 v = 0, i;
    if (c->erro || o + ((pos + n + 7) >> 3) > fim) {
        Conv_Erro(c, "Flash: campo de bits em 0x%lX passa do bloco", (unsigned long)o);
        return 0;
    }
    for (i = 0; i < n; i++) {
        conv_u32 p = pos + i;
        v = v << 1 | ((c->buf[o + (p >> 3)] >> (7 - (p & 7))) & 1);
    }
    return v;
}

/* §7.8: matriz (Flash_ReadMtx): byte de flags e campos de bits, tudo byte; devolve o fim */
static conv_ofs matriz(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    static const conv_u8 par[3][3] = {{0x11, 1, 0x10}, {0x0A, 2, 8}, {0x24, 4, 0x20}};
    int flags = byte(c, o, fim), k;
    conv_u32 pos = 0, n;
    for (k = 0; k < 3 && flags >= 0 && !c->erro; k++) {
        if (flags & par[k][0]) {
            n = bits(c, o + 1, pos, 5, fim);
            pos += 5;
            pos += (flags & par[k][1]) ? n : 0;
            pos += (flags & par[k][2]) ? n : 0;
        }
    }
    if (c->erro || o + 1 + ((pos + 7) >> 3) > fim) {
        Conv_Erro(c, "Flash: matriz em 0x%lX passa do bloco", (unsigned long)o);
        return o;
    }
    Conv_Bytes(c, o + 1, (pos + 7) >> 3);
    return o + 1 + ((pos + 7) >> 3);
}

/* §7.8: transformação de cor (Flash_ReadCxform): byte de flags, largura em 4 bits e os campos, tudo byte */
static conv_ofs cor(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    int flags = byte(c, o, fim), i;
    conv_u32 pos = 0, n;
    if (flags > 0) {
        n = bits(c, o + 1, 0, 4, fim);
        pos = 4;
        for (i = 0; i < 8; i++) {
            pos += ((flags >> i) & 1) ? n : 0;
        }
    }
    if (c->erro || o + 1 + ((pos + 7) >> 3) > fim) {
        Conv_Erro(c, "Flash: cor em 0x%lX passa do bloco", (unsigned long)o);
        return o;
    }
    Conv_Bytes(c, o + 1, (pos + 7) >> 3);
    return o + 1 + ((pos + 7) >> 3);
}

/* cadeia terminada em zero (Flash_ReadString), bytes; devolve o fim */
static conv_ofs cadeia(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    conv_ofs z;
    for (z = o; z < fim && !c->erro; z++) {
        if (c->buf[z] == 0) {
            Conv_Bytes(c, o, z + 1 - o);
            return z + 1;
        }
    }
    Conv_Erro(c, "Flash: cadeia em 0x%lX sem fim no bloco", (unsigned long)o);
    return o;
}

/* §7.7: lista de ações (FlashTl_TagActions / FlashTl_SkipActions), até a ação 0 */
static conv_ofs acoes(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    int a;
    do {
        a = byte(c, o, fim);
        o++;
        switch (a) {
        case 0x81: /* GotoFrame: u16 quadro */
            u16(c, o, fim);
            o += 2;
            break;
        case 0x83: /* GetURL: duas cadeias */
            o = cadeia(c, o, fim);
            o = cadeia(c, o, fim);
            break;
        case 0x8A: /* WaitForFrame: 3 bytes que o jogo pula sem ler */
            if (!c->erro && o + 3 > fim) {
                Conv_Erro(c, "Flash: WaitForFrame em 0x%lX passa do bloco", (unsigned long)o);
            } else {
                Conv_Bytes(c, o, 3);
            }
            o += 3;
            break;
        case 0x8B: /* SetTarget */
        case 0x8C: /* GotoLabel */
            o = cadeia(c, o, fim);
            break;
        case 0: case 4: case 5: case 6: case 7: case -1:
            break;
        default:
            Conv_Erro(c, "Flash: ação 0x%X em 0x%lX sem leitor", (unsigned)a, (unsigned long)(o - 1));
            break;
        }
    } while (a > 0 && !c->erro);
    return o;
}

/* §7.6: as tags de um quadro (FlashTl_RunFrame), de o até a ShowFrame (1), que tem de fechar o bloco em fim */
static void tags_quadro(ConvCtx *c, conv_ofs o, conv_ofs fim) {
    conv_u32 cod;
    int flags, tipo;
    do {
        cod = u16(c, o, fim);
        o += 2;
        switch (cod) {
        case 4: /* PlaceObject: u16 profundidade, u8 tipo, u16 id, matriz e (sprite) cor */
            u16(c, o, fim);
            tipo = byte(c, o + 2, fim);
            u16(c, o + 3, fim);
            o += 5;
            if (tipo == 4) {
                o = matriz(c, o, fim);
            } else if (tipo == 6) {
                o = matriz(c, o, fim);
                o = cor(c, o, fim);
            }
            break;
        case 26: /* PlaceObject2 */
            flags = byte(c, o, fim);
            u16(c, o + 1, fim);
            o += 3;
            if (flags < 0) {
                break;
            }
            if (flags & 2) {
                byte(c, o, fim);
                u16(c, o + 1, fim);
                o += 3;
            }
            if (flags & 4) {
                o = matriz(c, o, fim);
            }
            if (flags & 8) {
                o = cor(c, o, fim);
            }
            if ((flags & 2) && (flags & 0x20)) { /* o nome só se lê num objeto novo */
                o = cadeia(c, o, fim);
            }
            break;
        case 5: /* RemoveObject: u16 profundidade, u8 tipo, u16 id */
            u16(c, o, fim);
            byte(c, o + 2, fim);
            u16(c, o + 3, fim);
            o += 5;
            break;
        case 28: /* RemoveObject2: u16 profundidade */
            u16(c, o, fim);
            o += 2;
            break;
        case 12: /* DoAction */
            o = acoes(c, o, fim);
            break;
        case 1: /* ShowFrame */
            break;
        default:
            if (!c->erro) {
                Conv_Erro(c, "Flash: tag de quadro %lu em 0x%lX sem leitor", (unsigned long)cod, (unsigned long)(o - 2));
            }
            break;
        }
    } while (cod != 1 && !c->erro);
    if (!c->erro && o != fim) {
        Conv_Erro(c, "Flash: quadro termina em 0x%lX, bloco em 0x%lX", (unsigned long)o, (unsigned long)fim);
    }
}

/* §7.5: blocos nomeados de quadros (nome, u16 tamanho, tags) em [a, b), até o bloco de tamanho 0; o resto é zero */
static void quadros(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_ofs o = a, fim;
    conv_u32 tam;
    for (;;) {
        o = cadeia(c, o, b);
        tam = u16(c, o, b);
        o += 2;
        if (c->erro || tam == 0) {
            break;
        }
        fim = o + tam;
        if (fim > b) {
            Conv_Erro(c, "Flash: quadro em 0x%lX de 0x%lX bytes passa do registro", (unsigned long)o, (unsigned long)tam);
            break;
        }
        tags_quadro(c, o, fim);
        o = fim;
    }
    Conv_Zeros(c, a, b, "fim da linha do tempo");
}

/* §7.3: cabeçalho de 8 bytes de uma tag ou de um registro (FlashTag): u8 código, u8, u16 contagem, u32 tamanho */
static conv_u32 cabecalho(ConvCtx *c, conv_ofs o, conv_ofs fim, conv_u32 *cont) {
    if (!c->erro && o + 8 > fim) {
        Conv_Erro(c, "Flash: cabeçalho em 0x%lX passa do fim", (unsigned long)o);
        return 0;
    }
    Conv_Bytes(c, o, 2);
    *cont = Conv_U16(c, o + 2);
    return Conv_U32(c, o + 4);
}

/* §7: filme Flash ("FOD" 0x11 "LIT") em [a, b) */
void Conv_Flash(ConvCtx *c, conv_ofs a, conv_ofs b) {
    static const conv_u8 magia[8] = {'F', 'O', 'D', 0x11, 'L', 'I', 'T', 0};
    conv_ofs o, r, fimtag;
    conv_u32 cod, cont, tam, i, rcont, rtam;
    if (!Conv_Cabe(c, a, 8, "Flash") || b - a < 16) {
        Conv_Erro(c, "Flash em 0x%lX: seção curta", (unsigned long)a);
        return;
    }
    for (i = 0; i < 8; i++) {
        if (c->buf[a + i] != magia[i]) {
            Conv_Erro(c, "Flash em 0x%lX: sem o cabeçalho FOD", (unsigned long)a);
            return;
        }
    }
    Conv_Bytes(c, a, 8);
    for (o = a + 8; !c->erro; o = fimtag) {
        if (o + 8 > b) {
            Conv_Erro(c, "Flash em 0x%lX: sem a tag de fim", (unsigned long)a);
            break;
        }
        cod = c->buf[o];
        tam = cabecalho(c, o, b, &cont);
        if (c->erro) {
            break;
        }
        if (cod == 0) { /* fim da lista (Flash_FindTag) */
            if (cont != 0 || tam != 0) {
                Conv_Erro(c, "Flash em 0x%lX: tag de fim com dados", (unsigned long)a);
            }
            Conv_Zeros(c, o + 8, b, "Flash depois da tag de fim");
            break;
        }
        fimtag = o + 8 + tam;
        if (fimtag > b || fimtag < o) {
            Conv_Erro(c, "Flash em 0x%lX: tag %lu de 0x%lX bytes passa do fim", (unsigned long)a, (unsigned long)cod,
                      (unsigned long)tam);
            break;
        }
        for (r = o + 8, i = 0; i < cont && !c->erro; i++) {
            conv_ofs d, z;
            rtam = cabecalho(c, r, fimtag, &rcont);
            d = r + 8;
            z = d + rtam;
            if (c->erro || z > fimtag || z < d) {
                Conv_Erro(c, "Flash em 0x%lX: registro %lu da tag %lu passa do fim", (unsigned long)a, (unsigned long)i,
                          (unsigned long)cod);
                break;
            }
            switch (cod) {
            case 2: /* §7.4: lista de sprites: u16 id do registro da tag 6 */
            case 3: /* imagem: u16 largura, u16 altura */
                if (rtam != (cod == 2 ? 2u : 4u)) {
                    Conv_Erro(c, "Flash em 0x%lX: registro da tag %lu com 0x%lX bytes", (unsigned long)a,
                              (unsigned long)cod, (unsigned long)rtam);
                    break;
                }
                Conv_U16s(c, d, rtam / 2);
                break;
            case 4: { /* forma: u16 n e n FlashShapeQuad (s16, u16, u32, 4 x s32), sem alinhamento */
                conv_u32 n = u16(c, d, z), k;
                if (!c->erro && rtam != 2 + n * 0x18) {
                    Conv_Erro(c, "Flash em 0x%lX: forma %lu com %lu quadriláteros em 0x%lX bytes", (unsigned long)a,
                              (unsigned long)i, (unsigned long)n, (unsigned long)rtam);
                    break;
                }
                for (k = 0; k < n && !c->erro; k++) {
                    conv_ofs q = d + 2 + k * 0x18;
                    Conv_U16s(c, q, 2);     /* image, unk02 (sem leitor; u16 pela struct) */
                    Conv_U32s(c, q + 4, 5); /* rgba, x0, x1, y0, y1 */
                }
                break;
            }
            case 6: /* §7.5: quadros de um sprite */
            case 7: /* quadros da raiz */
                quadros(c, d, z);
                break;
            default:
                Conv_Erro(c, "Flash em 0x%lX: tag %lu sem leitor", (unsigned long)a, (unsigned long)cod);
                break;
            }
            r = z;
        }
        if (!c->erro && r != fimtag) {
            Conv_Erro(c, "Flash em 0x%lX: registros da tag %lu terminam em 0x%lX, a tag em 0x%lX", (unsigned long)a,
                      (unsigned long)cod, (unsigned long)r, (unsigned long)fimtag);
        }
    }
}

/* ------------------------------------------------------------------ diálogo (§6) */

/* §6.4: tabela de textos (Dialog_GetTitleTable, DIALOG_TEXT): a tabela de §2.1 e um texto u16 por entrada */
void Conv_Textos(ConvCtx *c, conv_ofs a, conv_ofs b) {
    static conv_u32 ent[0x400 + 2]; /* os textos de item têm um por item (SAVE_ITEM_COUNT = 350; menus-pecas.md 5.1) */
    conv_u32 n, i;
    n = Conv_Tabela(c, a, b - a, ent, 0x400);
    for (i = 1; i <= n && !c->erro; i++) {
        Conv_Texto(c, a + ent[i], a + ent[i + 1]);
        Conv_Onde(c, "texto %lu", (unsigned long)i);
    }
}

/* §6: arquivo do diálogo (Dialog_Init): seis partes */
void Conv_Dialogo(ConvCtx *c, conv_ofs a, conv_ofs b) {
    conv_u32 ent[8], i;
    conv_ofs base = a;
    if (!Conv_Secoes(c, a, b - a, ent, 6, "diálogo")) {
        return;
    }
    for (i = 1; i <= 4 && !c->erro; i++) {
        Conv_TexFile(c, ENT(i), 0);
        Conv_Onde(c, "parte %lu do diálogo", (unsigned long)i);
    }
    Conv_Flash(c, ENT(5));
    Conv_Textos(c, ENT(6));
}

/* ------------------------------------------------------------------ pacotes de tela, em tempo de jogo (§5) */

/* §5.1: tela do cartão (BootCard_Init, seção 2 de 00473): só o diálogo */
void Conv_MenuCartao(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[4];
    if (Conv_Secoes(c, base, tam, ent, 1, "tela do cartão")) {
        Conv_Dialogo(c, ENT(1));
    }
}

/* §5.2: logos (FirstRun_Main, seção 3 de 00473): 3..8 lidas por Logo_Show, 2 sem leitor (TexFile pela forma) */
void Conv_MenuLogos(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[12], i;
    if (!Conv_Secoes(c, base, tam, ent, 9, "logos")) {
        return;
    }
    Conv_Vazia(c, ent, 1);
    Conv_Vazia(c, ent, 9);
    for (i = 2; i <= 8 && !c->erro; i++) {
        Conv_TexFile(c, ENT(i), 0);
        Conv_Onde(c, "logo %lu", (unsigned long)i);
    }
}

/* §5.3: recursos do cartão (McFlow_Init, 00477): os dois ícones do cartão de memória, que vão ao cartão como bytes
   (sceMcWrite) com os tamanhos fixos do código; depois deles, zeros */
void Conv_MenuMcFlow(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_ofs icone[2] = {0x9C9A, 0xBE8A}; /* McFlow_SetupRequest */
    conv_u32 ent[6], i;
    if (!Conv_Secoes(c, base, tam, ent, 3, "recursos do cartão")) {
        return;
    }
    for (i = 1; i <= 2 && !c->erro; i++) {
        if (ent[i + 1] - ent[i] < icone[i - 1]) {
            Conv_Erro(c, "ícone %lu de 0x%lX bytes (o cartão grava 0x%lX)", (unsigned long)i,
                      (unsigned long)(ent[i + 1] - ent[i]), (unsigned long)icone[i - 1]);
            break;
        }
        Conv_Bytes(c, base + ent[i], icone[i - 1]);
        Conv_Zeros(c, ENT(i), "ícone do cartão");
    }
    Conv_Vazia(c, ent, 3);
}

/* §5.4: título (Title_Init, seção 1 de 00449): 1..3, 5..10 e 14 relocadas por Title_Init, 4 sem leitor (TexFile pela
   forma), 11 o filme, 12 o diálogo */
void Conv_MenuTitulo(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[16], i;
    if (!Conv_Secoes(c, base, tam, ent, 14, "título")) {
        return;
    }
    for (i = 1; i <= 14 && !c->erro; i++) {
        switch (i) {
        case 11: Conv_Flash(c, ENT(11)); break;
        case 12: Conv_Dialogo(c, ENT(12)); break;
        case 13: Conv_Vazia(c, ent, 13); break;
        default: Conv_TexFile(c, ENT(i), 0); break;
        }
        Conv_Onde(c, "seção %lu do título", (unsigned long)i);
    }
}

/* menus-pecas.md 6: menu principal (MainMenu_Init, src/menu/menu_a.c:45, seção 2 de 00449): 1..19 e 23..26 TexFile
   relocadas por MainMenu_Init (MM_RES; a 23 são os ícones da janela de ícone), 20 o filme, 21 a janela de mensagem,
   22 a de ícone, 27 as legendas com a boca, 28 os textos da janela de mensagem */
void Conv_MenuPrincipal(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[30], i;
    if (!Conv_Secoes(c, base, tam, ent, 28, "menu principal")) {
        return;
    }
    for (i = 1; i <= 28 && !c->erro; i++) {
        switch (i) {
        case 20: Conv_Flash(c, ENT(20)); break;
        case 21: Conv_JanelaMsg(c, ENT(21)); break;
        case 22: Conv_JanelaIcone(c, ENT(22)); break;
        case 27: Conv_LipPack(c, ENT(27)); break;
        case 28: Conv_Textos(c, ENT(28)); break;
        default: Conv_TexFile(c, ENT(i), 0); break;
        }
        Conv_Onde(c, "seção %lu do menu principal", (unsigned long)i);
    }
}

/* ================================================================== as telas do resto do caminho essencial (F4, 5b)
   Especificação: docs/x360/formatos/menus-telas.md. Cada tela é a tabela de seções do pacote, seção a seção, com
   as peças de menus_pecas.c; um TexFile leva o mínimo de registros que o leitor usa (1 + o maior índice de MTEX,
   e 8 no fundo desenhado por Sprite_DrawPicture, que lê as texturas 0..7). */

/* §2 (menus-telas.md): o submenu do Duelo (DuelMenu_Init, src/menu/menu_g_c.c:186, seção 1 de 00448). 1..18 e 24
   TexFile (DM_RES; 1 o fundo, 24 os ícones da janela de ícone), 19 o filme, 20 vazia, 21 os textos da janela de
   mensagem, 22 a janela de mensagem, 23 a de ícone, 25 o diálogo (com os textos da 27), 26 as legendas com a boca */
void Conv_MenuDuelo(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u32 minimo[25] = {0, 8, 3, 4, 4, 6, 3, 3, 4, 1, 3, 2, 3, 2, 2, 2, 2, 6, 2, 0, 0, 0, 0, 0, 1};
    conv_u32 ent[29], i;
    if (!Conv_Secoes(c, base, tam, ent, 27, "Duelo")) {
        return;
    }
    for (i = 1; i <= 27 && !c->erro; i++) {
        switch (i) {
        case 19: Conv_Flash(c, ENT(19)); break;
        case 20: Conv_Vazia(c, ent, 20); break; /* sem leitor, e vazia no disco */
        case 21: Conv_Textos(c, ENT(21)); break;
        case 22: Conv_JanelaMsg(c, ENT(22)); break;
        case 23: Conv_JanelaIcone(c, ENT(23)); break;
        case 25: Conv_Dialogo(c, ENT(25)); break;
        case 26: Conv_LipPack(c, ENT(26)); break;
        case 27: Conv_Textos(c, ENT(27)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do Duelo", (unsigned long)i);
    }
}

/* §3: as seleções de personagem e de time (CharSel_Init, src/menu/menu_c_e.c; TeamSel_Init, src/menu/menu_e_b.c),
   seção 2 de 00448 e, no CharSel do Treino (modo 0x2D), seção 2 de 00461, com as mesmas peças nas mesmas seções.
   A união das duas telas: TexFile 1..14, 16..23, 26..31, 34..36, 47..50, 55; filmes 15, 24, 25, 32, 33, 37, 38; 39
   lista de estágios, 42 células, 43 e 44 textos de nomes, 45 fichas, 46 ícones de estágio, 51 ItemHelp, 53
   ItemPanel, 54 janela de ícone, 56 lista de músicas. 40, 41 e 52 sem leitor (menus-telas.md 3.3): a 41 tem a forma
   de células e se converte como tal; a 40 e a 52 não têm a forma de nenhuma peça e ficam em bytes */
void Conv_MenuSelecao(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[57] = {
        0, 2, 2, 1, 1, 2, 1, 1, 5, 2, 1, 4, 2, 2, 2, 0, 3, 2, 2, 5, 5, 1, 1, 2, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0,
        1, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 1, 0};
    conv_u32 ent[58], i;
    if (!Conv_Secoes(c, base, tam, ent, 56, "seleção")) {
        return;
    }
    for (i = 1; i <= 56 && !c->erro; i++) {
        switch (i) {
        case 15: case 24: case 25: case 32: case 33: case 37: case 38:
            Conv_Flash(c, ENT(i));
            break;
        case 39: Conv_ListaEstagios(c, ENT(39)); break;
        case 40: case 52: /* sem leitor e sem forma conhecida (menus-telas.md 3.3): ficam em bytes */
            Conv_RestoBytes(c, ENT(i));
            break;
        case 41: /* sem leitor, com a forma de células (menus-telas.md 3.3) */
        case 42: Conv_Celulas(c, ENT(i)); break;
        case 43: case 44: Conv_Textos(c, ENT(i)); break;
        case 45: Conv_FichasPersonagem(c, ENT(45)); break;
        case 46: Conv_IconesEstagio(c, ENT(46)); break;
        case 51: Conv_ItemHelp(c, ENT(51)); break;
        case 53: Conv_ItemPanel(c, ENT(53)); break;
        case 54: Conv_JanelaIcone(c, ENT(54)); break;
        case 56: Conv_ListaMusicas(c, ENT(56)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu da seleção", (unsigned long)i);
    }
}

/* §6: uma imagem única (o arquivo inteiro é um contêiner BPE, e o conteúdo, um TexFile): retratos (0x2F9 +
   personagem) e páginas do Treino (0x402 + página), lidos pelo registro 0 (MTEX(res, 0)) */
void Conv_MenuTexFile(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    Conv_TexFileMin(c, base, base + tam, 1);
}

/* §6: uma imagem de fundo (o mesmo, desenhada por Sprite_DrawPicture, que lê as texturas 0..7): as imagens de
   estágio (0x39D + estágio) do CharSel e do TeamSel */
void Conv_MenuFundo(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    Conv_TexFileMin(c, base, base + tam, 8);
}

/* §4.2: as aulas do Treino (TrainLesson, include/menu/menu_i.h:62; seção 22 de 461 s1): 13, 11 e 14 registros de
   0x10 seguidos, um por classe, o último de cada classe com TRAIN_LESSON_LAST (Train_BuildLists anda até ele);
   depois, zeros. Por registro: u16 id, u8 pageNum, u8 flags, s32 firstPage, s32 voice, u8 stage, u8 bgm, u8 chara[2]
   (Train_BuildLists, Train_FindLesson, Train_Leave). As páginas são os arquivos 0x402 + página, antes dos slots de
   lutador (0x590): firstPage + pageNum <= 0x18E; e cabem nas listas (TRAIN_PAGE_MAX = 30) */
void Conv_LicoesTreino(ConvCtx *c, conv_ofs a, conv_ofs b) {
    static const conv_u32 fimClasse[3] = {13, 13 + 11, 13 + 11 + 14};
    conv_u32 i, k = 0, primeira, n, flags;
    if (!Conv_Cabe(c, a, 38 * 0x10, "aulas do Treino") || b - a < 38 * 0x10) {
        Conv_Erro(c, "aulas do Treino em 0x%lX: 0x%lX bytes (38 registros de 0x10)", (unsigned long)a,
                  (unsigned long)(b - a));
        return;
    }
    if (a % 4 != 0) {
        Conv_Erro(c, "aulas do Treino em 0x%lX: início fora de múltiplo de 4", (unsigned long)a);
        return;
    }
    for (i = 0; i < 38 && !c->erro; i++) {
        conv_ofs r = a + (conv_ofs)i * 0x10;
        Conv_U16(c, r);
        Conv_Bytes(c, r + 2, 2);
        n = c->buf[r + 2];
        flags = c->buf[r + 3];
        primeira = Conv_U32(c, r + 4);
        Conv_U32(c, r + 8);
        Conv_Bytes(c, r + 0xC, 4);
        if (!c->erro && ((flags & 4) != 0) != (i + 1 == fimClasse[k])) {
            Conv_Erro(c, "aula %lu: TRAIN_LESSON_LAST %s", (unsigned long)i, (flags & 4) ? "fora do fim da classe" :
                      "faltando no fim da classe");
        }
        if (!c->erro && (n > 30 || primeira > 0x18E || primeira + n > 0x18E)) {
            Conv_Erro(c, "aula %lu: páginas %lu + %lu fora dos arquivos de página", (unsigned long)i,
                      (unsigned long)primeira, (unsigned long)n);
        }
        if (i + 1 == fimClasse[k]) {
            k++;
        }
    }
    Conv_Zeros(c, a, b, "depois das aulas do Treino");
}

/* §4: o Treino (Train_Init, src/menu/menu_h_d.c, seção 1 de 00461). 1 o filme, 2..9 TexFile (2 o fundo, 5 os ícones
   da janela de ícone), 10 a janela de ícone, 11 a de mensagem, 12 e 13 as legendas com a boca (duas falas), 14..17
   textos (14 e 15 da janela de mensagem, 16 da caixa de texto, 17 das páginas), 22 as aulas, 24 e 26 listas de ids
   (páginas fora das listas, falas sem figura); 18..20 vazias; 21, 23 e 25 sem leitor (menus-telas.md 4.3), com a
   forma de uma tabela de aulas (21) e de listas de ids (23, 25): convertidas pela forma */
void Conv_MenuTreino(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[10] = {0, 0, 8, 9, 5, 1, 4, 16, 1, 5};
    conv_u32 ent[28], i;
    if (!Conv_Secoes(c, base, tam, ent, 26, "Treino")) {
        return;
    }
    for (i = 1; i <= 26 && !c->erro; i++) {
        switch (i) {
        case 1: Conv_Flash(c, ENT(1)); break;
        case 10: Conv_JanelaIcone(c, ENT(10)); break;
        case 11: Conv_JanelaMsg(c, ENT(11)); break;
        case 12: case 13: Conv_LipPack(c, ENT(i)); break;
        case 14: case 15: case 16: case 17: Conv_Textos(c, ENT(i)); break;
        case 18: case 19: case 20: Conv_Vazia(c, ent, i); break;
        case 21: /* sem leitor, com a forma das aulas */
        case 22: Conv_LicoesTreino(c, ENT(i)); break;
        case 23: case 25: /* sem leitor, com a forma de lista de ids */
        case 24: case 26: Conv_ListaIds(c, ENT(i)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu do Treino", (unsigned long)i);
    }
}

/* §5: as Opções (Option_Init, src/menu/menu_x_c.c, seção 1 de 00453). TexFile 1..6, 9..30 e 34 (OPT_RES; 30 o fundo,
   18 os ícones da janela de ícone), 8 o filme, 31 a janela de mensagem, 32 a de ícone, 33 os textos dela, 35 o
   diálogo do cartão (McFlow), 36 as legendas com a boca, 37 a lista de músicas do teste de som, 38 os textos do
   diálogo; 7 sem leitor e sem forma conhecida (menus-telas.md 5.3) */
void Conv_MenuOpcoes(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    static const conv_u8 minimo[35] = {0, 2, 1, 2, 1, 1, 3, 0, 0, 1, 3, 2, 1, 2, 3, 3, 3, 3, 1, 10, 1, 4, 1, 4, 1, 1, 1,
                                       2, 1, 1, 8, 0, 0, 0, 2};
    conv_u32 ent[40], i;
    if (!Conv_Secoes(c, base, tam, ent, 38, "Opções")) {
        return;
    }
    for (i = 1; i <= 38 && !c->erro; i++) {
        switch (i) {
        case 7: Conv_RestoBytes(c, ENT(7)); break; /* sem leitor: fica em bytes */
        case 8: Conv_Flash(c, ENT(8)); break;
        case 31: Conv_JanelaMsg(c, ENT(31)); break;
        case 32: Conv_JanelaIcone(c, ENT(32)); break;
        case 33: case 38: Conv_Textos(c, ENT(i)); break;
        case 35: Conv_Dialogo(c, ENT(35)); break;
        case 36: Conv_LipPack(c, ENT(36)); break;
        case 37: Conv_ListaMusicasOpcoes(c, ENT(37)); break;
        default: Conv_TexFileMin(c, ENT(i), minimo[i]); break;
        }
        Conv_Onde(c, "seção %lu das Opções", (unsigned long)i);
    }
}
