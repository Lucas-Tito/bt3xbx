#!/usr/bin/env python3
"""F2: o que o .xex leva ao lado (decisão 13), pelo mapa do link.exe do XDK. É o esqueleto() de headless.py para o
360: lá os endereços vêm do mapa do ld, aqui do .map da Microsoft.

    dat360.py MAPA.map TROCAS.tsv SAIDA    ->  SAIDA.dat e SAIDA.swap (game:\\default.dat e .swap no console)

  SAIDA.dat   a lista de make_dat.py do upstream (endereço no programa, arquivo do disco, deslocamento, tamanho de
              cada `.space` de port/data) e o hash de port/data/index.txt; little-endian, o formato do upstream;
  SAIDA.swap  endereço e largura de cada posição viva de número que vem do disco (elftipos.py --trocas: objeto,
              deslocamento, largura), que Port_LoadGameData troca de ordem depois da cópia e do hash.

O endereço de cada objeto sai dos rótulos dele: o .map dá o endereço de cada símbolo (públicos e estáticos) e o
coff.py o deslocamento do rótulo no objeto; todos os rótulos do objeto têm de concordar. Nenhum byte do jogo."""
import bisect, collections, pathlib, re, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/x360/tools"))
import coff  # noqa: E402


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__.split("\n\n")[1])
    mapa, trocas, saida = sys.argv[1:]
    # endereço de cada símbolo dos objetos de port/data: " 0004:00000050  D_002C3380  82260050  cod_1C3380.data.obj"
    endereco = collections.defaultdict(dict)
    for l in open(mapa, errors="replace"):
        m = re.match(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+(?:f\s+)?(?:i\s+)?(\S+)\.obj\s*$", l)
        if m:
            endereco[m.group(3)][m.group(1)] = int(m.group(2), 16)
    onde, checksum = {}, None
    for l in (ROOT / "port/data/index.txt").read_text().splitlines():
        p = l.split()
        if len(p) == 3 and not l.startswith("#"):
            onde[p[0]] = (int(p[1]), int(p[2], 16))
        elif len(p) == 2 and p[0] == "checksum":
            checksum = int(p[1], 16)
    lugar, regs = {}, []
    for nome in sorted(onde):
        mont = coff.Montagem(str(ROOT / "port/data" / (nome + ".s")))
        bases = {endereco[nome][r] - off for r, (_, off) in mont.rotulos.items() if r in endereco[nome]}
        if len(bases) != 1:
            sys.exit(f"{nome}: {'sem rótulo no mapa' if not bases else 'rótulos em lugares que não concordam'}")
        ini = bases.pop()
        lugar[nome] = ini
        qual, so = onde[nome]
        off = 0
        for l in (ROOT / "port/data" / (nome + ".s")).read_text().splitlines():
            p = l.split()
            if not p or p[0].startswith("#"):
                continue
            if p[0] == ".space":
                regs.append((ini + off, qual, so + off, int(p[1])))
                off += int(p[1])
            elif p[0] == ".zero":
                off += int(p[1])
            elif p[0] == ".long":
                off += 4
    pathlib.Path(saida + ".dat").write_bytes(struct.pack("<4sIQ", b"BT3D", len(regs), checksum) +
                                              b"".join(struct.pack("<IIII", *r) for r in regs))
    # cada posição tem de cair inteira no que a lista copia (os `.long Símbolo` o linker já escreveu big-endian)
    faixas = []
    for a, _, _, n in sorted(regs):
        if faixas and a <= faixas[-1][1]:
            faixas[-1][1] = max(faixas[-1][1], a + n)
        else:
            faixas.append([a, a + n])
    inicios = [f[0] for f in faixas]
    pos = []
    for l in open(trocas).read().splitlines():
        nome, ofs, w = l.split("\t")
        if nome not in lugar:
            sys.exit(f"trocas: {nome} não está no mapa")
        a, w = lugar[nome] + int(ofs), int(w)
        i = bisect.bisect_right(inicios, a) - 1
        if i < 0 or a + w > faixas[i][1]:
            sys.exit(f"trocas: {nome}+{int(ofs):#x} [{w}] fora do que a lista copia")
        pos.append((a, w))
    pos.sort()
    pathlib.Path(saida + ".swap").write_bytes(struct.pack("<4sI", b"BT3S", len(pos)) +
                                               b"".join(struct.pack("<II", a, w) for a, w in pos))
    print(f"tabelas: {len(lugar)}, {sum(r[3] for r in regs)} bytes do disco em {len(regs)} trechos; "
          f"{len(pos)} posições a trocar")


if __name__ == "__main__":
    main()
