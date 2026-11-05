#!/usr/bin/env python3
"""Controles positivos e negativos reais da instrumentação, sem dados do jogo."""
import pathlib, subprocess, tempfile
from vu0_conta import Contador
from vu0_conta_resumo import ler
ROOT=pathlib.Path(__file__).resolve().parents[3]

def run(args):
    return subprocess.run(list(map(str,args)),check=True,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

def main():
    with tempfile.TemporaryDirectory(prefix='vu0-conta-') as t:
        t=pathlib.Path(t);c=Contador(ROOT,t)
        fonte=c.preparar(ROOT/'port/x360/src/f2/vu0_nativo.c','vu0_nativo')
        matriz=t/'matriz.c';matriz.write_text('typedef struct {int x;} RefVec4f;\nRefVec4f mtx_apply(RefVec4f a) {return a;}\nvoid ChamaMatriz(void) {RefVec4f a={0}; mtx_apply(a);}')
        matriz=c.preparar(matriz,'matriz');c.escrever()
        driver=t/'teste.c'
        driver.write_text('''#include "vu0_nativo.h"
#include "vu0_conta.h"
int main(void) {
 unsigned a[16]={0}, b[4]={0}, d[4]={0}; int i;
 unsigned ruins[]={0x7fc00000u,0x7f800000u,1u,0x00800000u,0x4f000000u};
 Vc_Passo(1,0); {void ChamaMatriz(void); ChamaMatriz();}
 Vn_Op4(a,b,0,d,0); Vn_Bc4(a,0,0,d,1); Vn_MtxApply(a,b,d);
 for(i=0;i<5;++i) { a[0]=ruins[i]; Vn_Op4(a,b,0,d,0); Vn_Bc4(a,0,0,d,1); Vn_MtxApply(a,b,d); }
 Vn_Configurar("0"); Vn_Op4(a,b,0,d,0); Vn_Bc4(a,0,0,d,1); Vn_MtxApply(a,b,d);
 Vc_Passo(0,2); return 0;
}''')
        run(['gcc','-O2','-std=gnu99','-fno-strict-aliasing','-ffp-contract=off','-I'+str(c.pasta),'-I'+str(ROOT/'port/x360/include'),'-I'+str(ROOT/'port/x360/headless'),fonte,matriz,ROOT/'port/x360/headless/vu0_conta.c',driver,'-o',t/'teste'])
        log=run([t/'teste']).stderr;p=t/'log';p.write_text(log);r=ler(p)
        for nome,nativo,total in [('Vn_Op4',2,14),('Vn_Bc4',1,7),('Vn_MtxApply',1,7)]:
            v=r['totais']['vu0_nativo/'+nome]
            mult=2 if nome=='Vn_Op4' else 1
            assert v==[total,nativo,0,mult,2*mult,mult,mult,mult],v
        assert r['totais']['matriz/mtx_apply']==[1,0,0,0,0,0,0,0]
        print('tudo certo: entrada, retorno efetivo e cinco motivos nos três núcleos')
        # Cada mutação precisa alterar a entrada e ser recusada pelo mesmo leitor.
        linhas=log.splitlines(keepends=True)
        casos={
          'sem passo':log.replace('vu0conta: passo 1 vb 2\n',''),
          'retorno a menos':log.replace('14 2 0 2 4 2 2 2','14 1 0 2 4 2 2 2'),
          'conta duplicada':log+next(l for l in linhas if l.startswith('vu0conta: conta ')),
          'conta truncada':log+'vu0conta: conta 0 1\n',
          'sem dados':'',
          'janela não fecha':log+'roteiro: janela 1 passos 1 vblank 3\n',
          'janela incompleta':log+'roteiro: janela 1 passos 2 vblank 2\n'}
        p.write_text(log+'roteiro: janela 1 passos 1 vblank 2\n')
        assert ler(p)['janelas'][0]['totais']==r['totais']
        for nome,estragado in casos.items():
            assert estragado!=log,nome
            p.write_text(estragado)
            try:ler(p)
            except (ValueError,KeyError): print('recusado:',nome)
            else:raise AssertionError('mutante sobreviveu: '+nome)
if __name__=='__main__':main()
