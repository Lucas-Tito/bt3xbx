/* A ponte do jogo para a conversão em tempo de jogo, em host big-endian (as chamadas ficam em #ifdef
   PORT_BIG_ENDIAN no código do jogo). Formato que não bate é erro fatal: seguir com dados meio convertidos só
   adiaria o problema para um lugar pior. */
#include <stdio.h>
#include <stdlib.h>

#include "conv.h"

/* src/battle/bobj_a.c, BtlObjAnim_Load: uma animação de personagem logo depois de Bpe_Decode */
void Port_ConvertAnim(void *buf, int size) {
    char msg[240];
    if (size <= 0) {
        return; /* "BPE vazio": nada decodificado */
    }
    if (Conv_Animacao((conv_u8 *)buf, (conv_ofs)size, msg, sizeof(msg)) < 0) {
        fprintf(stderr, "conv: animação de 0x%X bytes: %s\n", (unsigned)size, msg);
        abort();
    }
}

/* src/sys/loading.c, Load_InitScreen: a folha da tela de carregamento logo depois de Sprite_Unpack */
void Port_ConvertLoadSheet(void *buf, int size) {
    char msg[240];
    conv_u8 *feito;
    int r;
    if (size <= 0) {
        return;
    }
    feito = (conv_u8 *)calloc((size_t)size, 1);
    if (feito == NULL) {
        fprintf(stderr, "conv: folha da tela de carregamento: sem memória\n");
        abort();
    }
    r = Conv_Formato("folha", "folha da tela de carregamento", (conv_u8 *)buf, (conv_ofs)size, feito, msg, sizeof(msg));
    free(feito);
    if (r < 0) {
        fprintf(stderr, "conv: folha da tela de carregamento de 0x%X bytes: %s\n", (unsigned)size, msg);
        abort();
    }
}
