/* Compara funções completas, estado, máscaras e alias, nos dois modelos. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#include "port/vu0_a.h"
#include "port/vu0_b.h"
#include "vu0_nativo.h"
extern RefVu0State Ant_gRefVu0;
extern void Ant_Ref_Mtx_Mul(RefMtx44 *, RefMtx44 *, RefMtx44 *);
extern void Ant_Ref_Vu0_SinCos(float);
extern void Ant_Ref_Mtx_MulVec4(RefVec4f *, const RefMtx44f *, const RefVec4f *);
extern void Ant_Ref_Vec4_Add(RefVec4f *, const RefVec4f *, const RefVec4f *);
extern void Ant_Ref_Vec4_Scale(RefVec4f *, const RefVec4f *, float);
extern void Teste_Op(int, int, int, int, int, int);
extern void Ant_Teste_Op(int, int, int, int, int, int);
static const char *corrompe;
static unsigned estado = 0x91f3e2a5u;
static unsigned rnd(void) { estado ^= estado << 13; estado ^= estado >> 17; estado ^= estado << 5; return estado; }
static const uint32_t limites[] = {0,0x80000000,1,0x80000001,0x7fffff,0x807fffff,0x800000,0x800001,
    0x7f7fffff,0xff7fffff,0x7f800000,0x7fc00001,0x7f800001,0xff800000,0x30800000,0x307fffff,
    0x4e800000,0x4e800001,0x3f800000,0x3f800001,0x3f7fffff,0xbf800000};
static uint32_t numero(unsigned i) {
    unsigned r = rnd();
    switch (i & 3) {
    case 0: return r;
    case 1: return (r & 0x807fffff) | ((107 + rnd() % 41) << 23);
    case 2: return limites[rnd() % (sizeof(limites)/sizeof(*limites))];
    default: return (r & 0x80700000) | ((124 + rnd() % 7) << 23);
    }
}
static void confere(const char *nome, unsigned i, const void *a, const void *b, size_t n) {
    if (corrompe && !strcmp(corrompe,nome)) ((uint32_t *)a)[0] ^= 1;
    if (memcmp(a,b,n)) {
        const uint32_t *x=a,*y=b;
        size_t k;
        for(k=0;k<n/4;k++) if(x[k]!=y[k]) {
            printf("FALHA %s caso %u palavra %u: %08x != %08x\n",nome,i,(unsigned)k,x[k],y[k]); break;
        }
        exit(1);
    }
}
static void estados(const char *nome, unsigned i) { confere(nome,i,&gRefVu0,&Ant_gRefVu0,sizeof(gRefVu0)); }
static void prepara(unsigned i) {
    int k;
    /* Todo resíduo varia, inclusive vf0, ACC, flags e registradores não escritos. */
    for(k=0;k<32;k++) {
        int c; for(c=0;c<4;c++) gRefVu0.vf[k].u[c]=numero(i);
    }
    for(k=0;k<4;k++) gRefVu0.acc.u[k]=numero(i);
    gRefVu0.status=rnd(); gRefVu0.q=rnd();
    Ant_gRefVu0=gRefVu0;
}
int main(int argc,char **argv) {
    unsigned n=argc>1?strtoul(argv[1],0,10):2000000, i;
    unsigned aceita=0, recusa=0;
    corrompe=argc>2?argv[2]:0;
    if(fesetround(FE_TOWARDZERO)) return 2;
    /* Desistir não pode publicar nem um byte: inclusive denormal e expoente 255. */
    for(i=0;i<sizeof(limites)/sizeof(*limites);i++) {
        uint32_t r[16]={0},v[4]={0x3f800000,0,0,0},out[4],antes[4]={1,2,3,4};
        uint32_t a=limites[i]&0x7fffffffu;
        int esperado=a==0 || (a>=0x30800000u && a<=0x4e800000u);
        r[0]=limites[i];memcpy(out,antes,16);
        if(Vn_MtxApply(r,v,out)!=esperado) { printf("FALHA desistência: %08x\n",r[0]);return 1; }
        if(!esperado) confere("desistência sem escrita",i,out,antes,16);
    }
    /* Os núcleos publicam só depois das leituras, mesmo com alias parcial. */
    for(i=0;i<1000;i++) {
        uint32_t r[20], v[8], acc[8], o[4], sobreposto[20];
        int k;
        for(k=0;k<20;k++) r[k]=numero(1);
        for(k=0;k<8;k++) { v[k]=numero(1); acc[k]=numero(1); }
        if(!Vn_MtxApply(r,v,o)) return 2;
        memcpy(sobreposto,r,sizeof(r));
        if(!Vn_MtxApply(sobreposto,v,sobreposto+1)) return 2;
        confere("matriz alias parcial r",i,o,sobreposto+1,16);
        memcpy(sobreposto,v,sizeof(v));
        if(!Vn_MtxApply(r,sobreposto,sobreposto+1)) return 2;
        confere("matriz alias parcial v",i,o,sobreposto+1,16);
        if(!Vn_Op4(r,v,acc,o,1)) return 2;
        memcpy(sobreposto,acc,sizeof(acc));
        if(!Vn_Op4(r,v,sobreposto,sobreposto+1,1)) return 2;
        confere("op alias parcial acc",i,o,sobreposto+1,16);
    }
    /* A cobertura pseudoaleatória principal conserva a semente anterior. */
    estado=0x91f3e2a5u;
    for(i=0;i<n;i++) {
        RefMtx44 a,b,an,bn,ao,bo;
        RefVec4f va,vb,x,y,xx,yy;
        uint32_t s;
        float escala,angulo;
        int k,alias=i%3;
        prepara(i);
        for(k=0;k<16;k++) ((uint32_t*)&a)[k]=numero(i);
        for(k=0;k<16;k++) ((uint32_t*)&b)[k]=numero(i);
        memcpy(&va,&b,16); memcpy(&vb,&b.r[1],16);
        s=numero(i); memcpy(&escala,&s,4);
        x=xx=va; y=yy=vb;
        Ref_Vec4_Add(alias==1?&x:alias==2?&y:&va,&x,&y);
        Ant_Ref_Vec4_Add(alias==1?&xx:alias==2?&yy:&vb,&xx,&yy);
        confere("Vec4_Add",i,alias==1?&x:alias==2?&y:&va,alias==1?&xx:alias==2?&yy:&vb,16);
        estados("Vec4_Add estado",i);
        memcpy(&x,&b,16); xx=x;
        Ref_Vec4_Scale(alias?&x:&va,&x,escala);
        Ant_Ref_Vec4_Scale(alias?&xx:&vb,&xx,escala);
        confere("Vec4_Scale",i,alias?&x:&va,alias?&xx:&vb,16);
        estados("Vec4_Scale estado",i);
        memcpy(&gRefVu0.vf[16],&a,64); Ant_gRefVu0=gRefVu0;
        memcpy(&x,&b,16);xx=x;
        Ref_Mtx_MulVec4(alias?&x:&va,(const RefMtx44f*)&a,&x);
        Ant_Ref_Mtx_MulVec4(alias?&xx:&vb,(const RefMtx44f*)&a,&xx);
        confere("mtx_apply",i,alias?&x:&va,alias?&xx:&vb,16);
        estados("mtx_apply estado",i);
        if(Vn_MtxApply((uint32_t*)&a,(uint32_t*)&b,(uint32_t*)&va)) aceita++; else recusa++;
        an=ao=a;bn=bo=b;
        Ref_Mtx_Mul(alias==1?&an:alias==2?&bn:&a,&an,&bn);
        Ant_Ref_Mtx_Mul(alias==1?&ao:alias==2?&bo:&b,&ao,&bo);
        confere("Mtx_Mul",i,alias==1?&an:alias==2?&bn:&a,alias==1?&ao:alias==2?&bo:&b,64);
        estados("Mtx_Mul estado",i);
        /* Os oito formatos alterados, toda máscara, vf0 e destinos sobrepostos. */
        for(k=0;k<8;k++) {
            int m=rnd()%16,fd=rnd()%32,fs=rnd()%32,ft=rnd()%32,c=rnd()%4;
            if(i%5==0) fd=fs;
            if(i%7==0) fd=ft;
            Teste_Op(k,m,fd,fs,ft,c); Ant_Teste_Op(k,m,fd,fs,ft,c);
            if(memcmp(&gRefVu0,&Ant_gRefVu0,sizeof(gRefVu0))) printf("op %d m %d fd %d fs %d ft %d c %d\n",k,m,fd,fs,ft,c);
            { static const char *nomes[]={"op_vadd","op_vmul","op_vaddbc","op_vsubbc","op_vmulbc","op_vmulabc","op_vmaddabc","op_vmaddbc"}; estados(nomes[k],i); }
        }
        s=numero(i);memcpy(&angulo,&s,4);
        Ref_Vu0_SinCos(angulo);Ant_Ref_Vu0_SinCos(angulo);
        estados("Vu0_SinCos",i);
    }
    printf("tudo certo: %u casos por função, %u instruções, matriz aceita %u, recusa %u; estado e alias iguais\n",n,n*8,aceita,recusa);
    return 0;
}
