"""F1: as tabelas de dados do executável (port/build/gen/data/*.s, do gen_data.py do upstream) com os tipos
que o jogo usa, para o assembler big-endian.

O gen_data.py repassa os tipos que o splat adivinhou: quase tudo vira `.long`, inclusive tabelas de `s16` e de
`u8` e campos menores dentro de structs. Num assembler little-endian isso não importa (os bytes saem os mesmos);
num big-endian, um `.long 0x00020003` que no código é `{s16 0, s16 2}`... sai com as metades trocadas.

O rastreio (rastreio.c) marca os bytes de cada objeto de dados com a origem "ELF:<objeto>.<seção>" e anota a
largura de cada leitura. retipar() reescreve cada diretiva numérica pela largura lida:
  .long lido como 4 (ou nunca lido)      fica
  .long lido como dois u16              .short baixo, alto
  .long lido byte a byte                .byte b0, b1, b2, b3        (ordem little-endian: a do PS2)
  .short lido byte a byte               .byte b0, b1
  par de .long lido como u64            as duas palavras trocam de lugar
Ponteiros (`.long Símbolo`) e floats ficam. Diretiva lida de dois jeitos (byte e número, u16 e u32, u64 sem par
de .long...) fica como está e vai para a lista de conflitos (retipar devolve).

Só serve para a F1 (cobre o que uma luta leu). No produto as tabelas vêm do SLUS do usuário e precisam de um
mapa de tipos estático, das declarações da decomp (decisão 8).
"""
import collections, re

DIRETIVA = re.compile(r"^\s*(\.\w+)\s*(.*)$")


def larguras_elf(rastreio):
    """{'cod_1C3400.data': {deslocamento: máscara de larguras}} a partir de um rastreio.tsv."""
    out = collections.defaultdict(dict)
    for l in open(rastreio, encoding="utf-8", errors="replace"):
        c = l.split("\t")
        if c[0].startswith("ELF:"):
            obj = c[0][4:].rsplit(".", 1)[0]  # "ELF:cod_1C3400.data.data" -> objeto "cod_1C3400.data"
            # a largura da cópia só conta onde não houve leitura (como em rastreio.py)
            lido = int(c[2]) or (int(c[6]) if len(c) > 6 else 0)
            out[obj][int(c[1])] = out[obj].get(int(c[1]), 0) | lido
    return out


def _num(s):
    s = s.strip()
    try:
        return int(s, 0) & 0xFFFFFFFF
    except ValueError:
        return None


def _asciz_tamanho(arg, zero):
    corpo = arg.strip()[1:-1]
    n, i = 0, 0
    while i < len(corpo):
        if corpo[i] == "\\":
            i += 1
            if corpo[i] in "01234567":
                j = i
                while j < len(corpo) and j < i + 3 and corpo[j] in "01234567":
                    j += 1
                i = j
            elif corpo[i] == "x":
                i += 1
                while i < len(corpo) and corpo[i] in "0123456789abcdefABCDEF":
                    i += 1
            else:
                i += 1
        else:
            i += 1
        n += 1
    return n + (1 if zero else 0)


def retipar(texto, larg):
    """Reescreve as diretivas de um .s de dados; larg = {deslocamento: máscara} do objeto."""
    linhas = texto.splitlines()
    out, ofs, trocas, conflitos = [], 0, collections.Counter(), []
    i = 0
    while i < len(linhas):
        l = linhas[i]
        m = DIRETIVA.match(l)
        if not m:
            out.append(l)
            i += 1
            continue
        d, arg = m.group(1), m.group(2)
        if d == ".section":
            ofs = 0
        elif d == ".p2align":
            a = 1 << int(arg.split(",")[0])
            ofs = (ofs + a - 1) // a * a
        elif d == ".space":
            ofs += int(arg.split(",")[0], 0)
        elif d in (".asciz", ".string", ".ascii"):
            ofs += _asciz_tamanho(arg, d != ".ascii")
        elif d == ".incbin":
            ofs += int(arg.split(",")[2], 0)
        elif d == ".float":
            ofs += 4 * len(arg.split(","))
        elif d == ".double":
            ofs += 8 * len(arg.split(","))
        elif d == ".byte":
            ofs += len(arg.split(","))
        elif d == ".short":
            novos = []
            for v in arg.split(","):
                n = _num(v)
                lido = larg.get(ofs, 0) | larg.get(ofs + 1, 0)
                if n is not None and lido & 1 and not lido & ~1:
                    novos.append(f"    .byte {n & 0xFF}, {(n >> 8) & 0xFF}")
                    trocas[".short->2 bytes"] += 1
                else:
                    if lido & 1 and lido & ~1 or lido & ~3 or larg.get(ofs + 1, 0) & ~1:
                        conflitos.append((ofs, ".short", lido))
                    novos.append(f"    .short {v.strip()}")
                ofs += 2
            out.extend(novos)
            i += 1
            continue
        elif d == ".long":
            n = _num(arg)
            w = [larg.get(ofs + k, 0) for k in range(4)]
            # o que foi lido nesta palavra: 1 byte, 2 (u16 em 0 ou 2), 4, 8 (u64 começando aqui), 16 (vetor)
            tipos = {k for k, c in ((1, any(x & 1 for x in w)), (2, w[0] & 2 or w[2] & 2), (4, w[0] & 4),
                                    (8, w[0] & 8), (16, w[0] & 16)) if c}
            fora = w[1] & ~1 or w[3] & ~1 or w[2] & ~3  # número começando no meio da palavra
            if len(tipos - {16}) > 1 or fora:
                conflitos.append((ofs, ".long", sum(tipos)))
                tipos = set()
            # par de palavras lido como u64: esta e a seguinte trocam de lugar
            if tipos == {8}:
                m2 = DIRETIVA.match(linhas[i + 1]) if i + 1 < len(linhas) else None
                prox = [larg.get(ofs + 4 + k, 0) for k in range(4)]
                if ofs % 8 == 0 and m2 and m2.group(1) == ".long" and not any(prox):
                    out.append(f"    .long {m2.group(2).strip()}")
                    out.append(f"    .long {arg.strip()}")
                    trocas["u64"] += 1
                    ofs += 8
                    i += 2
                    continue
                conflitos.append((ofs, "u64 sem par de .long", 8))
                tipos = set()
            if n is None or tipos & {4, 16} or not tipos:
                out.append(l)
            elif tipos == {2}:
                out.append(f"    .short {n & 0xFFFF}, {n >> 16}")
                trocas[".long->2 u16"] += 1
            else:
                out.append(f"    .byte {n & 0xFF}, {(n >> 8) & 0xFF}, {(n >> 16) & 0xFF}, {n >> 24}")
                trocas[".long->4 bytes"] += 1
            ofs += 4
            i += 1
            continue
        out.append(l)
        i += 1
    trocas["conflitos (ficam)"] += len(conflitos)
    return "\n".join(out) + "\n", trocas, conflitos
