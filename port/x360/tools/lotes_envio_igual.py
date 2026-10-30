#!/usr/bin/env python3
"""Com a chave ausente, os três fontes do console conservam os tokens da base.
Não substitui a comparação da desmontagem do XDK. Inclui sabotagem obrigatória.
"""
from pathlib import Path
import argparse
import re
import subprocess

RAIZ = Path(__file__).resolve().parents[3]
FONTES = ('port/src/gs/gs_draw.c', 'port/src/gs/gs_draw.h', 'port/x360/src/gs/gs_d3d.cpp')


def normal(s):
    # Não temos XDK no WSL. As inclusões viram sentinelas, preservadas na comparação.
    s = re.sub(r'^\s*#include\s+([^\n]+)', lambda m: 'inclui(' + repr(m[1]) + ');', s, flags=re.M)
    r = subprocess.run(['gcc', '-E', '-P', '-x', 'c', '-DPORT_X360', '-DPORT_BIG_ENDIAN',
                        '-DBT3_GS_ESPERAS', '-'], input=s, text=True, capture_output=True, check=True)
    return re.findall(r'\w+|[^\w\s]', r.stdout)


def compara(a, b, f):
    assert normal(a) == normal(b), f + ': tokens diferentes na normal'


def confere(base):
    for f in FONTES:
        a = subprocess.check_output(['git', 'show', base + ':' + f], cwd=RAIZ, text=True)
        b = (RAIZ / f).read_text()
        compara(a, b, f)
        # Cada verificador é visto rejeitando uma entrada modificada de propósito.
        ruim = b + '\nint sabotagem_normal = 1;\n'
        try: compara(a, ruim, f)
        except AssertionError: pass
        else: raise AssertionError(f + ': sabotagem aceita')
        print(f + ': normal igual; sabotagem recusada')
    print('tudo certo nos tokens da build normal')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('base', nargs='?', default='979cc928')
    a = p.parse_args()
    confere(a.base)
