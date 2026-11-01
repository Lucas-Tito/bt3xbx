/* Contas da tela de conversão: categorias com contagem, peso extra dos menus e batidas.
   Só aritmética em C89, sem E/S, para o núcleo, a tela e os ensaios usarem o mesmo código. */
#ifndef ISO_CONTAS_H
#define ISO_CONTAS_H
#include "conv.h"
/* Cada entrada convertida pertence a uma categoria pelo formato (Conv_FormatoDe). */
enum { IC_CAT_PERSONAGENS, IC_CAT_CENARIOS, IC_CAT_MENUS, IC_CAT_BATALHA, IC_CAT_SONS, IC_NCATS };
typedef struct IcContas {
    conv_u32 total[IC_NCATS], feito[IC_NCATS];
    /* Peso extra dos menus em KiB (IC_PESO_MENU por KiB da entrada): planejado e já convertido. */
    conv_u32 extra_total, extra_feito;
    /* Avança uma vez por entrada convertida: é o que gira o indicador da tela. */
    conv_u32 batidas;
    /* Categoria da entrada em conversão agora, ou -1. */
    int atual;
} IcContas;
#ifdef __cplusplus
extern "C" {
#endif
/* characters: personagem, personagem_anim, personagem_efeitos, rosto; stages: cenario; menus: tudo que
   começa com menu_; sounds: som; battle data: o resto (formato NULL também). */
int IsoContas_Categoria(const char *formato);
conv_u32 IsoContas_ExtraMenu(const char *formato, conv_u32 tam);
void IsoContas_Iniciar(IcContas *c);
void IsoContas_Planejar(IcContas *c, const char *formato, conv_u32 tam);
void IsoContas_Comecar(IcContas *c, const char *formato);
void IsoContas_Concluir(IcContas *c, const char *formato, conv_u32 tam);
/* Unidades da barra, em KiB. iso = tamanho da ISO em KiB. A passagem única lê e grava (2 partes), a
   verificação relê (1 parte); os menus acrescentam o extra uma vez, na passagem. */
conv_u32 IsoContas_Total(const IcContas *c, conv_u32 iso);
conv_u32 IsoContas_Passagem(const IcContas *c, conv_u32 pos_kib);
conv_u32 IsoContas_Verificacao(const IcContas *c, conv_u32 iso, conv_u32 pos_kib);
#ifdef __cplusplus
}
#endif
#endif
