/* Prova host da tela SAVE IMPORT: compila o desenho real (f2/importa_tela_desenho.h) com o D3D falso e
     prova_importa gravar DIR [VERSAO]   grava importa-*.ppm (1280x720) de estados
     prova_importa checar                confere fonte, colunas, paleta, seleção e janela; sai com 1 e "FALHA:" se algo quebra */
#include "xtl.h"
#include <string.h>
#include <stdio.h>
#include "importa_tela_desenho.h"
static D3DDevice gA, gB;
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
static SiLista gLista;
static void item(int i, const char *nome, int erro) {
    strcpy(gLista.item[i].nome, nome);
    gLista.item[i].erro = erro;
    if (i + 1 > gLista.n)
        gLista.n = i + 1;
}
static void lista3(bool haSave) {
    memset(&gLista, 0, sizeof(gLista));
    item(0, "SAVE_BT3.psu", 0);
    item(1, "a_very_long_file_name_for_a_ps2_save_backup_2007.psu", PSU_E_BT3_PASTA);
    item(2, "damaged.PSU", SAVECONV_E_SOMA_PS2);
    gLista.ha_save = haSave;
}
static void todaLetra(const char *t, const char *onde) {
    for (int i = 0; t[i]; i++)
        if (!(t[i] == ' ' || indiceLetra(t[i]) >= 0)) {
            std::fprintf(stderr, "FALHA: caractere '%c' sem desenho na fonte (%s)\n", t[i], onde);
            std::exit(1);
        }
}
static void paleta(const D3DDevice &d, const char *msg) {
    for (int i = 0; i < 1280 * 720; i++)
        exigir(d.px[i] == TELA_FUNDO || d.px[i] == TELA_TEXTO || d.px[i] == TELA_OK || d.px[i] == TELA_FALHA ||
                   d.px[i] == IMP_SEL,
               msg);
}
static ImpMsg resultado(int r, SiFalha f, bool skip) {
    ImpMsg m;
    memset(&m, 0, sizeof(m));
    impResultado(m, r, f, skip, 2, 1);
    return m;
}
int main(int argc, char **argv) {
    const char *v = argc == 4 ? argv[3] : "1.0.1";
    SiFalha f;
    memset(&f, 0, sizeof(f));
    if (argc < 2)
        return 2;
    if (!strcmp(argv[1], "gravar") && argc >= 3) {
        lista3(true);
        telaImportar(&gA, v, gLista, 0, 0, false);
        ppm(argv[2], "importa-1-import", gA);
        telaImportar(&gA, v, gLista, 0, 1, false);
        ppm(argv[2], "importa-2-skip-ao-abrir", gA);
        telaImportar(&gA, v, gLista, 1, 0, true);
        ppm(argv[2], "importa-3-recusado", gA);
        lista3(false);
        telaImportar(&gA, v, gLista, 0, 0, false);
        ppm(argv[2], "importa-4-sem-save-atual", gA);
        memset(&gLista, 0, sizeof(gLista));
        for (int i = 0; i < 12; i++) {
            char n[32];
            sprintf(n, "save_%02d.psu", i + 1);
            item(i, n, i % 5 == 3 ? PSU_E_NAO_PSU : 0);
        }
        gLista.ha_save = 1;
        telaImportar(&gA, v, gLista, 10, 0, false);
        ppm(argv[2], "importa-5-lista-longa", gA);
        ImpMsg m;
        memset(&m, 0, sizeof(m));
        m.titulo = ISO_IMP_T_TRABALHANDO_TITULO;
        m.cor = TELA_TEXTO;
        strcpy(m.l[0], ISO_IMP_T_TRABALHANDO);
        m.n = 1;
        telaImportaMsg(&gA, v, m);
        ppm(argv[2], "importa-6-trabalhando", gA);
        f.backup = 1;
        telaImportaMsg(&gA, v, resultado(0, f, false));
        ppm(argv[2], "importa-7-feito", gA);
        memset(&f, 0, sizeof(f));
        f.passo = SI_P_TROCA; f.sistema = 112; f.restaurado = 1; f.backup = 2;
        telaImportaMsg(&gA, v, resultado(SI_E_TROCA, f, false));
        ppm(argv[2], "importa-8-falha-troca", gA);
        f.restaurado = 0;
        telaImportaMsg(&gA, v, resultado(SI_E_TROCA, f, false));
        ppm(argv[2], "importa-9-falha-troca-sem-restaurar", gA);
        memset(&f, 0, sizeof(f));
        f.passo = SI_P_BACKUP; f.sistema = 39;
        telaImportaMsg(&gA, v, resultado(SI_E_BACKUP, f, false));
        ppm(argv[2], "importa-10-falha-backup", gA);
        memset(&f, 0, sizeof(f));
        f.passo = SI_P_MOVER; f.importado = 1; f.movidos_mal = 2; f.sistema = 32;
        telaImportaMsg(&gA, v, resultado(SI_E_MOVER, f, false));
        ppm(argv[2], "importa-11-falha-mover", gA);
        memset(&f, 0, sizeof(f));
        f.passo = SI_P_MOVER; f.sistema = 32;
        telaImportaMsg(&gA, v, resultado(2, f, true));
        ppm(argv[2], "importa-12-skip-incompleto", gA);
        return 0;
    }
    if (!strcmp(argv[1], "checar")) {
        /* toda letra dos textos fixos e dos motivos tem desenho */
        static const char *const fixos[] = {ISO_IMP_T_TITULO, ISO_IMP_T_ACHADOS, ISO_IMP_T_AVISO, ISO_IMP_T_IMPORTAR,
            ISO_IMP_T_PULAR, ISO_IMP_T_AJUDA, ISO_IMP_T_RECUSADO, ISO_IMP_T_OK, ISO_IMP_T_TRABALHANDO,
            ISO_IMP_T_TRABALHANDO_TITULO, ISO_IMP_T_AJUDA_OK, ISO_IMP_T_FEITO_TITULO, ISO_IMP_T_FEITO,
            ISO_IMP_T_FEITO_BACKUP, ISO_IMP_T_FEITO_ARQUIVOS, ISO_IMP_T_FALHA_TITULO, ISO_IMP_T_PASSO, ISO_IMP_T_MOTIVO,
            ISO_IMP_T_SISTEMA, ISO_IMP_T_INTACTO, ISO_IMP_T_RECOLOCADO, ISO_IMP_T_NAO_RECOLOCADO, ISO_IMP_T_COPIA,
            ISO_IMP_T_MOVER, ISO_IMP_T_MOVER_DE_NOVO, ISO_IMP_T_PULAR_TITULO, ISO_IMP_T_PULAR_MOVER, ISO_IMP_T_PULAR_DE_NOVO, ISO_IMP_T_REMOVER, ISO_IMP_T_FEITO_PULADOS, ISO_IMP_T_FEITO_RECUSADOS};
        for (unsigned k = 0; k < sizeof(fixos) / sizeof(fixos[0]); k++)
            todaLetra(fixos[k], "texto fixo");
        /* os caracteres comuns em nome de arquivo (sublinhado, parênteses de "arquivo (1).psu") têm desenho */
        todaLetra("save_1 (2)-3.psu", "nome de arquivo");
        /* as linhas da tela de escolha cabem nas colunas: as que não quebram */
        {
            char b[200];
            sprintf(b, ISO_IMP_T_TITULO, "1.0.1");
            exigir(strlen(b) <= IMP_COLUNAS, "o título passa das colunas");
            /* o aviso do backup tem 67 letras, uma a mais que IMP_COLUNAS: cabe porque a última é um ponto (estreito); a prova
               de que cabe é o Clear do D3D falso, que recusa retângulo fora da tela, ao desenhá-lo abaixo */
            exigir(strlen(ISO_IMP_T_AVISO) <= IMP_COLUNAS + 1, "o aviso do backup passa das colunas");
            exigir(strlen(ISO_IMP_T_AJUDA) <= IMP_COLUNAS, "a ajuda passa das colunas");
            exigir(strlen(ISO_IMP_T_RECUSADO) <= IMP_COLUNAS, "o aviso do recusado passa das colunas");
            exigir(strlen(ISO_IMP_T_ACHADOS) + 8 <= IMP_COLUNAS, "o título da lista com a contagem passa das colunas");
        }
        /* todos os motivos: com desenho, e a linha (marca, nome, motivo) cabe em 66 colunas */
        {
            static const int codigos[] = {PSU_E_ARGS, PSU_E_NAO_PSU, PSU_E_TRUNCADO, PSU_E_ENTRADA, PSU_E_NOME, PSU_E_SOBRA,
                PSU_E_MUITOS, PSU_E_ESPACO, PSU_E_BT3_PASTA, PSU_E_BT3_FALTA, PSU_E_BT3_TAMANHO, PSU_E_BT3_EXTRA,
                SAVECONV_E_ARGS, SAVECONV_E_SOMA_PS2, SAVECONV_E_SOMA, SAVECONV_E_TAMANHO, SI_E_GRANDE, SI_E_LER,
                SI_E_RECUSADO, SI_E_CAMINHO, SI_E_BACKUP, SI_E_GRAVAR, SI_E_TROCA, SI_E_MOVER, SI_E_PASTA, SI_E_ESCOLHA,
                SI_E_EXPORTA};
            for (unsigned k = 0; k < sizeof(codigos) / sizeof(codigos[0]); k++) {
                const char *t = SaveImp_TextoErro(codigos[k]);
                todaLetra(t, "motivo");
                exigir(strlen(t) > 0 && strcmp(t, "unknown error") != 0, "código sem texto");
                exigir(2 + IMP_NOME + 1 + strlen(t) <= IMP_COLUNAS, "o motivo passa das colunas");
            }
            for (int p = SI_P_LER; p <= SI_P_EXPORTA_GRAVAR; p++)
                todaLetra(SaveImp_TextoPasso(p), "passo");
        }
        /* o nome longo vira reticências; o curto fica */
        {
            char out[64];
            impNome(out, "a_very_long_file_name_for_a_ps2_save_backup_2007.psu");
            exigir(strlen(out) == IMP_NOME && !strcmp(out + IMP_NOME - 3, "..."), "o nome longo não foi truncado com reticências");
            impNome(out, "save.psu");
            exigir(!strcmp(out, "save.psu"), "o nome curto mudou");
            impNome(out, "1234567890123456789012345.p!");
            exigir(!strcmp(out, "1234567890123456789012345.p!"), "o nome de 28 colunas foi truncado");
        }
        /* a janela da lista acompanha o selecionado */
        exigir(impInicio(5, 4) == 0 && impInicio(8, 7) == 0, "janela de lista curta");
        exigir(impInicio(12, 0) == 0 && impInicio(12, 7) == 0 && impInicio(12, 8) == 1 && impInicio(12, 11) == 4,
               "a janela não acompanha o selecionado");
        /* a tela de escolha: só a paleta, tudo dentro da tela (o D3D falso recusa retângulo fora), verde e vermelho */
        lista3(true);
        telaImportar(&gA, v, gLista, 0, 0, false);
        paleta(gA, "cor fora da paleta na tela de escolha");
        exigir(contar(gA, TELA_OK) > 0 && contar(gA, TELA_FALHA) > 0, "a lista sem OK verde ou sem motivo vermelho");
        exigir(contar(gA, IMP_SEL) > 0, "o arquivo selecionado não se destaca");
        /* sem o aviso do recusado, sem vermelho além dos motivos: com o aviso há mais vermelho */
        telaImportar(&gB, v, gLista, 0, 0, true);
        exigir(contar(gB, TELA_FALHA) > contar(gA, TELA_FALHA), "o aviso do recusado não aparece");
        /* a ação muda só a linha dos botões: linhas 0 a 7 (título, branco, achados, 3 itens, 3 vazias), aviso 12, botões 14 */
        telaImportar(&gB, v, gLista, 0, 1, false);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "a seleção IMPORT/SKIP não aparece");
        for (int i = 0; i < 1280 * 720; i++)
            if (gA.px[i] != gB.px[i])
                exigir(i / 1280 >= TELA_Y + 14 * TELA_PASSO && i / 1280 < TELA_Y + 15 * TELA_PASSO,
                       "a ação mexeu fora da linha dos botões");
        /* o arquivo selecionado muda só a lista */
        telaImportar(&gB, v, gLista, 2, 0, false);
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "o arquivo selecionado não aparece");
        for (int i = 0; i < 1280 * 720; i++)
            if (gA.px[i] != gB.px[i])
                exigir(i / 1280 >= TELA_Y + 3 * TELA_PASSO && i / 1280 < TELA_Y + 6 * TELA_PASSO,
                       "o arquivo selecionado mexeu fora das linhas da lista");
        /* sem save atual não há o aviso do backup */
        lista3(false);
        telaImportar(&gB, v, gLista, 0, 0, false);
        lista3(true);
        telaImportar(&gA, v, gLista, 0, 0, false);
        exigir(contar(gB, TELA_TEXTO) < contar(gA, TELA_TEXTO), "o aviso do backup não depende de haver save atual");
        for (int i = 0; i < 1280 * 720; i++)
            if (gA.px[i] != gB.px[i])
                exigir(i / 1280 >= TELA_Y + 12 * TELA_PASSO && i / 1280 < TELA_Y + 13 * TELA_PASSO,
                       "o aviso do backup fora da sua linha");
        /* a lista longa cabe na tela e rola */
        memset(&gLista, 0, sizeof(gLista));
        for (int i = 0; i < 40; i++)
            item(i, "x.psu", 0);
        telaImportar(&gA, v, gLista, 0, 1, false);
        telaImportar(&gB, v, gLista, 39, 1, false);
        paleta(gB, "cor fora da paleta na lista longa");
        exigir(contar(gB, IMP_SEL) > 0, "a lista longa não rola: o selecionado ficou fora da janela");
        exigir(memcmp(gA.px, gB.px, sizeof(gA.px)) != 0, "a lista longa não rola");
        /* as telas de resultado: cabem, paleta, e cada caminho diz o que aconteceu com o save */
        {
            SiFalha g;
            ImpMsg m;
            memset(&g, 0, sizeof(g));
            g.backup = 3;
            m = resultado(0, g, false);
            exigir(m.cor == TELA_OK && m.n >= 2, "resultado de sucesso sem verde ou sem linhas");
            telaImportaMsg(&gA, v, m);
            paleta(gA, "cor fora da paleta no resultado");
            g.passo = SI_P_GRAVAR; g.sistema = 5;
            m = resultado(SI_E_GRAVAR, g, false);
            exigir(strstr(m.l[0], "WRITING") != NULL && strstr(m.l[2], "5") != NULL, "falha de gravação sem passo ou código");
            exigir(strstr(m.l[m.n - 1], "NOT CHANGED") != NULL, "falha de gravação não diz que o save ficou intacto");
            g.passo = SI_P_TROCA; g.restaurado = 0; g.backup = 4;
            m = resultado(SI_E_TROCA, g, false);
            exigir(strstr(m.l[m.n - 1], "BACKUP\\004") != NULL, "troca sem restauração não aponta o backup");
            g.restaurado = 1;
            m = resultado(SI_E_TROCA, g, false);
            exigir(strstr(m.l[m.n - 1], "PUT BACK") != NULL, "troca restaurada não diz que recolocou");
            memset(&g, 0, sizeof(g));
            g.passo = SI_P_MOVER; g.importado = 1; g.movidos_mal = 3;
            m = resultado(SI_E_MOVER, g, false);
            exigir(strstr(m.l[0], "3 FILE") != NULL, "falha de movimento sem a contagem");
            g.passo = SI_P_LER; g.importado = 0; g.codigo = SAVECONV_E_SOMA_PS2;
            m = resultado(SAVECONV_E_SOMA_PS2, g, false);
            exigir(strstr(m.l[1], "CHECKSUM") != NULL, "falha de leitura sem o motivo da conferência");
            for (int k = 0; k < m.n; k++)
                todaLetra(m.l[k], "linha de resultado");
        }
        std::puts("prova_importa: tudo certo");
        return 0;
    }
    return 2;
}
