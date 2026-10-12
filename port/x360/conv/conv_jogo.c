/* A ponte do jogo para a conversão em tempo de jogo, em host big-endian (as chamadas ficam em #ifdef
   PORT_BIG_ENDIAN no código do jogo). Formato que não bate é erro fatal: seguir com dados meio convertidos só
   adiaria o problema para um lugar pior. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "conv.h"

#ifdef _XBOX /* o 360 compila este arquivo com a CRT do XDK, fora do shim: a falha vai ao log e à tela vermelha */
void Diario_Escrever(const char *formato, ...);
void Port_Abortar(void);
#endif

static void falha(const char *formato, ...) {
    char linha[400];
    va_list ap;
    va_start(ap, formato);
    vsprintf(linha, formato, ap);
    va_end(ap);
#ifdef _XBOX
    Diario_Escrever("%s", linha);
    Port_Abortar();
#else
    fprintf(stderr, "%s\n", linha);
    abort();
#endif
}

/* src/battle/bobj_a.c, BtlObjAnim_Load: uma animação de personagem logo depois de Bpe_Decode */
void Port_ConvertAnim(void *buf, int size) {
    char msg[240];
    if (size <= 0) {
        return; /* "BPE vazio": nada decodificado */
    }
    if (Conv_Animacao((conv_u8 *)buf, (conv_ofs)size, msg, sizeof(msg)) < 0) {
        falha("conv: animação de 0x%X bytes: %s", (unsigned)size, msg);
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
        falha("conv: folha da tela de carregamento: sem memória");
    }
    r = Conv_Formato("folha", "folha da tela de carregamento", (conv_u8 *)buf, (conv_ofs)size, feito, msg, sizeof(msg));
    free(feito);
    if (r < 0) {
        falha("conv: folha da tela de carregamento de 0x%X bytes: %s", (unsigned)size, msg);
    }
}
