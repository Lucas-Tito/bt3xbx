/* O sistema de arquivos do 360 para a importação e a exportação de saves (conv/save_importa.c). */
#ifndef SAVE_IMPORTA_360_H
#define SAVE_IMPORTA_360_H
#include "save_importa.h"
#ifdef __cplusplus
extern "C" {
#endif
/* As três usam game:\import, game:\backup, game:\export e a pasta do save sob BT3_SAVES. */
int Importa360_Procurar(SiLista *lista);
int Importa360_Importar(const SiLista *lista, int indice, SiFalha *falha);
int Importa360_Pular(const SiLista *lista, SiFalha *falha);
/* O gancho do mc_commit (port/src/plat_mc.c): o jogo gravou 'caminho' com sucesso. Nunca falha. */
void Port_X360SaveGravado(const char *caminho);
#ifdef __cplusplus
}
#endif
#endif
