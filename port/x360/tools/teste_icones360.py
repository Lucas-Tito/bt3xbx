#!/usr/bin/env python3
"""Prova a escrita isolada da folha, a arte e a volta fiel PSMT4, sem emulador."""
import argparse
import ctypes as c
from pathlib import Path
import struct
import subprocess
import tempfile
import icones360 as arte
from psmt4 import codificar, decodificar


def exigir(ok, texto):
    if not ok: raise ValueError(texto)


def folha(boot, ordem):
    a,b=struct.unpack_from(ordem+'II',boot,32)
    return a,b


def textura(boot,i,ordem):
    a,b=folha(boot,ordem)
    p,c,pt,ct=struct.unpack_from(ordem+'IIII',boot,a+32+i*64)
    return a+p*4+96,pt-128,a+c*4+96,ct-128


def conferir(original,normal,novo):
    exigir(len(original)==len(normal)==len(novo)==140416,'tamanho do boot mudou')
    exigir(folha(original,'<')==folha(novo,'>')==(0x19a40,0x22080),'entrada 8 mudou de posição')
    esperado=bytearray(normal)
    for i in range(28):
        p,n,c,t=textura(original,i,'<');lado=32 if n==512 else 64
        pixels=original[p:p+n];im=decodificar(pixels,lado)
        exigir(codificar(im)==pixels,f'volta PSMT4 original {i}')
        im[0][0]^=1
        exigir(codificar(im)!=pixels,f'pixel adulterado passou {i}')
        if i<10:
            exigir(novo[p:p+n]==pixels and novo[c:c+t]==original[c:c+t],f'ícone preservado {i} mudou')
            continue
        im,pal=arte.arte(i);pix=codificar(im)
        exigir(decodificar(pix,lado)==im,f'volta da arte {i}')
        exigir(novo[p:p+n]==pix and novo[c:c+t]==pal,f'arte diferente na textura {i}')
        exigir(pix!=pixels,f'ícone {i} permaneceu original')
        exigir(len(pal)==64 and max(pal[3::4])<=128,f'paleta PS2 inválida {i}')
        esperado[p:p+n]=pix;esperado[c:c+t]=pal
        bx,by,ex,ey=arte.CAIXAS[i]
        for y,l in enumerate(im):
            for x,v in enumerate(l):
                if pal[v*4+3]:
                    inicio = max(by, (9 if 20<=i<=23 else 3) if x>=32 else by)
                    exigir(y<32 and bx<=x%32<ex and inicio<=y<ey,f'arte fora da caixa {i}')
        if lado==64:
            exigir(any(im[y][x]!=im[y][x+32] for y in range(32) for x in range(32)),f'par sem estado {i}')
            exigir(any(im[y][x+32] for y in range(32) for x in range(32)),f'quadro apertado vazio {i}')
        if i>=24:
            exigir(any(v==1 for l in im for v in l),f'marcador amarelo ausente {i}')
    exigir(novo==esperado,'bytes fora dos 18 fluxos de pixel/CLUT foram alterados')


def testar_api(pasta, base):
    subprocess.run(['gcc','-std=c89','-shared','-fPIC','-Wall','-Wextra','-Werror',str(arte.RAIZ / "port/x360/conv/icones360.c"),'-o',str(pasta / "icones.so")],check=True)
    class Arte(c.Structure): _fields_=[('i',c.c_uint),('n',c.c_uint),('pix',c.POINTER(c.c_ubyte)),('pal',c.POINTER(c.c_ubyte))]
    lib=c.CDLL(str(pasta / "icones.so"));f=lib.Conv_Icones360
    f.argtypes=[c.POINTER(c.c_ubyte),c.c_ulong,c.POINTER(Arte),c.c_uint,c.c_char_p];f.restype=c.c_int
    ar=[];keep=[]
    for i in range(10,28):
     im,p=arte.arte(i);b=codificar(im);x=(c.c_ubyte*len(b)).from_buffer_copy(b);y=(c.c_ubyte*64).from_buffer_copy(p);keep.extend([x,y]);ar.append(Arte(i,len(b),x,y))
    ar=(Arte*18)(*ar)
    for caso in range(5):
     buf=(c.c_ubyte*len(base)).from_buffer_copy(base);msg=c.create_string_buffer(128)
     if caso==0: ar[0].i=8
     if caso==1: ar[0].n=511
     if caso==2: ar[0].pal[3]=129
     if caso==3: buf[0x19a40+32+10*64]=255
     antes=bytes(buf);r=f(buf,len(buf),ar,18,msg)
     if caso<4: assert r==-1 and bytes(buf)==antes,(caso,r)
     else: assert r==1 and bytes(buf)!=antes
     ar[0].i=10;ar[0].n=512;ar[0].pal[3]=0
    print('API: índice, tamanho, alfa e deslocamento estragados recusados sem escrita; válida aceita')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dados',type=Path,required=True)
    p.add_argument('--conv-tool',type=Path,required=True)
    p.add_argument('--boot',type=Path,help='conferir cópia externa já convertida com ícones')
    a=p.parse_args();tool=str(a.conv_tool.resolve());nome='pzs3us1/00004.bin'
    with tempfile.TemporaryDirectory(prefix='icones-') as t:
        t=Path(t)
        for d,op in [('normal',[]),('novo',['--icones360'])]:
            subprocess.run([tool,*op,'--saida',str(t/d),str(a.dados),nome],check=True,capture_output=True)
        original=(a.dados/nome).read_bytes();normal=(t/'normal'/nome).read_bytes()
        novo=(a.boot or t/'novo'/nome).read_bytes()
        conferir(original,normal,novo)
        testar_api(t, normal)
        # O verificador completo precisa falhar em execução com arte adulterada.
        p,n,_,_=textura(novo,10,'>');estrago=bytearray(novo);estrago[p]^=1
        try: conferir(original,normal,estrago)
        except ValueError as e:
            exigir('arte diferente' in str(e),'estrago falhou por outro motivo')
        else: raise ValueError('verificador aceitou arte adulterada')
    print('28 voltas originais fiéis; 18 ícones novos; 0 a 9 e todos os demais bytes preservados')
    print('pixel adulterado e arte adulterada: verificador visto falhando')
    print('teste_icones360: tudo certo')


if __name__=='__main__':
    try:main()
    except (ValueError, OSError, subprocess.CalledProcessError) as e:
        raise SystemExit('FALHA: '+str(e))
