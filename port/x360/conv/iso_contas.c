/* Contas da tela de conversão (ver iso_contas.h). */
#include "iso_conv.h"
#include <string.h>
int IsoContas_Categoria(const char *formato) {
    if (formato == NULL)
        return IC_CAT_BATALHA;
    if (!strcmp(formato, "personagem") || !strcmp(formato, "personagem_anim") ||
        !strcmp(formato, "personagem_efeitos") || !strcmp(formato, "rosto"))
        return IC_CAT_PERSONAGENS;
    if (!strcmp(formato, "cenario"))
        return IC_CAT_CENARIOS;
    if (!strncmp(formato, "menu_", 5))
        return IC_CAT_MENUS;
    if (!strcmp(formato, "som"))
        return IC_CAT_SONS;
    return IC_CAT_BATALHA;
}
conv_u32 IsoContas_ExtraMenu(const char *formato, conv_u32 tam) {
    if (IsoContas_Categoria(formato) != IC_CAT_MENUS)
        return 0;
    return (conv_u32)IC_PESO_MENU * ((tam + 1023U) >> 10);
}
void IsoContas_Iniciar(IcContas *c) {
    memset(c, 0, sizeof(*c));
    c->atual = -1;
}
void IsoContas_Planejar(IcContas *c, const char *formato, conv_u32 tam) {
    c->total[IsoContas_Categoria(formato)]++;
    c->extra_total += IsoContas_ExtraMenu(formato, tam);
}
void IsoContas_Comecar(IcContas *c, const char *formato) { c->atual = IsoContas_Categoria(formato); }
void IsoContas_Concluir(IcContas *c, const char *formato, conv_u32 tam) {
    c->feito[IsoContas_Categoria(formato)]++;
    c->extra_feito += IsoContas_ExtraMenu(formato, tam);
    c->batidas++;
}
conv_u32 IsoContas_Total(const IcContas *c, conv_u32 iso) { return 3 * iso + c->extra_total; }
conv_u32 IsoContas_Passagem(const IcContas *c, conv_u32 pos_kib) { return 2 * pos_kib + c->extra_feito; }
conv_u32 IsoContas_Verificacao(const IcContas *c, conv_u32 iso, conv_u32 pos_kib) {
    return 2 * iso + c->extra_total + pos_kib;
}
