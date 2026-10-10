#!/usr/bin/env python3
"""Confere que duas builds (ordens de bytes diferentes) dão às structs do jogo o mesmo layout, pelo DWARF.

    layout.py PROG_A PROG_B [--todas]

Para cada struct/union do jogo (declarada em include/ ou nos fontes do jogo; as do port, da libc e do GS
ficam de fora, a menos que --todas), compara o tamanho e, membro a membro (pelo caminho do nome, com as
structs anônimas abertas), o deslocamento e o tamanho. Campo de bits compara a posição no VALOR da unidade
(bit 0 = menos significativo), que é o que o código vê quando lê a unidade como número: no little-endian
ela é o data_bit_offset; no big-endian, a unidade menos o data_bit_offset menos a largura.
Sai com 1 se algo divergir (headless.py chama na build big-endian).
"""
import collections, os, re, subprocess, sys

DWARFDUMP = os.environ.get("LLVM_DWARFDUMP", "llvm-dwarfdump")
# Structs que o código do jogo arruma de propósito para host big-endian (#ifdef PORT_BIG_ENDIAN): os campos mudam
# de lugar dentro da mesma palavra para o VALOR que o resto do código grava continuar certo. Delas se exige uma
# permutação: mesmo tamanho total, cada campo com o mesmo tamanho (ou a mesma largura de bits), e só campos de
# preenchimento (com "pad" no nome) a mais.
PERMUTADAS = {
    "OtPrim": "include/sys/gfx_ot.h: etiqueta DMA gravada como u32, qwc lido como u16",
    "EftPart9Arg": "include/battle/eft_x.h: objId gravado como s32 pelo criador, lido como u8",
    "BtlCapiBPose": "include/battle/btl_capi_b.h: groundFlags gravado como s32, lido como campo de bits de u8",
    "BObjModelBytes": "include/battle/bobj_b.h: flags do cabeçalho do modelo (u32 no arquivo), byte alto lido como char",
}
DIE = re.compile(r"^(0x[0-9a-f]+):(\s+)(DW_TAG_\w+|NULL)")
ATTR = re.compile(r"^\s+(DW_AT_\w+)\s+\((.*)\)\s*$")
GUARDA = {"DW_AT_name", "DW_AT_byte_size", "DW_AT_type", "DW_AT_data_member_location", "DW_AT_bit_size",
          "DW_AT_data_bit_offset", "DW_AT_decl_file", "DW_AT_decl_line", "DW_AT_upper_bound", "DW_AT_count"}


def big_endian(prog):
    with open(prog, "rb") as f:
        return f.read(6)[5] == 2


def ler(prog):
    """{offset: [tag, attrs, filhos]} do .debug_info inteiro."""
    p = subprocess.Popen([DWARFDUMP, "--debug-info", prog], stdout=subprocess.PIPE, text=True, errors="replace")
    dies, pilha, atual = {}, [], None
    for l in p.stdout:
        m = DIE.match(l)
        if m:
            nivel = len(m.group(2))
            while pilha and pilha[-1][0] >= nivel:
                pilha.pop()
            if m.group(3) == "NULL":
                atual = None
                continue
            off = int(m.group(1), 16)
            atual = dies[off] = [m.group(3), {}, []]
            if pilha:
                pilha[-1][1][2].append(off)
            pilha.append((nivel, atual))
            continue
        if atual is not None:
            a = ATTR.match(l)
            if a and a.group(1) in GUARDA:
                atual[1][a.group(1)] = a.group(2)
    p.wait()
    return dies


def num(v):
    return int(v.split()[0], 0)


def nome(v):
    return v.strip('"') if v else None


class Tipos:
    def __init__(self, dies):
        self.d = dies

    def ref(self, die):
        t = die[1].get("DW_AT_type")
        return self.d.get(int(t.split()[0], 16)) if t else None

    def base(self, die):
        while die and die[0] in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
            die = self.ref(die)
        return die

    def tamanho(self, die):
        die = self.base(die)
        if die is None:
            return 0
        if "DW_AT_byte_size" in die[1]:
            return num(die[1]["DW_AT_byte_size"])
        if die[0] == "DW_TAG_pointer_type":
            return 4  # o clang não grava o tamanho do ponteiro; as duas builds são de 32 bits
        if die[0] == "DW_TAG_array_type":
            n = 1
            for f in die[2]:
                s = self.d[f]
                if s[0] == "DW_TAG_subrange_type":
                    if "DW_AT_count" in s[1]:
                        n *= num(s[1]["DW_AT_count"])
                    elif "DW_AT_upper_bound" in s[1]:
                        n *= num(s[1]["DW_AT_upper_bound"]) + 1
                    else:
                        n *= 0
            return n * self.tamanho(self.ref(die))
        return 0

    def membros(self, die, base, prefixo, be, out):
        anon = 0
        for f in die[2]:
            m = self.d[f]
            if m[0] != "DW_TAG_member":
                continue
            a = m[1]
            n = nome(a.get("DW_AT_name"))
            t = self.ref(m)
            tb = self.base(t)
            tam = self.tamanho(t)
            if "DW_AT_bit_size" in a:
                if not n:
                    continue  # preenchimento (bitfields.py acrescenta um no topo de cada unidade)
                w = num(a["DW_AT_bit_size"])
                dbo = num(a["DW_AT_data_bit_offset"]) if "DW_AT_data_bit_offset" in a else 8 * num(a.get("DW_AT_data_member_location", "0"))
                u = 8 * tam
                ini = dbo // u * u
                v = (u - (dbo - ini) - w) if be else (dbo - ini)
                out[prefixo + n] = f"bits u{u}@{base + ini // 8:#x} bit {v} largura {w}"
                continue
            loc = base + num(a.get("DW_AT_data_member_location", "0"))
            if not n and tb is not None and tb[0] in ("DW_TAG_structure_type", "DW_TAG_union_type"):
                self.membros(tb, loc, f"{prefixo}<anon{anon}>.", be, out)
                anon += 1
                continue
            out[prefixo + (n or f"<?{anon}>")] = f"@{loc:#x} tamanho {tam:#x}"


def layouts(prog, todas=False):
    """{(nome, arquivo, linha): {layout congelado: n de unidades de compilação}}."""
    be = big_endian(prog)
    dies = ler(prog)
    tp = Tipos(dies)
    nomes_typedef = {}
    for off, d in dies.items():
        if d[0] == "DW_TAG_typedef" and "DW_AT_type" in d[1]:
            nomes_typedef.setdefault(int(d[1]["DW_AT_type"].split()[0], 16), nome(d[1].get("DW_AT_name")))
    out = collections.defaultdict(collections.Counter)
    for off, d in dies.items():
        if d[0] not in ("DW_TAG_structure_type", "DW_TAG_union_type") or "DW_AT_byte_size" not in d[1]:
            continue
        n = nome(d[1].get("DW_AT_name")) or nomes_typedef.get(off)
        if not n:
            continue  # anônima dentro de outra: entra aberta na de fora
        arq = nome(d[1].get("DW_AT_decl_file")) or ""
        if not todas and (arq.startswith("/usr/") or ("/include/" not in arq and "/gen/src/" not in arq) or
                          "/port/" in arq.replace("/port/build/gen/", "/")):
            continue
        arq = arq[arq.find("/include/") + 1:] if "/include/" in arq else arq[arq.find("/gen/src/") + 9:]
        m = {"": f"tamanho {num(d[1]['DW_AT_byte_size']):#x}"}
        tp.membros(d, 0, "", be, m)
        out[(n, arq, num(d[1].get("DW_AT_decl_line", "0")))][tuple(sorted(m.items()))] += 1
    return out


def permutacao(da, db):
    """Os mesmos campos, cada um com o mesmo tamanho, em outros lugares (ver PERMUTADAS)."""
    def forma(d):
        out = {}
        for campo, v in d.items():
            if "pad" in campo:
                continue
            out[campo] = re.sub(r"@0x[0-9a-f]+ ?", "", v)  # tira o deslocamento, fica tamanho/largura/bit
        return out
    return da[""] == db[""] and forma(da) == forma(db)


def main():
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    todas = "--todas" in sys.argv
    if len(a) != 2:
        sys.exit(__doc__)
    la, lb = layouts(a[0], todas), layouts(a[1], todas)
    ruins = 0
    for k in sorted(set(la) & set(lb), key=lambda k: (k[1], k[2], k[0])):
        xa, xb = set(la[k]), set(lb[k])
        if xa == xb:
            continue
        if k[0] in PERMUTADAS and len(xa) == len(xb) == 1 and permutacao(dict(next(iter(xa))), dict(next(iter(xb)))):
            continue
        ruins += 1
        print(f"{k[0]} ({k[1]}:{k[2]}): {len(xa)} layout(s) em {a[0]}, {len(xb)} em {a[1]}")
        da, db = dict(next(iter(xa))), dict(next(iter(xb)))
        for campo in sorted(set(da) | set(db)):
            if da.get(campo) != db.get(campo):
                print(f"    {campo or '(struct)'}: {da.get(campo, '-')}  |  {db.get(campo, '-')}")
    so = sorted(set(la) ^ set(lb), key=lambda k: (k[1], k[2], k[0]))
    print(f"layout: {len(set(la) & set(lb))} structs comparadas, {ruins} diferentes, {len(so)} só num dos lados")
    if "--so" in sys.argv:
        for k in so:
            print(f"  só em {a[0] if k in la else a[1]}: {k[0]} ({k[1]}:{k[2]})")
    sys.exit(1 if ruins else 0)


if __name__ == "__main__":
    main()
