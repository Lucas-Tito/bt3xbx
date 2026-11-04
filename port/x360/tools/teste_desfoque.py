#!/usr/bin/env python3
"""Confere o isolamento dos sete cortes; aceita uma cópia mutante do núcleo."""
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


if __name__ == "__main__":
    conferir(pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else RAIZ / "port/x360/headless/desfoque.c")
