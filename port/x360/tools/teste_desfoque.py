#!/usr/bin/env python3
"""Confere os cortes e a força da distância; aceita cópia mutante do núcleo."""
import os
import pathlib
import subprocess
import sys
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]
NOMES = ["DISTANCIA", "PANORAMICO", "RADIAL", "BRUMA", "NEVOA", "BRILHO", "CLIMA"]


def conferir(fonte):
    with tempfile.TemporaryDirectory() as pasta:
        pasta = pathlib.Path(pasta)
        (pasta / "teste.c").write_text('''#include <stdio.h>
int Port_DesfoqueOff(int);
int main(void) {
    int i, mask = 0;
    for (i = 0; i < 7; i++) if (Port_DesfoqueOff(i)) mask |= 1 << i;
    printf("mascara=%d\\n", mask);
    return 0;
}
''')
        subprocess.run(["gcc", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                        str(fonte), str(pasta / "teste.c"), "-o", str(pasta / "teste")], check=True)
        for mask in range(128):
            ambiente = {k: v for k, v in os.environ.items() if not k.startswith("BT3_SEM_")}
            ambiente.update({"BT3_SEM_" + n: str((mask >> i) & 1) for i, n in enumerate(NOMES)})
            r = subprocess.run([str(pasta / "teste")], env=ambiente, capture_output=True, text=True, check=True)
            if r.stdout.splitlines()[-1] != "mascara=" + str(mask):
                raise AssertionError("cortes não isolados: %d: %s" % (mask, r.stdout))
    print("teste_desfoque: 128 combinações, tudo certo")


def conferir_forca(fonte):
    with tempfile.TemporaryDirectory() as pasta:
        pasta = pathlib.Path(pasta)
        (pasta / 'forca.c').write_text('''#include <stdio.h>
int Port_DistanciaForca(void);
int main(void) {
    int forca = Port_DistanciaForca();
    printf("forca=%d\\n", forca);
    return 0;
}
''')
        for console in (False, True):
            subprocess.run(['gcc', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                            *(['-DPORT_X360'] if console else []), str(fonte), str(pasta / 'forca.c'),
                            '-o', str(pasta / 'forca')], check=True)
            padrao = 25 if console else 100
            for valor, esperado in ((None, padrao), ('', padrao), ('-1', 0), ('0', 0),
                                    ('25', 25), ('55', 55), ('100', 100), ('101', 100)):
                env = {k: v for k, v in os.environ.items() if k != 'BT3_DISTANCIA_FORCA'}
                if valor is not None:
                    env['BT3_DISTANCIA_FORCA'] = valor
                saida = subprocess.check_output([str(pasta / 'forca')], env=env, text=True).splitlines()[-1]
                if saida != 'forca=' + str(esperado):
                    raise AssertionError('força incorreta: console=%s valor=%r saída=%s' % (console, valor, saida))
    print('teste_desfoque: força, 16 casos incluindo padrões e limites, tudo certo')


if __name__ == "__main__":
    fonte = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else RAIZ / "port/x360/headless/desfoque.c"
    conferir(fonte)
    conferir_forca(fonte)
