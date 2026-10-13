#!/usr/bin/env python3
"""Tipagem estática das tabelas de dados do executável (protótipo; revisão nº 3, achado 8).

    elftipos.py [--dados DIR] [--saida DIR] [-j N] [--sem-rastreio] [--trocas ARQ]

Roda no WSL, na raiz do clone, depois de headless.py --alvo x86 (precisa de port/build/gen/data/*.s e dos .i de
port/build/x86/obj). Projeto e resultados em docs/x360/formatos/elf.md.

O problema: gen_data.py repassa os tipos que o splat adivinhou (quase tudo `.long`), e num assembler big-endian uma
tabela de s16 ou de u8 escrita como `.long` sai trocada. elfdados.py retipa pela largura que o RASTREIO viu ler, o
que só cobre o que uma luta leu. Aqui a largura de cada posição sai do CÓDIGO, sem rodar o jogo:

1. Objetos: cada port/build/gen/data/*.s vira uma lista de rótulos (nome, deslocamento) e de diretivas
   (deslocamento, tamanho, tipo; `.long Símbolo` é ponteiro, o gen_data já o escreve assim).
2. Declarações (AST JSON do clang de cada .i): toda VarDecl com o nome de um rótulo que não é definição
   (extern, ou tentativa sem inicializador com -fcommon; extern dentro de função também) e todo uso do rótulo
   convertido para outro ponteiro (cast explícito, ou conversão implícita entre ponteiros que o -w cala), com o
   deslocamento quando é `(T *)&D_x[k]` ou `(T *)(D_x + k)`. Cast para void*/u8*/char* não diz nada (memcpy).
3. Layout (DWARF de uma sonda): para cada tipo pedido, o .i ganha no fim `__typeof__(X) *__elftipo_N;` e é
   compilado com -g e as flags de layout da build x86 (-malign-double); o DWARF dá deslocamento e largura de cada
   campo, com structs, vetores, uniões e campos de bits. O DWARF do bt3_x86 sozinho não basta: o clang não grava
   as declarações extern (o objeto é definido no .s, sem DWARF), só os tipos.
4. Atribuição: cada rótulo recebe as candidatas, em ordem de confiança: declaração tipada, visão por cast, alvo
   de ponteiro (campo `T *` de uma tabela já tipada cujo valor no .s é `.long Rótulo`: o rótulo é um T; repete
   até parar de crescer), declaração genérica (vetor de u8/char, que costuma ser lido por cast). Um vetor de u8
   que nenhum código converte para outro tipo (nem void*) conta como tipado: os bytes são bytes. Vetor sem
   tamanho (`extern T x[]`) e alvo de ponteiro vão até o próximo rótulo, qualquer um. Rótulo dentro do alcance
   de outro é interior: as candidatas dele só conferem. Duas candidatas com larguras diferentes no mesmo byte:
   conflito.
5. Alcance no port: rótulo vivo é o que algum objeto C da build x86 referencia (`nm -u`) ou que se alcança de um
   vivo por `.long Rótulo`. O resto (tabelas da biblioteca da Sony, do CRI e da libc, que só o assembly não
   compilado pelo port usa) não precisa de conversão: o valor não importa no 360.
6. Decisão por diretiva (o que o assembler big-endian precisa): fica, 2x2 (.long de dois u16), 4x1, misto,
   u64 (par de .long que troca), 2x1, junta (diretivas menores que o campo), texto->num (o splat viu texto
   onde o C diz número), conflito, ou "nd" com o motivo. Sem tipo no C, `.long Símbolo` (ponteiro), `.float` e
   `.asciz` ficam como o .s diz.
7. Comparação com o rastreio: a decisão que elfdados.retipar tomaria em cada diretiva, pela mesma regra dele.

Com --trocas ARQ (e --dados port/data): o mapa que o host big-endian aplica na carga (decisão 13; headless.py o
grava ao lado da build PPC), as posições vivas de número que vêm do disco, por objeto. Ver exportar_trocas.

Saída em DIR (padrão /tmp/elftipos): mapa.tsv (objeto, deslocamento, largura, tipo, origem, caminho do campo:
posições e larguras, nenhum byte do jogo), diretivas.tsv (decisão estática e do rastreio por diretiva) e
relatorio.txt (cobertura por objeto, rótulos, conflitos, comparação). Dos dados, só a marca "valor zero" por diretiva.
"""
import argparse, bisect, collections, concurrent.futures, json, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(os.environ.get("BT3_RAIZ") or pathlib.Path(__file__).resolve().parents[3])
sys.path.insert(0, str(ROOT / "port/x360/tools"))
import elfdados  # noqa: E402

CLANG = os.environ.get("CLANG", "clang")
DWARFDUMP = os.environ.get("LLVM_DWARFDUMP", "llvm-dwarfdump")
ROTULO = re.compile(r"^([A-Za-z_.$][\w.$]*):\s*$")
SIMBOLO = re.compile(r"^([A-Za-z_.$][\w.$]*)\s*(?:([+-])\s*(0x[0-9A-Fa-f]+|\d+))?$")
TOLERA = ["-Wno-error=return-mismatch", "-Wno-error=int-conversion", "-Wno-error=incompatible-pointer-types",
          "-Wno-error=implicit-function-declaration", "-Wno-error=implicit-int"]
GENERICOS = {"void", "char", "signed char", "unsigned char", "u8", "s8", "u_char", "uchar"}
# Pacotes da GPU nas tabelas (decisão 19): um campo de 64 bits fica como duas palavras de 32, a baixa primeiro, e se
# troca como dois u32. Só o TexBltPacket tem u64 vivo nas tabelas (gTexBltPacket, include/sys/gfxm_b_b.h).
PACOTES_GPU = re.compile(r"^(typedef |struct )?TexBltPacket\b")


# ------------------------------------------------------------------------------------------------ 1. os objetos

class Diretiva:
    __slots__ = ("ofs", "tam", "tipo", "sim", "zero")

    def __init__(self, ofs, tam, tipo, sim=None, zero=False):
        self.ofs, self.tam, self.tipo, self.sim, self.zero = ofs, tam, tipo, sim, zero


class Objeto:
    def __init__(self, caminho):
        self.nome = caminho.name[:-2]
        self.rotulos, self.dirs, ofs = {}, [], 0
        self.copia = []  # (deslocamento, tamanho) dos `.space` do esqueleto port/data: os valores que vêm do disco
        for l in caminho.read_text(errors="replace").splitlines():
            m = ROTULO.match(l.strip())
            if m:
                self.rotulos.setdefault(m.group(1), ofs)
                continue
            m = elfdados.DIRETIVA.match(l)
            if not m:
                continue
            d, arg = m.group(1), m.group(2)
            if d == ".section":
                ofs = 0
            elif d == ".p2align":
                a = 1 << int(arg.split(",")[0])
                ofs = (ofs + a - 1) // a * a
            elif d in (".space", ".zero"):  # .zero: o esqueleto do upstream (port/data, make_skeleton.py)
                n = int(arg.split(",")[0], 0)
                self.dirs.append(Diretiva(ofs, n, "space", zero=True))
                if d == ".space":
                    self.copia.append((ofs, n))
                ofs += n
            elif d in (".asciz", ".string", ".ascii"):
                n = elfdados._asciz_tamanho(arg, d != ".ascii")
                self.dirs.append(Diretiva(ofs, n, "texto"))
                ofs += n
            elif d == ".incbin":
                n = int(arg.split(",")[2], 0)
                self.dirs.append(Diretiva(ofs, n, "incbin"))
                ofs += n
            elif d in (".float", ".double", ".byte", ".short", ".long"):
                w = {".float": 4, ".double": 8, ".byte": 1, ".short": 2, ".long": 4}[d]
                for v in arg.split(","):
                    v = v.strip()
                    tipo, sim, n = d[1:], None, elfdados._num(v)
                    if d == ".long" and n is None:
                        ms = SIMBOLO.match(v)
                        sim = (ms.group(1), (-1 if ms.group(2) == "-" else 1) * int(ms.group(3), 0) if ms.group(3) else 0) \
                            if ms else (v, 0)
                        tipo = "ptr"
                    self.dirs.append(Diretiva(ofs, w, tipo, sim, n == 0))
                    ofs += w
        self.tam = ofs


# --------------------------------------------------------------------------------------- 2. declarações e usos

def _std(i):
    return "-std=gnu99" if i.name.startswith(("pc_", "gs_")) else "-std=gnu89"


def _pontee(qual):
    """'const Foo *' -> 'Foo'; None se não é ponteiro simples."""
    q = qual.strip()
    if not q.endswith("*") or "(" in q:
        return None
    return re.sub(r"\b(const|volatile)\b", "", q[:-1]).strip()


def varrer(args):
    """Um .i: (decls, visões, genéricas, deslocadas). decls = [(nome, qualType, escopo, arquivo)];
    visões = [(nome, tipo do ponteiro, k elementos, arquivo)]."""
    caminho_i, nomes = args
    r = subprocess.run([CLANG, "-m32", _std(caminho_i), "-w"] + TOLERA + ["-fsyntax-only", "-x", "cpp-output",
                        "-Xclang", "-ast-dump=json", str(caminho_i)], capture_output=True, text=True)
    if not r.stdout:
        return caminho_i, [], [], collections.Counter(), []
    sys.setrecursionlimit(100000)
    arvore = json.loads(r.stdout)
    decls, visoes, genericas, deslocadas = [], [], collections.Counter(), []
    estaticas = set()
    estado = {"arq": ""}

    def loc(d):
        if not isinstance(d, dict):
            return
        for k in ("spellingLoc", "expansionLoc", "begin", "end"):
            if k in d:
                loc(d[k])
        if "presumedFile" in d:
            estado["arq"] = d["presumedFile"]

    def uso(nome, pilha, no):
        filho, k = no, 0
        for p in reversed(pilha):
            pk = p.get("kind")
            if pk == "ParenExpr":
                pass
            elif pk == "ImplicitCastExpr":
                ck = p.get("castKind")
                if ck == "BitCast":
                    return visao(nome, p, k)
                if ck not in ("ArrayToPointerDecay", "NoOp"):
                    return
            elif pk == "UnaryOperator" and p.get("opcode") == "&":
                pass
            elif pk in ("ArraySubscriptExpr", "BinaryOperator") and (pk == "ArraySubscriptExpr" or p.get("opcode") == "+"):
                dentro = p.get("inner", [])
                if len(dentro) != 2 or dentro[0] is not filho:
                    return
                lit = dentro[1]
                while lit.get("kind") in ("ImplicitCastExpr", "ParenExpr") and lit.get("inner"):
                    lit = lit["inner"][0]
                if lit.get("kind") != "IntegerLiteral":
                    return
                k += int(lit.get("value", "0"))
            elif pk == "CStyleCastExpr":
                return visao(nome, p, k)
            else:
                return
            filho = p

    def visao(nome, p, k):
        q = p.get("type", {}).get("qualType", "")
        alvo = _pontee(q)
        if alvo is None:
            return
        if alvo in GENERICOS:
            genericas[nome] += 1
            return
        if k < 0:
            deslocadas.append((nome, q, k))
            return
        visoes.append((nome, q, k, estado["arq"]))

    def andar(n, pilha, em_funcao):
        for k in ("loc", "range"):
            loc(n.get(k))
        kind = n.get("kind")
        if kind == "VarDecl":
            nome = n.get("name")
            if nome in nomes:
                sc = n.get("storageClass")
                valida = sc == "extern" if em_funcao else (sc != "static" and "init" not in n)
                if valida:
                    decls.append((nome, n["type"]["qualType"], "bloco" if em_funcao else "arquivo", estado["arq"]))
                else:
                    estaticas.add(n.get("id"))
        elif kind == "DeclRefExpr":
            rd = n.get("referencedDecl", {})
            if rd.get("kind") == "VarDecl" and rd.get("name") in nomes and rd.get("id") not in estaticas:
                uso(rd["name"], pilha, n)
        dentro = n.get("inner")
        if dentro:
            pilha.append(n)
            f = em_funcao or kind == "FunctionDecl"
            for c in dentro:
                if isinstance(c, dict):
                    andar(c, pilha, f)
            pilha.pop()

    andar(arvore, [], False)
    return caminho_i, decls, visoes, genericas, deslocadas


# --------------------------------------------------------------------------------- 3. layout pelo DWARF da sonda

DIE = re.compile(r"^(0x[0-9a-f]+):(\s+)(DW_TAG_\w+|NULL)")
ATTR = re.compile(r"^\s+(DW_AT_\w+)\s+\((.*)\)\s*$")
GUARDA = {"DW_AT_name", "DW_AT_byte_size", "DW_AT_type", "DW_AT_data_member_location", "DW_AT_bit_size",
          "DW_AT_data_bit_offset", "DW_AT_encoding", "DW_AT_upper_bound", "DW_AT_count", "DW_AT_declaration"}


def ler_dwarf(obj):
    texto = subprocess.run([DWARFDUMP, "--debug-info", str(obj)], capture_output=True, text=True, errors="replace").stdout
    dies, pilha, atual = {}, [], None
    for l in texto.splitlines():
        m = DIE.match(l)
        if m:
            nivel = len(m.group(2))
            while pilha and pilha[-1][0] >= nivel:
                pilha.pop()
            if m.group(3) == "NULL":
                atual = None
                continue
            atual = dies[int(m.group(1), 16)] = [m.group(3), {}, []]
            if pilha:
                pilha[-1][1][2].append(int(m.group(1), 16))
            pilha.append((nivel, atual))
            continue
        if atual is not None:
            a = ATTR.match(l)
            if a and a.group(1) in GUARDA:
                atual[1][a.group(1)] = a.group(2)
    return dies


def _num(v):
    return int(v.split()[0], 0)


def _nome(v):
    return v.strip('"') if v else None


class Dwarf:
    """Achata um tipo em escalares (deslocamento, largura, tipo, caminho, tipo apontado)."""

    def __init__(self, dies):
        self.d = dies

    def ref(self, die):
        t = die[1].get("DW_AT_type") if die else None
        return self.d.get(int(t.split()[0], 16)) if t else None

    def base(self, die):
        while die and die[0] in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type",
                                 "DW_TAG_restrict_type", "DW_TAG_atomic_type"):
            die = self.ref(die)
        return die

    def dims(self, die):
        out = []
        for f in die[2]:
            s = self.d[f]
            if s[0] == "DW_TAG_subrange_type":
                if "DW_AT_count" in s[1]:
                    out.append(_num(s[1]["DW_AT_count"]))
                elif "DW_AT_upper_bound" in s[1]:
                    out.append(_num(s[1]["DW_AT_upper_bound"]) + 1)
                else:
                    out.append(None)
        return out

    def tamanho(self, die):
        die = self.base(die)
        if die is None:
            return 0
        if "DW_AT_byte_size" in die[1]:
            return _num(die[1]["DW_AT_byte_size"])
        if die[0] == "DW_TAG_pointer_type":
            return 4
        if die[0] == "DW_TAG_array_type":
            n = 1
            for x in self.dims(die):
                n *= x or 0
            return n * self.tamanho(self.ref(die))
        return 0

    def desc(self, die, prof=0):
        if die is None:
            return "void"
        tag, a = die[0], die[1]
        n = _nome(a.get("DW_AT_name"))
        if prof > 6:
            return "..."
        if tag in ("DW_TAG_typedef", "DW_TAG_base_type"):
            return n
        if tag in ("DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_enumeration_type"):
            return f"{tag[7:].split('_')[0]} {n or '<anon>'}"
        if tag == "DW_TAG_pointer_type":
            return self.desc(self.ref(die), prof + 1) + " *"
        if tag in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
            return self.desc(self.ref(die), prof + 1)
        if tag == "DW_TAG_array_type":
            return self.desc(self.ref(die), prof + 1) + "".join(f"[{x if x is not None else ''}]" for x in self.dims(die))
        if tag == "DW_TAG_subroutine_type":
            return "função"
        return tag

    def generico(self, die):
        """Tipo que não diz largura: void, char/u8 (e vetores deles), função."""
        b = self.base(die)
        while b is not None and b[0] == "DW_TAG_array_type":
            b = self.base(self.ref(b))
        if b is None or b[0] == "DW_TAG_subroutine_type":
            return True
        return b[0] == "DW_TAG_base_type" and self.tamanho(b) == 1

    def achatar(self, die, base, caminho, out, notas):
        b = self.base(die)
        if b is None:
            out.append((base, 0, "void", caminho, None))
            return
        tag, a = b[0], b[1]
        if "DW_AT_declaration" in a:
            out.append((base, 0, "incompleto", caminho, None))
            notas["struct opaca"] += 1
            return
        if tag == "DW_TAG_base_type":
            enc = a.get("DW_AT_encoding", "")
            out.append((base, self.tamanho(b), "float" if "float" in enc else "int", caminho, None))
        elif tag == "DW_TAG_enumeration_type":
            out.append((base, self.tamanho(b), "int", caminho, None))
        elif tag == "DW_TAG_pointer_type":
            out.append((base, 4, "ptr", caminho, self.ref(b)))
        elif tag == "DW_TAG_array_type":
            ds = self.dims(b)
            el = self.ref(b)
            esz = self.tamanho(el)
            if None in ds or not esz:
                out.append((base, 0, "incompleto", caminho, None))
                notas["vetor sem tamanho dentro de tipo"] += 1
                return
            n = 1
            for x in ds:
                n *= x
            sub = []
            self.achatar(el, 0, "", sub, notas)
            for i in range(n):
                for o, w, k, c, al in sub:
                    out.append((base + i * esz + o, w, k, f"{caminho}[{i}]{c}", al))
        elif tag == "DW_TAG_structure_type":
            bits = set()
            for f in b[2]:
                m = self.d[f]
                if m[0] != "DW_TAG_member":
                    continue
                ma = m[1]
                nm = _nome(ma.get("DW_AT_name")) or "<anon>"
                t = self.ref(m)
                if "DW_AT_bit_size" in ma:
                    u = self.tamanho(t)
                    dbo = _num(ma["DW_AT_data_bit_offset"]) if "DW_AT_data_bit_offset" in ma else \
                        8 * _num(ma.get("DW_AT_data_member_location", "0"))
                    uo = base + dbo // (8 * u) * u
                    if uo not in bits:
                        bits.add(uo)
                        out.append((uo, u, "bits", f"{caminho}.{nm}", None))
                    continue
                self.achatar(t, base + _num(ma.get("DW_AT_data_member_location", "0")), f"{caminho}.{nm}", out, notas)
        elif tag == "DW_TAG_union_type":
            membros = []
            for f in b[2]:
                m = self.d[f]
                if m[0] == "DW_TAG_member":
                    sub = []
                    self.achatar(self.ref(m), 0, f"{caminho}.{_nome(m[1].get('DW_AT_name')) or '<anon>'}", sub, notas)
                    membros.append(sub)
            # regra: membro que é só bytes (u8 raw[N]) é acesso cru, não manda na largura se outro membro tiver número
            tipados = [s for s in membros if not all(k == "int" and w == 1 for _, w, k, _, _ in s)]
            if tipados and len(tipados) < len(membros):
                notas["união: membro de bytes ignorado"] += 1
            usar = tipados or membros
            por_byte, conflito = {}, False
            for s in usar:
                for e in s:
                    for x in range(e[0], e[0] + max(e[1], 1)):
                        p = por_byte.get(x)
                        if p is None:
                            por_byte[x] = e
                        elif (p[0], p[1]) != (e[0], e[1]):
                            conflito = True
            if conflito:
                notas["união com larguras diferentes"] += 1
                out.append((base, self.tamanho(b), "conflito", f"{caminho} (união {self.desc(b)})", None))
            else:
                vistos = set()
                for e in sorted(por_byte.values(), key=lambda e: e[0]):
                    if (e[0], e[1]) not in vistos:
                        vistos.add((e[0], e[1]))
                        out.append((base + e[0], e[1], e[2], e[3], e[4]))
        elif tag == "DW_TAG_subroutine_type":
            out.append((base, 0, "função", caminho, None))
        else:
            out.append((base, self.tamanho(b), "int", caminho, None))

    def layout(self, die, notas=None):
        """({'desc', 'tam', 'completo', 'generico', 'notas', 'esc' (escalares) ou, se vetor sem tamanho, 'elem'
        (tamanho do elemento, escalares)}, [DIEs apontados pelos campos ponteiro])."""
        notas = notas if notas is not None else collections.Counter()
        b = self.base(die)
        lay = {"desc": self.desc(die), "generico": self.generico(die), "notas": notas}
        if b is not None and b[0] == "DW_TAG_array_type" and self.dims(b) and self.dims(b)[0] is None:
            el = self.ref(b)
            ds = self.dims(b)[1:]
            n = 1
            for x in ds:
                n *= x or 0
            esz = self.tamanho(el) * n
            sub = []
            if None in ds or not esz:
                lay.update(tam=0, completo=False, elem=(0, []))
                return lay, []
            for i in range(n):
                tmp = []
                self.achatar(el, i * self.tamanho(el), "", tmp, notas)
                sub += [(o, w, k, (f"[{i}]" if ds else "") + c, al) for o, w, k, c, al in tmp]
            lay.update(tam=0, completo=False, elem=(esz, sub))
            esc = sub
        else:
            esc = []
            self.achatar(die, 0, "", esc, notas)
            lay.update(tam=self.tamanho(die), completo=True, esc=esc)
        return lay, [e[4] for e in esc if e[2] == "ptr"]


def _limpa(lay):
    """O layout sem os DIEs (para voltar do processo filho)."""
    def esc(lst):
        return [(o, w, k, c, al) for o, w, k, c, al in lst]
    out = dict(lay)
    out["notas"] = dict(lay["notas"])
    if "esc" in lay:
        out["esc"] = esc(lay["esc"])
    else:
        out["elem"] = (lay["elem"][0], esc(lay["elem"][1]))
    return out


def sondar(args):
    """Um .i com as sondas no fim: {chave: layout}, {desc: layout} dos tipos apontados, erros."""
    caminho_i, pedidos, pasta = args
    pasta = pathlib.Path(pasta)
    texto = caminho_i.read_text(errors="replace")
    vivos = dict(enumerate(pedidos))
    flags = ["-m32", _std(caminho_i), "-c", "-g", "-O0", "-w", "-fcommon", "-fno-pic", "-x", "cpp-output"] + TOLERA
    flags += ["-msse2", "-mfpmath=sse"] if caminho_i.name.startswith("gs_") else \
        ["-msoft-float", "-mno-sse", "-mno-mmx", "-mno-x87", "-malign-double"]
    fonte, obj = pasta / caminho_i.name, pasta / (caminho_i.name[:-2] + ".o")
    for _ in range(6):
        linhas = [f"__typeof__({expr}) *__elftipo_{i};" for i, (_, expr) in vivos.items()]
        ordem = list(vivos)
        fonte.write_text(texto + '\n# 1 "__elftipo_sonda__"\n' + "\n".join(linhas) + "\n")
        r = subprocess.run([CLANG] + flags + [str(fonte), "-o", str(obj)], capture_output=True, text=True)
        if r.returncode == 0:
            break
        ruins = {int(m) for m in re.findall(r"__elftipo_sonda__:(\d+):\d+: error", r.stderr)}
        if not ruins:
            erro = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr[:200])
            return caminho_i.name, {}, {}, [("compilação", erro[:200])]
        for n in ruins:
            if 1 <= n <= len(ordem):
                vivos.pop(ordem[n - 1], None)
    else:
        return caminho_i.name, {}, {}, [("compilação", "sondas não convergiram")]
    caidas = [(pedidos[i][0], pedidos[i][1]) for i in range(len(pedidos)) if i not in vivos]
    dies = ler_dwarf(obj)
    dw = Dwarf(dies)
    por_nome = {}
    for off, d in dies.items():
        n = _nome(d[1].get("DW_AT_name"))
        if d[0] == "DW_TAG_variable" and n and n.startswith("__elftipo_"):
            por_nome[int(n[10:])] = d
    res, apontados, fila, vistos = {}, {}, [], set()
    for i, (chave, _) in enumerate(pedidos):
        d = por_nome.get(i)
        if d is None:
            continue
        lay, alvos = dw.layout(dw.ref(dw.ref(d)))  # variável -> ponteiro -> tipo pedido
        res[chave] = _limpa(lay)
        fila += alvos
    # os tipos apontados por campos ponteiro (para propagar o tipo ao rótulo que o .s põe no campo)
    while fila and len(vistos) < 3000:
        al = fila.pop()
        if al is None or id(al) in vistos:
            continue
        vistos.add(id(al))
        if dw.generico(al):
            continue
        lay, mais = dw.layout(al)
        if lay.get("completo") and lay["tam"]:
            apontados.setdefault(lay["desc"], _limpa(lay))
            fila += mais
    # o tipo apontado vira o nome dele (a chave de `apontados`), ou None se não diz largura (void*, u8*, função)
    for lay in list(res.values()) + list(apontados.values()):
        lst = lay["esc"] if "esc" in lay else lay["elem"][1]
        lst[:] = [(o, w, k, c, None if not isinstance(al, list) or dw.generico(al) else dw.desc(al))
                  for o, w, k, c, al in lst]
    obj.unlink(missing_ok=True)
    fonte.unlink(missing_ok=True)
    return caminho_i.name, res, apontados, caidas


# ---------------------------------------------------------------------------------------------- 4. atribuição

class Mapa:
    """Por objeto: em cada byte, o escalar que o cobre (índice) e os conflitos."""

    def __init__(self, obj):
        self.obj = obj
        self.esc = []           # (início, largura, tipo, caminho, origem, alvo)
        self.byte = [None] * obj.tam
        self.dono = [None] * obj.tam  # rótulo cujo alcance cobre o byte (enchimento se não houver escalar)
        self.conflitos = collections.defaultdict(set)


ORDEM = {"decl": 0, "visao": 1, "alvo": 2, "generica": 3}


def atribuir(objetos, cands):
    """cands[rótulo] = [(fonte, origem, layout, deslocamento)]. Devolve {objeto: Mapa}, notas."""
    mapas, notas = {}, collections.Counter()
    cortes = set()
    for o in objetos.values():
        mp = Mapa(o)
        mapas[o.nome] = mp
        rot = sorted(o.rotulos.items(), key=lambda x: x[1])
        todos = sorted({ofs for r, ofs in rot})
        por_ofs = collections.defaultdict(list)
        for r, ofs in rot:
            por_ofs[ofs].append(r)
        trabalhos = []
        for r, ofs in rot:
            for fonte, origem, lay, k in cands.get(r, []):
                f = "generica" if lay["generico"] and fonte != "alvo" else fonte
                trabalhos.append((ORDEM[f], ofs, r, f, origem, lay, k))
        for ordem, ofs, r, fonte, origem, lay, k in sorted(trabalhos, key=lambda t: (t[0], t[1])):
            ini = ofs + k
            if lay["completo"]:
                fim, esc = ini + lay["tam"], lay["esc"]
            else:
                esz, sub = lay["elem"]
                # sem tamanho no C: até o próximo rótulo, qualquer um (estender até o próximo DECLARADO cobriu 70 KB
                # de tabelas alheias com o s32 de um `extern s32 x[]`)
                j = bisect.bisect_right(todos, ini)
                lim = todos[j] if j < len(todos) else o.tam
                for rr in por_ofs.get(lim, []):
                    if not cands.get(rr):
                        cortes.add(rr)
                if not esz:
                    notas["vetor sem tamanho de elemento"] += 1
                    continue
                n = max(1, (lim - ini) // esz)
                fim = ini + n * esz
                esc = [(i * esz + a, w, kk, f"[{i}]{c}", al) for i in range(n) for a, w, kk, c, al in sub]
                if (lim - ini) % esz:
                    notas["vetor aberto com resto até o próximo rótulo"] += 1
            if fim > o.tam:
                notas["tipo passa do fim do objeto"] += 1
                fim = o.tam
            for b in range(max(ini, 0), fim):
                if mp.dono[b] is None:
                    mp.dono[b] = r
            for a, w, kk, c, al in esc:
                a += ini
                if a < 0 or a + max(w, 1) > o.tam or w == 0 or kk in ("void", "função", "incompleto"):
                    continue
                desc = f"{lay['desc']}{c} ({fonte})"
                if kk == "conflito":
                    for b in range(a, a + w):
                        mp.conflitos[b].add(desc)
                    continue
                ocupados = {mp.byte[b] for b in range(a, a + w)} - {None}
                if not ocupados:
                    novo = len(mp.esc)
                    mp.esc.append((a, w, kk, desc, fonte, al))
                    for b in range(a, a + w):
                        mp.byte[b] = novo
                    continue
                if fonte == "generica":
                    continue  # declaração genérica não manda onde já há tipo nem gera conflito
                for i in ocupados:
                    e = mp.esc[i]
                    if (e[0], e[1]) != (a, w):  # mesma posição e largura: visão que concorda
                        for b in range(max(a, e[0]), min(a + w, e[0] + e[1])):
                            mp.conflitos[b].add(f"{e[3]} [{e[1]}] x {desc} [{w}]")
    notas["cortes"] = cortes
    return mapas, notas


def propagar(objetos, mapas, cands, apontados, onde):
    """Campo ponteiro tipado cujo valor no .s é `.long Rótulo`: o rótulo ganha o tipo apontado."""
    novos, sem_tipo = 0, collections.Counter()
    for o in objetos.values():
        mp = mapas[o.nome]
        for d in o.dirs:
            if d.tipo != "ptr" or d.sim is None:
                continue
            i = mp.byte[d.ofs]
            if i is None:
                continue
            e = mp.esc[i]
            if e[2] != "ptr" or e[0] != d.ofs:
                continue
            alvo, k = d.sim
            if alvo not in onde:
                continue
            if not e[5]:
                sem_tipo[alvo] += 1
                continue
            lay = apontados.get(e[5])
            if lay is None:
                sem_tipo[alvo] += 1
                continue
            aberto = dict(lay, completo=False, elem=(lay["tam"], lay["esc"]))
            aberto.pop("esc", None)
            chave = ("alvo", e[5], k)
            if any(c[1] == chave for c in cands.get(alvo, [])):
                continue
            cands.setdefault(alvo, []).append(("alvo", chave, aberto, k))
            novos += 1
    return novos, sem_tipo


# ------------------------------------------------------------------------------------------ 5. decisões

def decisao_estatica(mp, d, estado_rotulo):
    o = mp.obj
    bs = range(d.ofs, d.ofs + d.tam)
    if d.tipo == "space":
        return "zero", ""
    if d.tipo == "incbin":
        return "nd", "binário (.incbin, microcódigo VU1)"
    cf = set()
    for b in bs:
        cf |= mp.conflitos.get(b, set())
    if cf:
        return "conflito", "; ".join(sorted(cf))[:300]
    esc, nd, gen = [], 0, 0
    for b in bs:
        i = mp.byte[b]
        if i is None:
            if mp.dono[b] is not None:
                esc.append(("pad", b))
            else:
                nd += 1
            continue
        e = mp.esc[i]
        if e[4] == "generica":
            gen += 1
        if not esc or esc[-1] != ("esc", i):
            esc.append(("esc", i))
    if gen and gen + nd == d.tam:
        if d.tipo in ("byte", "texto"):
            return "fica", "byte (declaração genérica)"
        nd = d.tam  # o C só diz "bytes": vale o que o .s diz, como se não houvesse declaração
        motivo_gen = "só declaração genérica (vetor de u8/char)"
    else:
        motivo_gen = None
    if nd == d.tam:
        if d.tipo == "ptr":
            return "fica", "ponteiro (do .s)"
        if d.tipo in ("float", "double"):
            return "fica?", "float só pelo .s (palpite do splat)"
        if d.tipo == "texto":
            return "fica", "texto (do .s)"
        return "nd", motivo_gen or motivo_nd(o, d.ofs, estado_rotulo)
    if nd:
        return "nd", "parcial: " + motivo_nd(o, d.ofs, estado_rotulo)
    if gen and d.tipo in ("long", "short", "float", "double"):
        return "nd", "só declaração genérica (vetor de u8/char)"
    ws = []
    for t, i in esc:
        if t == "pad":
            ws.append((i, 1, "pad"))
        else:
            e = mp.esc[i]
            ws.append((e[0], e[1], e[2]))
    caminho = next((mp.esc[i][3] for t, i in esc if t == "esc"), "enchimento")
    if all(w[2] == "pad" for w in ws):
        return "fica", "enchimento" + ("" if d.zero or d.tipo in ("ptr", "texto") else " NÃO NULO")
    dentro = all(a >= d.ofs and a + w <= d.ofs + d.tam for a, w, _ in ws)
    if d.tipo in ("long", "ptr", "float"):
        if len(ws) == 1 and ws[0][:2] == (d.ofs, 4):
            k = ws[0][2]
            if d.tipo == "ptr" and k != "ptr":
                return "fica", f"ponteiro no .s, {k} no C: {caminho}"
            if d.tipo == "long" and k == "ptr" and not d.zero:
                return "fica", f"PONTEIRO NUMÉRICO: {caminho}"
            return "fica", f"{k}: {caminho}"
        if len(ws) == 1 and ws[0][1] == 8 and ws[0][0] in (d.ofs, d.ofs - 4):
            if d.tipo != "long":
                return "conflito", f"u64 sobre {d.tipo}: {caminho}"
            return ("u64" if ws[0][0] == d.ofs else "u64-alto"), caminho
        if dentro and d.tipo == "long":
            padrao = [w for _, w, _ in ws]
            if padrao == [2, 2]:
                return "2x2", caminho
            if padrao == [1, 1, 1, 1]:
                return "4x1", caminho
            return "misto " + ",".join(map(str, padrao)), caminho
        return "conflito", f"atravessa a diretiva: {caminho} [{ws[0][1]}]"
    if d.tipo == "double":
        return ("fica", caminho) if len(ws) == 1 and ws[0][:2] == (d.ofs, 8) else ("conflito", f"double x {caminho}")
    if d.tipo == "short":
        if len(ws) == 1 and ws[0][:2] == (d.ofs, 2):
            return "fica", f"{ws[0][2]}: {caminho}"
        if dentro:
            return "2x1", caminho
        return "junta", f"campo de {max(w for _, w, _ in ws)}: {caminho}"
    if d.tipo == "byte":
        if ws[0][1] == 1:
            return "fica", f"{ws[0][2]}: {caminho}"
        return "junta", f"campo de {ws[0][1]}: {caminho}"
    if d.tipo == "texto":
        largos = [w for _, w, _ in ws if w > 1]
        # o splat viu bytes com cara de texto; o C diz número: vira número (o splat errou o palpite)
        return ("fica", f"texto: {caminho}") if not largos else ("texto->num", f"campo de {max(largos)}: {caminho}")
    return "nd", f"diretiva {d.tipo}"


def motivo_nd(o, ofs, estado_rotulo):
    rot = sorted(o.rotulos.items(), key=lambda x: x[1])
    ofs_r = [x[1] for x in rot]
    i = bisect.bisect_right(ofs_r, ofs) - 1
    if i < 0:
        return "antes do primeiro rótulo"
    e = estado_rotulo.get(rot[i][0], "rótulo sem tipo")
    if e.startswith(("tipado", "interior")):
        return "depois do fim do tipo do rótulo anterior (" + e.split(" (")[0] + ")"
    return e


def decisao_rastreio(larg, o, idx):
    """A decisão de elfdados.retipar para a diretiva o.dirs[idx] (a mesma regra)."""
    d = o.dirs[idx]
    if d.tipo in ("long", "ptr"):
        w = [larg.get(d.ofs + k, 0) for k in range(4)]
        tipos = {k for k, c in ((1, any(x & 1 for x in w)), (2, w[0] & 2 or w[2] & 2), (4, w[0] & 4),
                                (8, w[0] & 8), (16, w[0] & 16)) if c}
        fora = w[1] & ~1 or w[3] & ~1 or w[2] & ~3
        if len(tipos - {16}) > 1 or fora:
            return "conflito"
        if tipos == {8}:
            prox = o.dirs[idx + 1] if idx + 1 < len(o.dirs) else None
            pw = [larg.get(d.ofs + 4 + k, 0) for k in range(4)]
            if d.ofs % 8 == 0 and prox is not None and prox.tipo in ("long", "ptr") and not any(pw):
                return "u64"
            return "conflito"
        if d.tipo == "ptr" or tipos & {4, 16}:
            return "fica" if tipos else "não lido"
        if not tipos:
            return "não lido"
        return "2x2" if tipos == {2} else "4x1"
    if d.tipo == "short":
        lido = larg.get(d.ofs, 0) | larg.get(d.ofs + 1, 0)
        if lido & 1 and not lido & ~1:
            return "2x1"
        if lido & 1 and lido & ~1 or lido & ~3 or larg.get(d.ofs + 1, 0) & ~1:
            return "conflito"
        return "fica" if lido else "não lido"
    lido = 0
    for b in range(d.ofs, d.ofs + d.tam):
        lido |= larg.get(b, 0)
    if not lido:
        return "não lido"
    if d.tipo == "byte":
        return "fica" if lido == 1 else f"lido como {lido & ~1 or 1}"  # elfdados não mexe em .byte
    if d.tipo in ("float", "double"):
        return "fica" if lido & (d.tam | 16) and not lido & ~(d.tam | 16) else f"lido como máscara {lido}"
    return "fica" if lido & 1 and not lido & ~1 else f"lido como máscara {lido}"


def compara(est, ras):
    """Categoria de uma diretiva: (estática, rastreio)."""
    if ras == "não lido":
        return "só estática" if est not in ("nd",) else "nenhum"
    if est == "nd":
        return "só rastreio"
    norm = {"fica?": "fica"}
    e = norm.get(est, est)
    if e == ras:
        return "concordam"
    if ras == "conflito":
        return "estática decide conflito do rastreio"
    return "discordam"


# --------------------------------------------------------------------------------------------------- main

def exportar_trocas(objetos, mapas, vivo_byte, caminho):
    """O mapa do host big-endian (decisão 13): cada posição viva de número com mais de um byte cujo valor vem do
    disco (`.space` do esqueleto port/data, que Port_LoadGameData copia do SLUS), uma por linha: objeto,
    deslocamento, largura. Só posições e larguras, nenhum byte do jogo. Ficam de fora: o morto, o que é zero no PC
    (`.zero`) e os `.long Símbolo`, que o linker escreve na ordem do host. Em ARQ.sem_tipo, os trechos vivos que vêm
    do disco sem tipo no C (ficam como vieram). Devolve (contagens, erros)."""
    cont, erros = collections.Counter(), []
    with open(caminho, "w") as f, open(str(caminho) + ".sem_tipo", "w") as fs:
        for o in objetos.values():
            mp, vivo = mapas[o.nome], vivo_byte[o.nome]
            marca = bytearray(o.tam)  # 0 zero no PC (.zero, alinhamento), 1 vem do disco, 2 ponteiro do linker
            for ofs, n in o.copia:
                marca[ofs:ofs + n] = b"\x01" * n
            for d in o.dirs:
                if d.tipo == "ptr":
                    marca[d.ofs:d.ofs + 4] = b"\x02" * 4
            cf = sorted(b for b in mp.conflitos if vivo[b] and marca[b] == 1)
            if cf:  # larguras diferentes no mesmo byte vivo que vem do disco: não há troca certa
                erros.append(f"{o.nome}+{cf[0]:#x}: {len(cf)} bytes vivos em conflito ({sorted(mp.conflitos[cf[0]])[0][:160]})")
            coberto = bytearray(o.tam)
            for a, w, kk, desc, fonte, _ in sorted(mp.esc):
                coberto[a:a + w] = b"\x01" * w
                if w < 2:
                    continue
                if not any(vivo[a:a + w]):
                    cont["morto"] += 1
                    continue
                m = set(marca[a:a + w])
                if m == {0}:
                    cont["zero no PC (.zero)"] += 1
                elif m == {2}:
                    cont["ponteiro do linker (.long Símbolo)"] += 1
                elif m != {1}:
                    erros.append(f"{o.nome}+{a:#x} [{w}] {desc}: parte vem do disco, parte não ({sorted(m)})")
                elif kk not in ("int", "float", "bits", "ptr") or w not in (2, 4, 8):
                    erros.append(f"{o.nome}+{a:#x} [{w}] {desc}: tipo {kk}")
                elif w == 8 and PACOTES_GPU.match(desc):
                    f.write(f"{o.nome}\t{a}\t4\n{o.nome}\t{a + 4}\t4\n")
                    cont["trocar: 8 bytes de pacote da GPU, como dois u32 (decisão 19)"] += 1
                else:
                    f.write(f"{o.nome}\t{a}\t{w}\n")
                    cont[f"trocar: {w} bytes ({'ponteiro numérico' if kk == 'ptr' else kk})"] += 1
            # o que fica como veio sem o C dizer o tipo, em trechos (ARQ.sem_tipo: objeto, deslocamento, tamanho, rótulo)
            rot = sorted(o.rotulos.items(), key=lambda x: x[1])
            b = 0
            while b < o.tam:
                if vivo[b] and marca[b] == 1 and not coberto[b]:
                    e = b
                    while e < o.tam and vivo[e] and marca[e] == 1 and not coberto[e]:
                        e += 1
                    i = bisect.bisect_right([x[1] for x in rot], b) - 1
                    lugar = f"{rot[i][0]}+{b - rot[i][1]:#x}" if i >= 0 else ""
                    fs.write(f"{o.nome}\t{b}\t{e - b}\t{lugar}\n")
                    cont["bytes vivos do disco sem tipo (ficam como vieram)"] += e - b
                    b = e
                else:
                    b += 1
    return cont, erros


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--dados", default=str(ROOT / "port/build/gen/data"))
    ap.add_argument("--obj", default=str(ROOT / "port/build/x86/obj"), help="pasta dos .i da build x86")
    ap.add_argument("--rastreio", default=str(ROOT / "port/build/x86r/rastreio.tsv"))
    ap.add_argument("--prog-rastreio", default=str(ROOT / "port/build/x86r/bt3_x86r"))
    ap.add_argument("--asm", default=str(ROOT / "asm"), help="asm do splat (só para saber quem referencia o rótulo)")
    ap.add_argument("--saida", default="/tmp/elftipos")
    ap.add_argument("-j", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    ap.add_argument("--sem-rastreio", action="store_true")
    ap.add_argument("--trocas", help="grava o mapa do host big-endian (com --dados port/data): objeto, deslocamento, "
                    "largura de cada posição viva que vem do disco; falha se houver posição sem troca certa")
    a = ap.parse_args()
    saida = pathlib.Path(a.saida)
    (saida / "sondas").mkdir(parents=True, exist_ok=True)

    objetos = {}
    for s in sorted(pathlib.Path(a.dados).glob("*.s")):
        o = Objeto(s)
        objetos[o.nome] = o
    onde = {}
    for o in objetos.values():
        for r, ofs in o.rotulos.items():
            onde.setdefault(r, (o.nome, ofs))
    nomes = frozenset(onde)
    print(f"{len(objetos)} objetos, {len(nomes)} rótulos", file=sys.stderr)

    # 2. AST
    ii = sorted(pathlib.Path(a.obj).glob("*.i"))
    decls, visoes = collections.defaultdict(list), collections.defaultdict(list)
    genericas, deslocadas = collections.Counter(), []
    with concurrent.futures.ProcessPoolExecutor(a.j) as ex:
        for n, (i, ds, vs, gs, dl) in enumerate(ex.map(varrer, [(i, nomes) for i in ii])):
            for nome, q, esc, arq in ds:
                decls[nome].append((q, esc, arq, i))
            for nome, q, k, arq in vs:
                visoes[nome].append((q, k, arq, i))
            genericas.update(gs)
            deslocadas += dl
            print(f"\rAST {n + 1}/{len(ii)}", end="", file=sys.stderr)
    print(file=sys.stderr)

    # 3. sondas: um pedido por (rótulo, tipo), no primeiro .i que o declara
    pedidos = collections.defaultdict(list)
    vistos = set()
    for nome, lst in decls.items():
        for q, esc, arq, i in lst:
            chave = ("decl", nome, q)
            if chave in vistos:
                continue
            vistos.add(chave)
            pedidos[i].append((chave, nome if esc == "arquivo" else q))
    for nome, lst in visoes.items():
        for q, k, arq, i in lst:
            chave = ("visao", nome, q)
            if chave not in vistos:
                vistos.add(chave)
                pedidos[i].append((chave, f"*({q})0"))
            if k and ("elem", nome) not in vistos:
                vistos.add(("elem", nome))
                pedidos[i].append((("elem", nome, ""), f"{nome}[0]"))
    layouts, apontados, caidas = {}, {}, []
    with concurrent.futures.ProcessPoolExecutor(a.j) as ex:
        trab = [(i, p, str(saida / "sondas")) for i, p in pedidos.items()]
        for n, (nome_i, res, ap_, cd) in enumerate(ex.map(sondar, trab)):
            layouts.update(res)
            for k, v in ap_.items():
                apontados.setdefault(k, v)
            caidas += [(nome_i,) + c for c in cd]
            print(f"\rsondas {n + 1}/{len(trab)}", end="", file=sys.stderr)
    print(file=sys.stderr)

    # 4. candidatas e atribuição
    cands = collections.defaultdict(list)
    visoes_tipos = collections.defaultdict(set)
    u8_confiaveis = set()
    for nome, lst in decls.items():
        for q in dict.fromkeys(q for q, _, _, _ in lst):
            lay = layouts.get(("decl", nome, q))
            if lay and lay["generico"] and nome not in genericas and nome not in visoes and \
                    all(k == "int" and w == 1 for _, w, k, _, _ in (lay.get("esc") or lay["elem"][1])):
                # vetor de u8 que nenhum código converte para outro tipo (nem para void*): os bytes são bytes
                lay = dict(lay, generico=False)
                u8_confiaveis.add(nome)
            if lay:
                cands[nome].append(("decl", ("decl", q), lay, 0))
                visoes_tipos[nome].add(("decl", q))
    for nome, lst in visoes.items():
        for q, k, _, _ in lst:
            lay = layouts.get(("visao", nome, q))
            if not lay:
                continue
            desl = 0
            if k:
                el = layouts.get(("elem", nome, ""))
                if not el or not el.get("completo"):
                    deslocadas.append((nome, q, k))
                    continue
                desl = k * el["tam"]
            chave = ("visao", q, desl)
            if not any(c[1] == chave for c in cands[nome]):
                cands[nome].append(("visao", chave, lay, desl))
                visoes_tipos[nome].add(("visao", q))
    rodadas, sem_tipo = 0, collections.Counter()
    while True:
        mapas, notas_atr = atribuir(objetos, cands)
        novos, sem_tipo = propagar(objetos, mapas, cands, apontados, onde)
        rodadas += 1
        if not novos or rodadas >= 8:
            break

    # estado de cada rótulo (para o motivo do "nd")
    ref_asm = set()
    pasta_asm = pathlib.Path(a.asm)
    if pasta_asm.is_dir():
        padrao = re.compile(r"\b(?:D_[0-9A-F]{8}|[A-Za-z_]\w*)\b")
        for f in list((pasta_asm / "cod").glob("*.s")) + list((pasta_asm / "nonmatchings").rglob("*.s")):
            ref_asm |= set(padrao.findall(f.read_text(errors="replace"))) & nomes
    alvo_de = set()
    for o in objetos.values():
        for d in o.dirs:
            if d.tipo == "ptr" and d.sim and d.sim[0] in nomes:
                alvo_de.add(d.sim[0])
    estado_rotulo = {}
    for o in objetos.values():
        mp = mapas[o.nome]
        for r, ofs in o.rotulos.items():
            if cands.get(r):
                fontes = {c[0] for c in cands[r]}
                gen = all(c[2]["generico"] and c[0] != "alvo" for c in cands[r])
                estado_rotulo[r] = "rótulo só com declaração genérica" if gen else "tipado (" + "+".join(sorted(fontes)) + ")"
                continue
            dono = mp.dono[ofs] if ofs < o.tam else None
            if dono is not None and dono != r:
                estado_rotulo[r] = f"interior de {dono}"
            elif r in notas_atr["cortes"] and r not in sem_tipo:
                estado_rotulo[r] = "sem declaração em C; logo depois de um vetor sem tamanho (pode ser o mesmo vetor)"
            elif r in sem_tipo:
                estado_rotulo[r] = "alvo de ponteiro sem tipo (void*, u8* ou função)"
            elif r in ref_asm:
                estado_rotulo[r] = "sem declaração em C; referenciado por assembly não decompilado"
            elif r in alvo_de:
                estado_rotulo[r] = "sem declaração em C; alvo de ponteiro de tabela sem tipo"
            else:
                estado_rotulo[r] = "sem declaração em C e sem referência"

    # alcance no port: rótulos que o código do port referencia (símbolos indefinidos dos objetos C da build x86) e o
    # que se alcança deles por ponteiro. O resto é dado que só o código não compilado pelo port usava (assembly
    # sem C, middleware trocado pelo port): o valor não importa no 360.
    usados = set()
    objs_c = sorted(str(f) for f in pathlib.Path(a.obj).glob("*.o"))
    for j in range(0, len(objs_c), 200):
        for l in subprocess.run(["nm", "-u"] + objs_c[j:j + 200], capture_output=True, text=True).stdout.splitlines():
            x = l.split()
            if x and not l.endswith(":"):
                usados.add(x[-1])
    ext, ptrs = {}, {}
    for o in objetos.values():
        mp = mapas[o.nome]
        ultimo = {}
        for b, dn in enumerate(mp.dono):
            if dn:
                ultimo[dn] = b + 1
        rot = sorted(o.rotulos.items(), key=lambda x: x[1])
        for j, (r, ofs) in enumerate(rot):
            prox = next((x[1] for x in rot[j + 1:] if x[1] > ofs), o.tam)
            ext[r] = (o.nome, ofs, max(prox, ultimo.get(r, 0)))
        ptrs[o.nome] = [(d.ofs, d.sim[0]) for d in o.dirs if d.tipo == "ptr" and d.sim and d.sim[0] in nomes]
    vivos = {r for r in nomes if r in usados}
    fila = list(vivos)
    while fila:
        on, ini, fim = ext[fila.pop()]
        for ofs, s in ptrs[on]:
            if ini <= ofs < fim and s not in vivos:
                vivos.add(s)
                fila.append(s)
    vivo_byte = {o.nome: bytearray(o.tam) for o in objetos.values()}
    for r in vivos:
        on, ini, fim = ext[r]
        vivo_byte[on][ini:fim] = b"\x01" * (fim - ini)

    # 5. decisões, cobertura e comparação
    larg = elfdados.larguras_elf(a.rastreio) if not a.sem_rastreio and os.path.exists(a.rastreio) else {}
    pcs = collections.defaultdict(set)
    if larg:
        for l in open(a.rastreio, encoding="utf-8", errors="replace"):
            c = l.rstrip("\n").split("\t")
            if c[0].startswith("ELF:"):
                for p in c[5].split(",") + (c[9].split(",") if len(c) > 9 else []):
                    if p and int(p, 16):
                        pcs[(c[0][4:].rsplit(".", 1)[0], int(c[1]))].add(int(p, 16))
    funcao = None
    if pcs and os.path.exists(a.prog_rastreio):
        nm = subprocess.run(["nm", "-n", a.prog_rastreio], capture_output=True, text=True).stdout
        tab = [(int(x[0], 16), x[2]) for x in (l.split() for l in nm.splitlines()) if len(x) == 3 and x[1] in "tT"]
        ends = [x[0] for x in tab]

        def funcao(pc):
            i = bisect.bisect_right(ends, pc) - 1
            return tab[i][1] if i >= 0 else f"{pc:08x}"

    cob = collections.defaultdict(collections.Counter)
    acoes = collections.Counter()
    nd_motivos = collections.Counter()
    comp = collections.Counter()
    comp_obj = collections.defaultdict(collections.Counter)
    detalhes = collections.defaultdict(list)
    ponteiro_numerico, enchimento_nao_nulo = 0, 0
    with open(saida / "diretivas.tsv", "w") as fd, open(saida / "mapa.tsv", "w") as fm:
        for o in objetos.values():
            mp = mapas[o.nome]
            for e in sorted(mp.esc):
                fm.write(f"{o.nome}\t{e[0]}\t{e[1]}\t{e[2]}\t{e[4]}\t{e[3]}\n")
            lo = larg.get(o.nome, {})
            rot = sorted(o.rotulos.items(), key=lambda x: x[1])
            ofs_r = [x[1] for x in rot]
            ras_ant = None
            for idx, d in enumerate(o.dirs):
                est, porque = decisao_estatica(mp, d, estado_rotulo)
                ras = decisao_rastreio(lo, o, idx) if lo is not None else "não lido"
                if ras_ant == "u64":
                    ras = "u64-alto"
                ras_ant = ras
                if "PONTEIRO NUMÉRICO" in porque:
                    ponteiro_numerico += 1
                if "NÃO NULO" in porque:
                    enchimento_nao_nulo += 1
                acoes[(d.tipo, est)] += 1
                # cobertura por byte
                vivo = any(vivo_byte[o.nome][d.ofs:d.ofs + d.tam])
                if est == "zero":
                    k = "zero (.space)"
                elif not vivo:
                    k = "morto no port"
                elif est in ("nd", "conflito"):
                    k = f"{est}: " + ("valor zero" if d.zero else "RISCO")
                    if est == "nd":
                        nd_motivos[porque.split(":")[0] if porque.startswith("parcial") else porque] += d.tam
                elif d.tipo == "ptr" or porque.startswith(("ptr", "PONTEIRO")):
                    k = "ponteiro"
                elif est == "fica?":
                    k = "float só pelo .s"
                elif "(do .s)" in porque:
                    k = "texto só pelo .s"
                elif porque.startswith("enchimento"):
                    k = "enchimento"
                else:
                    k = "tipado pelo C"
                cob[o.nome][k] += d.tam
                cob[o.nome]["total"] += d.tam
                if d.tipo in ("space", "incbin"):
                    continue
                cat = compara(est, ras)
                comp[cat] += 1
                comp_obj[o.nome][cat] += 1
                i = bisect.bisect_right(ofs_r, d.ofs) - 1
                lugar = f"{rot[i][0]}+{d.ofs - rot[i][1]:#x}" if i >= 0 else f"{d.ofs:#x}"
                vivo = "vivo" if any(vivo_byte[o.nome][d.ofs:d.ofs + d.tam]) else "morto"
                fd.write(f"{o.nome}\t{d.ofs}\t{d.tipo}\t{lugar}\t{est}\t{ras}\t{cat}\t{porque}\t{vivo}\t{int(d.zero)}\t"
                         f"{mp.dono[d.ofs] if d.ofs < o.tam else ''}\n")
                if cat in ("discordam", "só rastreio", "estática decide conflito do rastreio"):
                    lidos = sorted({(b - d.ofs, w) for b in range(d.ofs, d.ofs + d.tam)
                                    for w in (1, 2, 4, 8, 16) if lo.get(b, 0) & w})
                    fs = sorted({funcao(p) for b in range(d.ofs - 7, d.ofs + d.tam) for p in pcs.get((o.nome, b), ())}) \
                        if funcao else []
                    detalhes[cat].append(f"{o.nome} {lugar} [{d.tipo}] estática={est} ({porque[:160]}) "
                                         f"rastreio={ras} lidos={lidos} por {', '.join(fs[:4])}")

    # relatório
    with open(saida / "relatorio.txt", "w") as f:
        def p(*x):
            print(*x, file=f)
        p("== Cobertura por objeto (bytes) ==")
        cats = ["zero (.space)", "morto no port", "tipado pelo C", "ponteiro", "enchimento", "texto só pelo .s",
                "float só pelo .s", "conflito: valor zero", "conflito: RISCO", "nd: valor zero", "nd: RISCO"]
        p("colunas: " + " | ".join(f"{i}={c}" for i, c in enumerate(cats)))
        p("objeto".ljust(30) + "total".rjust(8) + "".join(str(i).rjust(8) for i in range(len(cats))))
        tot = collections.Counter()
        for nome in sorted(cob, key=lambda n: -cob[n]["total"]):
            c = cob[nome]
            tot.update(c)
            p(nome.ljust(30) + str(c["total"]).rjust(8) + "".join(str(c[k]).rjust(8) for k in cats))
        p("TOTAL".ljust(30) + str(tot["total"]).rjust(8) + "".join(str(tot[k]).rjust(8) for k in cats))
        p(f"rótulos referenciados pelo código C do port: {len(vivos & usados)}; alcançados por ponteiro: "
          f"{len(vivos - usados)}; mortos no port: {len(nomes - vivos)}")
        p("\n== Bytes vivos não determinados, por motivo ==")
        for m, n in nd_motivos.most_common(40):
            p(f"{n:8d}  {m}")
        p("\n== Decisões por diretiva (tipo no .s, decisão estática) ==")
        for (t, e), n in sorted(acoes.items(), key=lambda x: -x[1]):
            p(f"{n:8d}  .{t:6s} -> {e}")
        p(f"\nponteiros numéricos não nulos em campo ponteiro: {ponteiro_numerico}; enchimento não nulo: {enchimento_nao_nulo}")
        p("\n== Rótulos ==")
        est_cnt = collections.Counter(re.sub(r"interior de \S+", "interior de outro rótulo", v) for v in estado_rotulo.values())
        for k, n in est_cnt.most_common():
            p(f"{n:6d}  {k}")
        p(f"declarados em C: {len(decls)} rótulos ({sum(len(v) for v in decls.values())} declarações em {len(ii)} .i); "
          f"com visão por cast: {len(visoes)}; usos com cast genérico (void*/u8*): {sum(genericas.values())} "
          f"em {len(genericas)} rótulos; visões com deslocamento não resolvido: {len(deslocadas)}; "
          f"vetores de u8 sem nenhum cast (tratados como bytes): {len(u8_confiaveis)}")
        p(f"sondas: {len(layouts)} layouts, {len(apontados)} tipos apontados; caídas: {len(caidas)}; rodadas de propagação: {rodadas}")
        for c in caidas[:20]:
            p(f"  sonda caída: {c}")
        p("notas da atribuição: " + ", ".join(f"{k} {v if k != 'cortes' else len(v)}" for k, v in notas_atr.items()))
        notas_lay = collections.Counter()
        for lay in layouts.values():
            notas_lay.update(lay["notas"])
        p("notas dos layouts: " + ", ".join(f"{k} {v}" for k, v in notas_lay.items()))
        p("\n== Rótulos com mais de um tipo (declarações diferentes ou visões) ==")
        for nome in sorted(visoes_tipos):
            if len(visoes_tipos[nome]) > 1:
                p(f"  {nome} ({onde[nome][0]}): " + " | ".join(f"{s} {q}" for s, q in sorted(visoes_tipos[nome])))
        p("\n== Conflitos (bytes) ==")
        cf_obj = collections.Counter()
        cf_desc = collections.Counter()
        for o in objetos.values():
            for b, s in mapas[o.nome].conflitos.items():
                cf_obj[o.nome] += 1
                for x in s:
                    cf_desc[re.sub(r"\[\d+\]", "[]", x)] += 1
        for k, n in cf_obj.most_common():
            p(f"{n:8d}  {k}")
        for k, n in cf_desc.most_common(40):
            p(f"   {n:6d}  {k[:230]}")
        p("\n== Comparação com o rastreio (diretivas numéricas, decisão de elfdados.retipar) ==")
        for k, n in comp.most_common():
            p(f"{n:8d}  {k}")
        for nome in sorted(comp_obj, key=lambda n: -sum(v for k, v in comp_obj[n].items() if k != "nenhum")):
            c = comp_obj[nome]
            lidas = sum(v for k, v in c.items() if k not in ("só estática", "nenhum"))
            if lidas:
                p(f"  {nome:34s} " + ", ".join(f"{k} {v}" for k, v in c.most_common()))
        for cat in ("discordam", "estática decide conflito do rastreio", "só rastreio"):
            p(f"\n-- {cat}: {len(detalhes[cat])} --")
            for x in detalhes[cat][:300]:
                p("  " + x)
    print(open(saida / "relatorio.txt").read())
    if a.trocas:
        cont, erros = exportar_trocas(objetos, mapas, vivo_byte, a.trocas)
        print("trocas: " + ", ".join(f"{k} {v}" for k, v in sorted(cont.items())))
        for e in erros[:40]:
            print("ERRO trocas: " + e)
        if erros:
            sys.exit(f"{len(erros)} posições sem troca certa (o mapa {a.trocas} está incompleto)")


if __name__ == "__main__":
    main()
