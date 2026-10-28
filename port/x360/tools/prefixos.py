#!/usr/bin/env python3
"""Confere o censo do ensaio; não transforma fechamento em autorização D3D."""
import argparse
from pathlib import Path
import re

PADRAO=re.compile(r'^prefixos: quadro (\d+) fechados (\d+) liberados (\d+) barreira (\d+) fila (\d+)/(\d+) bytes (\d+) esperas (\d+) hash ([0-9a-f]{16})$',re.M)


def confere(s):
    linhas=PADRAO.findall(s)
    listas=re.findall(r'^gpu_falso: quadro (\d+) lista [0-9a-f]{16} desenhos (\d+) ',s,re.M)
    assert linhas and listas,'censo ou listas ausentes'
    assert len(linhas)==len({x[0] for x in linhas}),'quadro duplicado'
    assert len(listas)==len({x[0] for x in listas}),'lista duplicada'
    assert {int(x[0]):int(x[1]) for x in linhas}=={int(q):int(n) for q,n in listas},'cobertura por quadro/desenho'
    assert len(re.findall(r'^prefixos:',s,re.M))==len(linhas),'linha incompleta'
    assert re.search(r'^roteiro: .* fim ',s,re.M),'fim ausente'
    total=re.search(r'^gpu_falso: (\d+) quadros, hash do jogo ',s,re.M)
    assert total and int(total[1])==len(linhas),'total de quadros'
    assert not re.search(r'FALHA|CRASH|race:|textura solta usada',s),'falha na execução'
    for q,n,lib,b,m,cap,mem,esp,h in linhas:
        assert int(lib)==0 and int(b)==int(n),'barreira integral alterada'
        assert int(cap)==8 and 0<=int(m)<=8 and (int(n)==0 or int(m)>0),'fila fora do limite'
        assert int(mem) in (2592,2664),'memória inesperada no layout de 32 bits'
    return linhas


def mutantes():
    s=('prefixos: quadro 7 fechados 33 liberados 0 barreira 33 fila 8/8 bytes 2592 esperas 4 hash 9b3728724d06c871\n'
       'gpu_falso: quadro 7 lista 0000000000000000 desenhos 33 vértices 33\n'
       'roteiro: f5_duelo.txt:99 fim vblank 11478 quadro 7757\n'
       'gpu_falso: 1 quadros, hash do jogo 0000000000000000\n')
    confere(s)
    for a,b in [('liberados 0','liberados 1'),('barreira 33','barreira 32'),('fila 8/8','fila 9/8'),
                ('bytes 2592','bytes 0'),('desenhos 33','desenhos 32'),(' fim ',' cortado '),
                ('gpu_falso: 1 quadros','gpu_falso: 2 quadros'),('prefixos: quadro 7','prefixos: quadro 8')]:
        try: confere(s.replace(a,b))
        except AssertionError: print(f'mutante recusado: {a}')
        else: raise AssertionError(f'mutante aceito: {a}')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('log',type=Path,nargs='?'); p.add_argument('--mutantes',action='store_true')
    a=p.parse_args()
    try:
        if a.mutantes: mutantes()
        else:
            if a.log is None: p.error('informe o log')
            linhas=confere(a.log.read_text(errors='replace'))
            print(f'{len(linhas)} quadros, {sum(int(x[1]) for x in linhas)} descritores fechados, 0 liberados ao D3D.')
            for x in linhas:
                if int(x[0]) in (6930,6960,7170): print(f'quadro {x[0]}: {x[1]} fechados, {x[3]} com barreira integral; teto desta partição 0 ms.')
        print('tudo certo no ensaio; não é aceite de prefixos D3D')
    except (OSError,AssertionError) as e: p.exit(1,f'FALHA: {e}\n')
