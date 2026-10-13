#!/usr/bin/env python3
"""A guarda do Sprite_Unpack (port/x360/conv/conv_jogo.c) dá o nome de quem chamou pela tabela CHAMADORES: a função da
tabela de maior endereço que não passa do retorno. Isso só vale se, no binário, cada chamada ficou dentro de uma
função da tabela: com inlining (ou uma função nova que chama Sprite_Unpack), o nome sairia errado sem aviso.

    chamadores_bl.py default.exe default.map [ALVO]      (ALVO: Sprite_Unpack)

Varre o código do .exe do 360 (PE, PowerPC big-endian) atrás de cada `bl` para o ALVO, acha pelo .map a função que o
contém e confere contra a tabela (unpacks.chamadores_da_guarda). Sai com 1 se alguma chamada cai fora da tabela, se
alguma função da tabela não tem chamada no binário, ou se não acha nenhuma. O build_f2.sh roda no fim.
"""
import bisect
import collections
import pathlib
import re
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import unpacks  # noqa: E402


def secoes(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    n = struct.unpack_from("<H", d, pe + 6)[0]
    opt = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    o = pe + 24 + opt
    for _ in range(n):
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        car = struct.unpack_from("<I", d, o + 36)[0]
        if car & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
            yield va + base, vs, rp, rs
        o += 40


def funcoes(mapa):
    out = []
    for l in open(mapa, errors="replace"):
        m = re.match(r"\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8}) f\s", l)
        if m:
            out.append((int(m.group(2), 16), m.group(1)))
    return sorted(out)


def main():
    if len(sys.argv) < 3:
        print(__doc__.strip().splitlines()[4].strip())
        return 2
    exe, mapa = sys.argv[1], sys.argv[2]
    alvo = sys.argv[3] if len(sys.argv) > 3 else "Sprite_Unpack"
    d = pathlib.Path(exe).read_bytes()
    fs = funcoes(mapa)
    ends = {n: a for a, n in fs}
    if alvo not in ends:
        print(f"chamadores_bl: {alvo} não está no .map")
        return 1
    t = ends[alvo]
    inicios = [a for a, _ in fs]
    cont = collections.Counter()
    for va, vs, rp, rs in secoes(d):
        for off in range(0, min(vs, rs) - 3, 4):
            w = struct.unpack_from(">I", d, rp + off)[0]
            if (w >> 26) == 18 and (w & 3) == 1:  # bl (AA = 0, LK = 1)
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                pc = va + off
                if pc + li == t:
                    i = bisect.bisect_right(inicios, pc) - 1
                    cont[fs[i][1] if i >= 0 else "?"] += 1
    tabela = unpacks.chamadores_da_guarda()
    fora = sorted(set(cont) - tabela)
    sem = sorted(tabela - set(cont))
    print(f"chamadores_bl: {sum(cont.values())} chamadas de {alvo} em {len(cont)} funções; tabela CHAMADORES com "
          f"{len(tabela)}")
    for f in fora:
        print(f"  FORA DA TABELA: {f} ({cont[f]} chamadas): a guarda daria o nome da função anterior da tabela")
    for f in sem:
        print(f"  SEM CHAMADA NO BINÁRIO: {f} (inlining? a chamada foi parar em outra função)")
    if not cont or fora or sem:
        return 1
    print("chamadores_bl: toda chamada cai numa função da tabela")
    return 0


if __name__ == "__main__":
    sys.exit(main())
