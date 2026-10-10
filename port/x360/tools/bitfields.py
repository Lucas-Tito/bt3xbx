"""F1: campos de bits na ordem do PS2 (do bit menos significativo para o mais) numa build big-endian com GCC.

O jogo foi escrito para um compilador que põe o primeiro campo de bits no bit 0 da unidade; o código às
vezes lê a unidade como número (`task->bits = ~3` conta com `dead` no bit 0). O GCC de PowerPC começa pelo
bit mais significativo. No Xbox 360 isso se resolve com `#pragma bitfield_order(lsb_to_msb)` do compilador
do XDK; o GCC da F1 não tem pragma igual, então lsb() reescreve o C já pré-processado: em cada unidade de
armazenamento, os campos vão em ordem inversa, com um campo sem nome no topo ocupando os bits que sobram.
O resultado no GCC big-endian é o mesmo layout que o lsb_to_msb dá no 360.

As linhas do arquivo se mantêm (as quebras dentro dos campos reescritos vão para o fim deles), para o DWARF e
o rastreio apontarem a linha certa. tools/layout.py confere o resultado contra a build little-endian.

Simplificações (avisadas em `avisos`): uma sequência de campos que muda de tamanho de tipo no meio começa
unidade nova; campos de largura 0 fecham a unidade.
"""
import re

TAM = {"u8": 8, "s8": 8, "char": 8, "signed char": 8, "unsigned char": 8,
       "u16": 16, "s16": 16, "short": 16, "unsigned short": 16, "signed short": 16,
       "u32": 32, "s32": 32, "int": 32, "unsigned int": 32, "signed int": 32, "unsigned": 32, "signed": 32,
       "long": 32, "unsigned long": 32, "u64": 64, "s64": 64, "long long": 64, "unsigned long long": 64}
TIPO = r"(?:(?:const|volatile)\s+)*(" + "|".join(sorted((re.escape(t) for t in TAM), key=len, reverse=True)) + r")"
CAMPO = re.compile(r"^\s*" + TIPO + r"\s+(.*?)\s*$", re.S)
DECL = re.compile(r"^\s*([A-Za-z_]\w*)?\s*:\s*(\d+)\s*$")

avisos = []


def _campos(seg):
    """[(tipo, nome, largura)] se o membro `seg` (sem o ;) for só campos de bits, senão None."""
    m = CAMPO.match(seg)
    if not m:
        return None
    out = []
    for d in m.group(2).split(","):
        dm = DECL.match(d)
        if not dm:
            return None
        out.append((m.group(1), dm.group(1) or "", int(dm.group(2))))
    return out


def _unidades(run):
    """Agrupa uma sequência de campos em unidades de armazenamento, como o compilador do PS2 (bit 0 primeiro)."""
    unidades, atual, usados, bits = [], [], 0, None
    for tipo, nome, w in run:
        b = TAM[tipo]
        if bits is not None and b != bits:
            avisos.append(f"campos de tamanhos diferentes seguidos ({bits} e {b} bits): unidade nova")
        if bits is None or b != bits or w == 0 or usados + w > b:
            if atual:
                unidades.append((bits, atual, usados))
            atual, usados, bits = [], 0, b
        if w == 0:
            continue
        atual.append((tipo, nome, w))
        usados += w
    if atual:
        unidades.append((bits, atual, usados))
    return unidades


def _reescrever_corpo(corpo):
    segs = corpo.split(";")
    out, run, linhas = [], [], [0]

    def fecha():
        for bits, campos, usados in _unidades(run):
            tipo = campos[0][0]
            if usados < bits:
                out.append(f" {tipo} : {bits - usados}")
            for t, n, w in reversed(campos):
                out.append(f" {t} {n} : {w}")
        if run:  # as quebras de linha dos campos reescritos voltam no fim: o resto do arquivo mantém as linhas
            out[-1] += "\n" * linhas[0]
        run.clear()
        linhas[0] = 0

    for i, seg in enumerate(segs):
        if i == len(segs) - 1:  # depois do último ;
            fecha()
            out.append(seg)
            break
        c = _campos(seg)
        if c is not None:
            run.extend(c)
            linhas[0] += seg.count("\n")
            continue
        fecha()
        out.append(seg)
    return ";".join(out)


def lsb(texto):
    """Reescreve os corpos de struct/union que têm campos de bits, cada corpo uma vez só (de dentro para fora:
    um corpo já reescrito vira um marcador opaco para o corpo de fora)."""
    if ":" not in texto:
        return texto
    corpos = []
    padrao = re.compile(r"\{([^{}]*)\}")

    def guarda(m):
        corpo = m.group(1)
        if re.search(r":\s*\d+\s*[;,]", corpo):
            corpo = _reescrever_corpo(corpo)
        corpos.append(corpo)
        return f"\x01{len(corpos) - 1}\x02"

    while True:
        novo = padrao.sub(guarda, texto)
        if novo == texto:
            break
        texto = novo
    marcador = re.compile(r"\x01(\d+)\x02")
    while marcador.search(texto):
        texto = marcador.sub(lambda m: "{" + corpos[int(m.group(1))] + "}", texto)
    return texto
