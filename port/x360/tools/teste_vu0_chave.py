#!/usr/bin/env python3
"""Prova as duas posições da chave no mesmo binário e recusa cópias sabotadas."""
import pathlib, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[3]
DRIVER='''#include "vu0_nativo.h"
int Port_FastMtxApply(const unsigned *,const unsigned *,unsigned *);
int main(void) {
 unsigned a[16]={0},b[4]={0},d[4]; int k,i,esperado;
 const char *valores[]={0,"1","0","00","1"};
 for(k=0;k<5;++k) {
  Vn_Configurar(valores[k]); esperado=k!=2;
  for(i=0;i<4;++i)d[i]=0x12345678u;
  if(Vn_Op4(a,b,0,d,0)!=esperado)return 1;
  if(Vn_Bc4(a,0,0,d,1)!=esperado)return 2;
  if(Vn_MtxApply(a,b,d)!=esperado)return 3;
  if(Port_FastMtxApply(a,b,d)!=esperado)return 4;
  for(i=0;i<4;++i)if(d[i]!=(esperado?0u:0x12345678u))return 5;
 }
 return 0;
}'''
def main():
    with tempfile.TemporaryDirectory(prefix='vu0-chave-') as t:
        t=pathlib.Path(t);(t/'teste.c').write_text(DRIVER)
        original=(ROOT/'port/x360/src/f2/vu0_nativo.c').read_text().replace('#include "../../include/vu0_nativo.h"','#include "vu0_nativo.h"')
        alvo='    if (!gVnAtivo) return 0;'
        assert original.count(alvo)==2
        variantes={'integro':original,'op ignora chave':original.replace(alvo,'',1),
                   'matriz ignora chave':original[::-1].replace(alvo[::-1],'',1)[::-1],
                   'config ignora zero':original.replace("valor[0] == '0'","valor[0] == '9'")}
        for nome,s in variantes.items():
            (t/'nucleo.c').write_text(s)
            subprocess.run(['gcc','-O2','-ffunction-sections','-Wl,--gc-sections','-std=gnu89','-fno-strict-aliasing','-ffp-contract=off','-I'+str(ROOT/'port/x360/include'),t/'teste.c',t/'nucleo.c',ROOT/'port/x360/src/f2/float360.c','-lm','-o',t/'teste'],check=True)
            p=subprocess.run([t/'teste'])
            assert (p.returncode==0)==(nome=='integro'),(nome,p.returncode)
            print(nome+': '+('tudo certo' if p.returncode==0 else 'recusado, saída '+str(p.returncode)))
if __name__=='__main__':main()
