#!/usr/bin/env python3
"""Reduz a arte CC0 do Kenney para PSMT4, sem ler dados do jogo.

As caixas são medidas da folha USA, apenas coordenadas. A arte é uma entrada
(--arte); o restante do conversor recebe só pixels e paletas gerados aqui.
"""
import argparse
from collections import Counter
from pathlib import Path
import struct
import zlib
from psmt4 import codificar, decodificar, png

RAIZ = Path(__file__).resolve().parents[3]
ARTE = RAIZ / 'port/x360/arte/kenney'
NOMES = dict(enumerate(['button_color_b', 'button_color_x', 'button_color_y',
                       'button_color_a', 'lb', 'lt', 'stick_l_press', 'rb', 'rt',
                       'stick_r_press', 'button_color_b', 'button_color_x',
                       'button_color_y', 'button_color_a', 'button_color_x',
                       'button_color_y', 'stick_l_press', 'stick_r_press'], 10))
CAIXAS = {i: (0, 4, 20, 29) for i in range(10, 28)}
CAIXAS.update({i: (0, 6, 29, 27) for i in (14, 15, 17, 18)})
CAIXAS.update({i: (0, 3, 31, 31) for i in (16, 19)})
CAIXAS.update({i: (0, 3, 20, 29) for i in (24, 25)})
CAIXAS.update({i: (0, 0, 31, 31) for i in (26, 27)})
GRANDES = (20, 21, 22, 23, 26, 27)


def ler_png(caminho):
    """PNG indexado de 4 ou 8 bits do pacote original, com os cinco filtros e CRC."""
    b = caminho.read_bytes()
    if b[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('assinatura PNG inválida')
    pos, dados, pal, alfa = 8, bytearray(), None, b''
    while pos < len(b):
        n = struct.unpack_from('>I', b, pos)[0]
        tipo, v = b[pos+4:pos+8], b[pos+8:pos+8+n]
        if len(v) != n or zlib.crc32(tipo + v) != struct.unpack_from('>I', b, pos+8+n)[0]:
            raise ValueError('CRC PNG inválido')
        if tipo == b'IHDR':
            w, h, bits, cor, comp, filtro, entrelaco = struct.unpack('>IIBBBBB', v)
            if bits not in (4, 8) or (cor, comp, filtro, entrelaco) != (3, 0, 0, 0):
                raise ValueError('esperado PNG indexado de 4 ou 8 bits sem entrelaçamento')
        elif tipo == b'PLTE':
            pal = [tuple(v[k:k+3]) for k in range(0, n, 3)]
        elif tipo == b'tRNS':
            alfa = v
        elif tipo == b'IDAT':
            dados.extend(v)
        elif tipo == b'IEND':
            break
        pos += 12 + n
    bruto = zlib.decompress(dados)
    passo = (w*bits+7)//8
    if pal is None or len(bruto) != (passo+1)*h:
        raise ValueError('tamanho ou paleta PNG inválida')
    linhas, anterior = [], [0]*passo
    for y in range(h):
        f, linha = bruto[y*(passo+1)], list(bruto[y*(passo+1)+1:(y+1)*(passo+1)])
        if f > 4:
            raise ValueError('filtro PNG inválido')
        for x in range(passo):
            a, c, d = linha[x-1] if x else 0, anterior[x], anterior[x-1] if x else 0
            p = a+c-d
            dist = [abs(p-a), abs(p-c), abs(p-d)]
            pred = (a, c, d)[dist.index(min(dist))]
            linha[x] = (linha[x] + (0, a, c, (a+c)//2, pred)[f]) & 255
        indices = linha if bits == 8 else [v for b in linha for v in (b>>4, b&15)][:w]
        linhas.append([pal[v] + (alfa[v] if v < len(alfa) else 255,) for v in indices])
        anterior = linha
    return linhas


def reduzir(im, w, h):
    # Média de área com alfa premultiplicado, toda em inteiros para reprodução.
    sh, sw = len(im), len(im[0])
    out = []
    for y in range(h):
        linha = []
        for x in range(w):
            soma = [0]*4
            for sy in range(y*sh//h, ((y+1)*sh+h-1)//h):
                dy = min((y+1)*sh, (sy+1)*h)-max(y*sh, sy*h)
                for sx in range(x*sw//w, ((x+1)*sw+w-1)//w):
                    dx = min((x+1)*sw, (sx+1)*w)-max(x*sw, sx*w)
                    r,g,b,a = im[sy][sx]; peso = dx*dy
                    for k,v in enumerate((r*a,g*a,b*a,a)): soma[k] += peso*v
            a = (soma[3]+sw*sh//2)//(sw*sh)
            linha.append(tuple((v+soma[3]//2)//soma[3] for v in soma[:3])+(a,)
                         if soma[3] else (0,0,0,0))
        out.append(linha)
    return out


def quantizar(im):
    # Reserva transparente e amarelo; median cut ponderado nas outras 14 cores.
    amarelo = (255, 220, 0, 128)
    rgba = [[tuple(p[:3])+((p[3]*128+127)//255,) if p[3] else (0,0,0,0)
             for p in linha] for linha in im]
    cont = Counter(p for l in rgba for p in l if p[3] and p != amarelo)
    caixas = [list(sorted(cont))]
    while len(caixas) < 14:
        candidatas = [(max(max(p[k] for p in c)-min(p[k] for p in c) for k in range(4)),
                       sum(cont[p] for p in c), j) for j,c in enumerate(caixas) if len(c)>1]
        if not candidatas: break
        _,_,j = max(candidatas); c=caixas.pop(j)
        k=max(range(4), key=lambda k: max(p[k] for p in c)-min(p[k] for p in c))
        c.sort(key=lambda p:(p[k],p)); total=sum(cont[p] for p in c); acum=0
        for corte,p in enumerate(c[:-1],1):
            acum+=cont[p]
            if acum*2>=total: break
        caixas.extend((c[:corte],c[corte:]))
    pal = [(0,0,0,0), amarelo]
    for c in caixas:
        n=sum(cont[p] for p in c)
        pal.append(tuple((sum(p[k]*cont[p] for p in c)+n//2)//n for k in range(4)))
    pal += [(0,0,0,0)]*(16-len(pal))
    def indice(p):
        if not p[3]: return 0
        if p==amarelo: return 1
        return min(range(2,2+len(caixas)), key=lambda i: sum((p[k]-pal[i][k])**2 for k in range(4)))
    return [[indice(p) for p in l] for l in rgba], bytes(v for p in pal for v in p)


def arte(indice, pasta=ARTE):
    fonte = ler_png(pasta / ('xbox_'+NOMES[indice]+'.png'))
    pontos = [(x,y) for y,l in enumerate(fonte) for x,p in enumerate(l) if p[3]]
    x0,y0=min(x for x,y in pontos),min(y for x,y in pontos)
    x1,y1=max(x for x,y in pontos)+1,max(y for x,y in pontos)+1
    fonte = [l[x0:x1] for l in fonte[y0:y1]]
    bx,by,ex,ey=CAIXAS[indice]; marcador=indice>=24
    topo=by+(6 if marcador else 0)
    w,h=ex-bx,ey-topo
    if w*(y1-y0)>h*(x1-x0): w=max(1,h*(x1-x0)//(y1-y0))
    else: h=max(1,w*(y1-y0)//(x1-x0))
    fonte=reduzir(fonte,w,h); lado=64 if indice in GRANDES else 32
    im=[[(0,0,0,0)]*lado for _ in range(lado)]
    for quadro in range(2 if lado==64 else 1):
        # O segundo quadro cabe também na caixa menor do original apertado.
        byq = max(by, 3 if indice >= 26 and quadro else by)
        t=max(topo, 9 if 20<=indice<=23 and quadro else topo, byq+6 if marcador else byq)
        f=reduzir(fonte,w,min(h,ey-t))
        ox=bx+(ex-bx-w)//2+quadro*32; oy=t+(ey-t-len(f))//2
        for y,l in enumerate(f):
            for x,p in enumerate(l):
                im[oy+y][ox+x]=tuple(v*3//4 for v in p[:3])+(p[3],) if quadro else p
        if marcador:
            cx=quadro*32+(bx+ex)//2
            for y in range(5):
                for x in range(cx-4+y,cx+5-y): im[byq+y][x]=(255,220,0,255)
    return quantizar(im)


def cabecalho(pasta=ARTE):
    linhas=['/* Kenney, Input Prompts 1.5A, CC0. Gerado por tools/icones360.py. */']
    for i in NOMES:
        im,pal=arte(i,pasta)
        for nome,dados in [('pix',codificar(im)),('pal',pal)]:
            linhas.append('static const unsigned char icone_%s_%d[] = {'%(nome,i))
            for k in range(0,len(dados),24): linhas.append('    '+','.join('0x%02x'%v for v in dados[k:k+24])+',')
            linhas.append('};')
    linhas.append('static const ConvIcone360 icones360[] = {')
    for i in NOMES: linhas.append('    {%d, sizeof(icone_pix_%d), icone_pix_%d, icone_pal_%d},'%(i,i,i,i))
    return '\n'.join(linhas+['};',''])


def contato(caminho,pasta):
    # Cada textura inteira, identificada pelo índice (dígitos vetoriais simples).
    tela=[[(48,48,48,255)]*640 for _ in range(4*158)]
    digitos=('111101101101111','010110010010111','111001111100111','111001111001111','101101111001001',
             '111100111001111','111100111101111','111001001001001','111101111101111','111101111001111')
    for j,i in enumerate(NOMES):
        im,pal=arte(i,pasta); ox=j%5*128;oy=j//5*158+26
        for y,l in enumerate(im):
            for x,v in enumerate(l):
                p=pal[v*4:v*4+4];a=p[3]*255//128
                cor=tuple((c*a+75*(255-a))//255 for c in p[:3])+(255,)
                for dy in range(2):
                    for dx in range(2):tela[oy+y*2+dy][ox+x*2+dx]=cor
        for k,d in enumerate(str(i)):
            for n,v in enumerate(digitos[int(d)]):
                if v=='1':
                    for yy in range(2):
                        for xx in range(2):tela[oy-20+n//3*2+yy][ox+k*8+n%3*2+xx]=(255,255,255,255)
    png(caminho,tela)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arte',type=Path,default=ARTE)
    p.add_argument('--cabecalho',type=Path,default=RAIZ/'port/x360/conv/icones360_arte.h')
    p.add_argument('--conferir',action='store_true')
    p.add_argument('--contato',type=Path)
    a=p.parse_args();texto=cabecalho(a.arte)
    if a.conferir:
        if a.cabecalho.read_text()!=texto: raise ValueError('arte gerada diverge dos PNG')
    else:a.cabecalho.write_text(texto)
    if a.contato:contato(a.contato,a.arte)
    print('icones360: tudo certo')


if __name__=='__main__':main()
