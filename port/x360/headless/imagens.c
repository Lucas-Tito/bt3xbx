/*
 * Imagens das telas pelo rasterizador de software (F4, etapa 6), nas builds sem janela (x86 e PPC): o mesmo momento
 * de um roteiro de controle (roteiro.c) nas duas builds, gravado em PPM, para comparar byte a byte
 * (port/x360/tools/roteiros.sh com IMG=1, roteiros.py imagens).
 *
 *   BT3_IMAGENS=<lista>   uma imagem por linha: "NOME N ÂNCORA...". A âncora é o texto de uma linha do rastro do
 *                         roteiro antes do " vblank " ("tela 39", "marca memoria 00923", "ok luta"; com "#K" no fim,
 *                         a K-ésima vez, "tela 4#2"); a imagem é a do quadro desenhado logo depois da leitura do
 *                         controle N quadros depois da âncora. Grava NOME.ppm na pasta da lista (640 x 448, o
 *                         quadro do GS que o jogo mostra, como o screenshot() do gs_core.c) e diz
 *                         "imagem: NOME quadro Q vblank V gs G" na saída (G: o quadro do GS da imagem, o número do
 *                         BT3_GS_DUMP mais o BT3_GS_FROM; fora do rastro: não começa com "roteiro:").
 *   BT3_IMAGENS_JANELA=W  quadros desenhados antes de cada imagem (padrão 8). Só vale com BT3_GS=1 e
 *                         BT3_GS_FROM=D (D bem maior que o número de quadros da rodada): o gs_core.c desenha só os
 *                         quadros do GS a partir de D, e aqui o contador de quadros do GS (gGsFrame) passa a contar
 *                         a partir de D durante a janela de cada imagem e volta depois. Fora das janelas o GS
 *                         processa tudo (cargas de textura, registradores, VU1) e não pinta nada: na PPC, no qemu, o
 *                         rasterizador pintando todo quadro leva umas três horas por roteiro; assim, minutos.
 *
 * A âncora é uma linha do rastro, não um vblank: as duas builds capturam o mesmo momento do roteiro mesmo que uma
 * chegue à tela em outro vblank (o roteiros.sh confere também que o rastro das rodadas com imagens é o da
 * referência). Por que a janela não muda a imagem: cada quadro do jogo começa limpando o buffer que vai desenhar
 * (sceGsSwapDBuff, gs_sony.c), e as texturas chegam por carga, que o GS processa sempre; o que um quadro desenha
 * para outro ler (um alvo fora da tela) cabe na janela. Conferido na x86 contra a rodada pintando todo quadro
 * (plano, etapa 6).
 *
 * Entra por --wrap no Port_GsVif1Chain (a lista de cada quadro, plat_gs.c) e no Port_GsGifChannel (as cargas e a
 * limpeza fora da lista, plat_gs.c e gs_sony.c), headless.py. Sem as variáveis nada muda: os dois só chamam o
 * original. O 360 não compila este arquivo.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/gs/gs_internal.h" /* gGs (o registrador FRAME), gGsFrame, Gs_VramRead, Gs_PsmBits */
#undef getenv                         /* (o gs_internal.h guarda a resposta por ponto de chamada) */

extern int gGsMainFbp;        /* gs_sony.c: o buffer do quadro que o jogo desenha (sceGsSwapDBuff) */
extern unsigned gPortVBlanks; /* plat_stub.c */
extern void __real_Port_GsVif1Chain(uint32_t tadr, int tte);
extern void __real_Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain);

#define MAX_IMAGENS 64

static struct {
    char nome[64];
    char ancora[96]; /* "TEXTO vblank " */
    int vez, vistas; /* a K-ésima linha com a âncora; quantas já vieram */
    int n;
    long alvo;       /* o quadro do roteiro da imagem, -1 antes da âncora */
    int feita, pendente; /* gravada; a gravar depois da lista seguinte */
} sImg[MAX_IMAGENS];
static int sN = -1;
static char sPasta[300];
static int sJanela = 8;
static unsigned sDesloc;     /* BT3_GS_FROM: somado ao gGsFrame enquanto a janela desenha */
static int sDesenha;         /* a lista seguinte é desenhada */
static int sCaptura;         /* e, depois dela, grava as imagens pendentes (mais de uma no mesmo quadro) */
static unsigned sQuadroAgora;

static void iniciar(void) {
    const char *lista = getenv("BT3_IMAGENS"), *e;
    char buf[300];
    FILE *fp;
    sN = 0;
    if (lista == NULL || lista[0] == '\0') {
        return;
    }
    e = getenv("BT3_IMAGENS_JANELA");
    if (e != NULL) {
        sJanela = atoi(e);
    }
    e = getenv("BT3_GS_FROM");
    sDesloc = e != NULL ? (unsigned)strtoul(e, NULL, 10) : 0;
    snprintf(sPasta, sizeof(sPasta), "%s", lista);
    if (strrchr(sPasta, '/') != NULL) {
        strrchr(sPasta, '/')[1] = '\0';
    } else {
        sPasta[0] = '\0';
    }
    fp = fopen(lista, "r");
    if (fp == NULL) {
        printf("imagem: FALHA: não abre %s\n", lista);
        exit(2);
    }
    while (fgets(buf, sizeof(buf), fp) != NULL && sN < MAX_IMAGENS) {
        char nome[64], *anc, *k;
        int n, pos = 0;
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0' || buf[0] == '#') {
            continue;
        }
        if (sscanf(buf, "%63s %d %n", nome, &n, &pos) != 2 || pos == 0 || buf[pos] == '\0') {
            printf("imagem: FALHA: linha ruim em %s: %s\n", lista, buf);
            exit(2);
        }
        anc = buf + pos;
        sImg[sN].vez = 1;
        k = strrchr(anc, '#');
        if (k != NULL) {
            sImg[sN].vez = atoi(k + 1);
            *k = '\0';
        }
        snprintf(sImg[sN].nome, sizeof(sImg[sN].nome), "%s", nome);
        snprintf(sImg[sN].ancora, sizeof(sImg[sN].ancora), "%s vblank ", anc);
        sImg[sN].n = n;
        sImg[sN].alvo = -1;
        sN++;
    }
    fclose(fp);
}

/* Cada linha do rastro do roteiro (roteiro.c, diz), no quadro do roteiro Q: arma as imagens ancoradas nela. */
void Imagens_Evento(const char *linha, unsigned q) {
    int i;
    if (sN < 0) {
        iniciar();
    }
    /* o texto da linha, sem o "roteiro: arquivo:linha " das esperas e marcas (o mesmo que o roteiros.py imagens conta):
       a âncora é o texto inteiro antes do " vblank ", então "tela 4" não casa com "tela 44" nem com "ok tela 4" */
    if (strncmp(linha, "roteiro: ", 9) == 0) {
        linha += 9;
        linha += strcspn(linha, " ");
        if (*linha == ' ') {
            linha++;
        }
    }
    for (i = 0; i < sN; i++) {
        if (sImg[i].alvo < 0 && strncmp(linha, sImg[i].ancora, strlen(sImg[i].ancora)) == 0 &&
            ++sImg[i].vistas == sImg[i].vez) {
            sImg[i].alvo = (long)q + sImg[i].n;
        }
    }
}

/* Cada leitura do controle do jogador 1 (roteiro.c, Roteiro_Pad), no quadro do roteiro Q, depois das linhas do rastro
   dele: decide se a lista seguinte se desenha e se grava uma imagem. */
void Imagens_Quadro(unsigned q) {
    int i;
    if (sN < 0) {
        iniciar();
    }
    sQuadroAgora = q;
    sDesenha = 0;
    for (i = 0; i < sN; i++) {
        if (sImg[i].alvo >= 0 && !sImg[i].feita && (long)q <= sImg[i].alvo && (long)q + sJanela >= sImg[i].alvo) {
            sDesenha = 1;
            if ((long)q == sImg[i].alvo) {
                sImg[i].pendente = 1;
                sCaptura = 1;
            }
        }
    }
}

static void grava(int i) {
    static uint8_t px[640 * 448 * 3];
    char caminho[400];
    uint32_t fbw = 10, psm = 0, x, y;
    FILE *fp;
    int k;
    if (gGsMainFbp < 0) {
        printf("imagem: FALHA: %s sem buffer de quadro (antes do primeiro sceGsSwapDBuff)\n", sImg[i].nome);
        exit(2);
    }
    /* o FRAME do buffer mostrado, como o screenshot() do gs_core.c: o de um dos contextos que desenha nele */
    for (k = 0; k < 2; k++) {
        if ((gGs.frame[k] & 0x1FF) == (uint64_t)gGsMainFbp) {
            fbw = (uint32_t)(gGs.frame[k] >> 16) & 0x3F;
            psm = (uint32_t)(gGs.frame[k] >> 24) & 0x3F;
            break;
        }
    }
    for (y = 0; y < 448; y++) {
        for (x = 0; x < 640; x++) {
            uint32_t c = x < (fbw ? fbw : 10) * 64 ? Gs_VramRead((uint32_t)gGsMainFbp << 5, fbw ? fbw : 10, psm, x, y) : 0;
            uint8_t *o = &px[(y * 640 + x) * 3];
            if (Gs_PsmBits(psm) == 16) {
                c = (c & 31) << 3 | ((c >> 5) & 31) << 11 | ((c >> 10) & 31) << 19;
            }
            o[0] = (uint8_t)c;
            o[1] = (uint8_t)(c >> 8);
            o[2] = (uint8_t)(c >> 16);
        }
    }
    snprintf(caminho, sizeof(caminho), "%s%s.ppm", sPasta, sImg[i].nome);
    fp = fopen(caminho, "wb");
    if (fp == NULL || fprintf(fp, "P6\n640 448\n255\n") < 0 || fwrite(px, 1, sizeof(px), fp) != sizeof(px) || fclose(fp) != 0) {
        printf("imagem: FALHA: não grava %s\n", caminho);
        exit(2);
    }
    sImg[i].feita = 1;
    printf("imagem: %s quadro %u vblank %u gs %u fbp %03x fbw %u psm %02x\n", sImg[i].nome, sQuadroAgora, gPortVBlanks,
           gGsFrame - 1, (unsigned)gGsMainFbp, fbw, psm);
    fflush(stdout);
}

void __wrap_Port_GsVif1Chain(uint32_t tadr, int tte) {
    int desenha = sDesenha && sDesloc != 0;
    if (desenha) {
        gGsFrame += sDesloc;
    }
    __real_Port_GsVif1Chain(tadr, tte);
    if (desenha) {
        gGsFrame -= sDesloc;
    }
    if (sCaptura) {
        int i;
        for (i = 0; i < sN; i++) {
            if (sImg[i].pendente) {
                sImg[i].pendente = 0;
                grava(i);
            }
        }
        sCaptura = 0;
    }
}

void __wrap_Port_GsGifChannel(uint32_t addr, uint32_t qwc, int chain) {
    int desenha = sDesenha && sDesloc != 0;
    if (desenha) {
        gGsFrame += sDesloc;
    }
    __real_Port_GsGifChannel(addr, qwc, chain);
    if (desenha) {
        gGsFrame -= sDesloc;
    }
}
