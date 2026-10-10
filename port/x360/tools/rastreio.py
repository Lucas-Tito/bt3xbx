#!/usr/bin/env python3
"""F1: resume a saída do rastreio (port/x360/headless/rastreio.c) e gera o mapa de inversão de bytes.

    rastreio.py resumo RASTREIO.tsv [PROGRAMA]    por arquivo: quantas posições de 2/4/8 bytes, conflitos,
                                                  e (com o programa x86r) as funções do jogo que leem
    rastreio.py mapa RASTREIO.tsv... -o MAPA.tsv  junta um ou mais rastreios num mapa:
                                                  arquivo  deslocamento  largura  repetições
                                                  (largura 2/4/8; repetições = posições seguidas iguais)

Regra de largura de cada posição: o maior tamanho lido como número (2, 4 ou 8); 16 (cópia de 16 bytes)
não decide nada. Conflito = a mesma posição lida com larguras diferentes (ex.: como palavra e como meia
palavra); vai para o resumo, para olhar à mão, e no mapa fica a MENOR largura maior que 1.
"""
import bisect, collections, subprocess, sys

LARG = (8, 4, 2)


def ler(caminho, com_copias=False):
    """(arquivo, deslocamento, larguras, usos, escritas, pc). com_copias: as larguras de cópia valem quando a
    posição não foi lida como número por ninguém (o jogo leu e guardou o mesmo valor noutro lugar)."""
    for l in open(caminho, encoding="utf-8", errors="replace"):
        c = l.rstrip("\n").split("\t")
        arq, ofs, tam, usos, esc, pc = c[:6]
        tam = int(tam)
        if com_copias and len(c) > 6 and not larguras(tam):
            tam |= int(c[6])
        yield arq, int(ofs), tam, int(usos), int(esc), int(pc, 16)


def larguras(tam):
    return [w for w in LARG if tam & w]


def simbolos(programa):
    out = subprocess.run(["nm", "-n", programa], capture_output=True, text=True).stdout
    tab = []
    for l in out.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in "tT":
            tab.append((int(p[0], 16), p[2]))
    return tab


def nome_pc(tab, pc):
    i = bisect.bisect_right([a for a, _ in tab], pc) - 1
    return tab[i][1] if i >= 0 else f"{pc:08x}"


def resumo(caminho, programa=None):
    tab = simbolos(programa) if programa else None
    por = collections.defaultdict(lambda: {"n": collections.Counter(), "conflito": 0, "misto": 0, "fun": collections.Counter()})
    for arq, ofs, tam, usos, esc, pc in ler(caminho):
        d = por[arq]
        ls = larguras(tam)
        d["n"][ls[0] if ls else 1] += 1
        if len(ls) > 1:
            d["conflito"] += 1
        if ls and tam & 1:
            d["misto"] += 1  # lida também byte a byte (ex.: parser little-endian explícito do port)
        if tab and ls:
            d["fun"][nome_pc(tab, pc)] += 1
    tot = collections.Counter()
    for arq in sorted(por, key=lambda a: -sum(por[a]["n"].values())):
        d = por[arq]
        tot.update(d["n"])
        fun = ", ".join(f"{f}({n})" for f, n in d["fun"].most_common(4))
        print(f"{arq:46s} 8:{d['n'][8]:6d} 4:{d['n'][4]:7d} 2:{d['n'][2]:6d} 1:{d['n'][1]:7d} conflitos:{d['conflito']:5d} mistos:{d['misto']:5d}  {fun}")
    print(f"total: 8:{tot[8]} 4:{tot[4]} 2:{tot[2]} 1:{tot[1]} em {len(por)} arquivos")


def expandir(pos):
    """Uma leitura larga (8 ou 16 bytes) cobre várias posições, mas o rastreio só anota a primeira. Ex.: dois
    floats copiados numa leitura de 8 bytes, um vetor em 16. As posições cobertas recebem a menor largura
    conhecida da primeira (4 se ela só foi lida larga e de 16; uma de 8 sozinha fica u64/double)."""
    novos = {}
    for (arq, ofs), tam in pos.items():
        larga = 16 if tam & 16 else 8 if tam & 8 else 0
        if not larga:
            continue
        menores = [w for w in (2, 4) if tam & w]
        if menores:
            unidade = menores[0]
        elif larga == 16:
            unidade = 8 if tam & 8 else 4
        else:
            continue  # só 8: u64 ou double
        for o in range(ofs + unidade, ofs + larga, unidade):
            if (arq, o) not in pos:
                novos[(arq, o)] = novos.get((arq, o), 0) | unidade
        if tam & 16 and not menores and not tam & 8:
            pos[(arq, ofs)] = tam | 4
    pos.update(novos)
    return len(novos)


def mapa(caminhos, saida):
    pos = {}
    for c in caminhos:
        for arq, ofs, tam, usos, esc, pc in ler(c, com_copias=True):
            pos[(arq, ofs)] = pos.get((arq, ofs), 0) | tam
    print(f"{expandir(pos)} posições cobertas por leituras largas acrescentadas")
    # largura de cada posição (em conflito, a menor) e divisão das que contêm outra menor: uma palavra lida
    # inteira por um copiador e campo a campo pelo jogo vale campo a campo (ex.: s32 + u16 + u16)
    larg = {}
    desalinhadas = 0
    for k, tam in pos.items():
        # o MIPS do PS2 não lê número desalinhado: uma posição fora do alinhamento da própria largura é erro de
        # atribuição da origem (cópia deslocada), não campo do arquivo
        ls = [w for w in larguras(tam) if k[1] % w == 0]
        if len(ls) < len(larguras(tam)):
            desalinhadas += 1
        if ls:
            larg[k] = ls[-1]
    print(f"{desalinhadas} posições com leitura desalinhada descartadas")
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
    print(f"{divididas} posições divididas pela largura menor de dentro")
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
    print(f"{len(linhas)} trechos de {len({l[0] for l in linhas})} arquivos em {saida}")


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "resumo":
        resumo(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    elif len(sys.argv) >= 5 and sys.argv[1] == "mapa" and "-o" in sys.argv:
        i = sys.argv.index("-o")
        mapa(sys.argv[2:i], sys.argv[i + 1])
    else:
        sys.exit(__doc__)


main()
