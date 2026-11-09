/* O amostrador de perfil da thread do jogo (amostra360.cpp; BT3_AMOSTRA=US no environment.txt). Sem a chave as três
   voltam logo, sem alocar nem criar thread. */
#ifndef AMOSTRA360_H
#define AMOSTRA360_H
#include <xtl.h>

#ifdef __cplusplus
extern "C" {
#endif

void Amostra_Preparar(void);                  /* a main, antes de criar a thread do jogo: a chave, a memória, o autoteste */
void Amostra_Comecar(HANDLE jogo, DWORD id);  /* a main, depois de soltar a thread do jogo */
void Amostra_Parar(void);                     /* Port_Sair (a thread do jogo): o último despejo */

#ifdef __cplusplus
}
#endif

#endif
