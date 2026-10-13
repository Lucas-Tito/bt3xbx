#!/usr/bin/env python3
"""F1: compara a luta de demonstração de duas builds pelo hash.txt de port/src/gs/state.c (BT3_HASH).

    compara.py REF.txt OUTRA.txt [--big] [--ref-rastreio R.txt]

Por quadro de luta: vida, posições, relógio e o gerador aleatório (gRandState). O gerador é impresso como u64
montado de duas palavras (w[11], w[10]): numa build big-endian (--big) as metades saem trocadas e são
destrocadas antes de comparar. Sai com 1 se algum quadro diferir ou se o número de quadros não bater.
"""
import sys


def ler(caminho, big=False):
    out = {}
    for l in open(caminho):
        p = l.split()
        if " hp " not in l or not l.endswith("\n"):
            continue  # (sem o fim de linha: a última linha de uma rodada cortada pelo timeout, que o stdio gravou pela metade)
        if big and "rnd" in p:
            r = p[p.index("rnd") + 1]
            p[p.index("rnd") + 1] = r[8:] + r[:8]
        out[int(p[0])] = " ".join(p[3:])
    return out


def comparar(ref, outra, rotulo):
    comuns = sorted(set(ref) & set(outra))
    dif = [q for q in comuns if ref[q] != outra[q]]
    print(f"{rotulo}: quadros de luta {len(ref)} e {len(outra)}, comparados {len(comuns)}, diferentes {len(dif)}")
    if dif:
        q = dif[0]
        print(f"  primeiro diferente: quadro {q}\n    ref   {ref[q]}\n    outra {outra[q]}")
    return not dif and len(ref) == len(outra) and len(ref) > 0


def main():
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    if len(a) < 2:
        sys.exit(__doc__)
    ok = comparar(ler(a[0]), ler(a[1], "--big" in sys.argv), "ref x outra")
    if "--ref-rastreio" in sys.argv:
        r = sys.argv[sys.argv.index("--ref-rastreio") + 1]
        ok &= comparar(ler(a[0]), ler(r), "ref x rastreio")
    sys.exit(0 if ok else 1)


main()
