#!/usr/bin/env python3
"""Censo da lista do backend falso. Cobertura geométrica, não contadores do Xenos.

BT3_SCALE=1; a escala 1,33 é a fração 4/3. Soma fragmentos antes de alfa/DATE/Z,
com centros de pixel e bordas semiabertas. Não interpreta texturas, Z nem os shaders.
O intervalo de escritas usa como mínimo só desenhos sem testes que possam recusá-las.
Saída e entrada com dados do jogo devem ficar fora do git.
"""
import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path


def le(p):
    linhas = [json.loads(x) for x in p.read_text().splitlines()]
    h, ds = linhas[0], linhas[1:]
    if h['desenhos'] != len(ds) or [d['n'] for d in ds] != list(range(len(ds))):
        raise ValueError(f'{p}: lista incompleta ou fora de ordem')
    if not h.get('alvos'):
        raise ValueError(f'{p}: alvos ausentes')
    if h.get('escala') != 1:
        raise ValueError(f'{p}: exige BT3_SCALE=1')
    for d in ds:
        if not 0 <= d['alvo'] < len(h['alvos']):
            raise ValueError(f'{p}: alvo inválido')
        if not d['vu'] and not d['native']:
            if d['count'] != len(d['xy']) or (d['pipe'] >> 13 & 3) != 0 or d['count'] % 3:
                raise ValueError(f'{p}: posições incompletas ou topologia não suportada')
            if any(not math.isfinite(v) for xy in d['xy'] for v in xy):
                raise ValueError(f'{p}: posição não finita')
    return h, ds


def shader(d, principal):
    if d['modo'][0] == 1 and d['misc'][3] and d['alvo'] == principal:
        return 'afiado'
    if not d['misc'][1] and not d['misc'][3] and not d['modo'][3] and d['modo'][0] != 2:
        return 'rapido textura' if d['modo'][0] else 'rapido sem'
    return ('gs sem', 'gs VRAM', 'gs alvo', 'gs pacote')[d['modo'][0]]


def faixas(d, num, den, largura=640, altura=448):
    """Intervalos de pixels por triângulo. Rect de vértices não substitui área pintada."""
    esc = lambda x: (int(x) * num + den - 1) // den
    w, h = esc(largura), esc(altura)
    sx, sy, sw, sh = d['tesoura']
    x0, y0, x1, y1 = max(0, esc(sx)), max(0, esc(sy)), min(w, esc(sx + sw)), min(h, esc(sy + sh))
    for k in range(0, len(d['xy']), 3):
        vs = [(x * num / den, y * num / den) for x, y in d['xy'][k:k+3]]
        for y in range(max(y0, math.ceil(min(v[1] for v in vs) - .5)), min(y1, math.ceil(max(v[1] for v in vs) - .5))):
            cy = y + .5
            xs = []
            for a, b in zip(vs, vs[1:] + vs[:1]):
                if min(a[1], b[1]) <= cy < max(a[1], b[1]):
                    xs.append(a[0] + (cy - a[1]) * (b[0] - a[0]) / (b[1] - a[1]))
            if len(xs) == 2:
                a, b = max(x0, math.ceil(min(xs) - .5)), min(x1, math.ceil(max(xs) - .5))
                if b > a:
                    yield y, a, b


def censo(h, ds, num, den):
    # O primeiro desenho é a limpeza do buffer que será mostrado; alterna 0/2.
    principal = h['alvos'].index(h['fbp'])
    grupos, uniao = {}, defaultdict(int)
    trans = Counter()
    anterior = None
    por_desenho = []
    for d in ds:
        estado = (d['vu'], d['native'], d['alvo'], d['pipe'], d['tex'], d['sampler'], tuple(d['modo']), tuple(d['misc']), tuple(d['tesoura']), d['blendc'], tuple(d['rect']))
        if anterior:
            for i, nome in enumerate(('vu', 'nativa', 'alvo', 'pipeline', 'textura', 'sampler', 'modo', 'misc', 'tesoura', 'blendc', 'rect')):
                trans[nome] += estado[i] != anterior[i]
        anterior = estado
        if d['vu'] or d['native']:
            continue
        nome = f"{shader(d, principal)} / alvo {d['alvo']} / pipe {d['pipe']:x}"
        g = grupos.setdefault(nome, Counter())
        g['desenhos'] += 1
        g['vertices'] += d['count']
        g['mistura'] += bool(d['pipe'] & 1)
        # ZTST=ALWAYS, nenhum clip de alfa nem DATE, máscara não vazia.
        certo = not d['modo'][3] and not d['misc'][1] and (d['pipe'] >> 10 & 3) == 1
        mascara = d['pipe'] >> 16 & 15
        k0, k1 = d['pipe'] >> 1 & 7, d['pipe'] >> 4 & 7
        fba_duas = d['misc'][2] != 0 and d['pipe'] & 1 and (d['pipe'] >> 7 & 3) == 0 and (
            2 <= k0 <= 4 or 2 <= k1 <= 4)
        area = 0
        for y, a, b in faixas(d, num, den):
            area += b-a
            chave = (d['alvo'], y)
            bits = ((1 << (b-a)) - 1) << a
            uniao[chave] |= bits
        g['fragmentos'] += area
        if mascara:
            g['escritas_max'] += area
            if certo:
                # Na primeira passada FBA só RGB escreve; só alfa fica para T_FBA.
                # O máximo continua conservador, admitindo todos os fragmentos.
                g['escritas_min'] += area if mascara & 7 or not fba_duas else 0
        # A passada extra do FBA é separada da comum no perfil, não somada aqui.
        area_alvo = sum(b-a for _, a, b in faixas(d, num, den, 648, 456))
        por_desenho.append(dict(n=d['n'], shader=shader(d, principal), alvo=d['alvo'], fonte=d['modo'][0],
                                pixels=area, pixels_alvo=area_alvo))
    soma = sum(x['fragmentos'] for x in grupos.values())
    unica = sum(x.bit_count() for x in uniao.values())
    return dict(escala=f'{num}/{den}', grupos=grupos, fragmentos=soma, uniao=unica,
                sobreposicao=soma-unica, transicoes=trans, desenhos=por_desenho)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('pasta', type=Path)
    p.add_argument('--quadros', default='6900,6929,6930,6931,6959,6960,6961')
    p.add_argument('--saida', required=True, type=Path)
    a = p.parse_args()
    resultado = {}
    for n in map(int, a.quadros.split(',')):
        h, ds = le(a.pasta / f'{n}.jsonl')
        if h['quadro'] != n:
            raise ValueError('quadro errado')
        comum = [d for d in ds if not d['vu'] and not d['native']]
        resultado[n] = dict(cabecalho=h, comum=len(comum), nativas=Counter(d['native'] for d in ds if d['native']),
                            escalas=[censo(h, ds, 1, 1), censo(h, ds, 4, 3)])
        print(n, 'desenhos', len(ds), 'comuns', len(comum), 'fragmentos', *(x['fragmentos'] for x in resultado[n]['escalas']))
    a.saida.write_text(json.dumps(resultado, ensure_ascii=False, indent=2)+'\n')
    print('tudo certo no censo geométrico; escritas com testes continuam limitadas por intervalo')

if __name__ == '__main__':
    main()
