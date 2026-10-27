#!/usr/bin/env python3
"""F5: exige diagnóstico completo, sem colisões nem saturação, e roteiro terminado.
Uso: clut16_confere.py LOG [--colisoes N] [--sintetico]
"""
import argparse
import pathlib
import re
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('log')
p.add_argument('--colisoes', type=int, default=0)
p.add_argument('--sintetico', action='store_true')
a = p.parse_args()
s = pathlib.Path(a.log).read_text(errors='replace')
r = re.findall(r'^clut16: leituras (\d+) colisoes (\d+) altas (\d+) cheia (\d+) csm2 (\d+)$', s, re.M)
linhas = re.findall(r'^clut16: colisao quadro \d+ tex0 [0-9a-f]+ texa [0-9a-f]+ gen [0-9a-f]+ alta ([01])$', s, re.M)
ok = len(r) == 1
if ok:
    leituras, colisoes, altas, cheia, csm2 = map(int, r[0])
    ok = leituras > 0 and colisoes == a.colisoes and cheia == 0 and csm2 == 0
    ok &= len(linhas) == colisoes and linhas.count('1') == altas
    print(f'leituras {leituras}, colisões {colisoes}, só nas cores 8 a 15 {altas}, saturação {cheia}, CSM2 {csm2}')
if not a.sintetico:
    ok &= bool(re.search(r'^gpu_falso: \d+ quadros, hash do jogo [0-9a-f]+(?:,.*)?$', s, re.M))
    ok &= bool(re.search(r'^roteiro: .* fim ', s, re.M))
    ok &= not bool(re.search(r'FALHA|CRASH|race:', s))
print('tudo certo' if ok else 'FALHA: diagnóstico incompleto ou resultado inesperado')
sys.exit(0 if ok else 1)
