#!/usr/bin/env python3
"""Exercita fronteira e publicação de produção, sem jogo, e exige sabotagens recusadas."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]


def roda(ppc=False):
    with tempfile.TemporaryDirectory() as t:
        p = Path(t)
        arquivos = ('gs_lotes.h', 'gs_lotes_grav.h')
        for f in arquivos:
            shutil.copyfile(RAIZ / 'port/x360/include' / f, p / f)
        fonte = RAIZ / 'port/x360/headless/teste_lotes_envio.c'
        cc = ['powerpc-linux-gnu-gcc', '-static'] if ppc else ['gcc']
        exe = ['qemu-ppc'] if ppc else []
        def compila():
            subprocess.run(cc + [ '-std=gnu99', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread', '-I' + str(p),
                            str(fonte), '-o', str(p / 'teste')], check=True)
        compila()
        for atraso in (1, 5, 20):
            subprocess.run(exe + [str(p / 'teste'), str(atraso)], check=True, timeout=15)
        for f, a, b in (
            ('gs_lotes.h', 'if (j == q->drawCount) break;', 'if (0) break;'),
            ('gs_lotes.h', 'if (i >= grupo) seguro = i;', 'seguro = i;'),
            ('gs_lotes_grav.h', 'sLote.drawCount = gsDrawCount - 1;', 'sLote.drawCount = gsDrawCount;'),
            ('gs_lotes_grav.h', 'sLoteAte = ate;', 'sLoteAte = ate + 1;'),
            ('gs_lotes_grav.h', 'while (sDesTem) {', 'while (0) {'),
        ):
            original = (p / f).read_text()
            assert a in original, (f, a)
            (p / f).write_text(original.replace(a, b))
            # grupo pode ficar apenas escrito no mutante que tira sua leitura.
            comp = subprocess.run(cc + ['-std=gnu99', '-O2', '-pthread', '-I' + str(p), str(fonte),
                                   '-o', str(p / 'teste')], capture_output=True)
            assert comp.returncode == 0, comp.stderr.decode()
            ruim = subprocess.run(exe + [str(p / 'teste'), '20'], capture_output=True, timeout=15)
            assert ruim.returncode != 0, ('mutante aceito', f, a)
            assert b'Assertion' in ruim.stderr, ruim.stderr
            print('mutante recusado:', f, a)
            (p / f).write_text(original)
    print('tudo certo: testes e cinco sabotagens do envio')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ppc", action="store_true", help="usar somente sob flock ~/wsl.lock")
    roda(p.parse_args().ppc)
