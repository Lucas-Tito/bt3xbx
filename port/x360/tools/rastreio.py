#!/usr/bin/env python3
"""F1: resume a saída do rastreio (port/x360/headless/rastreio.c) e gera o mapa de inversão de bytes.

    rastreio.py resumo RASTREIO.tsv [PROGRAMA]           por arquivo: posições de 2/4/8 bytes, conflitos, e
                                                          (com o programa x86r) as funções que leem
    rastreio.py mapa RASTREIO.tsv... -o MAPA.tsv [--prog PROGRAMA]
                                                          mapa "arquivo deslocamento largura repetições" e,
                                                          ao lado, MAPA.tsv.conflitos (para olhar à mão)

Regras (o rastreio vem de uma build sem junção de leituras: cada leitura tem a largura do tipo no código):
  - largura de uma posição = a única largura > 1 com que o jogo a leu como número;
  - lida também como byte: NÃO inverte (leitura byte a byte de dado little-endian, ou punning: classe 3 da
    decisão 7, que pede correção no código); vai para os conflitos;
  - lida com duas larguras > 1: vai para os conflitos e o mapa usa a menor;
  - lida num endereço desalinhado ou num tamanho que não é de número (3, 5...): fora do mapa, nos conflitos;
  - só copiada, nunca lida como número: usa a largura da cópia (o jogo lê um campo e guarda o valor noutro
    lugar, e o uso fica nos registradores: o tamanho no cabeçalho da descompressão é assim); nunca vence uma
    leitura como byte;
  - leitura de 16 bytes (vetor) espalha a menor largura conhecida da posição pelo trecho, e 4 se não houver;
    uma de 8 com largura menor conhecida também; 8 sozinha fica u64/double;
  - uma palavra com campo menor lido dentro dela se divide pela largura menor.
"""
import bisect, collections, subprocess, sys

LARG = (16, 8, 4, 2)


def larguras(tam):
    return [w for w in LARG if tam & w]


def ler(caminho):
    """(arquivo, deslocamento, larguras, usos, escritas, pcs, cópias, ímpar, desalinhada)."""
    for l in open(caminho, encoding="utf-8", errors="replace"):
        c = l.rstrip("\n").split("\t")
        pcs = [int(p, 16) for p in c[5].split(",") if int(p, 16)]
        yield (c[0], int(c[1]), int(c[2]), int(c[3]), int(c[4]), pcs,
               int(c[6]) if len(c) > 6 else 0, int(c[7]) if len(c) > 7 else 0, int(c[8]) if len(c) > 8 else 0)


def simbolos(programa):
    out = subprocess.run(["nm", "-n", programa], capture_output=True, text=True).stdout
    tab = [(int(p[0], 16), p[2]) for p in (l.split() for l in out.splitlines()) if len(p) == 3 and p[1] in "tT"]
    ad = [a for a, _ in tab]

    def nome(pc):
        i = bisect.bisect_right(ad, pc) - 1
        return tab[i][1] if i >= 0 else f"{pc:08x}"
    return nome


def resumo(caminho, programa=None):
    nome = simbolos(programa) if programa else None
    por = collections.defaultdict(lambda: {"n": collections.Counter(), "conflito": 0, "byte": 0, "copia": 0,
                                           "fun": collections.Counter()})
    for arq, ofs, tam, usos, esc, pcs, cop, impar, desal in ler(caminho):
        d = por[arq]
        ls = [w for w in larguras(tam) if w < 16]
        d["n"][ls[0] if ls else (1 if tam & 1 else 0)] += 1
        d["conflito"] += len(ls) > 1
        d["byte"] += bool(ls) and bool(tam & 1)
        d["copia"] += not tam and bool(cop)
        if nome and ls:
            for pc in pcs:
                d["fun"][nome(pc)] += 1
    tot = collections.Counter()
    for arq in sorted(por, key=lambda a: -sum(por[a]["n"].values())):
        d = por[arq]
        tot.update(d["n"])
        fun = ", ".join(f"{f}({n})" for f, n in d["fun"].most_common(4))
        print(f"{arq:46s} 8:{d['n'][8]:6d} 4:{d['n'][4]:7d} 2:{d['n'][2]:6d} 1:{d['n'][1]:7d} conflitos:{d['conflito']:5d} "
              f"byte+n:{d['byte']:5d} só-cópia:{d['copia']:5d}  {fun}")
    print(f"total: 8:{tot[8]} 4:{tot[4]} 2:{tot[2]} 1:{tot[1]} em {len(por)} arquivos")


def mapa(caminhos, saida, programa=None):
    nome = simbolos(programa) if programa else (lambda pc: f"{pc:08x}")
    pos, pcs_de, desal, impar, copia = {}, collections.defaultdict(set), set(), set(), {}
    for c in caminhos:
        for arq, ofs, tam, usos, esc, pcs, cop, imp, des in ler(c):
            k = (arq, ofs)
            pos[k] = pos.get(k, 0) | tam
            copia[k] = copia.get(k, 0) | cop
            pcs_de[k].update(pcs)
            if des:
                desal.add(k)
            if imp:
                impar.add(k)
    conflitos, larg = [], {}
    so_copia = 0
    for k, tam in pos.items():
        if not larguras(tam) and not tam & 1 and larguras(copia[k]):
            tam = copia[k]  # só copiada: a largura da cópia
            so_copia += 1
        ls = [w for w in larguras(tam) if w < 16]
        if k in desal:
            conflitos.append((k, tam, "endereço desalinhado"))
            continue
        if not ls:
            if tam & 16:
                larg[k] = 4  # vetor lido inteiro: 4 floats (ver a regra no topo)
            continue
        if tam & 1:
            conflitos.append((k, tam, "lida como byte e como número: não inverte (punning ou leitura byte a byte)"))
            continue
        if len(ls) > 1:
            conflitos.append((k, tam, f"larguras {sorted(ls)}: usa a menor"))
        larg[k] = ls[-1]
        for larga in (16, 8):  # leituras largas cobrem várias posições
            if tam & larga and ls[-1] < larga:
                for o in range(k[1] + ls[-1], k[1] + larga, ls[-1]):
                    larg.setdefault((k[0], o), ls[-1])
    for k in impar - set(c[0] for c in conflitos):
        conflitos.append((k, pos[k], "também lida num tamanho que não é de número (trecho)"))
    divididas = 0
    for (arq, ofs), w in sorted(larg.items()):
        if larg.get((arq, ofs)) != w:
            continue
        dentro = [larg[(arq, o)] for o in range(ofs + 1, ofs + w) if (arq, o) in larg]
        if dentro:
            u = min(dentro + [w])
            larg[(arq, ofs)] = u
            for o in range(ofs + u, ofs + w, u):
                larg[(arq, o)] = min(larg.get((arq, o), u), u)
            divididas += 1
    linhas, ant = [], None
    for (arq, ofs) in sorted(larg):
        w = larg[(arq, ofs)]
        if ant and ant[0] == arq and ant[2] == w and ant[1] + ant[2] * ant[3] == ofs:
            ant[3] += 1
            continue
        if ant:
            linhas.append(ant)
        ant = [arq, ofs, w, 1]
    if ant:
        linhas.append(ant)
    with open(saida, "w") as f:
        for arq, ofs, w, n in linhas:
            f.write(f"{arq}\t{ofs}\t{w}\t{n}\n")
    with open(saida + ".conflitos", "w") as f:
        for (arq, ofs), tam, motivo in sorted(conflitos):
            funcs = ", ".join(sorted({nome(pc) for pc in pcs_de[(arq, ofs)]}))
            ws = sorted(larguras(tam) + ([1] if tam & 1 else []))
            f.write(f"{arq}\t{ofs}\tlarguras={ws}\t{motivo}\t{funcs}\n")
    porque = collections.Counter(m.split(":")[0] for _, _, m in conflitos)
    print(f"{len(linhas)} trechos de {len({l[0] for l in linhas})} arquivos em {saida}; {divididas} palavras divididas; "
          f"{so_copia} posições pela largura da cópia")
    print(f"{len(conflitos)} conflitos em {saida}.conflitos: " + ", ".join(f"{m} {n}" for m, n in porque.most_common()))


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == "resumo":
        resumo(a[1], a[2] if len(a) > 2 else None)
    elif len(a) >= 4 and a[0] == "mapa" and "-o" in a:
        prog = a[a.index("--prog") + 1] if "--prog" in a else None
        mapa(a[1:a.index("-o")], a[a.index("-o") + 1], prog)
    else:
        sys.exit(__doc__)


main()
