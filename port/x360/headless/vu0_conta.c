/* Só ligado na build de contagem. Nunca usar os tempos desta build como perfil. */
#include <stdio.h>
#include <string.h>
#include "vu0_conta.h"
unsigned gVcChamadas[512];
static unsigned sNativo[512], sRecuo[512][6];
static int sMotivo, sAnterior, sDito;
static unsigned sPasso;
static const char *sNomes[] = {
#include "nomes.inc"
};
extern int gVnAtivo;
int Vc_Motivo(const unsigned *a, int na, const unsigned *b, int nb, const unsigned *c, int nc) {
    int j, i, ns[3];
    const unsigned *ps[3];
    if (!gVnAtivo) return 1;
    ps[0]=a; ps[1]=b; ps[2]=c; ns[0]=na; ns[1]=nb; ns[2]=nc;
    for (j=0;j<3;++j) for(i=0;i<ns[j];++i) {
        unsigned u=ps[j][i]&0x7fffffffu;
        if (u==0 || (u>=0x30800000u && u<=0x4e800000u)) continue;
        if (u>=0x7f800000u) return 2;
        if (u<0x00800000u) return 3;
        return u<0x30800000u ? 4 : 5;
    }
    return 0;
}
int Vc_Resultado(int id, int ok, int motivo) {
    if (ok) ++sNativo[id];
    else {
        if(motivo<0) motivo=sMotivo;
        ++sRecuo[id][motivo];
    }
    sMotivo=ok ? 0 : motivo;
    return ok;
}
void Vc_Passo(int combate, unsigned vb) {
    unsigned i, j;
    if (!sDito) {
        sDito=1;
        fprintf(stderr,"vu0conta: motivos 0=sem-motivo 1=desligado 2=nan-inf 3=denormal 4=pequeno 5=grande\n");
        for(i=0;i<sizeof(sNomes)/sizeof(sNomes[0]);++i)
            fprintf(stderr,"vu0conta: funcao %u %s\n",i,sNomes[i]);
    }
    if(sAnterior) {
        fprintf(stderr,"vu0conta: passo %u vb %u\n",++sPasso,vb);
        for(i=0;i<sizeof(sNomes)/sizeof(sNomes[0]);++i) {
            if(!gVcChamadas[i]) continue;
            fprintf(stderr,"vu0conta: conta %u %u %u",i,gVcChamadas[i],sNativo[i]);
            for(j=0;j<6;++j) fprintf(stderr," %u",sRecuo[i][j]);
            fputc('\n',stderr);
        }
    }
    memset(gVcChamadas,0,sizeof(gVcChamadas)); memset(sNativo,0,sizeof(sNativo)); memset(sRecuo,0,sizeof(sRecuo));
    sAnterior=combate;
}
