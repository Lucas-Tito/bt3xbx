#!/usr/bin/env python3
"""F6, etapa 3: o default.dat e o default.swap de duas builds do 360 (dat360.py) com o mesmo conteúdo e outro lugar.

Quando o executável cresce antes das tabelas (a xaudio2.lib da etapa 3), todo endereço do programa nos dois arquivos
anda o mesmo tanto e o md5 muda sem nada ter mudado. Este confere isso: mesmo tamanho, e toda palavra de 32 bits
(little-endian, o formato do dat360.py) que difere difere pelo mesmo deslocamento, que aparece em cada arquivo.

    dat_compara.py BASE_DIR NOVO_DIR      (as pastas com default.dat e default.swap, por exemplo build/x360/f2)

Sai com 0 com "mesmo conteúdo, deslocamento 0x...", com 1 em qualquer outra diferença (tamanho, dois deslocamentos,
palavra que só uma build tem).
"""
import collections
import pathlib
import struct
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.split("\n\n")[2])
    base, novo = (pathlib.Path(p) for p in sys.argv[1:])
    deltas = collections.Counter()
    erros = []
    for nome in ("default.dat", "default.swap"):
        a, b = (base / nome).read_bytes(), (novo / nome).read_bytes()
        if len(a) != len(b) or len(a) % 4:
            erros.append(f"{nome}: tamanhos {len(a)} e {len(b)}")
            continue
        n = len(a) // 4
        wa, wb = struct.unpack(f"<{n}I", a), struct.unpack(f"<{n}I", b)
        d = collections.Counter((y - x) & 0xFFFFFFFF for x, y in zip(wa, wb) if x != y)
        print(f"{nome}: {n} palavras, {sum(d.values())} diferentes, deslocamentos {[hex(k) for k in d]}")
        deltas.update(d)
    if len(deltas) > 1:
        erros.append(f"mais de um deslocamento: {[(hex(k), v) for k, v in deltas.most_common()]}")
    if erros:
        for e in erros:
            print(">> FALHA: " + e)
        return 1
    print("mesmo conteúdo" + (f", deslocamento 0x{next(iter(deltas)):X}" if deltas else ", sem deslocamento"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
