/* Instalação síncrona, chamada somente pela thread de hardware 2, antes do jogo. */
#ifndef ISO_INSTALAR_H
#define ISO_INSTALAR_H
#include "iso_contas.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Contagem por categoria e batidas, lidas por Interlocked (a tela só lê). */
void IsoInstalar_Contas(IcContas *c);
int IsoInstalar_Limpar(void);
int IsoInstalar_Executar(const char *origem);
void IsoInstalar_Progresso(int *fase, unsigned *porcento);
#ifdef __cplusplus
}
#endif
#endif
