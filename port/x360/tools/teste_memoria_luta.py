#!/usr/bin/env python3
"""Provas negativas da F5 sobre cópias de dumps reais, nunca sobre os dados originais.

Uso: teste_memoria_luta.py X86.dump PPC.dump PROG_X86 PROG_PPC BASE_DONOS
Cada subalocação com campo vivo recebe um número errado; slot, geração e dono são estragados separadamente.
As uniões, formas, câmeras e tarefas GSC que o ponto alcança têm provas próprias.
"""
import contextlib
import copy
import io
import pathlib
import struct
import sys
import tempfile

import memdiff
from memoria_luta import Contexto, ContextoErro, parear_testemunhos


def main():
    dx,dp,ex,ep,base=sys.argv[1:]
    x,p=memdiff.ler(dx),memdiff.ler(dp)
    lx,lp=memdiff.nm(ex,False),memdiff.nm(ep,True)
    ctx=Contexto(x,p,lx,lp,dx,dp,base,(ex,ep))
    fx,fp=memdiff.faixa_do_programa(lx),memdiff.faixa_do_programa(lp)
    provas=0

    def numero(a,m,nome,ap=None):
        nonlocal provas
        # Primeiro número/ponteiro tipado; só escolher byte se a peça viva não tiver número.
        candidatos=[i for i,c in enumerate(m) if c in (2,4,8,0x44,0x48,0x54)]
        if not candidatos:candidatos=[i for i,c in enumerate(m) if c==0x84]
        if not candidatos:candidatos=[i for i,c in enumerate(m) if c==1]
        if not candidatos:
            print(f'SEM CONTEÚDO VIVO: {nome}')
            return
        o=candidatos[0];c=m[o]
        w=c if c in (2,4,8) else 4 if c in (0x44,0x54,0x84) else 8 if c==0x48 else 1
        bx=ctx.x.bytes(a,len(m));bp=bytearray(ctx.p.bytes(a if ap is None else ap,len(m)))
        bp[o:o+w]=bytes([0xde,0xad,0xbe,0xef,0xa5,0x5a,0x31,0x73][:w])
        cl,_=memdiff.pelo_tipo(bx,bytes(bp),m,fx,fp,len(m),a)
        assert cl['fora-do-formato']+cl['float-proximo'],f'ACEITOU número vivo errado: {nome}'
        provas+=1
        print(f'RECUSOU número: {nome}, +{o:#x}')

    # Intacto deve passar inclusive a comparação de todo campo dinâmico.
    for a,m in ctx.mapas.items():
        ap=ctx.p.simbolos['gBtlScript'] if a==ctx.x.simbolos['gBtlScript'] else a
        cl,_=memdiff.pelo_tipo(ctx.x.bytes(a,len(m)),ctx.p.bytes(ap,len(m)),m,fx,fp,len(m),a)
        assert not cl['fora-do-formato'] and not cl['float-proximo'],f'Ponto intacto diferente em {a:#x}'
    print('INTACTO: contextos e campos dinâmicos conformes')
    for a,(n,t,m,dono) in ctx.aloc.items():
        numero(a,m,f'BtlPool {dono}, {t or "trabalho contextual"}')
    # Cada testemunho de subalocação com contexto errado, mesmo que todos os bytes do jogo estejam intactos.
    original=pathlib.Path(dp[:-5]+'.pool').read_text().splitlines()
    with tempfile.TemporaryDirectory(prefix='f5-negativos-',dir=pathlib.Path.home()) as temp:
        alvo=pathlib.Path(temp)/'ppc.pool'
        for i,linha in enumerate(original):
            if not linha.startswith('A '):continue
            for campo,rotulo in ((1,'slot'),(2,'geração'),(6,'dono')):
                q=linha.split()
                if campo==6:
                    outro=next(l.split()[6] for l in original if l.startswith('A ') and
                               ctx.p.funcao(int(l.split()[6])) != ctx.p.funcao(int(q[6])))
                    q[campo]=outro
                else:q[campo]=str(int(q[campo])+1)
                alteradas=original.copy();alteradas[i]=' '.join(q)
                alvo.write_text('\n'.join(alteradas)+'\n')
                try:
                    parear_testemunhos(dx[:-5]+'.pool',str(alvo),ctx.x,ctx.p,ctx.schema['vblank'])
                except (ContextoErro,ValueError):pass
                else:raise AssertionError(f'ACEITOU {rotulo} errado na subalocação {linha.split()[3]}')
                provas+=1
            print(f'RECUSOU slot, geração e dono: registro {linha.split()[1:4]}')
    def alterar(regs,a,raw):
        saida=[];achou=False
        for nome,ra,d in regs:
            if ra<=a and a+len(raw)<=ra+len(d):
                d=d[:a-ra]+raw+d[a-ra+len(raw):];achou=True
            saida.append((nome,ra,d))
        assert achou,'endereço do estrago fora do dump'
        return saida

    def contexto_errado(a,raw,nome):
        nonlocal provas
        try:Contexto(x,alterar(p,a,raw),lx,lp,dx,dp,base,(ex,ep))
        except (ContextoErro,ValueError):pass
        else:raise AssertionError(f'ACEITOU contexto errado: {nome}')
        provas+=1
        print(f'RECUSOU contexto: {nome}')

    # As variantes ativas de requisição têm campo e discriminante provados à parte.
    ax,ap=ctx.x.simbolos['gBtlScript'],ctx.p.simbolos['gBtlScript']
    n=ctx.x.u32(ax+0xbc4)
    for i in range(n):
        o=0x8a4+i*16;kind=ctx.x.u32(ax+o)
        numero(ax+o+8,ctx.mapas[ax][o+8:o+16],f'BtlScriptRequest[{i}] kind {kind}',ap+o+8)
        contexto_errado(ap+o,struct.pack('>I',3-kind),f'kind da requisição {i}')
    for a,m,task,classe in ctx.trabalhos_vivos:
        numero(a,m,f'trabalho vivo {classe}')
        outros=[t for _,_,t,c in ctx.trabalhos_vivos if c!=classe]
        errado=ctx.p.u32(outros[0]+0x28) if outros else 0
        contexto_errado(task+0x28,struct.pack('>I',errado),f'classe dona de {classe}')
    for a,m,rec,tipo in ctx.formas_vivas:
        numero(a,m,f'forma {tipo}')
        contexto_errado(rec+0x70,struct.pack('>I',9),f'tipo dono de {tipo}')
    # Cada sub-bloco de câmera, além da alocação maior que o contém.
    cam=ctx.x.global_('gBtlCam');demo=ctx.x.global_('gDemoCam')
    if cam:
        for i in range(3):numero(cam+i*0x260,ctx.mapas[cam][i*0x260:(i+1)*0x260],f'layout de câmera {i}')
        for i in range(2):numero(cam+0x720+i*0x290,ctx.mapas[cam][0x720+i*0x290:0x720+(i+1)*0x290],f'vista do lado {i}')
        numero(demo,ctx.mapas[demo],'câmera demo')
        contexto_errado(cam+0xc40,struct.pack('>I',cam+0x10),'dono da vista corrente')
        contexto_errado(demo+0x308,struct.pack('>I',0x100),'modo da câmera demo')
    for nome,passo in (('gGscTaskPool',0x50),('gGscFilePool',0x24)):
        a=ctx.x.global_(nome)
        if a in ctx.mapas:
            m=ctx.mapas[a]
            for i in range(len(m)//passo):numero(a+i*passo,m[i*passo:(i+1)*passo],f'{nome}[{i}]')
            contexto_errado(ctx.p.simbolos[nome],struct.pack('>I',a+4),f'dono de {nome}')
    print(f'TUDO CERTO: {provas} estragos recusados, intacto conforme')


if __name__=='__main__':
    main()
