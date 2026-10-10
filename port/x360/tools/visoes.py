#!/usr/bin/env python3
"""F1: resume o detector de visões (port/x360/headless/rastreio.c, BT3_VISOES=<arquivo>).

    visoes.py VISOES.tsv PROGRAMA_X86R [--tudo] [--linhas]   (--linhas: arquivo:linha das duas pontas)

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


def linhas_de(prog, pcs):
    """pc -> 'arquivo:linha' (addr2line; o pc é o retorno da chamada de instrumentação, logo depois do acesso)."""
    pcs = sorted(pc for pc in pcs if pc)
    out = subprocess.run(["addr2line", "-e", prog] + [f"0x{pc - 1:x}" for pc in pcs], capture_output=True, text=True).stdout.split("\n")
    return {pc: out[i].split("/bt3xbx/")[-1].split(" ")[0] for i, pc in enumerate(pcs) if i < len(out)}


def main():
    if "--linhas" in sys.argv:
        prog = sys.argv[2]
        bruto = [l.split() for l in open(sys.argv[1])]
        bruto = [b for b in bruto if int(b[3]) != 16 or "--tudo" in sys.argv]
        lin = linhas_de(prog, {int(b[0], 16) for b in bruto} | {int(b[1], 16) for b in bruto})
        nome = simbolos(prog)
        por = {}
        for g, r, gw, lw, pos, n in bruto:
            g, r = int(g, 16), int(r, 16)
            k = (lin.get(r, "?"), lin.get(g, "?"), gw, lw)
            por[k] = por.get(k, 0) + int(n)
        for (lr, lg, gw, lw), n in sorted(por.items()):
            print(f"leu {lw} em {lr:42s} <- gravou {gw} em {lg:42s} ({n}x)")
        return
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
