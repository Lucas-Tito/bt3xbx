/* Ensaio do primeiro degrau, somente no falso. Não emite comandos D3D.
   BT3_GS_PREFIXOS=1 conserva as consultas integrais do backend: plano EDRAM, Z, DATE,
   faixa VU e versões da entrega. Nenhum lote está liberado ao D3D.
   BT3_GS_PREFIXOS=2 (seção 36, o segundo desenho) faz a mesma fila sem a barreira integral e, no fim de cada quadro,
   o censo das regras reais (px_regras): o plano da EDRAM previsto pelo do quadro anterior, a consulta de z de cada
   visita como hoje, a do DATE só quando o próximo marcador está a até PX_DATE_CURTO desenhos (os outros copiariam a área
   inteira), e a fusão do último. Diz quantos desenhos sairiam antes do fim e, nos quadros de BT3_PREFIXOS_QUADROS
   (6930,6960,7170 sem ela), a curva do relógio da gravação até cada dezesseis avos da lista, para o prefixos_teto.py. */
#include <pthread.h>
#include <unistd.h>
#define PX_CAP 8
#define PX_DATE_CURTO 64
/* As regras reais de liberação de um quadro inteiro (função pura: o teste a chama com listas sintéticas). chaves: a chave
   de pipeline de cada índice (a do falso, sPipeKey). rel[i]: quantos desenhos têm de estar gravados para i sair; drawCount:
   só no fim. */
typedef struct PxCenso { unsigned liberados, zmax, dateLongos; uint64_t plano; } PxCenso;
static int px_usa_z(const GsDraw *d, const uint32_t *chaves) {
    uint32_t k = chaves[d->pipeline];
    return d->native == 0 && (((k >> 12) & 1) || ((k >> 10) & 3) >= 2);
}
static void px_regras(const GsQuadro *q, const uint32_t *chaves, unsigned *rel, PxCenso *c) {
    unsigned n = q->drawCount, i, j, m = 0, ent[MAX_TARGETS], x1[MAX_TARGETS], y1[MAX_TARGETS];
    unsigned char grande[MAX_TARGETS];
    int cur = -1, A = -1, t;
    uint64_t h = 1469598103934665603ull;
    memset(c, 0, sizeof(*c));
    memset(ent, 0, sizeof(ent)); memset(x1, 0, sizeof(x1)); memset(y1, 0, sizeof(y1)); memset(grande, 0, sizeof(grande));
    for (i = 0; i < n; i++) rel[i] = i + 1 < n ? i + 1 : n; /* a fusão: o seguinte já existe */
    for (i = 0; i < n; i++) {
        const GsDraw *d = &q->draws[i];
        if (d->native == 5) { /* date_area: até o próximo marcador */
            for (j = i + 1; j < n && q->draws[j].native != 5; j++) {}
            if (j - i <= PX_DATE_CURTO) { if (j > rel[i]) rel[i] = j; }
            else c->dateLongos++;
            continue;
        }
        if (d->native >= 100 || d->target < 0 || d->target >= MAX_TARGETS) continue;
        t = d->target;
        if (d->native != 0 && !(d->native == 1 || d->native == 3 || d->native == 4)) continue;
        if (d->native != 0) grande[t] = 1;
        else if (!((chaves[d->pipeline] >> 22) & 1) && (((chaves[d->pipeline] >> 12) & 1) || ((chaves[d->pipeline] >> 10) & 3) != 1)) grande[t] = 1;
        if (d->native == 0 && d->scissor[0] + d->scissor[2] > 0 && d->scissor[1] + d->scissor[3] > 0) {
            if ((unsigned)(d->scissor[0] + d->scissor[2]) > x1[t]) x1[t] = (unsigned)(d->scissor[0] + d->scissor[2]);
            if ((unsigned)(d->scissor[1] + d->scissor[3]) > y1[t]) y1[t] = (unsigned)(d->scissor[1] + d->scissor[3]);
        }
        if (t != cur) { /* a entrada numa visita: usa_z olha até o primeiro z dela ou a troca de alvo */
            ent[t]++;
            cur = t;
            for (j = i; j < n; j++) {
                const GsDraw *e = &q->draws[j];
                if (e->native != 0) continue;
                if (e->target != t || px_usa_z(e, chaves)) break;
            }
            if (j - i > c->zmax) c->zmax = j < n ? j - i : n - i;
            j = j < n ? j + 1 : n;
            if (j > rel[i]) rel[i] = j;
        }
    }
    for (i = 0; i < n; i++) { /* em ordem: ninguém sai antes do anterior */
        if (rel[i] < m) rel[i] = m;
        m = rel[i];
        c->liberados += rel[i] < n;
    }
    for (t = 0; t < MAX_TARGETS; t++) if (ent[t] > 0 && (A < 0 || ent[t] > ent[A])) A = t;
    h = (h ^ (uint64_t)(A + 1)) * 1099511628211ull; /* o plano: a A e as pequenas com o tamanho em blocos de 8 */
    for (t = 0; t < MAX_TARGETS; t++) {
        if (t == A || ent[t] == 0) continue;
        h = (h ^ (uint64_t)(t + 1)) * 1099511628211ull;
        h = (h ^ grande[t]) * 1099511628211ull;
        if (!grande[t]) h = (h ^ (((x1[t] + 7) / 8) << 16 | (y1[t] + 7) / 8)) * 1099511628211ull;
    }
    c->plano = h;
}
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
    uint64_t hash, planoAnt;
    int ligado, iniciou, fim, atraso, temPlano;
    unsigned fechados, curva[8], nCurva;
    double t0, tf[MAX_DRAWS];
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
        px_exige(x.bloqueios==(sPx.ligado==1?31u:0u),"barreira integral (1) ou regras reais (2)");
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
        sPx.ligado=e?atoi(e):0; sPx.iniciou=1;
        px_exige(sPx.ligado>=0 && sPx.ligado<=2,"BT3_GS_PREFIXOS 0/1/2");
        {
            const char *qs=getenv("BT3_PREFIXOS_QUADROS");
            char *f;
            if(!qs) qs="6930,6960,7170";
            while(*qs && sPx.nCurva<8) {
                sPx.curva[sPx.nCurva++]=(unsigned)strtoul(qs,&f,10);
                px_exige(f!=qs,"BT3_PREFIXOS_QUADROS");
                qs=*f==','?f+1:f;
            }
        }
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
        x.ordem=sPx.publicado; x.bloqueios=sPx.ligado==1?31:0; x.leitor=-1;
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
            if(!strcmp(sPx.mutante,"barreira")) x.bloqueios^=31;
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
static double px_agora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec*1e3+t.tv_nsec/1e6;
}
static void px_progresso(void) {
    GsQuadro q;
    if(!px_ligado() || gsDrawCount<2) return;
    if(sPx.ligado==2) { /* o relógio em que cada desenho fechou (o seguinte começou) */
        double t=px_agora();
        if(!sPx.fechados) sPx.t0=t;
        while(sPx.fechados<gsDrawCount-1 && sPx.fechados<MAX_DRAWS) sPx.tf[sPx.fechados++]=t;
    }
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
    if(sPx.ligado==1) fprintf(stderr,"prefixos: quadro %u fechados %u liberados 0 barreira %u fila %u/%u bytes %u esperas %u hash %016llx\n",
        q->quadro,sPx.consumido,sPx.consumido,sPx.maximo,PX_CAP,(unsigned)((PX_CAP+1)*sizeof(PxItem)),sPx.espera,(unsigned long long)h);
    else {
        static unsigned rel[MAX_DRAWS];
        PxCenso c;
        double tn=px_agora();
        unsigned k;
        px_regras(q,sPipeKey,rel,&c);
        fprintf(stderr,"prefixos2: quadro %u desenhos %u liberados %u plano %d zmax %u datelongos %u fila %u/%u esperas %u hash %016llx\n",
            q->quadro,q->drawCount,c.liberados,sPx.temPlano && c.plano==sPx.planoAnt,c.zmax,c.dateLongos,sPx.maximo,PX_CAP,
            sPx.espera,(unsigned long long)h);
        sPx.planoAnt=c.plano; sPx.temPlano=1;
        for(k=0;k<sPx.nCurva;k++) {
            if(sPx.curva[k]==q->quadro && q->drawCount>16 && sPx.fechados && tn>sPx.t0) {
                unsigned p;
                fprintf(stderr,"prefixos2: curva quadro %u desenhos %u ms %.3f milesimos",q->quadro,q->drawCount,tn-sPx.t0);
                for(p=0;p<=16;p++) {
                    unsigned i=p*q->drawCount/16;
                    double ti=i<sPx.fechados?sPx.tf[i]:tn;
                    fprintf(stderr," %u",(unsigned)((ti-sPx.t0)/(tn-sPx.t0)*1000.0+0.5));
                }
                fputc('\n',stderr);
            }
        }
    }
    sPx.fechados=0;
    sPx.ini=sPx.n=sPx.maximo=sPx.publicado=sPx.consumido=sPx.espera=0; sPx.fim=0;
    memset(sPx.versao,0,sizeof(sPx.versao)); memset(sPx.lida,0,sizeof(sPx.lida));
}
