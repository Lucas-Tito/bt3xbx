#!/usr/bin/env python3
"""Confere o rateio exclusivo da gravação e lista os quadros pesados em ms."""
import argparse
import pathlib
import re
import statistics

ETAPAS = 'dma vif gif cpu vu1 unpack envio entrega hash constantes fecho'.split()

def confere(texto):
    if 'roteiro: FALHA' in texto or 'CRASH' in texto:
        raise ValueError('o jogo falhou')
    if not re.search(r'^roteiro: .* fim ', texto, re.M) or not re.search(r'^gpu_falso: \d+ quadros, hash do jogo', texto, re.M):
        raise ValueError('falta o fim do roteiro ou da GPU')
    quadros = {}
    for linha in texto.splitlines():
        if not linha.startswith('cortes: gravacao '):
            continue
        m = re.fullmatch(r'cortes: gravacao quadro (\d+) total (\d+\.\d+)' + ''.join(r' ' + e + r' (\d+\.\d+)/(\d+)' for e in ETAPAS), linha)
        if not m:
            raise ValueError('linha de rateio incompleta')
        q = int(m[1])
        if q in quadros:
            raise ValueError(f'quadro {q} repetido')
        valores = [float(m[2])] + [float(m[3 + i * 2]) for i in range(len(ETAPAS))]
        if abs(valores[0] - sum(valores[1:])) > .00001:
            raise ValueError(f'a soma não conserva o tempo no quadro {q}')
        quadros[q] = valores
    if not {6930, 6960} <= quadros.keys():
        raise ValueError('faltam os quadros alvo 6930 e 6960')
    return quadros

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log', type=pathlib.Path)
    a = ap.parse_args()
    try:
        quadros = confere(a.log.read_text(errors='replace'))
    except ValueError as e:
        raise SystemExit('FALHA: ' + str(e))
    print('| quadro | total | ' + ' | '.join(ETAPAS) + ' |')
    print('|' + '---|' * (len(ETAPAS) + 2))
    for q in sorted({6930, 6960, *sorted(quadros, key=lambda k: quadros[k][0], reverse=True)[:5]}):
        print('| ' + str(q) + ' | ' + ' | '.join(f'{v:.4f}' for v in quadros[q]) + ' |')
    medianas = [statistics.median(v[k] for v in quadros.values()) for k in range(len(ETAPAS) + 1)]
    print('| mediana | ' + ' | '.join(f'{v:.4f}' for v in medianas) + ' |')
    print(f'tudo certo: {len(quadros)} quadros, soma conservada; ms instrumentados, incluem o custo das marcas')

if __name__ == '__main__':
    main()
