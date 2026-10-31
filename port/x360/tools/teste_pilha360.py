#!/usr/bin/env python3
"""Confere a cadeia da pilha com memória sintética, incluindo elos inválidos e leitura recusada.
--mutante remove uma guarda numa cópia para provar que o teste a detecta.
"""
from pathlib import Path
import subprocess
import tempfile
import sys

raiz = Path(__file__).resolve().parents[3]
fonte = r'''
#include <stdio.h>
#include "pilha360.h"
static unsigned elo = 0x1100, lr = 0x82252114;
static unsigned falha;
static int ler(unsigned p, unsigned *v) {
    if (p == falha) return 0;
    if (p == 0x1000) { *v = elo; return 1; }
    if (p == elo - 8) { *v = lr; return 1; }
    return 0;
}
#define EXIGE(c) if (!(c)) { fprintf(stderr, "FALHA linha %d\n", __LINE__); return 1; }
int main(void) {
    unsigned a, r;
    EXIGE(Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r) && a == elo && r == lr);
    elo = 0x1000;
    EXIGE(!Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r));
    elo = 0x0f00;
    EXIGE(!Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r));
    elo = 0x1101;
    EXIGE(!Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r));
    elo = 0x1001010;
    EXIGE(!Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r));
    elo = 0x1100; falha = 0x1000;
    EXIGE(!Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r));
    falha = elo - 8;
    EXIGE(!Pilha360_Proximo(0x1000, 0x1000, ler, &a, &r));
    puts("pilha360: tudo certo");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='queda-pilha-') as tmp:
    d = Path(tmp)
    h = (raiz / 'port/x360/src/f2/pilha360.h').read_text()
    if '--mutante' in sys.argv:
        h = h.replace('*anterior <= atual || ', '')
    (d / 'pilha360.h').write_text(h)
    (d / 'teste.c').write_text(fonte)
    subprocess.run(['gcc', '-std=c89', '-Wall', '-Wextra', '-Werror', str(d / 'teste.c'), '-o', str(d / 'teste')], check=True)
    sys.exit(subprocess.run([str(d / 'teste')]).returncode)
