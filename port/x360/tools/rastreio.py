#!/usr/bin/env python3
"""F1: resume a saída do rastreio (port/x360/headless/rastreio.c) e gera o mapa de inversão de bytes.

    rastreio.py resumo RASTREIO.tsv [PROGRAMA]           por arquivo: posições de 2/4/8 bytes, conflitos, e
                                                          (com o programa x86r) as funções que leem
    rastreio.py mapa RASTREIO.tsv... -o MAPA.tsv [--prog PROGRAMA]
                                                          mapa "arquivo deslocamento largura repetições" e,
                                                          ao lado, MAPA.tsv.conflitos (para olhar à mão)

Regras (o rastreio vem de uma build sem junção de leituras: cada leitura tem a largura do tipo no código).
Conflito NUNCA gera inversão (nem do trecho de que faz parte): vai para MAPA.tsv.conflitos, para o conversor do
formato (decisão 8) ou uma correção no código decidir.
  - largura de uma posição = a única largura > 1 com que o jogo a leu como número;
  - lida também como byte, ou com duas larguras > 1: conflito; a não ser que todas as leituras como byte sejam de
    campos de structs que o código já arruma para big-endian (layout.PERMUTADAS, campo pelo tipos.py): aí o
    byte lido é o certo e a posição se inverte pelo número;
  - lida num endereço desalinhado ou num tamanho que não é de número (3, 5...): conflito;
  - só copiada, nunca lida: usa a largura da cópia, se for uma só (o jogo lê um campo e guarda o valor noutro
    lugar, e o uso fica nos registradores: o tamanho no cabeçalho da descompressão é assim);
  - qualquer leitura que comece dentro de um número de 2, 4 ou 8 bytes (inclusive de um byte só): conflito
    para as duas posições;
  - leitura de 16 bytes (vetor): o elemento é a largura das leituras de dentro, se todas forem iguais e
    alinhadas, e 4 se não houver nenhuma; senão, conflito.
"""
import bisect, collections, subprocess, sys

LARG = (16, 8, 4, 2)


def larguras(tam):
    return [w for w in LARG if tam & w]


def ler(caminho, com_pcb=False):
    """(arquivo, deslocamento, larguras, usos, escritas, pcs, cópias, ímpar, desalinhada[, pcs de byte])."""
    for l in open(caminho, encoding="utf-8", errors="replace"):
        c = l.rstrip("\n").split("\t")
        pcs = [int(p, 16) for p in c[5].split(",") if int(p, 16)]
        r = (c[0], int(c[1]), int(c[2]), int(c[3]), int(c[4]), pcs,
             int(c[6]) if len(c) > 6 else 0, int(c[7]) if len(c) > 7 else 0, int(c[8]) if len(c) > 8 else 0)
        if com_pcb:
            r += ([int(p, 16) for p in c[9].split(",") if int(p, 16)] if len(c) > 9 else None,)
        yield r


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
    pcb_de = collections.defaultdict(set)
    sem_pcb = False
    for c in caminhos:
        for arq, ofs, tam, usos, esc, pcs, cop, imp, des, pcb in ler(c, com_pcb=True):
            k = (arq, ofs)
            if pcb is None:
                sem_pcb = True
            else:
                pcb_de[k].update(pcb)
            pos[k] = pos.get(k, 0) | tam
            copia[k] = copia.get(k, 0) | cop
            pcs_de[k].update(pcs)
            if des:
                desal.add(k)
            if imp:
                impar.add(k)
    conflitos = {}

    def conflito(k, motivo):
        conflitos.setdefault(k, motivo)

    # Leitura como byte feita por um campo de struct que o código já arruma para big-endian (layout.PERMUTADAS):
    # o jogo copiou o número do disco para essa struct e lê lá um byte dele; com a struct permutada o byte lido é
    # o certo, e a posição se inverte pela largura do número. Precisa saber o campo de cada leitura (tipos.py).
    resolvidos = collections.Counter()
    campo_de = {}
    if programa and not sem_pcb:
        import layout, tipos
        suspeitos = {pc for k, tam in pos.items() if tam & 1 and tam & 0xE for pc in pcb_de[k]}
        campo_de = tipos.campos(programa, suspeitos) if suspeitos else {}

    def byte_resolvido(k):
        pcb = pcb_de.get(k)
        if not pcb or len(pcb) >= 4:  # 4 = a lista encheu: pode haver outro leitor de byte não anotado
            return None
        structs = {campo_de[pc][0].split(".")[0] if pc in campo_de else None for pc in pcb}
        if None in structs or not structs <= set(layout.PERMUTADAS):
            return None
        return ", ".join(sorted(structs))

    # 1. cada posição sozinha: a largura que pede, ou conflito
    trecho, so_copia = {}, 0  # (arq, ofs) -> (largura do trecho, largura do elemento)
    for k, tam in pos.items():
        if k in desal:
            conflito(k, "endereço desalinhado")
            continue
        if k in impar:
            conflito(k, "também lida num tamanho que não é de número (trecho)")
            continue
        ls = [w for w in larguras(tam) if w < 16]
        if not ls and not tam & 1 and not tam & 16 and len(larguras(copia[k])) == 1:
            ls = larguras(copia[k])  # só copiada: a largura da cópia
            so_copia += 1
        if tam & 1:
            if not ls:
                continue
            r = byte_resolvido(k) if campo_de else None
            if r is None:
                conflito(k, "lida como byte e como número: não inverte (punning ou leitura byte a byte)")
                continue
            resolvidos[r] += 1
        if len(ls) > 1:
            conflito(k, f"larguras {sorted(ls)}: não inverte")
            continue
        if ls:
            trecho[k] = (ls[0], ls[0])
        elif tam & 16:
            trecho[k] = (16, 0)  # vetor: o elemento sai das leituras de dentro (4 se não houver)
    # 2. trechos que se sobrepõem a outra leitura
    por_arq = collections.defaultdict(list)
    for (arq, ofs) in pos:
        por_arq[arq].append(ofs)
    for arq in por_arq:
        por_arq[arq].sort()
    for (arq, ofs), (w, e) in list(trecho.items()):
        lista = por_arq[arq]
        i = bisect.bisect_right(lista, ofs)
        dentro = []
        while i < len(lista) and lista[i] < ofs + w:
            dentro.append(lista[i])
            i += 1
        if w == 16:
            ws = {trecho[(arq, o)][0] for o in dentro if (arq, o) in trecho}
            ruins = [o for o in dentro if (arq, o) not in trecho or (arq, o) in conflitos]
            el = ws.pop() if len(ws) == 1 else (4 if not ws else None)
            if el is None or ruins or any((o - ofs) % el for o in dentro):
                conflito((arq, ofs), "vetor com leituras de larguras diferentes dentro")
                continue
            trecho[(arq, ofs)] = (16, el)
            continue
        for o in dentro:  # um número não tem campo dentro: quem lê no meio dele lê outra coisa
            conflito((arq, ofs), f"leitura dentro de um número de {w} bytes (em +{o - ofs}): não inverte")
            conflito((arq, o), f"começa dentro de um número de {w} bytes lido em {ofs}: não inverte")
    # 3. conflito não inverte nada, nem o trecho de que faz parte
    larg = {}
    for (arq, ofs), (w, e) in trecho.items():
        if (arq, ofs) in conflitos:
            continue
        if w == 16:
            for o in range(ofs, ofs + 16, e):
                larg.setdefault((arq, o), e)
        else:
            larg[(arq, ofs)] = w
    for k in conflitos:
        larg.pop(k, None)
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
        for (arq, ofs), motivo in sorted(conflitos.items()):
            tam = pos.get((arq, ofs), 0)
            funcs = ", ".join(sorted({nome(pc) for pc in pcs_de[(arq, ofs)]}))
            ws = sorted(larguras(tam) + ([1] if tam & 1 else []))
            f.write(f"{arq}\t{ofs}\tlarguras={ws}\t{motivo}\t{funcs}\n")
    porque = collections.Counter(m.split(":")[0].split(" (em")[0] for m in conflitos.values())
    print(f"{len(linhas)} trechos de {len({l[0] for l in linhas})} arquivos em {saida}; "
          f"{so_copia} posições pela largura da cópia")
    print(f"{len(conflitos)} conflitos (não invertidos) em {saida}.conflitos: " +
          ", ".join(f"{m} {n}" for m, n in porque.most_common()))
    if resolvidos:
        print("lidas como byte por struct já corrigida no código (invertidas pelo número): " +
              ", ".join(f"{m} {n}" for m, n in resolvidos.most_common()))
    elif sem_pcb:
        print("(rastreio sem as funções que leram como byte: os conflitos de byte não puderam ser resolvidos)")


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
