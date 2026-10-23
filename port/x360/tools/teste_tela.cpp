/* O teste da tela por imagem (BT3_PICOS_TELA, docs/x360/plano/F5/08-gravacao.md, seção 25), sem o XDK: o teste_tela.sh
   recorta do gs_d3d.cpp os anéis e o tela_* e os inclui aqui (TELA_H, TELA_C), com o relógio, a DPC, o QuerySwapStatus e
   os carimbos do picos simulados. Uma cena de 9 trocas com uma imagem de 3 vblanks, uma de 1 e um engasgo de 4 escondido
   por uma repetição; confere o resumo da janela e a folga da GPU na linha da imagem de 3. Sai com 1 se algo não bate. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int DWORD;
#define __int64 long long
#define WINAPI
static unsigned long long gTb;
static unsigned long long __mftb(void) { return gTb; }
static void __lwsync(void) {}
struct D3DSWAP_STATUS { DWORD Flags, Swap, EnqueuedCount; };
struct Dev {
    DWORD swaps;
    void QuerySwapStatus(D3DSWAP_STATUS *s) { s->Swap = swaps; }
};
static Dev gDev;
static Dev *sDev = &gDev;
#define PICOS_N 16
struct PicoJ { unsigned q, esperas, vblanks, vblank, isoN, isoKb, diario; float per, espera, vb, isoMs, discoMs; unsigned long long tb; };
struct PicoG { unsigned q, qJogo, texN, texKpx, envios, envioKb, hash, lotes, desenhos, verts, vuVerts, vuIdx, press, descN, esperasMem;
               unsigned xg[3], xgKb, soltas, memMede; float per, ocupada, espDes, ocioso, espMem, texMs, xgMs; unsigned long long tb; };
struct PicoD { unsigned q, captura, repete; float per, fim, present, cova, copiaVu; unsigned long long marca; };
static PicoJ sPJ[PICOS_N];
static PicoG sPG[PICOS_N];
static PicoD sPD[PICOS_N];
static volatile unsigned long long sPicoGpu[PICOS_N];
static double sPicoTbMs = 1000.0; /* (o relógio do teste em microssegundos) */
static int gJanela = 1;
static int Passos_Medindo(void) { return gJanela; }
static char gResumo[2048], gLinhaGpu[2048];
static int gLinhas;
static void Diario_Escrever(const char *f, ...) {
    char b[2048];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    puts(b);
    if (strncmp(b, "picos: tela, janela", 19) == 0) {
        strcpy(gResumo, b);
    } else if (strncmp(b, "picos: tela:", 12) == 0) {
        gLinhas++;
        if (strstr(b, "a imagem do quadro 3 ") != NULL) {
            strcpy(gLinhaGpu, b);
        }
    }
}
#include TELA_H
#include TELA_C

#define V 16667ull /* um vblank em microssegundos */
static void dpc(DWORD swap, unsigned n) { /* a troca entrou no vblank n (o que a vblank_cb faz com a chave) */
    TelaEnt *e = &sTelaEnt[swap % TELA_N];
    e->swap = swap;
    e->n = n;
    e->tb = (unsigned long long)n * V;
    sTelaUlt = swap;
    sTelaTem = 1;
}
static void quadro(unsigned q, unsigned long long gpu) { /* os três lados do quadro q (o picos) */
    sPJ[q % PICOS_N].q = q;
    sPJ[q % PICOS_N].tb = gpu - 40000;
    sPG[q % PICOS_N].q = q;
    sPG[q % PICOS_N].qJogo = q;
    sPG[q % PICOS_N].tb = gpu - 25000;
    sPD[q % PICOS_N].q = q;
    sPicoGpu[q % PICOS_N] = gpu;
}
static void apresenta(unsigned q, int repete, unsigned long long t) {
    gTb = t;
    gDev.swaps++;
    tela_apresentou(q, repete);
}
int main(void) {
    /* troca s = quadro s até a 6; a 7 repete o 6; a 8 é o quadro 7, a 9 o 8 (esta entra com a janela já fechada e não conta).
       Vblanks de entrada: */
    static const unsigned n[10] = {0, 10, 12, 14, 17, 18, 20, 23, 24, 26};
    static const unsigned qd[10] = {0, 1, 2, 3, 4, 5, 6, 6, 7, 8};
    unsigned s;
    int falhas = 0;
    for (s = 1; s <= 9; s++) {
        int rep = s == 7;
        unsigned long long gpu = (unsigned long long)n[s] * V - 3000; /* a GPU 3 ms antes do vblank em que entrou */
        if (s == 4) {
            gpu = 16 * V + 2000; /* o quadro 4: a GPU 2 ms depois do prazo (o vblank 16), entrou no 17 */
        }
        if (!rep) {
            quadro(qd[s], gpu);
        }
        apresenta(qd[s], rep, gpu - 1000); /* o Present (a troca s) */
        dpc(s, n[s]);                      /* a troca entra */
    }
    gJanela = 0;
    apresenta(9, 0, 27 * V); /* a janela fechou: o resumo */
    if (strstr(gResumo, "imagens de fato por vblanks na tela 1 1, 2 3, 3 1, 4 ou mais 1") == NULL) {
        printf("FALHA: o resumo das imagens de fato\n");
        falhas++;
    }
    if (strstr(gResumo, "1 repetições") == NULL) {
        printf("FALHA: as repetições no resumo\n");
        falhas++;
    }
    if (gLinhas != 3) {
        printf("FALHA: %d linhas de imagem (esperadas 3: a de 3, a de 1 e a de 4)\n", gLinhas);
        falhas++;
    }
    if (strstr(gLinhaGpu, "a GPU acabou -2.0 ms") == NULL) {
        printf("FALHA: a folga da GPU do quadro 4 até o prazo (-2,0 ms)\n");
        falhas++;
    }
    printf(falhas ? "teste_tela: %d falhas\n" : "teste_tela: tudo certo\n", falhas);
    return falhas != 0;
}
