#!/usr/bin/env python3
"""F1: compara a memória do jogo de duas builds de ordem de bytes diferente num mesmo quadro.

    memdiff.py X86.dump PPC.dump [PROG_X86] [--max N]

Os .dump são os de port/src/gs/state.c (BT3_HASH=<arq> BT3_HASH_AT=<quadro> -> <arq>.dump): regiões do jogo
(heap, scratchpad...) cada uma depois de uma linha "nome endereço tamanho". O heap do jogo é determinístico
(o mesmo alocador nos dois), então o mesmo endereço guarda o mesmo dado se as duas builds fizeram o mesmo.

Cada palavra de 4 bytes alinhada conta como igual se for igual byte a byte, ou com os 4 bytes invertidos
(u32/float/ponteiro), ou com cada metade invertida (dois u16), ou se for metade de um u64 invertido (o par
de palavras trocado). O que sobra é divergência de verdade; sai agrupado em trechos, com a variável global
(nm do programa x86) quando o trecho cai numa.
"""
import bisect, struct, subprocess, sys


def ler(caminho):
    d = open(caminho, "rb").read()
    out, o = {}, 0
    while o < len(d):
        e = d.index(b"\n", o)
        nome, addr, size = d[o:e].split()
        addr, size = int(addr, 16), int(size, 16)
        out[nome.decode()] = (addr, d[e + 1:e + 1 + size])
        o = e + 1 + size
    return out


def ponteiro_do_port(a, b):
    """Endereço da memória do próprio processo (o port guarda alguns no jogo): diferente em cada build."""
    va, vb = struct.unpack("<I", a)[0], struct.unpack(">I", b)[0]
    return 0x20000000 <= va < 0x80000000 and 0x20000000 <= vb < 0x80000000


def iguais(a, b, a8=None, b8=None):
    if a == b or a == b[::-1] or ponteiro_do_port(a, b):
        return True
    if a[:2] == b[1::-1] and a[2:] == b[:1:-1]:
        return True
    return False


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    mx = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else 40
    faixa = [int(v, 16) for v in sys.argv[sys.argv.index("--faixa") + 1].split("-")] if "--faixa" in sys.argv else None
    x, p = ler(args[0]), ler(args[1])
    syms = []
    if len(args) > 2:
        for l in subprocess.run(["nm", "-n", args[2]], capture_output=True, text=True).stdout.splitlines():
            q = l.split()
            if len(q) == 3 and q[1] in "bBdDCsS":
                syms.append((int(q[0], 16), q[2]))
    sa = [s[0] for s in syms]
    total = 0
    for nome in x:
        if nome not in p:
            print(f"região {nome} só na x86")
            continue
        (ax, dx), (ap, dp) = x[nome], p[nome]
        if ax != ap or len(dx) != len(dp):
            print(f"região {nome}: x86 {ax:08x}+{len(dx):x}, ppc {ap:08x}+{len(dp):x} (comparando o começo)")
        n = min(len(dx), len(dp)) // 4 * 4
        trechos, ini, fim = [], None, None
        for o in range(0, n, 4):
            a, b = dx[o:o + 4], dp[o:o + 4]
            if iguais(a, b):
                continue
            # metade de um u64 invertido: a palavra par/ímpar trocada
            par = o ^ 4
            if par + 4 <= n and dx[o:o + 4] == dp[par:par + 4][::-1]:
                continue
            if ini is not None and o - fim <= 16:
                fim = o + 4
            else:
                if ini is not None:
                    trechos.append((ini, fim))
                ini, fim = o, o + 4
        if ini is not None:
            trechos.append((ini, fim))
        total += len(trechos)
        print(f"região {nome} em {ax:08x}: {len(trechos)} trechos diferentes")
        if faixa:
            trechos = [t for t in trechos if faixa[0] <= ax + t[0] < faixa[1]]
        for ini, fim in trechos[:mx]:
            addr = ax + ini
            i = bisect.bisect_right(sa, addr) - 1
            onde = f" ({syms[i][1]}+0x{addr - syms[i][0]:x})" if i >= 0 and addr - syms[i][0] < 0x10000 else ""
            print(f"  {addr:08x} +{fim - ini:<5x}{onde}  x86 {dx[ini:min(fim, ini + 16)].hex(' ', 4)}  ppc {dp[ini:min(fim, ini + 16)].hex(' ', 4)}")


main()
