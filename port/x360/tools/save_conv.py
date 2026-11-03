#!/usr/bin/env python3
"""Fino sobre port/x360/tools/save_conv_tool.c (a lógica fica só em C): compila com gcc -std=c89 e chama.

    save_conv.py psu-para-360 ARQ.psu PASTA
    save_conv.py 360-para-psu PASTA ARQ.psu [--data AAAA-MM-DDTHH:MM:SS]
    save_conv.py confere ARQ.psu|PASTA

Em código: compila(destino, conv=CONV) devolve o executável; roda(exe, *args) devolve o CompletedProcess.
Sai com o código da ferramenta (1 em erro, com o texto em inglês na saída de erro).
"""
import pathlib, subprocess, sys, tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]
FERRAMENTA = RAIZ / "port/x360/tools/save_conv_tool.c"
CONV = RAIZ / "port/x360/conv"


def compila(destino, conv=CONV):
    exe = pathlib.Path(destino) / "save_conv_tool"
    fontes = [str(pathlib.Path(conv) / n) for n in ("psu.c", "save_larguras.c", "save_conv.c")]
    r = subprocess.run(["gcc", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", f"-I{conv}", "-o", str(exe),
                        str(FERRAMENTA)] + fontes, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("compilação falhou:\n" + r.stderr)
    return exe


def roda(exe, *args):
    return subprocess.run([str(exe)] + [str(a) for a in args], capture_output=True, text=True)


def main(a):
    if not a:
        print(__doc__)
        return 2
    with tempfile.TemporaryDirectory() as d:
        r = roda(compila(d), *a)
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    return r.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
