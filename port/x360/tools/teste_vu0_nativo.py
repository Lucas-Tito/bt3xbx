#!/usr/bin/env python3
"""Prova bits da VU0 nativa e conta a desmontagem PPC; sem dados do jogo.

Rodar sob flock ~/wsl.lock, um emulador por vez:
  python3 port/x360/tools/teste_vu0_nativo.py [--casos 2000000] [--mutantes]
Compila o original de --base e o fonte atual, com os modelos inteiro e 360,
na x86_64 SSE e na PPC big-endian. As cópias e logs ficam em ~/vu0-nativo-testes.
"""
import argparse, os, pathlib, subprocess, re
ROOT=pathlib.Path(__file__).resolve().parents[3]
FERR=ROOT/'port/x360/tools'

def run(cmd, falha=False):
    p=subprocess.run(list(map(str,cmd)),cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if p.returncode and not falha: raise RuntimeError(' '.join(map(str,cmd))+'\n'+p.stdout[-5000:])
    return p

def montar(pasta, arch, modelo, base, mutante=None):
    pasta.mkdir(parents=True,exist_ok=True)
    cc='gcc' if arch=='x86' else 'powerpc-linux-gnu-gcc'
    objcopy='objcopy' if arch=='x86' else 'powerpc-linux-gnu-objcopy'
    nm='nm' if arch=='x86' else 'powerpc-linux-gnu-nm'
    flags=['-O2','-g','-std=gnu89','-fno-strict-aliasing','-ffp-contract=off','-frounding-math',
           '-fno-tree-vectorize','-fno-stack-protector','-ffunction-sections','-fdata-sections','-Iinclude','-Iport/x360/include',
           '-DREF_VU0_EXTERN_ARITH']
    if arch=='ppc': flags+=['-DPORT_BIG_ENDIAN']
    if modelo=='360': flags+=['-Iport/x360/src/hw']
    flags+=['-Iport/src']
    objetos=[]
    # Expõe as instruções estáticas do arquivo real, sem duplicar a implementação.
    wrapper='''
void Teste_Op(int op,int m,int fd,int fs,int ft,int c) {
 switch(op) {
 case 0:op_vadd(m,fd,fs,ft);break; case 1:op_vmul(m,fd,fs,ft);break;
 case 2:op_vaddbc(m,fd,fs,ft,c);break; case 3:op_vsubbc(m,fd,fs,ft,c);break;
 case 4:op_vmulbc(m,fd,fs,ft,c);break; case 5:op_vmulabc(m,fs,ft,c);break;
 case 6:op_vmaddabc(m,fs,ft,c);break; case 7:op_vmaddbc(m,fd,fs,ft,c);break;
 }
}
'''
    if mutante:
        h=(ROOT/'port/x360/src/f2/vu0_nativo.c').read_text().replace(
            '#include "../../include/vu0_nativo.h"', '#include "vu0_nativo.h"')
        header=(ROOT/'port/x360/include/vu0_nativo.h').read_text()
        if mutante=='ordem': h=h.replace('float a = p[c] + p[4 + c];\n        a = a + p[8 + c];','float a = p[4 + c] + p[8 + c];\n        a = p[c] + a;')
        if mutante=='denormal': header=header.replace('return a == 0 || (a >= 0x30800000u && a <= 0x4e800000u);','return a <= 0x4e800000u;')
        if mutante=='arredondamento': h=h.replace('p[i] = a[i] * b[i];','{ int modo = fegetround(); fesetround(FE_TONEAREST); p[i] = a[i] * b[i]; fesetround(modo); }').replace('p[i] = r[i] * v[i / 4];','{ int modo = fegetround(); fesetround(FE_TONEAREST); p[i] = r[i] * v[i / 4]; fesetround(modo); }').replace('#include "vu0_nativo.h"','#include "vu0_nativo.h"\n#include <fenv.h>')
        (pasta/'vu0_nativo.h').write_text(header)
        (pasta/'vu0_nativo.c').write_text(h)
        flags.insert(0,'-I'+str(pasta))
    for antigo in [True,False]:
        prefixo='ant_' if antigo else 'novo_'
        osrc=[]
        for f in ['vu0_a','vu0_b']:
            src=pasta/(prefixo+f+'.c')
            texto=run(['git','show',f'{base}:src/port/{f}.c']).stdout if antigo else (ROOT/f'src/port/{f}.c').read_text()
            src.write_text(texto+(wrapper if f=='vu0_a' else ''))
            obj=src.with_suffix('.o')
            run([cc,*flags,*([] if antigo else ['-DPORT_X360']),'-c',src,'-o',obj]);osrc.append(obj)
        # O gancho da matriz é o real do float360 no novo, e desiste no antigo.
        src=pasta/(prefixo+'gancho.c')
        if antigo: src.write_text('int Port_FastMtxApply(const unsigned *r,const unsigned *v,unsigned *o){return 0;}')
        else: src.write_text('#include "'+str(ROOT/'port/x360/src/f2/float360.c')+'"\n')
        obj=src.with_suffix('.o');run([cc,*flags,'-c',src,'-o',obj]);osrc.append(obj)
        if antigo:
            nomes=set()
            for o in osrc:
                for l in run([nm,'-g','--defined-only',o]).stdout.splitlines():
                    if len(l.split())==3: nomes.add(l.split()[2])
            mapa=pasta/'renomes.txt';mapa.write_text(''.join(f'{n} Ant_{n}\n' for n in sorted(nomes)))
            for o in osrc:run([objcopy,'--redefine-syms='+str(mapa),o])
        objetos+=osrc
    nucleo=pasta/'nucleo.o'
    run([cc,*flags,'-c',pasta/'vu0_nativo.c' if mutante else ROOT/'port/x360/src/f2/vu0_nativo.c','-o',nucleo])
    objetos.append(nucleo)
    exe=pasta/'confere'
    run([cc,*flags,'-Wl,--gc-sections',*(['-static'] if arch=='ppc' else []),
         FERR/'teste_vu0_nativo.c',*objetos,'-lm','-o',exe])
    # O verificador já existente, com a matriz nova e o modelo da variante.
    driver=pasta/'fastvec_driver.c'
    driver.write_text('''#include <stdlib.h>
#include <fenv.h>
#include "softfloat_ps2_inl.h"
#include "vu0_nativo.h"
uint32_t Port_FastAdd(uint32_t a,uint32_t b) { return Sf_AddBits(a,b); }
uint32_t Port_FastMul(uint32_t a,uint32_t b) { return Sf_MulBits(a,b); }
int Port_FastMtxApply(const uint32_t *r,const uint32_t *v,uint32_t *out) {
 int ok=Vn_MtxApply(r,v,out);
 if(ok && getenv("VU0_ESTRAGA_FASTVEC")) out[0]^=1;
 return ok;
}
#define main Fastvec_Original
#include "'''+str(ROOT/'port/tools/fastvec_check.c')+'''"
#undef main
int main(int argc,char **argv) {
 if(fesetround(FE_TOWARDZERO)) return 2;
 return Fastvec_Original(argc,argv);
}
''')
    run([cc,*flags,'-Wl,--gc-sections',*(['-static'] if arch=='ppc' else []),
         driver,nucleo,'-lm','-o',pasta/'fastvec'])
    return ([str(exe)] if arch=='x86' else ['qemu-ppc',str(exe)])

def contagem(pasta):
    # Contagem estática por função e pares em posições iguais da pilha, sem debug.
    for f in sorted(pasta.glob('*.o')):
        texto=run(['powerpc-linux-gnu-objdump','-dr',f]).stdout
        f.with_suffix('.dis').write_text(texto)
        grupos=re.split(r'^[0-9a-f]+ <([^>]+)>:\n',texto,flags=re.M)
        for nome,corpo in zip(grupos[1::2],grupos[2::2]):
            if any(n in nome for n in ['mtx_apply','vmadd','Ref_Mtx_Mul','Ref_Vec4_Add','Ref_Vec4_Scale','Ref_Vu0_SinCos','Vn_', 'vn_vec4']):
                ins=re.findall(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(.*)',corpo,re.M)
                stores=re.findall(r'\bstw\s+[^,]+,(-?\d+)\(r1\)',corpo)
                loads=re.findall(r'\blfs\s+[^,]+,(-?\d+)\(r1\)',corpo)
                pares=sum(min(stores.count(s),loads.count(s)) for s in set(stores))
                fma=sum(bool(re.search(r'\b(?:f[ n]?madds?|fnmsubs?|fmsubs?)\b',s)) for s in ins)
                print(f'{f.name}: {nome}: instruções {len(ins)}, pares stw/lfs {pares}, fusões {fma}',flush=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--casos',type=int,default=2000000)
    ap.add_argument('--alvo',choices=['x86','ppc'],action='append');
    ap.add_argument('--base',default='9c69c4ba');ap.add_argument('--mutantes',action='store_true')
    ap.add_argument('--pasta',type=pathlib.Path,default=pathlib.Path.home()/'vu0-nativo-testes')
    args=ap.parse_args()
    for arch in (args.alvo or ['x86','ppc']):
        for modelo in ['inteiro','360']:
            pasta=args.pasta/(arch+'-'+modelo)
            cmd=montar(pasta,arch,modelo,args.base)
            p=run([*cmd,args.casos]);print(arch,modelo,p.stdout.strip(),flush=True)
            fast=[str(pasta/'fastvec')] if arch=='x86' else ['qemu-ppc',str(pasta/'fastvec')]
            p=run([*fast,max(1,(args.casos+999999)//1000000)])
            print('fastvec_check',arch,modelo,p.stdout.strip(),flush=True)
            p=run(['env','VU0_ESTRAGA_FASTVEC=1',*fast,1],falha=True)
            print('fastvec_check cópia estragada',arch,modelo,'saída',p.returncode,p.stdout[-600:].strip(),flush=True)
            if p.returncode!=1: raise RuntimeError('fastvec_check aceitou a cópia estragada')
            if arch=='ppc':contagem(pasta)
            for nome in ['Vec4_Add','Vec4_Scale','mtx_apply','Mtx_Mul','Vu0_SinCos',
                         'op_vadd','op_vmul','op_vaddbc','op_vsubbc','op_vmulbc','op_vmulabc','op_vmaddabc','op_vmaddbc']:
                negativo=run([*cmd,1,nome],falha=True)
                print('cópia estragada',arch,modelo,nome,negativo.returncode,negativo.stdout.strip(),flush=True)
                if negativo.returncode!=1 or 'FALHA ' not in negativo.stdout: raise RuntimeError('comparador não recusou a cópia')
            if args.mutantes:
                for mutante in ['arredondamento','ordem','denormal']:
                    cmd=montar(pasta/mutante,arch,modelo,args.base,mutante)
                    p=run([*cmd,10000],falha=True)
                    print('mutante',arch,modelo,mutante,'saída',p.returncode,p.stdout.strip(),flush=True)
                    if p.returncode!=1 or 'FALHA ' not in p.stdout: raise RuntimeError('mutante não foi recusado pelo comparador')
if __name__=='__main__':main()
