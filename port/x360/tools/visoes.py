#!/usr/bin/env python3
"""F1: resume o detector de visões (port/x360/headless/rastreio.c, BT3_VISOES=<arquivo>).

    visoes.py VISOES.tsv PROGRAMA_X86R [--tudo]

Sem --tudo, ficam de fora as leituras de 16 bytes de palavras gravadas uma a uma (cópia de vetor: não lê número).

Cada linha: função que gravou, função que leu, largura gravada -> largura lida, posição do byte lido dentro da
gravação, quantas vezes. Num host big-endian cada caso lê outro valor: é a classe 3 da decisão 7 (uma struct
vista com campos de larguras diferentes, union, número montado byte a byte...). Ordenado por função que leu."""
import bisect, signal, subprocess, sys

signal.signal(signal.SIGPIPE, signal.SIG_DFL)


def simbolos(programa):
    out = subprocess.run(["nm", "-n", programa], capture_output=True, text=True).stdout
    tab = [(int(p[0], 16), p[2]) for p in (l.split() for l in out.splitlines()) if len(p) == 3 and p[1] in "tT"]
    ad = [a for a, _ in tab]

    def nome(pc):
        i = bisect.bisect_right(ad, pc) - 1
        return tab[i][1] if i >= 0 and pc else f"{pc:08x}"
    return nome


def main():
    nome = simbolos(sys.argv[2])
    linhas = []
    for l in open(sys.argv[1]):
        g, r, gw, lw, pos, n = l.split()
        linhas.append((nome(int(r, 16)), nome(int(g, 16)), int(gw), int(lw), int(pos), int(n)))
    agrup = {}
    for leu, gravou, gw, lw, pos, n in linhas:
        if lw == 16 and "--tudo" not in sys.argv:
            continue
        k = (leu, gravou, gw, lw)
        agrup[k] = agrup.get(k, 0) + n
    print(f"{len(agrup)} combinações (leu <- gravou)")
    for (leu, gravou, gw, lw), n in sorted(agrup.items()):
        print(f"{leu:40s} <- {gravou:40s} gravou {gw} leu {lw}  ({n}x)")


main()
