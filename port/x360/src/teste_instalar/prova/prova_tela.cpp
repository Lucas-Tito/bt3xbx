/* Prova host da tela de conversão: compila o desenho real (f2/iso_tela_desenho.h) com o D3D falso e
     prova_tela gravar DIR [VERSAO]   grava tela-*.ppm (1280x720) de estados
     prova_tela checar       confere o giro, as cores, a falha e os limites; sai com 1 e "FALHA:" se algo quebra */
#include "xtl.h"
#include <string.h>
#include <stdio.h>
#include "iso_tela_desenho.h"
static D3DDevice gA, gB;
static TelaEstado estado(int fase, unsigned pc, const unsigned feito[5], int atual, unsigned batidas, unsigned ms) {
    static const unsigned total[5] = {312, 40, 690, 30, 120};
    TelaEstado e;
    memset(&e, 0, sizeof(e));
    e.fase = fase;
    e.pc = pc;
    for (int i = 0; i < 5; i++) {
        e.c.total[i] = total[i];
        e.c.feito[i] = feito[i];
    }
    e.c.atual = atual;
    e.c.batidas = batidas;
    e.c.passe_total_mb = 2880;
    e.c.passe_mb = fase >= IC_VERIFICAR ? 2880 : pc * 2880 / 60;
    e.c.passe_blocos = batidas / 5;
    e.ms = ms;
    return e;
}
static void exigir(bool ok, const char *msg) {
    if (!ok) {
        std::fprintf(stderr, "FALHA: %s\n", msg);
        std::exit(1);
    }
}
static void ppm(const char *dir, const char *nome, const D3DDevice &d) {
    char caminho[512];
    std::snprintf(caminho, sizeof(caminho), "%s/%s.ppm", dir, nome);
    FILE *f = std::fopen(caminho, "wb");
    exigir(f != NULL, "não abriu o PPM");
    std::fprintf(f, "P6\n1280 720\n255\n");
    for (int i = 0; i < 1280 * 720; i++) {
        unsigned char rgb[3] = {(unsigned char)(d.px[i] >> 16), (unsigned char)(d.px[i] >> 8), (unsigned char)d.px[i]};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}
static unsigned contar(const D3DDevice &d, unsigned cor) {
    unsigned n = 0;
    for (int i = 0; i < 1280 * 720; i++)
        n += d.px[i] == cor;
    return n;
}
int main(int argc, char **argv) {
    static const unsigned zero[5] = {0, 0, 0, 0, 0}, ini[5] = {40, 0, 3, 5, 0}, meio[5] = {312, 12, 124, 30, 0},
                          tudo[5] = {312, 40, 690, 30, 120}, queda[5] = {312, 12, 124, 30, 0};
    const char *v = argc == 4 ? argv[3] : "1.0.0";
    if (argc < 2)
        return 2;
    if (!strcmp(argv[1], "gravar") && argc >= 3) {
        TelaEstado e = estado(IC_CONVERTER, 2, ini, IC_CAT_MENUS, 48, 41000);
        telaDesenhar(&gA, e, v);
        ppm(argv[2], "tela-1-inicio", gA);
        e = estado(IC_CONVERTER, 41, meio, IC_CAT_MENUS, 478, 372000);
        telaDesenhar(&gA, e, v);
        ppm(argv[2], "tela-2-meio", gA);
        e = estado(IC_VERIFICAR, 83, tudo, -1, 3366, 1311000);
        e.c.verif_total_mb = 2880;
        e.c.verif_mb = 1440;
        e.c.verif_blocos = 180;
        telaDesenhar(&gA, e, v);
        ppm(argv[2], "tela-3-verificacao", gA);
        e = estado(IC_PUBLICAR, 100, tudo, -1, 3366, 1500000);
        e.c.verif_total_mb = e.c.verif_mb = 2880;
        telaDesenhar(&gA, e, v);
        ppm(argv[2], "tela-4-pronto", gA);
        e = estado(IC_CONVERTER, 41, queda, IC_CAT_MENUS, 478, 372000);
        e.falhou = 1;
        telaDesenhar(&gA, e, v);
        ppm(argv[2], "tela-5-falha", gA);
        e = estado(IC_IDENTIFICAR, 0, zero, -1, 0, 0);
        e.c.total[0] = e.c.total[1] = e.c.total[2] = e.c.total[3] = e.c.total[4] = 0;
        telaDesenhar(&gA, e, v);
        ppm(argv[2], "tela-0-conferindo", gA);
        telaPergunta(&gA, v, ISO_CONV_TITULO, ISO_CONV_PERGUNTA, ISO_CONV_SIM, ISO_CONV_NAO, 1);
        ppm(argv[2], "pergunta-1-cancel", gA);
        telaPergunta(&gA, v, ISO_CONV_TITULO, ISO_CONV_PERGUNTA, ISO_CONV_SIM, ISO_CONV_NAO, 0);
        ppm(argv[2], "pergunta-2-convert", gA);
        telaPergunta(&gA, v, ISO_CONV_FALHA_TITULO, ISO_CONV_FALHA, ISO_CONV_OK, NULL, 0);
        ppm(argv[2], "pergunta-3-falha", gA);
        telaPergunta(&gA, v, ISO_CONV_FALHA_TITULO, ISO_CONV_DESTINO, ISO_CONV_OK, NULL, 0);
        ppm(argv[2], "pergunta-4-destino", gA);
        return 0;
    }
    if (!strcmp(argv[1], "checar")) {
        /* O giro depende só das batidas: mesmo estado, tempo diferente dentro do mesmo segundo, imagem igual. */
        TelaEstado e = estado(IC_CONVERTER, 41, meio, IC_CAT_MENUS, 5, 1000);
        telaDesenhar(&gA, e, v);
        e.ms = 1900;
        telaDesenhar(&gB, e, v);
        exigir(!memcmp(gA.px, gB.px, sizeof(gA.px)), "o giro mudou com o tempo, sem batida nova");
        /* Uma batida a mais, o mesmo tempo: o giro anda. */
        e.ms = 1000;
        e.c.batidas = 6;
        telaDesenhar(&gB, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)), "o giro não avançou com a batida");
        e.c.batidas = 7;
        telaDesenhar(&gB, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "o giro não avançou na batida seguinte");
        /* O giro fica só na linha da categoria em conversão (menus: a linha 8 da tela). */
        for (int i = 0; i < 1280 * 720; i++)
            if (gA.px[i] != gB.px[i])
                exigir(i / 1280 >= TELA_Y + 8 * TELA_PASSO && i / 1280 < TELA_Y + 9 * TELA_PASSO,
                       "o giro desenhou fora da linha da categoria atual");
        /* Todas as categorias prontas e a passagem ainda copiando o fim da ISO (o que o Lucas viu parado em
           1/11): o giro vai para a linha da cópia (linha 5) e anda por lote gravado, não por batida. */
        e = estado(IC_CONVERTER, 55, tudo, -1, 3366, 1000);
        e.c.passe_blocos = 10;
        telaDesenhar(&gA, e, v);
        e.c.batidas = 3367;
        telaDesenhar(&gB, e, v);
        exigir(!memcmp(gA.px, gB.px, sizeof(gA.px)), "a linha da cópia girou com batida de conversão");
        e.c.passe_blocos = 11;
        telaDesenhar(&gB, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "a cópia do fim da ISO não gira por lote");
        for (int i = 0; i < 1280 * 720; i++)
            if (gA.px[i] != gB.px[i])
                exigir(i / 1280 >= TELA_Y + 5 * TELA_PASSO && i / 1280 < TELA_Y + 6 * TELA_PASSO,
                       "o giro da cópia desenhou fora da linha da cópia");
        e.c.passe_mb = 2001;
        telaDesenhar(&gB, e, v);
        e.c.passe_mb = 2002;
        telaDesenhar(&gA, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "o contador de MB da cópia não aparece");
        /* Na verificação, o giro anda por bloco relido, na linha 11. */
        e = estado(IC_VERIFICAR, 80, tudo, -1, 3366, 1000);
        e.c.verif_total_mb = 2880;
        e.c.verif_blocos = 4;
        telaDesenhar(&gA, e, v);
        e.c.verif_blocos = 5;
        telaDesenhar(&gB, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "a verificação não gira por bloco relido");
        for (int i = 0; i < 1280 * 720; i++)
            if (gA.px[i] != gB.px[i])
                exigir(i / 1280 >= TELA_Y + 11 * TELA_PASSO && i / 1280 < TELA_Y + 12 * TELA_PASSO,
                       "o giro da verificação desenhou fora da linha dela");
        e.c.verif_mb = 1000;
        telaDesenhar(&gA, e, v);
        e.c.verif_mb = 1001;
        telaDesenhar(&gB, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "o contador de MB da verificação não aparece");
        /* Cores: sem falha, nada de vermelho e o OK é verde; com falha, o FAILED é vermelho e some o giro. */
        e = estado(IC_CONVERTER, 41, meio, IC_CAT_MENUS, 478, 372000);
        telaDesenhar(&gA, e, v);
        exigir(contar(gA, TELA_FALHA) == 0, "vermelho sem falha");
        exigir(contar(gA, TELA_OK) > 0, "sem OK verde");
        unsigned cinza = contar(gA, TELA_TEXTO);
        e.falhou = 1;
        telaDesenhar(&gB, e, v);
        exigir(contar(gB, TELA_FALHA) > 0, "falha sem FAILED vermelho");
        exigir(contar(gB, TELA_OK) == contar(gA, TELA_OK), "os OK mudaram com a falha");
        exigir(contar(gB, TELA_TEXTO) < cinza, "o giro continua na linha que falhou");
        /* Só pixels das quatro cores da tela. */
        for (int i = 0; i < 1280 * 720; i++)
            exigir(gB.px[i] == TELA_FUNDO || gB.px[i] == TELA_TEXTO || gB.px[i] == TELA_OK || gB.px[i] == TELA_FALHA,
                   "cor fora da paleta");
        /* A conferência ganha OK ao passar a converter; antes dela, nada verde. */
        e = estado(IC_IDENTIFICAR, 0, zero, -1, 0, 0);
        telaDesenhar(&gA, e, v);
        exigir(contar(gA, TELA_OK) == 0, "OK verde antes de conferir a ISO");
        e = estado(IC_PUBLICAR, 100, tudo, -1, 3366, 1);
        telaDesenhar(&gA, e, v);
        exigir(contar(gA, TELA_OK) > 0, "sem OK ao publicar");
        /* Barra e tempo mudam o desenho. */
        e = estado(IC_CONVERTER, 10, meio, IC_CAT_MENUS, 7, 0);
        telaDesenhar(&gA, e, v);
        e.pc = 11;
        e.ms = 61000;
        telaDesenhar(&gB, e, v);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)), "a barra e o tempo não aparecem");
        /* A pergunta própria: toda letra dos textos tem desenho, as linhas cabem em 60 colunas e na tela, só as quatro
           cores da paleta, e a seleção troca só os botões. */
        {
            static const wchar_t *const todos[] = {ISO_CONV_TITULO, ISO_CONV_PERGUNTA, ISO_CONV_SIM, ISO_CONV_NAO,
                                                   ISO_CONV_OK, ISO_CONV_SUCESSO, ISO_CONV_PRONTA,
                                                   ISO_CONV_ESPACO_TITULO, ISO_CONV_ESPACO, ISO_CONV_FALHA_TITULO,
                                                   ISO_CONV_FALHA, ISO_CONV_DESTINO};
            char a[600], linhas[TELA_P_LINHAS][TELA_P_COLUNAS + 1], botoes[96];
            for (unsigned k = 0; k < sizeof(todos) / sizeof(todos[0]); k++) {
                telaAscii(a, (int)sizeof(a), todos[k]);
                for (int i = 0; a[i]; i++)
                    exigir(a[i] == ' ' || indiceLetra(a[i]) >= 0, "caractere de texto sem desenho na fonte");
                int n = telaQuebrar(linhas, TELA_P_LINHAS, a), total = 0;
                exigir(n >= 1 && n < TELA_P_LINHAS, "mensagem sem linhas ou cortada");
                for (int i = 0; i < n; i++) {
                    exigir(strlen(linhas[i]) <= TELA_P_COLUNAS && linhas[i][0] != ' ', "linha longa ou com espaço na frente");
                    total += (int)strlen(linhas[i]) + (i + 1 < n);
                }
                exigir(total == (int)strlen(a), "a quebra perdeu texto");
            }
            telaBotoes(botoes, "CONVERT", "CANCEL", 1);
            exigir(!strcmp(botoes, "     CONVERT      > CANCEL <"), "botões com Cancel selecionado");
            telaBotoes(botoes, "CONVERT", "CANCEL", 0);
            exigir(!strcmp(botoes, "   > CONVERT <      CANCEL  "), "botões com Convert selecionado");
            telaBotoes(botoes, "OK", NULL, 0);
            exigir(!strcmp(botoes, "   > OK <"), "botão único");
            for (int k = 0; k < 3; k++) {
                const char *t = k == 0 ? botoes : k == 1 ? ISO_CONV_T_AJUDA_DOIS : ISO_CONV_T_AJUDA_UM;
                for (int i = 0; t[i]; i++)
                    exigir(t[i] == ' ' || indiceLetra(t[i]) >= 0, "caractere de botão ou ajuda sem desenho na fonte");
            }
            telaPergunta(&gA, v, ISO_CONV_TITULO, ISO_CONV_PERGUNTA, ISO_CONV_SIM, ISO_CONV_NAO, 1);
            telaPergunta(&gB, v, ISO_CONV_TITULO, ISO_CONV_PERGUNTA, ISO_CONV_SIM, ISO_CONV_NAO, 0);
            exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "a seleção não aparece");
            for (int i = 0; i < 1280 * 720; i++)
                exigir((gA.px[i] == TELA_FUNDO || gA.px[i] == TELA_TEXTO) && (gB.px[i] == TELA_FUNDO || gB.px[i] == TELA_TEXTO),
                       "cor fora da paleta na pergunta");
            /* O desenho só difere na linha dos botões: linha 6 (título, branco, caixa, 2 linhas, branco, botões). */
            for (int i = 0; i < 1280 * 720; i++)
                if (gA.px[i] != gB.px[i])
                    exigir(i / 1280 >= TELA_Y + 6 * TELA_PASSO && i / 1280 < TELA_Y + 7 * TELA_PASSO,
                           "a seleção mexeu fora da linha dos botões");
            /* Letras novas desenham algo. */
            exigir(indiceLetra('?') >= 0 && indiceLetra(',') >= 0, "faltam ? e , na fonte");
        }
        std::puts("prova_tela: tudo certo");
        return 0;
    }
    return 2;
}
