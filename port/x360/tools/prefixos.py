#!/usr/bin/env python3
"""Confere o censo do ensaio; não transforma fechamento em autorização D3D.

BT3_GS_PREFIXOS=1: a barreira integral (seção 35/36), nenhum liberado. BT3_GS_PREFIXOS=2: as regras reais (seção 36),
com quantos sairiam antes do fim, se o plano da EDRAM repetiria o do quadro anterior, e a curva do relógio da gravação
nos quadros pedidos (o prefixos_teto.py a usa como progresso medido na x86/PPC, não no Xenon)."""
import argparse
from pathlib import Path
import re

PADRAO=re.compile(r'^prefixos: quadro (\d+) fechados (\d+) liberados (\d+) barreira (\d+) fila (\d+)/(\d+) bytes (\d+) esperas (\d+) hash ([0-9a-f]{16})$',re.M)
PADRAO2=re.compile(r'^prefixos2: quadro (\d+) desenhos (\d+) liberados (\d+) plano ([01]) zmax (\d+) datelongos (\d+) fila (\d+)/(\d+) esperas (\d+) hash ([0-9a-f]{16})$',re.M)
CURVA=re.compile(r'^prefixos2: curva quadro (\d+) desenhos (\d+) ms ([0-9.]+) milesimos((?: \d+){17})$',re.M)
# (PX_CAP+1)*sizeof(PxItem) no layout de 32 bits: x86/PPC, sem e com o alfa (o GsQuadro tem o ponteiro dele só com GS_ALFA)
BYTES={2556,2592,2628,2664}


def confere(s):
    linhas=PADRAO.findall(s); linhas2=PADRAO2.findall(s)
    listas=re.findall(r'^gpu_falso: quadro (\d+) lista [0-9a-f]{16} desenhos (\d+) ',s,re.M)
    assert (linhas or linhas2) and listas,'censo ou listas ausentes'
    assert not (linhas and linhas2),'dois modos no mesmo log'
    assert len(listas)==len({x[0] for x in listas}),'lista duplicada'
    assert re.search(r'^roteiro: .* fim ',s,re.M),'fim ausente'
    total=re.search(r'^gpu_falso: (\d+) quadros, hash do jogo ',s,re.M)
    assert not re.search(r'FALHA|CRASH|race:|textura solta usada',s),'falha na execução'
    if linhas:
        assert len(linhas)==len({x[0] for x in linhas}),'quadro duplicado'
        assert {int(x[0]):int(x[1]) for x in linhas}=={int(q):int(n) for q,n in listas},'cobertura por quadro/desenho'
        assert len(re.findall(r'^prefixos:',s,re.M))==len(linhas),'linha incompleta'
        assert total and int(total[1])==len(linhas),'total de quadros'
        for q,n,lib,b,m,cap,mem,esp,h in linhas:
            assert int(lib)==0 and int(b)==int(n),'barreira integral alterada'
            assert int(cap)==8 and 0<=int(m)<=8 and (int(n)==0 or int(m)>0),'fila fora do limite'
            assert int(mem) in BYTES,'memória inesperada no layout de 32 bits'
        return 1,linhas,[]
    assert len(linhas2)==len({x[0] for x in linhas2}),'quadro duplicado'
    assert {int(x[0]):int(x[1]) for x in linhas2}=={int(q):int(n) for q,n in listas},'cobertura por quadro/desenho'
    assert len(re.findall(r'^prefixos2: quadro',s,re.M))==len(linhas2),'linha incompleta'
    assert len(re.findall(r'^prefixos2: curva',s,re.M))==len(CURVA.findall(s)),'curva incompleta'
    assert total and int(total[1])==len(linhas2),'total de quadros'
    for q,n,lib,pl,zm,dl,m,cap,esp,h in linhas2:
        assert int(lib)<=max(int(n)-1,0),'liberado além do último fechado'
        assert int(cap)==8 and 0<=int(m)<=8,'fila fora do limite'
    curvas=CURVA.findall(s)
    por={int(x[0]):int(x[1]) for x in linhas2}
    for q,n,ms,v in curvas:
        v=[int(x) for x in v.split()]
        assert int(q) in por and por[int(q)]==int(n),'curva de quadro sem censo'
        assert v[0]>=0 and v[-1]==1000 and all(a<=b for a,b in zip(v,v[1:])),'curva fora de ordem'
    return 2,linhas2,curvas


def mutantes():
    s=('prefixos: quadro 7 fechados 33 liberados 0 barreira 33 fila 8/8 bytes 2592 esperas 4 hash 9b3728724d06c871\n'
       'gpu_falso: quadro 7 lista 0000000000000000 desenhos 33 vértices 33\n'
       'roteiro: f5_duelo.txt:99 fim vblank 11478 quadro 7757\n'
       'gpu_falso: 1 quadros, hash do jogo 0000000000000000\n')
    s2=('prefixos2: quadro 7 desenhos 33 liberados 20 plano 1 zmax 5 datelongos 0 fila 8/8 esperas 4 hash 9b3728724d06c871\n'
        'prefixos2: curva quadro 7 desenhos 33 ms 1.500 milesimos 0 10 20 30 40 50 60 70 80 90 100 200 300 400 500 600 1000\n'
        'gpu_falso: quadro 7 lista 0000000000000000 desenhos 33 vértices 33\n'
        'roteiro: f5_duelo.txt:99 fim vblank 11478 quadro 7757\n'
        'gpu_falso: 1 quadros, hash do jogo 0000000000000000\n')
    confere(s); confere(s2)
    for base,a,b in [(s,'liberados 0','liberados 1'),(s,'barreira 33','barreira 32'),(s,'fila 8/8','fila 9/8'),
                     (s,'bytes 2592','bytes 0'),(s,'desenhos 33','desenhos 32'),(s,' fim ',' cortado '),
                     (s,'gpu_falso: 1 quadros','gpu_falso: 2 quadros'),(s,'prefixos: quadro 7','prefixos: quadro 8'),
                     (s2,'liberados 20','liberados 33'),(s2,'plano 1','plano 2'),(s2,'desenhos 33 liberados','desenhos 34 liberados'),
                     (s2,'milesimos 0 10 20','milesimos 0 30 20'),(s2,' 600 1000',' 600 999'),(s2,'curva quadro 7','curva quadro 8'),
                     (s2,'fila 8/8','fila 9/8'),(s2,'gpu_falso: 1 quadros','gpu_falso: 2 quadros')]:
        try: confere(base.replace(a,b))
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
            modo,linhas,curvas=confere(a.log.read_text(errors='replace'))
            if modo==1:
                print(f'{len(linhas)} quadros, {sum(int(x[1]) for x in linhas)} descritores fechados, 0 liberados ao D3D.')
                for x in linhas:
                    if int(x[0]) in (6930,6960,7170): print(f'quadro {x[0]}: {x[1]} fechados, {x[3]} com barreira integral; teto desta partição 0 ms.')
            else:
                muda=[int(x[0]) for x in linhas if x[3]=="0"]
                luta=[q for q in muda if 6000<=q<=8378]
                print(f'{len(linhas)} quadros nas regras reais, {sum(int(x[2]) for x in linhas)} de {sum(int(x[1]) for x in linhas)} '
                      f'desenhos sairiam antes do fim; plano da EDRAM diferente do anterior em {len(muda)} quadros, '
                      f'{len(luta)} entre 6000 e 8378{": " + ",".join(map(str, luta[:40])) if luta else ""}.')
                for x in linhas:
                    if int(x[0]) in (6929,6930,6959,6960,6961,7169,7170):
                        print(f'quadro {x[0]}: {x[2]} de {x[1]} antes do fim, plano igual ao anterior {x[3]}, zmax {x[4]}, DATE longos {x[5]}')
                for q,n,ms,v in curvas:
                    print(f'curva {q}: {n} desenhos, {ms} ms na gravação, milésimos a cada 1/16 da lista:{v}')
        print('tudo certo no ensaio; não é aceite de prefixos D3D')
    except (OSError,AssertionError) as e: p.exit(1,f'FALHA: {e}\n')
