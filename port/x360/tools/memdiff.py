#!/usr/bin/env python3
"""F1: compara a memória do jogo de duas builds de ordem de bytes diferente num mesmo quadro.

    memdiff.py X86.dump PPC.dump PROG_X86 PROG_PPC [--max N] [--faixa INI-FIM] [--estrito]

Os .dump são os de port/src/gs/state.c (BT3_HASH=<arq> BT3_HASH_AT=<quadro> -> <arq>.dump): heap,
scratchpad e as variáveis globais do jogo (<programa>.mem, escrito por tools/mem.py). O heap do jogo é
determinístico (o mesmo alocador nos dois); as globais ficam em endereços diferentes em cada programa e são
pareadas pela ordem das regiões.

Cada palavra de 4 bytes alinhada cai numa classe:
  igual           igual byte a byte ou com os 4 bytes invertidos (u32/float/ponteiro do jogo)
  ponteiro-port   endereço dentro da imagem ou do heap do próprio programa em cada lado (as faixas vêm do nm)
  2xu16           cada metade invertida: certo se forem dois u16, ERRADO se for um u32
  u64             o par de palavras trocado: certo se for u64/double, ERRADO se forem dois u32
  diferente       o resto
Sem largura gravada não dá para separar 2xu16 e u64 de erro; elas são contadas à parte e, com --estrito,
listadas como diferenças.
"""
import bisect, struct, subprocess, sys
from collections import Counter


def ler(caminho):
    d = open(caminho, "rb").read()
    out, o = [], 0
    while o < len(d):
        e = d.index(b"\n", o)
        nome, addr, size = d[o:e].split()
        addr, size = int(addr, 16), int(size, 16)
        out.append((nome.decode(), addr, d[e + 1:e + 1 + size]))
        o = e + 1 + size
    return out


def faixa_do_programa(prog, nm="nm"):
    """Do menor ao maior símbolo do programa, mais 128 MB de heap do C depois dele."""
    ad = [int(l.split()[0], 16) for l in subprocess.run([nm, prog], capture_output=True, text=True).stdout.splitlines()
          if len(l.split()) == 3 and l.split()[1] in "tTdDbBrR"]
    return min(ad), max(ad) + 0x08000000


def simbolos(prog, nm="nm"):
    syms = []
    for l in subprocess.run([nm, "-n", prog], capture_output=True, text=True).stdout.splitlines():
        q = l.split()
        if len(q) == 3 and q[1] in "bBdDCsS":
            syms.append((int(q[0], 16), q[2]))
    return syms


def main():
    args, opc, it = [], {}, iter(sys.argv[1:])
    for a in it:
        if a in ("--max", "--faixa"):
            opc[a] = next(it)
        elif a.startswith("--"):
            opc[a] = True
        else:
            args.append(a)
    mx = int(opc.get("--max", 40))
    faixa = [int(v, 16) for v in opc["--faixa"].split("-")] if "--faixa" in opc else None
    estrito = "--estrito" in opc
    x, p = ler(args[0]), ler(args[1])
    fx, fp = faixa_do_programa(args[2]), faixa_do_programa(args[3], "powerpc-linux-gnu-nm")
    syms = simbolos(args[2])
    sa = [s[0] for s in syms]
    if len(x) != len(p):
        print(f"AVISO: {len(x)} regiões na x86 e {len(p)} na ppc; comparando pela ordem")
    tot = Counter()
    for (nx, ax, dx), (np_, ap, dp) in zip(x, p):
        n = min(len(dx), len(dp)) // 4 * 4
        classes = Counter()
        trechos, ini, fim = [], None, None
        for o in range(0, n, 4):
            a, b = dx[o:o + 4], dp[o:o + 4]
            va, vb = struct.unpack("<I", a)[0], struct.unpack(">I", b)[0]
            if a == b or a == b[::-1]:
                c = "igual"
            elif fx[0] <= va < fx[1] and fp[0] <= vb < fp[1]:
                c = "ponteiro-port"
            elif a[:2] == b[1::-1] and a[2:] == b[:1:-1]:
                c = "2xu16"
            elif o ^ 4 < n and dx[o:o + 4] == dp[(o ^ 4):(o ^ 4) + 4][::-1]:
                c = "u64"
            else:
                c = "diferente"
            classes[c] += 1
            if c == "diferente" or (estrito and c in ("2xu16", "u64")):
                if ini is not None and o - fim <= 16:
                    fim = o + 4
                else:
                    if ini is not None:
                        trechos.append((ini, fim))
                    ini, fim = o, o + 4
        if ini is not None:
            trechos.append((ini, fim))
        tot.update(classes)
        print(f"região {nx} x86 {ax:08x} ppc {ap:08x} (+{n:x}): " + ", ".join(f"{k} {v}" for k, v in classes.most_common()) +
              f"; {len(trechos)} trechos a olhar")
        if faixa:
            trechos = [t for t in trechos if faixa[0] <= ax + t[0] < faixa[1]]
        for ini, fim in trechos[:mx]:
            addr = ax + ini
            i = bisect.bisect_right(sa, addr) - 1
            onde = f" ({syms[i][1]}+0x{addr - syms[i][0]:x})" if i >= 0 and addr - syms[i][0] < 0x10000 else ""
            print(f"  {addr:08x} +{fim - ini:<5x}{onde}  x86 {dx[ini:min(fim, ini + 16)].hex(' ', 4)}  ppc {dp[ini:min(fim, ini + 16)].hex(' ', 4)}")
    print("total: " + ", ".join(f"{k} {v}" for k, v in tot.most_common()))


main()
