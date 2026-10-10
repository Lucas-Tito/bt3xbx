/*
 * Bancos de som, folhas da tela de carregamento e rostos dos membros.
 * Especificação: docs/x360/formatos/som-e-sprites.md (as seções citadas abaixo).
 */
#include "conv.h"
#include "conv_pecas.h"

/* §1: banco de som. Saída A da §1.8: só os números que o jogo lê em ordem nativa (a tabela do cabeçalho,
   Head.vagInfoChunkAddr e Vagi.maxNumber); o resto (amostras ADPCM e as tabelas da Sony, que o tocador do port lê
   byte a byte com le32/le16) fica como está. */
void Conv_BancoSom(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 ent[8], n, vagi, max;
    conv_ofs hd, sq;
    n = Conv_Tabela(c, base, tam, ent, 3);
    if (c->erro || n != 3 || ent[1] != 0x40) {
        Conv_Erro(c, "banco de som: tabela de %lu entradas, amostras em 0x%lX", (unsigned long)n, (unsigned long)ent[1]);
        return;
    }
    hd = base + ent[2];
    sq = base + ent[3];
    /* §1.5: Head em hd + 0x10; vagInfoChunkAddr (+0x20) é o deslocamento do Vagi a partir da seção */
    if (sq - hd < 0x50) {
        Conv_Erro(c, "banco de som: seção hd de 0x%lX bytes", (unsigned long)(sq - hd));
        return;
    }
    vagi = Conv_U32(c, hd + 0x30);
    if (c->erro || vagi < 0x50 || vagi + 0x10 > sq - hd) {
        Conv_Erro(c, "banco de som: Vagi em +0x%lX fora da seção hd", (unsigned long)vagi);
        return;
    }
    max = Conv_U32(c, hd + vagi + 0x0C);
    if (!c->erro && max > 0x7F) {
        Conv_Erro(c, "banco de som: maxNumber %lu (o jogo guarda até 0x80 amostras)", (unsigned long)max);
        return;
    }
    Conv_RestoBytes(c, base, base + tam);
}

/* §2.2: folha da tela de carregamento no disco: um contêiner BPE inteiro; só o cabeçalho é número */
void Conv_FolhaBpe(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    conv_u32 pack;
    Conv_U32(c, base);
    pack = Conv_U32(c, base + 4);
    if (!c->erro && 8 + pack > tam) {
        Conv_Erro(c, "folha BPE: fluxo de 0x%lX passa do fim", (unsigned long)pack);
        return;
    }
    Conv_Bytes(c, base + 8, tam - 8); /* fluxo e a cauda que ninguém lê (com bytes não nulos em dois arquivos) */
}

/* §2.3: a folha descomprimida (Port_ConvertLoadSheet, em Load_InitScreen) */
void Conv_Folha(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    Conv_TexFile(c, base, base + tam, 0);
}

/* §3: rosto de membro, TexFile com a visão do HUD (+0x28 = s32 mark) */
void Conv_Rosto(ConvCtx *c, conv_ofs base, conv_ofs tam) {
    Conv_TexFile(c, base, base + tam, 1);
}
