#!/usr/bin/env python3
"""F1: compara a sequência de funções do jogo de duas builds (port/x360/headless/chamadas.c).

    chamadas.py X86.bin PROG_X86 PPC.bin PROG_PPC [contexto]

Traduz os endereços em nomes (nm de cada programa) e mostra a primeira chamada diferente, com as
`contexto` (padrão 25) anteriores, que dizem por onde o jogo passou até se separar. Uma chamada a mais
de um lado só (até 3 seguidas) é tolerada e listada: os compiladores apagam chamadas diferentes (ex.:
função `const` sem uso do resultado no GCC)."""
import struct, subprocess, sys
from collections import Counter


def nomes(prog):
    out = subprocess.run(["nm", prog], capture_output=True, text=True).stdout
    return {int(l.split()[0], 16): l.split()[2] for l in out.splitlines() if len(l.split()) == 3}


# Funções declaradas __attribute__((const/pure)) no jogo: cada compilador apaga ou junta chamadas a elas do
# seu jeito, então não contam na comparação.
IGNORAR = {"EftTexSet_CheckCount", "BObjChainA_WrapAngle", "Stg_WrapRange", "StgNode_IsRigid"}


def seq(arq, prog, fmt):
    tab = nomes(prog)
    d = open(arq, "rb").read()
    out = (tab.get(a, f"{a:08x}") for (a,) in struct.iter_unpack(fmt, d[: len(d) // 4 * 4]))
    return [f for f in out if f not in IGNORAR]


def main():
    x, px, p, pp = sys.argv[1:5]
    ctx = int(sys.argv[5]) if len(sys.argv) > 5 else 25
    a, b = seq(x, px, "<I"), seq(p, pp, ">I")
    i = j = 0
    pulos = Counter()
    while i < len(a) and j < len(b):
        if a[i] == b[j]:
            i += 1
            j += 1
            continue
        for k in range(1, 4):  # chamada(s) a mais de um lado só
            if a[i + k:i + k + 3] == b[j:j + 3]:
                pulos["x86: " + " ".join(a[i:i + k])] += 1
                i += k
                break
            if b[j + k:j + k + 3] == a[i:i + 3]:
                pulos["ppc: " + " ".join(b[j:j + k])] += 1
                j += k
                break
        else:
            print(f"primeira diferença na chamada {i} (x86) / {j} (ppc):")
            for h in a[max(0, i - ctx):i]:
                print("   ", h)
            print("  x86 depois:", " ".join(a[i:i + 10]))
            print("  ppc depois:", " ".join(b[j:j + 10]))
            break
    else:
        print(f"iguais até o fim de uma delas: x86 {i}/{len(a)}, ppc {j}/{len(b)} chamadas")
    for k, n in pulos.most_common(10):
        print(f"  tolerado {n}x: {k}")


main()
