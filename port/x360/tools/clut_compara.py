#!/usr/bin/env python3
"""(F5, 09.md, a correção da colisão das tabelas) Os quadros em que o hash normal difere entre duas rodadas, contra os quadros
com colisão do diagnóstico. Uso: clut_compara.py MAIN CLUT [DIAG_MAIN] [--mascara]
  --mascara: compara também as linhas normal_semtabela (clut_mascara.py): têm de ser iguais em todo quadro."""
import re, sys
a = [x for x in sys.argv[1:] if not x.startswith('--')]
masc = '--mascara' in sys.argv
def le(p, tag):
    d = {}
    r = re.compile(r'^gpu_falso: %s quadro (\d+) (\w+)$' % tag)
    for l in open(p, errors='replace'):
        m = r.match(l)
        if m:
            d[int(m.group(1))] = m.group(2)
    return d
def dif(x, y):
    c = sorted(set(x) & set(y))
    return c, [q for q in c if x[q] != y[q]], len(set(x) ^ set(y))
def faixa(l):
    return '%d (primeiro %s, último %s)' % (len(l), l[0] if l else '-', l[-1] if l else '-')
x, y = le(a[0], 'normal'), le(a[1], 'normal')
c, d, so = dif(x, y)
print('normal: %d quadros comuns, %d só de um lado, diferentes %s' % (len(c), so, faixa(d)))
if masc:
    xm, ym = le(a[0], 'normal_semtabela'), le(a[1], 'normal_semtabela')
    cm, dm, som = dif(xm, ym)
    print('sem a textura das tabelas: %d quadros comuns, %d só de um lado, diferentes %s' % (len(cm), som, faixa(dm)))
if len(a) > 2:
    col, n = set(), 0
    for l in open(a[2], errors='replace'):
        m = re.match(r'^tabdiag colisao tabela quadro (\d+) ', l)
        if m:
            col.add(int(m.group(1))); n += 1
    col = sorted(col)
    print('colisões no diagnóstico da main: %d passadas em %s' % (n, faixa(col)))
    print('quadros diferentes == quadros com colisão: %s; só diferentes %d, só com colisão %d' %
          ('sim' if set(d) == set(col) else 'NAO', len(set(d) - set(col)), len(set(col) - set(d))))
