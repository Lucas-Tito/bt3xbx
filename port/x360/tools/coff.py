#!/usr/bin/env python3
"""Converte um .s de tabelas (sintaxe do GNU as, como o esqueleto port/data/*.s) num objeto COFF do Xbox 360
que o link.exe do XDK aceita. No PC o GNU as monta esses arquivos; no 360 não há assembler para o formato da
Microsoft, e o esqueleto só precisa de seções, rótulos, zeros, números e endereços.

    coff.py ENTRADA.s -o SAIDA.obj        um arquivo
    coff.py --saida DIR A.s B.s ...       vários, cada um em DIR/<nome>.obj
    coff.py --rotulos A.s                 lista "seção rótulo deslocamento" (para conferir com o dumpbin)
    coff.py --externos A.s B.s ...        símbolos que os .long pedem e nenhum dos arquivos define
    --absoluto NOME=VALOR                 (repetível) símbolo de endereço fixo, como o --defsym do ld: o valor
                                          vai gravado no lugar e a relocação some (D_3BE71C, port/tools/link.py)

Formato, conferido com objetos do próprio cl.exe do XDK (dumpbin /headers /symbols /relocations):
- máquina 0x1F2 (PPCBE), características 0x180 como o cl; cabeçalho, seções, relocações e tabela de
  símbolos em LITTLE-ENDIAN (estruturas COFF de sempre); só o conteúdo das seções é big-endian;
- .data com 0xC0000040 (dado inicializado, leitura e escrita), .rdata com 0x40000040, .bss com 0xC0000080
  e sem bytes no arquivo; o alinhamento vai nos bits 20 a 23 ((log2 + 1) << 20);
- `.long Símbolo+N` é uma relocação IMAGE_REL_PPC_ADDR32 (tipo 2) contra o símbolo, com N gravado no lugar,
  big-endian, como o cl faz com `int *p = &v[3];`;
- cada seção tem o símbolo estático de seção com o registro auxiliar (tamanho e número de relocações);
  rótulo com .globl é externo (classe 2), sem .globl é estático (classe 3); rótulo .L... não vai para a
  tabela (como no GNU as) e o endereço dele sai do símbolo de seção mais o deslocamento.

Diretivas: .section NOME[,...] (e .data, .bss, .rodata, .sbss, .sdata como atalho), .globl/.global,
rótulos, .space/.skip N[,valor], .zero N, .byte, .short/.half/.hword/.2byte, .long/.word/.int/.4byte
(no MIPS .word tem 4 bytes), .quad/.8byte/.dword, .float, .double, .ascii, .asciz/.string, .p2align N,
.balign N, .align N (potência de 2, como no MIPS); .type, .size, .file, .ident e comentários (#, //, ;)
são ignorados; qualquer outra diretiva (.text, .weak, .set...) é erro. Seções: .data* e .sdata* viram .data; .rodata* vira .rdata; .bss* e .sbss* viram .bss.
Python sem dependências externas; não lê nenhum byte do jogo (o .s só tem formas e zeros)."""
import argparse, os, re, struct, sys

MAQUINA_PPCBE = 0x1F2
CARACTERISTICAS = 0x0180          # o que o cl grava: 32 bits, "bytes reversed"
REL_PPC_ADDR32 = 0x0002

SCN_DADO = 0x00000040
SCN_BSS = 0x00000080
SCN_LEITURA = 0x40000000
SCN_ESCRITA = 0x80000000

CLASSE_EXTERNA, CLASSE_ESTATICA = 2, 3

# nome no .s -> (nome no COFF, características sem o alinhamento)
def tipo_secao(nome):
    if re.match(r"\.(s?data)(\.|$)", nome):
        return ".data", SCN_DADO | SCN_LEITURA | SCN_ESCRITA
    if re.match(r"\.rodata(\.|$)", nome):
        return ".rdata", SCN_DADO | SCN_LEITURA
    if re.match(r"\.(s?bss)(\.|$)", nome):
        return ".bss", SCN_BSS | SCN_LEITURA | SCN_ESCRITA
    return None


class Erro(Exception):
    pass


class Secao:
    def __init__(self, nome_coff, carac):
        self.nome, self.carac = nome_coff, carac
        self.dados = bytearray()     # na .bss fica vazio e só `tamanho` cresce
        self.tamanho = 0
        self.alinhamento = 1
        self.relocs = []             # (deslocamento, nome do símbolo)
        self.bss = bool(carac & SCN_BSS)

    def poe(self, b, onde):
        if self.bss:
            if any(b):
                raise Erro(f"{onde}: valor diferente de zero numa seção .bss")
        else:
            self.dados += b
        self.tamanho += len(b)

    def alinha(self, n, onde):
        if n & (n - 1) or n < 1 or n > 8192:
            raise Erro(f"{onde}: alinhamento {n} inválido")
        self.alinhamento = max(self.alinhamento, n)
        falta = -self.tamanho % n
        self.poe(bytes(falta), onde)


def numero(texto, onde):
    t = texto.strip()
    m = re.fullmatch(r"'(\\?.)'", t)
    if m:
        c = m.group(1)
        return ord({"\\n": "\n", "\\t": "\t", "\\0": "\0"}.get(c, c[-1]))
    try:
        return int(t, 0) if not re.fullmatch(r"-?0[0-7]+", t) else int(t, 8)
    except ValueError:
        raise Erro(f"{onde}: número inválido '{texto}'")


def divide_args(texto):
    """Separa por vírgula fora de aspas."""
    partes, atual, aspas, escape = [], "", False, False
    for c in texto:
        if aspas:
            atual += c
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                aspas = False
        elif c == '"':
            aspas = True
            atual += c
        elif c == ",":
            partes.append(atual.strip())
            atual = ""
        else:
            atual += c
    if atual.strip():
        partes.append(atual.strip())
    return partes


def tira_comentario(linha):
    aspas, escape = False, False
    for i, c in enumerate(linha):
        if aspas:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                aspas = False
        elif c == '"':
            aspas = True
        elif c in "#;" or linha.startswith("//", i):
            return linha[:i]
    return linha


def texto_c(lit, onde):
    """Literal entre aspas do GNU as -> bytes (escapes \\n \\t \\\\ \\" \\NNN octal \\xHH)."""
    if len(lit) < 2 or lit[0] != '"' or lit[-1] != '"':
        raise Erro(f"{onde}: texto inválido {lit}")
    s, out, i = lit[1:-1], bytearray(), 0
    simples = {"n": 10, "t": 9, "r": 13, "b": 8, "f": 12, "\\": 92, '"': 34, "'": 39}
    while i < len(s):
        c = s[i]
        if c != "\\":
            out += c.encode("utf-8")
            i += 1
            continue
        i += 1
        if i >= len(s):
            raise Erro(f"{onde}: escape incompleto em {lit}")
        c = s[i]
        if c in simples:
            out.append(simples[c])
            i += 1
        elif c in "01234567":
            m = re.match(r"[0-7]{1,3}", s[i:])
            out.append(int(m.group(0), 8) & 0xFF)
            i += len(m.group(0))
        elif c == "x":
            m = re.match(r"[0-9A-Fa-f]+", s[i + 1:])
            if not m:
                raise Erro(f"{onde}: \\x sem dígitos em {lit}")
            out.append(int(m.group(0), 16) & 0xFF)
            i += 1 + len(m.group(0))
        else:
            raise Erro(f"{onde}: escape \\{c} desconhecido em {lit}")
    return bytes(out)


SIMBOLO = r"[A-Za-z_.$][\w.$]*"


class Montagem:
    def __init__(self, caminho):
        self.caminho = caminho
        self.secoes = {}              # nome no COFF -> Secao (na ordem em que aparecem)
        self.atual = None
        self.globais = set()
        self.rotulos = {}             # nome -> (Secao, deslocamento)
        self.ordem_rotulos = []
        self.monta(open(caminho, encoding="utf-8").read())

    def secao(self, nome_s, onde):
        t = tipo_secao(nome_s)
        if t is None:
            raise Erro(f"{onde}: seção {nome_s} não suportada (só dados, rodata e bss)")
        if t[0] not in self.secoes:
            self.secoes[t[0]] = Secao(*t)
        self.atual = self.secoes[t[0]]

    def precisa_secao(self, onde):
        if self.atual is None:
            raise Erro(f"{onde}: dado antes de qualquer .section")
        return self.atual

    def inteiros(self, args, largura, onde):
        s = self.precisa_secao(onde)
        fmt = {1: ">B", 2: ">H", 4: ">I", 8: ">Q"}[largura]
        for a in divide_args(args):
            m = re.fullmatch(rf"({SIMBOLO})\s*(?:([+-])\s*(\w+))?", a)
            if m:
                if largura != 4:
                    raise Erro(f"{onde}: endereço de símbolo só cabe em .long (32 bits): {a}")
                if s.bss:
                    raise Erro(f"{onde}: endereço de símbolo numa seção .bss")
                soma = numero(m.group(3), onde) if m.group(2) else 0
                if m.group(2) == "-":
                    soma = -soma
                s.relocs.append((s.tamanho, m.group(1)))
                s.poe(struct.pack(">i" if soma < 0 else ">I", soma), onde)
                continue
            v = numero(a, onde)
            if not -(1 << (8 * largura - 1)) <= v < (1 << (8 * largura)):
                raise Erro(f"{onde}: {a} não cabe em {largura} bytes")
            s.poe(struct.pack(fmt, v & ((1 << (8 * largura)) - 1)), onde)

    def monta(self, fonte):
        for n, bruta in enumerate(fonte.splitlines(), 1):
            onde = f"{self.caminho}:{n}"
            linha = tira_comentario(bruta).strip()
            while True:
                m = re.match(rf"({SIMBOLO})\s*:\s*", linha)
                if not m:
                    break
                self.rotulo(m.group(1), onde)
                linha = linha[m.end():]
            if not linha:
                continue
            m = re.match(r"(\.[A-Za-z0-9_]+)\s*(.*)$", linha)
            if not m:
                raise Erro(f"{onde}: linha não entendida: {bruta.strip()}")
            d, args = m.group(1).lower(), m.group(2).strip()
            self.diretiva(d, args, onde)

    def rotulo(self, nome, onde):
        if nome in self.rotulos:
            raise Erro(f"{onde}: rótulo {nome} definido duas vezes")
        s = self.precisa_secao(onde)
        self.rotulos[nome] = (s, s.tamanho)
        self.ordem_rotulos.append(nome)

    def diretiva(self, d, args, onde):
        if d == ".section":
            self.secao(divide_args(args)[0], onde)
        elif d in (".data", ".bss", ".rodata", ".sbss", ".sdata"):
            self.secao(d, onde)
        elif d in (".globl", ".global"):
            for nome in divide_args(args):
                self.globais.add(nome)
        elif d in (".type", ".size", ".file", ".ident"):
            pass
        elif d in (".space", ".skip", ".zero"):
            a = divide_args(args)
            n = numero(a[0], onde)
            v = numero(a[1], onde) & 0xFF if len(a) > 1 else 0
            if n < 0:
                raise Erro(f"{onde}: tamanho negativo")
            self.precisa_secao(onde).poe(bytes([v]) * n, onde)
        elif d == ".byte":
            self.inteiros(args, 1, onde)
        elif d in (".short", ".half", ".hword", ".2byte"):
            self.inteiros(args, 2, onde)
        elif d in (".long", ".word", ".int", ".4byte"):
            self.inteiros(args, 4, onde)
        elif d in (".quad", ".8byte", ".dword"):
            self.inteiros(args, 8, onde)
        elif d in (".float", ".single", ".double"):
            fmt = ">d" if d == ".double" else ">f"
            for a in divide_args(args):
                try:
                    v = float(a)
                except ValueError:
                    raise Erro(f"{onde}: número real inválido '{a}'")
                self.precisa_secao(onde).poe(struct.pack(fmt, v), onde)
        elif d in (".ascii", ".asciz", ".string"):
            for a in divide_args(args):
                b = texto_c(a, onde) + (b"\0" if d != ".ascii" else b"")
                self.precisa_secao(onde).poe(b, onde)
        elif d in (".p2align", ".align"):
            self.precisa_secao(onde).alinha(1 << numero(divide_args(args)[0], onde), onde)
        elif d == ".balign":
            self.precisa_secao(onde).alinha(numero(divide_args(args)[0], onde), onde)
        else:
            raise Erro(f"{onde}: diretiva {d} não suportada")


def local(nome):
    return nome.startswith(".L")


def objeto(m, absolutos={}):
    """Bytes do objeto COFF de uma Montagem."""
    secoes = list(m.secoes.values())
    for s in secoes:
        fica = []
        for off, nome in s.relocs:
            if nome in absolutos and nome not in m.rotulos:
                v = struct.unpack_from(">I", s.dados, off)[0]
                struct.pack_into(">I", s.dados, off, (v + absolutos[nome]) & 0xFFFFFFFF)
            else:
                fica.append((off, nome))
        s.relocs = fica
    indice_secao = {id(s): i + 1 for i, s in enumerate(secoes)}

    # tabela de textos: tamanho (4 bytes, conta ele mesmo) e nomes longos terminados em zero
    textos = bytearray(4)
    def nome8(nome, secao=False):
        b = nome.encode("utf-8")
        if len(b) <= 8:
            return b.ljust(8, b"\0")
        pos = len(textos)
        textos.extend(b + b"\0")
        if secao:
            if pos > 9999999:
                raise Erro("tabela de textos grande demais para nome de seção")
            return f"/{pos}".encode().ljust(8, b"\0")
        return struct.pack("<II", 0, pos)

    # símbolos: por seção, o de seção (com auxiliar) e os rótulos dela; depois os externos não definidos
    simbolos, indice = [], {}
    def simbolo(nome, valor, secao, classe, aux=None):
        indice[nome] = len(simbolos)
        simbolos.append(struct.pack("<8sIhHBB", nome8(nome), valor & 0xFFFFFFFF, secao, 0, classe,
                                    1 if aux else 0))
        if aux:
            simbolos.append(aux)
    indice_sec = {}
    for s in secoes:
        i = indice_secao[id(s)]
        indice_sec[id(s)] = len(simbolos)
        aux = struct.pack("<IHHIHB3x", s.tamanho, len(s.relocs), 0, 0, 0, 0)
        simbolos.append(struct.pack("<8sIhHBB", nome8(s.nome), 0, i, 0, CLASSE_ESTATICA, 1))
        simbolos.append(aux)
        for nome in m.ordem_rotulos:
            sec, off = m.rotulos[nome]
            if sec is s and not local(nome):
                simbolo(nome, off, i, CLASSE_EXTERNA if nome in m.globais else CLASSE_ESTATICA)
    for s in secoes:
        for _, nome in s.relocs:
            if nome not in m.rotulos and nome not in indice:
                simbolo(nome, 0, 0, CLASSE_EXTERNA)

    # relocações: rótulo local vira símbolo de seção, e o deslocamento dele soma no valor gravado no lugar
    for s in secoes:
        for off, nome in s.relocs:
            if nome in m.rotulos and local(nome):
                alvo, desl = m.rotulos[nome]
                v = struct.unpack_from(">I", s.dados, off)[0]
                struct.pack_into(">I", s.dados, off, (v + desl) & 0xFFFFFFFF)
        if len(s.relocs) > 0xFFFF:
            raise Erro(f"{m.caminho}: mais de 65535 relocações em {s.nome}")

    # disposição: cabeçalho, cabeçalhos de seção, dados e relocações de cada seção, símbolos, textos
    pos = 20 + 40 * len(secoes)
    cabecalhos, corpo = [], bytearray()
    for s in secoes:
        alin = (s.alinhamento.bit_length()) << 20
        if s.bss:
            p_dados, tam = 0, s.tamanho
        else:
            p_dados, tam = pos + len(corpo), len(s.dados)
            corpo += s.dados
            corpo += bytes(-len(corpo) % 4)
        p_rel = 0
        if s.relocs:
            p_rel = pos + len(corpo)
            for off, nome in s.relocs:
                if nome in m.rotulos and local(nome):
                    si = indice_sec[id(m.rotulos[nome][0])]
                else:
                    si = indice[nome]
                corpo += struct.pack("<IIH", off, si, REL_PPC_ADDR32)
            corpo += bytes(-len(corpo) % 4)
        cabecalhos.append(struct.pack("<8sIIIIIIHHI", nome8(s.nome, secao=True), 0, 0, tam, p_dados, p_rel, 0,
                                      len(s.relocs), 0, s.carac | alin))
    p_simbolos = pos + len(corpo)
    struct.pack_into("<I", textos, 0, len(textos))
    cab = struct.pack("<HHIIIHH", MAQUINA_PPCBE, len(secoes), 0, p_simbolos, len(simbolos), 0, CARACTERISTICAS)
    return cab + b"".join(cabecalhos) + bytes(corpo) + b"".join(simbolos) + bytes(textos)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("entradas", nargs="+")
    ap.add_argument("-o", dest="saida_arq")
    ap.add_argument("--saida", help="pasta dos .obj (um por .s, mesmo nome)")
    ap.add_argument("--rotulos", action="store_true", help="lista seção, rótulo e deslocamento")
    ap.add_argument("--externos", action="store_true", help="símbolos pedidos e não definidos nos arquivos")
    ap.add_argument("--absoluto", action="append", default=[], metavar="NOME=VALOR",
                    help="símbolo de endereço fixo, gravado no lugar sem relocação")
    a = ap.parse_args()
    try:
        absolutos = {}
        for par in a.absoluto:
            nome, _, valor = par.partition("=")
            absolutos[nome] = numero(valor, "--absoluto")
        montagens = [Montagem(p) for p in a.entradas]
        if a.rotulos:
            for m in montagens:
                for nome in m.ordem_rotulos:
                    s, off = m.rotulos[nome]
                    print(f"{os.path.basename(m.caminho)} {s.nome} {nome} {off:#x}")
            return
        if a.externos:
            definidos = {n for m in montagens for n in m.rotulos if n in m.globais}
            pedidos = {n for m in montagens for s in m.secoes.values() for _, n in s.relocs if n not in m.rotulos}
            for n in sorted(pedidos - definidos - set(absolutos)):
                print(n)
            return
        if a.saida_arq and len(montagens) != 1:
            raise Erro("-o pede um arquivo só; para vários use --saida")
        if not a.saida_arq and not a.saida:
            raise Erro("diga a saída: -o ARQ.obj ou --saida DIR")
        if a.saida:
            os.makedirs(a.saida, exist_ok=True)
        for m in montagens:
            destino = a.saida_arq or os.path.join(
                a.saida, os.path.splitext(os.path.basename(m.caminho))[0] + ".obj")
            with open(destino, "wb") as f:
                f.write(objeto(m, absolutos))
    except Erro as e:
        sys.exit(f"coff.py: {e}")


if __name__ == "__main__":
    main()
