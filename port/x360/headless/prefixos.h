/* Ensaio do primeiro degrau, somente no falso. Não emite comandos D3D.
   Conserva as consultas integrais do backend: plano EDRAM, Z, DATE,
   faixa VU e versões da entrega. Nenhum lote está liberado ao D3D. */
#include <pthread.h>
#include <unistd.h>
#define PX_CAP 8
typedef struct PxItem {
    GsQuadro q;
    GsDraw d;
    uint64_t hash;
    unsigned ordem, versao, leitura, quadro, bloqueios;
    int leitor;
} PxItem;
static struct {
    pthread_mutex_t lock;
    pthread_cond_t tem, livre;
    pthread_t thread;
    PxItem fila[PX_CAP];
    unsigned ini, n, maximo, publicado, consumido, quadro, espera, versao[MAX_TARGETS], lida[MAX_TARGETS];
    uint64_t hash;
    int ligado, iniciou, fim, atraso;
    const char *mutante;
} sPx = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};
static void px_exige(int ok, const char *regra) {
    if (!ok) { fprintf(stderr,"FALHA: prefixos %s\n",regra); abort(); }
}
static uint64_t px_palavras(uint64_t h, const void *dados, unsigned n) {
    const uint32_t *p=(const uint32_t *)dados;
    while(n--) h=(h ^ *p++)*1099511628211ull;
    return h;
}
static uint64_t px_hash(const GsQuadro *q, const GsDraw *d) {
    uint64_t h=px_palavras(1469598103934665603ull,d,sizeof(*d)/4);
    unsigned k;
    if (d->native || !d->count) return h;
    if (d->vu) {
        px_exige(d->uniform>=0 && (unsigned)d->uniform<q->vuUniCount,"uniforme vivo");
        px_exige(d->first<=q->vuIdxCount && d->count<=q->vuIdxCount-d->first,"índices vivos");
        h=px_palavras(h,&q->vuUni[d->uniform],sizeof(Vu0Uniform)/4);
        for(k=0;k<d->count;k++) {
            unsigned v=q->vuIdx[d->first+k];
            px_exige(v<q->vuVertCount,"vértice vivo");
            h=(h^v)*1099511628211ull;
            h=px_palavras(h,&q->vuVerts[v*12],12);
        }
    } else {
        px_exige(d->first<=q->vertCount && d->count<=q->vertCount-d->first,"primitiva viva");
        h=px_palavras(h,&q->verts[d->first],d->count*(sizeof(Vtx)/4));
    }
    return h;
}
static void *px_consumidor(void *ignorado) {
    (void)ignorado;
    for(;;) {
        PxItem x;
        pthread_mutex_lock(&sPx.lock);
        while(!sPx.n && !sPx.fim) pthread_cond_wait(&sPx.tem,&sPx.lock);
        if (!sPx.n && sPx.fim) { pthread_mutex_unlock(&sPx.lock); break; }
        x=sPx.fila[sPx.ini]; sPx.ini=(sPx.ini+1)%PX_CAP; sPx.n--;
        pthread_cond_signal(&sPx.livre);
        pthread_mutex_unlock(&sPx.lock);
        if (!sPx.consumido && sPx.atraso) usleep(sPx.atraso*1000);
        px_exige(x.quadro==sPx.quadro && x.ordem==sPx.consumido,"ordem/quadro");
        px_exige(x.bloqueios==31,"barreira integral");
        px_exige(x.hash==px_hash(&x.q,&x.d),"dados imutáveis/liberação");
        if(x.leitor>=0) px_exige(x.leitura==sPx.lida[x.leitor],"versão do leitor");
        if(x.d.target>=0 && x.d.target<MAX_TARGETS) {
            px_exige(x.versao==sPx.lida[x.d.target]+1,"última escrita");
            sPx.lida[x.d.target]=x.versao;
        }
        sPx.hash=(sPx.hash^x.hash)*1099511628211ull;
        sPx.consumido++;
    }
    return NULL;
}
static int px_ligado(void) {
    if(!sPx.iniciou) {
        const char *e=getenv("BT3_GS_PREFIXOS"), *a=getenv("BT3_PREFIXOS_ATRASO");
        sPx.ligado=e && atoi(e)!=0; sPx.iniciou=1;
        sPx.atraso=a?atoi(a):0;
        px_exige(sPx.atraso==0 || sPx.atraso==1 || sPx.atraso==5 || sPx.atraso==20,"atraso 0/1/5/20");
        sPx.mutante=getenv("BT3_PREFIXOS_MUTANTE");
    }
    return sPx.ligado;
}
static void px_publica(const GsQuadro *q, unsigned limite) {
    if(!px_ligado()) return;
    if(!sPx.publicado && !sPx.consumido && !sPx.fim) {
        sPx.quadro=q->quadro; sPx.hash=1469598103934665603ull;
        px_exige(pthread_create(&sPx.thread,NULL,px_consumidor,NULL)==0,"criar consumidor");
    }
    while(sPx.publicado<limite) {
        PxItem x;
        int t;
        memset(&x,0,sizeof(x));
        x.q=*q; x.d=q->draws[sPx.publicado]; x.quadro=q->quadro;
        x.ordem=sPx.publicado; x.bloqueios=31; x.leitor=-1;
        if(x.d.tex_is_target) {
            for(t=0;t<q->targetCount;t++) {
                if(x.d.tex==0x1000u+(unsigned)t || x.d.tex==0x2000u+(unsigned)t || x.d.tex==0x3000u+(unsigned)t) x.leitor=t;
            }
            px_exige(x.leitor>=0,"alvo leitor identificado");
        } else if(x.d.native && x.d.native<100) x.leitor=x.d.target;
        if(x.leitor>=0) x.leitura=sPx.versao[x.leitor];
        if(x.d.target>=0 && x.d.target<MAX_TARGETS) x.versao=++sPx.versao[x.d.target];
        x.hash=px_hash(q,&x.d);
        if(sPx.mutante && sPx.publicado==0) {
            if(!strcmp(sPx.mutante,"ordem")) x.ordem++;
            if(!strcmp(sPx.mutante,"versao")) x.versao++;
            if(!strcmp(sPx.mutante,"barreira")) x.bloqueios=0;
            if(!strcmp(sPx.mutante,"liberacao")) x.hash^=1;
        }
        pthread_mutex_lock(&sPx.lock);
        while(sPx.n==PX_CAP) { sPx.espera++; pthread_cond_wait(&sPx.livre,&sPx.lock); }
        sPx.fila[(sPx.ini+sPx.n)%PX_CAP]=x; sPx.n++;
        if(sPx.n>sPx.maximo) sPx.maximo=sPx.n;
        sPx.publicado++;
        pthread_cond_signal(&sPx.tem);
        pthread_mutex_unlock(&sPx.lock);
    }
}
static void px_progresso(void) {
    GsQuadro q;
    if(!px_ligado() || gsDrawCount<2) return;
    memset(&q,0,sizeof(q));
    q.draws=gsDraws; q.drawCount=gsDrawCount; q.verts=gsVerts; q.vertCount=gsVertCount;
    q.vuVerts=gsVuVerts; q.vuVertCount=gsVuVertCount; q.vuIdx=gsVuIdx; q.vuIdxCount=gsVuIdxCount;
    q.vuUni=gsVuUni; q.vuUniCount=gsVuUniCount; q.targetCount=gsTargetCount; q.quadro=gGsFrame;
    px_publica(&q,gsDrawCount-1); /* a última vaga ainda pode fundir */
}
static void px_final(const GsQuadro *q) {
    unsigned n;
    uint64_t h=1469598103934665603ull;
    if(!px_ligado()) return;
    px_publica(q,q->drawCount);
    pthread_mutex_lock(&sPx.lock); sPx.fim=1; pthread_cond_signal(&sPx.tem); pthread_mutex_unlock(&sPx.lock);
    pthread_join(sPx.thread,NULL);
    for(n=0;n<q->drawCount;n++) h=(h^px_hash(q,&q->draws[n]))*1099511628211ull;
    px_exige(h==sPx.hash && sPx.consumido==q->drawCount,"fecho integral/imutabilidade");
    fprintf(stderr,"prefixos: quadro %u fechados %u liberados 0 barreira %u fila %u/%u bytes %u esperas %u hash %016llx\n",
        q->quadro,sPx.consumido,sPx.consumido,sPx.maximo,PX_CAP,(unsigned)((PX_CAP+1)*sizeof(PxItem)),sPx.espera,(unsigned long long)h);
    sPx.ini=sPx.n=sPx.maximo=sPx.publicado=sPx.consumido=sPx.espera=0; sPx.fim=0;
    memset(sPx.versao,0,sizeof(sPx.versao)); memset(sPx.lida,0,sizeof(sPx.lida));
}
