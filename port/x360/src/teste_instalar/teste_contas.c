/* Ensaio das contas da tela de conversão com um inventário sintético: nenhum byte do jogo, só formatos e tamanhos.
   Reproduz a ordem de chamadas do laço de IsoConv_Executar (planejar, comecar/concluir por entrada, passagem,
   verificação) e confere categorias, peso dos menus, batidas e o progresso monótono de 0 a 100. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iso_conv.h"
#define N 3366
#define FALHA(...)                                                                                                     \
    do {                                                                                                               \
        fprintf(stderr, "FALHA: " __VA_ARGS__);                                                                        \
        fputc('\n', stderr);                                                                                           \
        exit(1);                                                                                                       \
    } while (0)
typedef struct Item {
    const char *formato;
    conv_u32 tam, pos;
} Item;
static unsigned semente = 12345;
static unsigned sorteio(unsigned n) {
    semente = semente * 1103515245u + 12345u;
    return (semente >> 16) % n;
}
int main(void) {
    /* O mapa de formato para categoria, escrito à mão (o oráculo não usa o código testado). */
    static const struct {
        const char *formato;
        int cat;
    } mapa[] = {{"personagem", 0}, {"personagem_anim", 0}, {"personagem_efeitos", 0}, {"rosto", 0}, {"cenario", 1},
                {"menu_pacote", 2}, {"menu_bpe", 2},       {"menu_titulo", 2}, {"menu_cartao", 2},        {"menu_hist_lista", 2},
                {"som", 4},        {"comum1", 3},          {"comum4", 3},             {"hud", 3},
                {"folha_bpe", 3},  {"roteiro_luta", 3},    {"textos_luta", 3},        {"labio_luta", 3},
                {"formato_novo_qualquer", 3}, {"menu", 3}};
    static Item itens[N];
    static IcContas c;
    unsigned i, quantos[IC_NCATS] = {0}, antes_batidas, ultimo_pc = 0, distintos = 0;
    conv_u32 iso, total, menu_kib = 0, pos = 0, anterior = 0, f;
    const conv_u32 peso = IC_PESO_MENU;
    for (i = 0; i < sizeof(mapa) / sizeof(mapa[0]); i++)
        if (IsoContas_Categoria(mapa[i].formato) != mapa[i].cat)
            FALHA("formato %s caiu na categoria %d, esperado %d", mapa[i].formato, IsoContas_Categoria(mapa[i].formato),
                  mapa[i].cat);
    if (IsoContas_Categoria(NULL) != IC_CAT_BATALHA)
        FALHA("formato nulo deveria ser dado de luta");
    /* Inventário: 690 menus (os pacotes pesados), o resto espalhado e intercalado como na ISO. */
    for (i = 0; i < N; i++) {
        int k = i % 5 == 0 || i % 7 == 0 ? 2 : (int)sorteio(5);
        static const char *const f2[] = {"menu_pacote", "menu_bpe", "menu_titulo"};
        static const char *const f0[] = {"personagem", "personagem_anim", "personagem_efeitos", "rosto"};
        static const char *const f3[] = {"comum1", "hud", "roteiro_luta", "folha_bpe", "textos_luta"};
        if (k == 2)
            itens[i].formato = f2[sorteio(3)];
        else if (k == 0)
            itens[i].formato = f0[sorteio(4)];
        else if (k == 1)
            itens[i].formato = "cenario";
        else if (k == 3)
            itens[i].formato = f3[sorteio(5)];
        else
            itens[i].formato = "som";
        itens[i].tam = 2048 * (1 + sorteio(k == 2 ? 120 : 400)) + (sorteio(2) ? 100 : 0);
        itens[i].pos = pos;
        pos += itens[i].tam + 2048 * sorteio(3);
        for (f = 0; f < sizeof(mapa) / sizeof(mapa[0]); f++)
            if (!strcmp(mapa[f].formato, itens[i].formato))
                quantos[mapa[f].cat]++;
        if (!strncmp(itens[i].formato, "menu_", 5))
            menu_kib += (itens[i].tam + 1023) >> 10;
    }
    iso = ((pos + 2047) / 2048 * 2048 + 2048 * 100) >> 10;
    IsoContas_Iniciar(&c);
    for (i = 0; i < N; i++)
        IsoContas_Planejar(&c, itens[i].formato, itens[i].tam);
    for (i = 0; i < IC_NCATS; i++) {
        if (c.total[i] != quantos[i])
            FALHA("categoria %u: total %u, esperado %u", i, (unsigned)c.total[i], quantos[i]);
        if (!c.total[i])
            FALHA("categoria %u vazia no inventário sintético", i);
    }
    if (c.total[0] + c.total[1] + c.total[2] + c.total[3] + c.total[4] != N)
        FALHA("os totais por categoria não fecham em %d", N);
    if (peso == 0 || c.extra_total == 0)
        FALHA("peso de menu ausente: extra_total=%u, IC_PESO_MENU=%u", (unsigned)c.extra_total, (unsigned)peso);
    if (c.extra_total != peso * menu_kib)
        FALHA("extra dos menus %u, esperado %u", (unsigned)c.extra_total, (unsigned)(peso * menu_kib));
    total = IsoContas_Total(&c, iso);
    if (total != 3 * iso + peso * menu_kib)
        FALHA("total da barra %u não inclui o extra dos menus (esperado %u)", (unsigned)total,
              (unsigned)(3 * iso + peso * menu_kib));
    /* A passagem única, entrada a entrada. */
    if (IsoContas_Passagem(&c, 0) != 0)
        FALHA("a passagem não começa em 0");
    for (i = 0; i < N; i++) {
        conv_u32 p;
        IsoContas_Comecar(&c, itens[i].formato);
        if (c.atual != IsoContas_Categoria(itens[i].formato))
            FALHA("categoria atual errada na entrada %u", i);
        antes_batidas = c.batidas;
        IsoContas_Concluir(&c, itens[i].formato, itens[i].tam);
        if (c.batidas != antes_batidas + 1)
            FALHA("as batidas não avançaram uma por entrada convertida (entrada %u)", i);
        p = IsoContas_Passagem(&c, (itens[i].pos + itens[i].tam) >> 10);
        if (p < anterior)
            FALHA("progresso retrocedeu na passagem, entrada %u: %u < %u", i, (unsigned)p, (unsigned)anterior);
        anterior = p;
    }
    if (anterior > 2 * iso + c.extra_total)
        FALHA("a passagem passou do começo da verificação");
    for (i = 0; i < IC_NCATS; i++)
        if (c.feito[i] != c.total[i])
            FALHA("categoria %u: feitos %u de %u, a contagem não fecha", i, (unsigned)c.feito[i], (unsigned)c.total[i]);
    if (c.batidas != N)
        FALHA("batidas %u, esperado %d", (unsigned)c.batidas, N);
    if (c.extra_feito != c.extra_total)
        FALHA("o extra feito %u não fecha com o total %u", (unsigned)c.extra_feito, (unsigned)c.extra_total);
    /* Verificação: relê a ISO em blocos de 8 MiB. */
    for (f = 0;; f += 8192) {
        conv_u32 p;
        if (f > iso)
            f = iso;
        p = IsoContas_Verificacao(&c, iso, f);
        if (p < anterior)
            FALHA("progresso retrocedeu na verificação: %u < %u", (unsigned)p, (unsigned)anterior);
        anterior = p;
        if (f == iso)
            break;
    }
    if (anterior != total)
        FALHA("a verificação termina em %u, o total é %u", (unsigned)anterior, (unsigned)total);
    /* A porcentagem do adaptador: de 0 a 100, sem retroceder, com muitos valores distintos. */
    {
        IcContas d;
        unsigned pc, ant = 0;
        conv_u32 base = 0;
        memset(&d, 0, sizeof(d));
        IsoContas_Iniciar(&d);
        for (i = 0; i < N; i++)
            IsoContas_Planejar(&d, itens[i].formato, itens[i].tam);
        for (i = 0; i < N; i++) {
            IsoContas_Concluir(&d, itens[i].formato, itens[i].tam);
            base = IsoContas_Passagem(&d, (itens[i].pos + itens[i].tam) >> 10);
            pc = (unsigned)((double)base * 100.0 / total);
            if (pc < ant)
                FALHA("porcentagem retrocedeu");
            if (pc != ant)
                distintos++;
            ant = pc;
            ultimo_pc = pc;
        }
        for (f = 0; f < iso; f += 8192) {
            pc = (unsigned)((double)IsoContas_Verificacao(&d, iso, f) * 100.0 / total);
            if (pc < ant)
                FALHA("porcentagem retrocedeu na verificação");
            if (pc != ant)
                distintos++;
            ant = pc;
        }
        if (ant > 100 || ultimo_pc >= ant || distintos < 90)
            FALHA("porcentagem mal distribuída: %u valores distintos, passagem termina em %u%%", distintos, ultimo_pc);
    }
    printf("teste_contas: tudo certo (peso %u, extra %u KiB de %u, %u valores de porcentagem)\n", (unsigned)peso,
           (unsigned)c.extra_total, (unsigned)total, distintos);
    return 0;
}
