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

/* menus-pecas.md: as peças comuns das telas */
void Conv_JanelaMsg(ConvCtx *c, conv_ofs a, conv_ofs b);   /* 2: MsgWin_Init */
void Conv_JanelaIcone(ConvCtx *c, conv_ofs a, conv_ofs b); /* 3: IconWin_Init */
void Conv_LipPack(ConvCtx *c, conv_ofs a, conv_ofs b);     /* 4: Voice_PlayWithSubtitle, LipSync_Start */

#endif
