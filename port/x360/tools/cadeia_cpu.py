#!/usr/bin/env python3
"""Confere o rateio exclusivo BT3_CADEIA_CPU, sem certificar apresentação."""
import argparse
from pathlib import Path
import re

GRUPOS = ('quadro', 'comum', 'vu0', 'vu4', 'vu6', 'nativa')
ETAPAS = ('resto', 'alfa', 'buffers', 'plano', 'edram', 'prepara', 'pipe', 'sampler',
          'estado', 'textura', 'uniforme', 'indices', 'UP', 'Present', 'limpeza', 'FXAA', 'tela')
NUM = r'([0-9]+\.[0-9]{6})'
TOTAL = re.compile(r'cadeia: cpu quadro (\d+) total ' + NUM + r' ms')
PARTE = re.compile(r'cadeia: cpu quadro (\d+) grupo (\S+) etapa (\S+) ms ' + NUM + r' chamadas (\d+)')
SOMA = re.compile(r'cadeia: cpu quadro (\d+) soma ' + NUM + r' ms')


def confere(texto, alvos):
    fim = re.search(r'^roteiro: f5_duelo.txt:99 fim .*$', texto, re.M)
    if not fim:
        raise ValueError('sem fim do roteiro')
    dados = {}
    for linha in texto[:fim.end()].splitlines():
        if not linha.startswith('cadeia: cpu quadro '):
            continue
        m = TOTAL.fullmatch(linha)
        if m:
            q = int(m[1])
            if q in dados:
                raise ValueError('quadro repetido')
            dados[q] = {'total': float(m[2]), 'partes': {}, 'soma': None}
            continue
        m = PARTE.fullmatch(linha)
        if m:
            q, g, e, ms, n = m.groups()
            d = dados.get(int(q))
            if d is None or d['soma'] is not None or g not in GRUPOS or e not in ETAPAS or (g, e) in d['partes']:
                raise ValueError('parcela fora de ordem, repetida ou desconhecida')
            d['partes'][g, e] = (float(ms), int(n))
            continue
        m = SOMA.fullmatch(linha)
        if m and int(m[1]) in dados and dados[int(m[1])]['soma'] is None:
            dados[int(m[1])]['soma'] = float(m[2])
            continue
        raise ValueError('linha incompleta ou amostra indisponível: ' + linha)
    if set(dados) != set(alvos):
        raise ValueError('quadros ausentes ou excedentes')
    for q, d in dados.items():
        # Até 102 parcelas impressas com seis casas, mais total/soma.
        if not d['partes'] or d['soma'] is None or abs(d['soma'] - d['total']) > 0.000002:
            raise ValueError(f'total/soma incompleto no quadro {q}')
        if abs(sum(v[0] for v in d['partes'].values()) - d['total']) > 0.000053:
            raise ValueError(f'parcelas não conservam quadro {q}')
    return dados


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    p.add_argument('--quadros', required=True, help='quadros separados por vírgula')
    a = p.parse_args()
    try:
        d = confere(a.log.read_text(), [int(q) for q in a.quadros.split(',')])
    except (ValueError, OSError) as e:
        p.exit(1, f'FALHA: {e}\n')
    print(f'tudo certo no rateio de {len(d)} quadros; apresentação não certificada')


if __name__ == '__main__':
    main()
