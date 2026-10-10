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
