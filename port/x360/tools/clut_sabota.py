#!/usr/bin/env python3
"""(temporário, só no WSL) As sabotagens da correção, sobre o gs_draw.c do f5-clut com o diag_clut.patch aplicado.
Uso: clut_sabota.py gs_draw.c S1|S2|S3|S4
  S1 a busca só pelo hash (o hash novo); S2 só pelo hash e o hash antigo (o FNV sem o deslocamento); S3 o hash antigo com a
  conferência (a correção sem a melhora do hash); S4 um hash de 4 bits com a conferência (16 valores: colisões o tempo todo)"""
import sys
p, k = sys.argv[1], sys.argv[2]; s = open(p, encoding='utf-8').read()
def rep(a, b):
    global s
    assert s.count(a) == 1, a[:60]
    s = s.replace(a, b)
if k in ('S1', 'S2'):
    rep(' && diag_cmp(sCluts[k].cor, px) == 0) {', ') {')
if k in ('S2', 'S3'):
    rep('        COR_MISTURA(hash, px[i]);\n', '        hash = (hash ^ px[i]) * 16777619u;\n')
if k == 'S4':
    rep('        COR_MISTURA(hash, px[i]);\n    }\n', '        COR_MISTURA(hash, px[i]);\n    }\n    hash &= 15;\n')
open(p, 'w', encoding='utf-8').write(s)
