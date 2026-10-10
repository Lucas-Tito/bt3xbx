/* Peças de formato que mais de um arquivo usa (definidas em personagem.c, onde foram especificadas primeiro:
   docs/x360/formatos/personagem.md). Cada uma converte o trecho [a, b) de c. */
#ifndef CONV_PECAS_H
#define CONV_PECAS_H

#include "conv.h"

/* §2: tabela de deslocamentos (N, N deslocamentos, fim = tam, enchimento); ent[1..N+1]; devolve N */
conv_u32 Conv_Tabela(ConvCtx *c, conv_ofs base, conv_ofs tam, conv_u32 *ent, conv_u32 max);
/* §4.2.1: pacote DMA/VIF terminado pela etiqueta end; devolve o fim */
conv_ofs Conv_PacoteVif(ConvCtx *c, conv_ofs ofs, conv_ofs fim);
/* §4.4: pacote de pixels ou de paleta (cabeçalho e fim em u32, imagem em bytes) */
void Conv_PacoteImagem(ConvCtx *c, conv_ofs ofs, conv_ofs tam);
/* §4.4: TexFile; hud = 1 trata TexEntry +0x28 como 2 x u32 (folhas do HUD, comuns.md 6.3) */
void Conv_TexFile(ConvCtx *c, conv_ofs a, conv_ofs b, int hud);
/* o que sobrou sem marca fica como bytes, sem exigir zero (só trechos que o código não lê) */
void Conv_RestoBytes(ConvCtx *c, conv_ofs a, conv_ofs b);
void Conv_Cortes(ConvCtx *c, conv_ofs a, conv_ofs b);       /* §5.11 ChrCamCutDef */
void Conv_Camera(ConvCtx *c, conv_ofs a, conv_ofs b);       /* §5.12 DemoCamAnim */
void Conv_Texto(ConvCtx *c, conv_ofs a, conv_ofs b);        /* §5.13 texto u16 */
void Conv_Labio(ConvCtx *c, conv_ofs a, conv_ofs b);        /* §6.6 BObjLipData */
void Conv_BytesEntrada(ConvCtx *c, conv_ofs a, conv_ofs b); /* a entrada inteira em bytes */
void Conv_U32Entrada(ConvCtx *c, conv_ofs a, conv_ofs b);   /* a entrada inteira em palavras u32 */
/* §7.8: parte de efeito do tipo de grupo `tipo`, entrada a (qual = 0) ou b (1) */
void Conv_ParteEfeito(ConvCtx *c, conv_u32 tipo, int qual, conv_ofs a, conv_ofs b);
/* §7.6: conjunto de emissores com cabeçalho na entrada idx; devolve a primeira entrada não consumida */
conv_u32 Conv_Conjunto(ConvCtx *c, conv_ofs base, const conv_u32 *ent, conv_u32 n, conv_u32 idx);
/* §7.14: malha de efeito (EftMeshData) */
void Conv_MalhaEfeito(ConvCtx *c, conv_ofs a, conv_ofs b);

#endif
