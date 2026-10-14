/* Peças dos pacotes de tela dos menus (docs/x360/formatos/menus.md e menus-pecas.md). Cada uma converte o trecho
   [a, b) de c; as de pacote (Conv_Secoes) conferem a tabela de seções antes. */
#ifndef CONV_MENUS_H
#define CONV_MENUS_H

#include "conv.h"

/* menus.md 2.1: tabela de seções com exatamente n entradas (ent[1..n+1]); devolve !erro. oque vai na mensagem */
int Conv_Secoes(ConvCtx *c, conv_ofs base, conv_ofs tam, conv_u32 *ent, conv_u32 n, const char *oque);
/* a seção n de uma tabela já lida tem de ser vazia (sem leitor) */
void Conv_Vazia(ConvCtx *c, const conv_u32 *ent, conv_u32 n);
void Conv_Flash(ConvCtx *c, conv_ofs a, conv_ofs b);   /* menus.md 7: filme Flash ("FOD" 0x11 "LIT") */
void Conv_Textos(ConvCtx *c, conv_ofs a, conv_ofs b);  /* menus.md 6.1: tabela de textos u16 */
void Conv_Dialogo(ConvCtx *c, conv_ofs a, conv_ofs b); /* menus.md 6: arquivo de diálogo (Dialog_Init) */

/* TexFile com pelo menos `minimo` registros (1 + o maior MTEX do leitor; 8 num fundo do Sprite_DrawPicture) */
void Conv_TexFileMin(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo);

/* menus-pecas.md: as peças comuns das telas */
void Conv_JanelaMsg(ConvCtx *c, conv_ofs a, conv_ofs b);   /* 2: MsgWin_Init */
void Conv_JanelaIcone(ConvCtx *c, conv_ofs a, conv_ofs b); /* 3: IconWin_Init */
void Conv_LipPack(ConvCtx *c, conv_ofs a, conv_ofs b);     /* 4: Voice_PlayWithSubtitle, LipSync_Start */
void Conv_ItemPanel(ConvCtx *c, conv_ofs a, conv_ofs b);   /* 5.1: ItemPanel_Init */
void Conv_ItemHelp(ConvCtx *c, conv_ofs a, conv_ofs b);    /* 5.2: ItemHelp_Init */
/* 5.3: lista de ids (contagem em +0, ids em +0x10), com a contagem mínima e o maior id que o leitor aceita */
void Conv_ListaIdsFaixa(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 minimo, conv_u32 teto);
void Conv_ListaIds(ConvCtx *c, conv_ofs a, conv_ofs b);            /* sem limites (Treino) */
void Conv_ListaEstagios(ConvCtx *c, conv_ofs a, conv_ofs b);       /* seleção 39 */
void Conv_ListaMusicas(ConvCtx *c, conv_ofs a, conv_ofs b);        /* seleção 56 */
void Conv_ListaMusicasOpcoes(ConvCtx *c, conv_ofs a, conv_ofs b);  /* Opções 37 */
void Conv_Celulas(ConvCtx *c, conv_ofs a, conv_ofs b);     /* 5.4: ChrGrid_Build */
/* 5.5: subpacote de TexFile; esperado = N exato (0: qualquer) */
void Conv_SubpacoteTexN(ConvCtx *c, conv_ofs a, conv_ofs b, conv_u32 esperado);
void Conv_SubpacoteTex(ConvCtx *c, conv_ofs a, conv_ofs b);
void Conv_FichasPersonagem(ConvCtx *c, conv_ofs a, conv_ofs b);    /* 165 subseções */
void Conv_IconesEstagio(ConvCtx *c, conv_ofs a, conv_ofs b);       /* 38 subseções */

#endif
