/* Sem dados do jogo: usa a fronteira e a publicação de produção, com a dona
   do dispositivo de brinquedo. O atraso ocorre no consumidor, não no produtor. */
#define BT3_GPU_FALSO 1
#define BT3_TESTE_VU1 1
#define BT3_GS_LOTES 1
#include "../../src/gs/gs_draw.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "gs_lotes.h"
static GsBackend backend, *sBackend = &backend;
static pthread_mutex_t sDesLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sDesAcorda = PTHREAD_COND_INITIALIZER, sDesFeito = PTHREAD_COND_INITIALIZER;
static int sDesTem, sDividido = 1, sVuDir = 1, sVdJogo;
static uint32_t sVdSeqDe[3] = {1, 0, 0};
static uint32_t *sVuFaixa;
unsigned gGsFrame = 1;
GsDraw *gsDraws;
uint32_t gsDrawCount, gsVertCount, gsVuVertCount, gsVuIdxCount, gsVuUniCount;
Vtx *gsVerts;
float *gsVuVerts;
uint32_t *gsVuIdx;
Vu0Uniform *gsVuUni;
GsTarget gsTargets[MAX_TARGETS];
int gsTargetCount = 2, gsAnchor, gsTexCount;
unsigned gsSkipped, gsNative;
static struct { uint64_t frame[2]; } gGs;
#define GS_FBP_QUADRO 0
#include "gs_lotes_grav.h"
static unsigned consumidos, faixas;
static int parar, atraso;
static int z(const GsDraw *d) { return d->pipeline != 0; }
static uint32_t prontos(const GsQuadro *q, uint32_t de) { return gs_lotes_limite(q, de, z); }
static void prepara(const GsQuadro *q) { assert(!sDesTem && q->vuSeq == 1); }
static void *consumidor(void *arg) {
    (void)arg;
    pthread_mutex_lock(&sDesLock);
    for (;;) {
        unsigned i;
        while (!sDesTem && !parar) pthread_cond_wait(&sDesAcorda, &sDesLock);
        if (parar) break;
        assert(sDesTem == 4);
        pthread_mutex_unlock(&sDesLock);
        usleep(atraso * 1000);
        assert(sLoteAte > consumidos && sLoteAte <= sLote.drawCount);
        assert(sLote.drawCount < gsDrawCount); /* último ainda fundível */
        for (i = consumidos; i < sLoteAte; i++) assert(sLote.draws[i].first == i);
        consumidos = sLoteAte;
        faixas++;
        pthread_mutex_lock(&sDesLock);
        sDesTem = 0;
        pthread_cond_broadcast(&sDesFeito);
    }
    pthread_mutex_unlock(&sDesLock);
    return NULL;
}
static void regras(void) {
    GsDraw d[200]; GsQuadro q;
    unsigned i;
    memset(&q, 0, sizeof(q)); memset(d, 0, sizeof(d));
    q.draws = d; q.drawCount = 200;
    for (i = 0; i < 200; i++) d[i].pipeline = 1;
    assert(gs_lotes_limite(&q, 0, z) == 200);
    d[70].native = 5;
    assert(gs_lotes_limite(&q, 0, z) == 70); /* DATE sem fecho: não cortar alias */
    d[180].native = 5;
    assert(gs_lotes_limite(&q, 0, z) == 180);
    d[179].pipeline = 0; /* precisa olhar além do marcador; nenhum testemunho ainda */
    for (i = 181; i < 200; i++) d[i].pipeline = 0;
    assert(gs_lotes_limite(&q, 0, z) == 70);
    d[199].target = 1;
    assert(gs_lotes_limite(&q, 0, z) == 180);
}
int main(int argc, char **argv) {
    pthread_t t;
    unsigned i;
    atraso = argc > 1 ? atoi(argv[1]) : 1;
    regras();
    backend.lotesProntos = prontos; backend.lotesPrepara = prepara;
    gsDraws = calloc(600, sizeof(*gsDraws)); assert(gsDraws);
    assert(!pthread_create(&t, NULL, consumidor, NULL));
    for (i = 0; i < 512; i++) {
        gsDraws[i].pipeline = 1;
        gsDraws[i].first = i;
        gsDrawCount++;
        gs_lotes_passo();
        assert(consumidos == sLoteAte && !sDesTem); /* produtor só segue após CPU */
        assert(gsTargets[0].cleared == 0); /* só o fecho pode mudá-lo */
    }
    assert(faixas >= 3 && consumidos < 512 && consumidos >= 384);
    pthread_mutex_lock(&sDesLock); parar = 1; pthread_cond_signal(&sDesAcorda); pthread_mutex_unlock(&sDesLock);
    assert(!pthread_join(t, NULL));
    free(gsDraws);
    printf("tudo certo: fronteiras DATE/Z, fusão, ordem e uma vaga com atraso %d ms, %u faixas\n", atraso, faixas);
    return 0;
}
