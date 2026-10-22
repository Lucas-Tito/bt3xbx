#!/usr/bin/env python3
"""F2: prepara cada fonte do jogo e do port para o cl.exe do XDK, que em modo C (-TC) é C89 e não conhece as extensões
do GCC.

    c89.py SAIDA [--grupo G] [FONTE...]   (na imagem bt3xbx-ferramentas, na raiz do repositório)

Sem FONTE, todos os fontes da build do 360 (F2), por grupo, como a build PowerPC da F1
(port/x360/tools/headless.py): o jogo, a biblioteca matemática do PS2 (newlib), o GS (com a gravação do quadro para o
backend D3D9 desde a F3), a camada do port e os stubs sem janela. O que o 360 substitui por versão própria (port/x360/src) fica de fora.

Para cada fonte do jogo (a lista e a preparação do port de PC: port/tools/portsrc.py), escreve SAIDA/<nome>.c, um
C autocontido que o cl compila sem headers:
  1. pré-processa com o clang para PowerPC de 32 bits big-endian, com os headers mínimos de port/x360/shim (os
     nomes e tipos da CRT do Xbox) e as definições da build do 360 (PORT, PORT_BIG_ENDIAN, PORT_X360);
  2. aplica as constantes de float do compilador do PS2 (port/tools/eeconst.py, como o port de PC);
  3. reescreve o que o cl não aceita: constantes de float em hexadecimal viram decimais com dígitos bastantes para
     dar o mesmo float; marcas de linha do GCC viram #line (os erros e o .pdb apontam para o fonte original).
O código do jogo não muda: a reescrita é só sobre o texto pré-processado.
"""
import concurrent.futures, json, os, pathlib, re, struct, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/tools"))
import eeconst, portsrc  # noqa: E402

CLANG = os.environ.get("CLANG", "clang")
DEFS = ["-DPORT_BIG_ENDIAN", "-DPORT_X360", "-D_XBOX"]
I = lambda p: "-I" + str(ROOT / p)  # noqa: E731

# O que o 360 troca por versão própria em port/x360/src (o resto de port/src e de port/src/gs entra como está).
SUBSTITUIDOS = {"plat_crash", "plat_libm", "softfloat_ps2", "plat_fastvec", "state"}
GS_SEM_JANELA = {"gs_core", "gs_sony", "gs_vu1", "namefont", "snd_adx", "snd_se"}  # e state.c (substituído)
# F3, etapa 7: a gravação do quadro para o backend D3D9 do 360 (port/x360/src/gs/gs_d3d.cpp). Compila com o pedaço
# de SDL que ela cita (port/x360/src/gs/sdl) e tira as entradas GsGpu_* vazias do sem_janela.c (X360_GS_D3D).
GS_D3D = {"gs_draw"}


def renomes(f):
    """O linker da Microsoft não tem --wrap: o embrulho de main e de Progress_Main (port/src/plat_mem.c e
    port/src/headless.c no PC) vira renomeação na preparação."""
    rel = str(f.relative_to(ROOT))
    if rel == "src/main.c":
        return ["-Dmain=__real_main", "-DProgress_Main=__wrap_Progress_Main"]
    if rel == "src/menu/menu_a_b.c":
        return ["-DProgress_Main=__real_Progress_Main"]
    if rel == "src/battle/battle.c":
        # o golpe do passo "termina luta" dos roteiros (port/x360/headless/roteiro.c, F4 etapa 9a; headless.py:
        # --wrap=BtlChars_SampleInput): só a chamada do Battle_Loop passa pelo embrulho, que chama a original
        return ["-DBtlChars_SampleInput=__wrap_BtlChars_SampleInput"]
    return []


def grupos():
    """[(nome do grupo, fonte, flags do clang, aplica eeconst)] da build sem janela do 360."""
    out = []
    for f in portsrc.sources():
        out.append(("", f, ["-std=gnu89", "-include", "port_libm.h", I("include"), I("port/include"),
                            "-include", "port_compat.h", "-I" + str(f.parent)] + renomes(f), True))
    for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c")):
        out.append(("nl_", f, ["-std=gnu89", I("port/include"), "-I" + str(f.parent), "-include", "newlib_shim.h"], True))
    for f in sorted((ROOT / "port/src/gs").glob("*.c")):
        if f.stem in GS_SEM_JANELA:
            # (F6, etapa 3) BT3_SOM360: o áudio da SDL3/SDL.h vira a camada do 360 (port/x360/src/som), não os stubs
            out.append(("gs_", f, ["-std=gnu99", "-DBT3_SOM360", I("port/x360/headless"), I("port/src/gs")], False))
        elif f.stem in GS_D3D:
            out.append(("gs_", f, ["-std=gnu99", I("port/x360/src/gs/sdl"), I("port/x360/headless"), I("port/src/gs")],
                        False))
    vu0 = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"]
    for f in [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"] + sorted((ROOT / "port/src").glob("*.c")):
        if f.stem in SUBSTITUIDOS:
            continue
        nomes = f.parent.name == "port" and f.parent.parent.name == "src"
        # port/x360/src/hw antes de port/src: o softfloat_ps2_inl.h do 360 faz a aritmética pela FPU
        base = ["-std=gnu99", I("include"), I("port/include"), I("port/x360/src/hw"), I("port/src"),
                I("port/x360/headless")]
        extra = vu0 if nomes else ["-DSF_FAST_CALLS", "-include", "math.h", "-include", "stdlib.h",
                                   "-include", "port_libm.h"]
        out.append(("pc_", f, base + extra, not nomes))
    out.append(("hl_", ROOT / "port/x360/headless/sem_janela.c", ["-std=gnu99", "-DX360_GS_D3D", I("port/x360/headless")],
                False))
    # o roteiro de controle (F4, etapa 4): o mesmo leitor das builds sem janela
    # (com os tipos do jogo, port_compat.h, como os fontes dele: o roteiro lê o save pela estrutura, sys/save.h)
    out.append(("hl_", ROOT / "port/x360/headless/roteiro.c",
                ["-std=gnu99", I("include"), I("port/include"), "-include", "port_compat.h", I("port/x360/headless")],
                False))
    # (F5, etapa 3) o tempo por passo da luta e os descartes da janela medida, com o carimbo do commit no log
    # (BT3_CARIMBO, que o build_f2.sh passa: o mesmo formato do headless.py)
    carimbo = os.environ.get("BT3_CARIMBO", "sem carimbo").strip().replace('"', "")
    out.append(("hl_", ROOT / "port/x360/headless/passos.c", ["-std=gnu99", f'-DBT3_CARIMBO="{carimbo}"'], False))
    return out


def preprocessar(src, f, flags):
    cmd = [CLANG, "-E", "--target=powerpc-unknown-unknown", "-m32", "-fsigned-char", "-nostdinc",
           "-isystem", str(ROOT / "port/x360/shim"), "-fno-builtin"] + DEFS + flags + [str(src)]
    r = subprocess.run(cmd, capture_output=True, encoding="utf-8", cwd=ROOT)
    if r.returncode:
        raise RuntimeError(r.stderr.strip()[-600:])
    return r.stdout


HEXF = re.compile(r"\b0[xX]([0-9a-fA-F]+)p([+-]?\d+)([fF]?)\b")


def decimal(m):
    """0x<m>p<e>[f] -> o mesmo valor em decimal: 9 dígitos significativos voltam ao mesmo float, 17 ao mesmo double."""
    v = int(m.group(1), 16) * 2.0 ** int(m.group(2))
    if m.group(3):
        f = struct.unpack(">f", struct.pack(">f", v))[0]
        if f != v:
            raise ValueError(f"constante {m.group(0)} não cabe num float")
        t = "%.9g" % f
        return (t if any(c in t for c in ".e") else t + ".0") + "f"
    t = "%.17g" % v
    return t if any(c in t for c in ".e") else t + ".0"


MARCA = re.compile(r'^# (\d+) "([^"]*)"[ 0-9]*$', re.M)


def linhas(texto):
    return MARCA.sub(lambda m: f'#line {m.group(1)} "{m.group(2)}"', texto)


# ------------------------------------------------------------------ reescrita pelo AST

# Prelúdio de cada .c: o que o cl precisa saber antes do código do jogo.
PRELUDIO = """/* Gerado por port/x360/tools/c89.py a partir do fonte do jogo: não editar. */
#pragma bitfield_order(lsb_to_msb) /* campos de bits do bit 0 para cima, como o compilador do PS2 (decisão 7) */
#pragma fp_contract(off)           /* uma operação por vez: sem fmadds (decisão 4) */
#define inline __inline
double fabs(double);
#define __builtin_fabsf(x) ((float)fabs((double)(x)))
#define __builtin_memcpy memcpy
#define __builtin_abs abs
void *memset(void *, int, unsigned int);
/* os atômicos do GCC que o port usa (port/x360/src/posix360.c) */
int __sync_lock_test_and_set(volatile int *, int);
void __sync_lock_release(volatile int *);
int __sync_bool_compare_and_swap(volatile int *, int, int);
void __sync_synchronize(void);
"""


def ast(texto):
    """O AST JSON do clang do texto pré-processado. As posições são deslocamentos em BYTES do arquivo; texto vem na
    vista de um caractere por byte (rebaixar), que volta aos mesmos bytes em latin-1."""
    with tempfile.NamedTemporaryFile("wb", suffix=".c", delete=False) as t:
        t.write(texto.encode("latin-1"))
    try:
        r = subprocess.run([CLANG, "--target=powerpc-unknown-unknown", "-m32", "-std=gnu89", "-fsigned-char",
                            "-fsyntax-only", "-w", "-Xclang", "-ast-dump=json", t.name], capture_output=True,
                           encoding="utf-8", errors="replace")
    finally:
        os.unlink(t.name)
    if not r.stdout:
        raise RuntimeError("AST: " + r.stderr.strip()[-600:])
    return json.loads(r.stdout)


def ofs(loc):
    loc = loc.get("expansionLoc", loc)
    return loc.get("offset")


def fim_atributo(texto, i):
    """i aponta para `__attribute__`: o fim do `__attribute__((...))` (parênteses balanceados)."""
    j = texto.index("(", i)
    prof = 0
    for k in range(j, len(texto)):
        if texto[k] == "(":
            prof += 1
        elif texto[k] == ")":
            prof -= 1
            if prof == 0:
                return k + 1
    raise ValueError("__attribute__ sem fechamento")


def fim_de(n):
    """Deslocamento logo depois do último token do nó."""
    e = n["range"]["end"]
    return ofs(e) + e.get("tokLen", 1)


def ini_de(n):
    return ofs(n["range"]["begin"])


class Reescrita:
    """Edições sobre o texto pré-processado, todas por deslocamento: apagar [a, a+n) e inserir texto. Num mesmo
    deslocamento os textos inseridos vão na ordem de `ordem` (o bloco de fora antes do de dentro)."""

    def __init__(self, texto):
        self.texto = texto
        self.apagar = {}
        self.inserir = {}

    def apaga(self, a, b):
        self.apagar[a] = max(self.apagar.get(a, 0), b - a)

    def insere(self, pos, txt, ordem=0):
        self.inserir.setdefault(pos, []).append((ordem, txt))

    def aplica(self):
        t = self.texto
        for pos in sorted(set(self.apagar) | set(self.inserir), reverse=True):
            ins = "".join(x for _, x in sorted(self.inserir.get(pos, []), key=lambda e: e[0]))
            t = t[:pos] + ins + t[pos + self.apagar.get(pos, 0):]
        return t


# Símbolos de endereço fixo que o linker do PC define (headless.py: --defsym): no 360 viram constante.
ABSOLUTOS = {"D_3BE71C": 0x3BE71C}  # fim do overlay do menu no PS2: o tamanho do heap do jogo sai dele (heap.c)

EXPRS_TOPO = {"BinaryOperator", "CompoundAssignOperator", "CallExpr", "UnaryOperator"}


def rebaixar(texto):
    """Reescreve o que o cl (C89) não aceita, com as posições que o AST do clang dá. O clang dá deslocamentos em
    bytes; o texto UTF-8 fica, até o fim, na vista de um caractere por byte (latin-1), para cada deslocamento cair no
    mesmo lugar do Python. Sem isso, um acento numa string ou num comentário desloca toda reescrita que vem depois."""
    texto = texto.encode("utf-8").decode("latin-1")
    raiz = ast(texto)
    r = Reescrita(texto)
    pragmas = []
    vistos = {}
    conta = [0]
    por_id, structs, variaveis, anonima = {}, {}, {}, {}
    absolutos = set()

    def indexar(n):
        if n.get("id"):
            por_id[n["id"]] = n
        if n.get("kind") == "RecordDecl" and n.get("name") and n.get("completeDefinition"):
            structs[n.get("tagUsed", "struct") + " " + n["name"]] = n
        dentro = [c for c in n.get("inner", []) if isinstance(c, dict)]
        for i, c in enumerate(dentro):
            # struct/união sem nome dentro de outra: o clang a declara logo antes do campo que a usa
            if c.get("kind") == "FieldDecl" and i and dentro[i - 1].get("kind") == "RecordDecl" and \
                    ("(anonymous" in c["type"]["qualType"] or "(unnamed" in c["type"]["qualType"]):
                anonima[c.get("id")] = dentro[i - 1]
            indexar(c)

    indexar(raiz)

    def registro(tipo):
        """A RecordDecl de um tipo (pelo typedef ou pelo nome da tag)."""
        if tipo.get("typeAliasDeclId") in por_id:
            pilha = [por_id[tipo["typeAliasDeclId"]]]
            while pilha:
                x = pilha.pop()
                if x.get("kind") == "RecordType" and x.get("decl", {}).get("id") in por_id:
                    return por_id[x["decl"]["id"]]
                pilha.extend(c for c in x.get("inner", []) if isinstance(c, dict))
        q = tipo.get("desugaredQualType", tipo.get("qualType", ""))
        return structs.get(q)

    def atribuicoes(init, alvo, out, reg=None):
        """Uma lista de inicialização (forma semântica do clang) como atribuições campo a campo; o que ela não
        dá fica com o zero do memset."""
        filhos = [c for c in init.get("inner", []) if isinstance(c, dict)]
        tipo = init.get("type", {})
        if init.get("field"):  # união: o campo que ela inicializa
            f = init["field"]
            if filhos:
                um(filhos[0], alvo + "." + f["name"] if f.get("name") else alvo, out, anonima.get(f.get("id")))
            return
        if tipo.get("qualType", "").endswith("]"):
            for i, c in enumerate(filhos):
                um(c, "%s[%d]" % (alvo, i), out)
            return
        reg = reg or registro(tipo)
        if reg is None:
            erro(init, "lista de inicialização de tipo %s sem struct" % tipo.get("qualType"))
        campos = [f for f in reg.get("inner", []) if isinstance(f, dict) and f.get("kind") == "FieldDecl" and
                  not (not f.get("name") and f.get("isBitfield"))]
        for f, c in zip(campos, filhos):
            um(c, alvo + "." + f["name"] if f.get("name") else alvo, out, anonima.get(f.get("id")))

    def um(c, alvo, out, reg=None):
        k = c.get("kind")
        if k == "ImplicitValueInitExpr":
            return
        if k == "InitListExpr":
            atribuicoes(c, alvo, out, reg)
            return
        out.append("%s = %s;" % (alvo, texto[ini_de(c):fim_de(c)]))

    def fim_comando(n):
        """Fim do comando n com o ';' que o fecha, se houver (o range do clang não inclui o ';' de expressões)."""
        f = fim_de(n)
        j = f
        while j < len(texto) and texto[j] in " \t\r\n":
            j += 1
        return j + 1 if j < len(texto) and texto[j] == ";" and n.get("kind") != "CompoundStmt" else f

    def valor_align(o):
        m = re.match(r"aligned\s*\(\s*(\d+)\s*\)", texto[o:o + 40])
        if not m:
            raise ValueError("aligned sem valor em %d: %r" % (o, texto[o:o + 40]))
        return int(m.group(1))

    def erro(n, oque):
        b = n["range"]["begin"]
        raise ValueError("%s em %s:%s" % (oque, b.get("file", "?"), texto[:ofs(b)].count("\n") + 1))

    def comando_em_bloco(caminho):
        """O comando que contém o nó: o filho de um CompoundStmt no caminho, e esse CompoundStmt."""
        for i in range(len(caminho) - 1, 0, -1):
            if caminho[i - 1].get("kind") == "CompoundStmt":
                return caminho[i], caminho[i - 1]
        return None, None

    def literal(n, caminho):
        """(T){...}: como inicializador de declaração vira só {...}; num comando de expressão, uma temporária num
        bloco em volta do comando."""
        a, b = ini_de(n), fim_de(n)
        m = re.match(r"\(\s*([^(){};]*?)\s*\)\s*(\{)", texto[a:b])
        if not m:
            erro(n, "literal composto de forma inesperada")
        tipo, chaves = m.group(1), a + m.start(2)
        pai = caminho[-1]
        if pai.get("kind") == "VarDecl" and pai.get("init") == "c" and caminho[-1]["inner"][-1] is n:
            r.apaga(a, chaves)
            return
        cmd, bloco = comando_em_bloco(caminho)
        if cmd is None or cmd.get("kind") not in EXPRS_TOPO | {"ReturnStmt"}:
            erro(n, "literal composto fora de um comando de expressão")
        conta[0] += 1
        nome = "c89_literal_%d" % conta[0]
        # o cl em C não inicializa campo struct com expressão struct entre chaves: zera e atribui campo a campo
        atrib = []
        atribuicoes(n["inner"][0], nome, atrib)
        r.insere(ini_de(cmd), "{ %s %s; memset(&%s, 0, sizeof(%s)); %s " % (tipo, nome, nome, nome, " ".join(atrib)), 1)
        r.apaga(a, b)
        r.insere(a, nome)
        r.insere(fim_comando(cmd), " }", 0)

    def expr_comando(n, caminho):
        """({ c1; ...; e; }): c1... num bloco antes do comando que a usa, e o valor vira (e)."""
        corpo = n["inner"][0]
        filhos = corpo.get("inner", [])
        if not filhos:
            erro(n, "expressão-comando vazia")
        ult = filhos[-1]
        cmd, bloco = comando_em_bloco(caminho)
        if cmd is None or cmd.get("kind") not in EXPRS_TOPO | {"ReturnStmt", "DeclStmt"}:
            erro(n, "expressão-comando fora de um comando de expressão")
        antes = "".join(texto[ini_de(x):fim_comando(x)] + " " for x in filhos[:-1])
        if cmd.get("kind") == "DeclStmt":
            if antes:
                erro(n, "expressão-comando com comandos dentro de uma declaração")
        else:
            r.insere(ini_de(cmd), "{ " + antes, 1)
            r.insere(fim_comando(cmd), " }", 0)
        a = ini_de(n)
        r.apaga(a, fim_de(n))
        r.insere(a, "(" + texto[ini_de(ult):fim_de(ult)] + ")")
        # o __extension__ que costuma vir antes: some junto
        k = a
        while k > 0 and texto[k - 1] in " \t":
            k -= 1
        if texto[max(0, k - 13):k] == "__extension__":
            r.apaga(k - 13, k)

    def andar(n, caminho):
        k = n.get("kind", "")
        for c in n.get("inner", []):
            if not isinstance(c, dict):
                continue
            if c.get("kind") == "AlignedAttr" and ofs(c["range"]["begin"]) is not None:
                v = valor_align(ofs(c["range"]["begin"]))
                b = n["range"]["begin"]
                if k in ("RecordDecl", "TypedefDecl"):  # struct __declspec(align(N)) / typedef __declspec(align(N))
                    r.insere(ofs(b) + b["tokLen"], " __declspec(align(%d))" % v)
                elif k in ("FieldDecl", "VarDecl"):
                    r.insere(ofs(b), "__declspec(align(%d)) " % v)
                else:
                    erro(n, "aligned em %s" % k)
            elif c.get("kind") == "AsmLabelAttr":
                o = ofs(c["range"]["begin"])
                m = re.match(r'"([A-Za-z_]\w*)"', texto[o:o + 200])
                if m and m.group(1) != n.get("name"):
                    pragmas.append('#pragma comment(linker, "/alternatename:%s=%s")' % (n["name"], m.group(1)))
        if k == "TypedefDecl" and caminho and n.get("name") and ini_de(n) is not None:
            nome, tipo = n["name"], json.dumps(n.get("type", {}))
            if nome in vistos and vistos[nome] == tipo:  # C89 não aceita repetir um typedef, mesmo igual
                j = texto.index(";", fim_de(n))
                r.apaga(ini_de(n), j + 1)
            vistos.setdefault(nome, tipo)
        if k == "VarDecl" and n.get("name") in ABSOLUTOS and n.get("storageClass") == "extern":
            # símbolo de endereço fixo (o --defsym da build de PC): a declaração sai e o nome vira a constante
            j = texto.index(";", fim_de(n))
            r.apaga(ini_de(n), j + 1)
            absolutos.add(n["name"])
        if k == "VarDecl" and n.get("name"):
            variaveis[n["name"]] = (n, caminho[-1] if caminho else None)
        if k == "VarDecl" and "typeof" in n.get("type", {}).get("qualType", ""):
            q = n["type"]["qualType"]
            m = re.match(r"(?:__)?typeof(?:__)?\s*\(\s*(\w+)\s*\)(.*)$", q)
            if not m or m.group(1) not in variaveis:
                erro(n, "typeof de algo que não é uma variável já vista")
            alvo, decl = variaveis[m.group(1)]
            tq = alvo["type"]["qualType"]
            if "(unnamed" in tq or "(anonymous" in tq:  # struct anônima: ganha um nome para o typeof usar
                rec = next((x for x in (decl or {}).get("inner", []) if x.get("kind") == "RecordDecl"), None)
                if rec is None:
                    erro(n, "typeof de struct anônima sem a declaração")
                conta[0] += 1
                tag = "c89_tipo_%d" % conta[0]
                b = rec["range"]["begin"]
                r.insere(ofs(b) + b["tokLen"], " " + tag)
                base = "%s %s" % (rec.get("tagUsed", "struct"), tag)
            else:
                base = tq
            mt = re.search(r"\b(?:__)?typeof(?:__)?\s*\(", texto[ini_de(n):fim_de(n)])
            a = ini_de(n) + mt.start()
            prof, j = 0, ini_de(n) + mt.end() - 1
            while True:
                prof += {"(": 1, ")": -1}.get(texto[j], 0)
                if prof == 0:
                    break
                j += 1
            r.apaga(a, j + 1)
            r.insere(a, base)
        if k == "CompoundStmt":
            comandos = [c for c in n.get("inner", []) if isinstance(c, dict)]
            ja_comando = False
            for c in comandos:
                if c.get("kind") == "DeclStmt" and ja_comando:  # C99: declaração depois de comando
                    r.insere(ini_de(c), "{ ", 0)
                    r.insere(ofs(n["range"]["end"]), "} ", 0)
                elif c.get("kind") != "DeclStmt":
                    ja_comando = True
        if k == "ForStmt":
            dentro = n.get("inner", [])
            if dentro and isinstance(dentro[0], dict) and dentro[0].get("kind") == "DeclStmt":
                d = dentro[0]
                a, b = ini_de(d), fim_de(d)
                decl = texto[a:b].rstrip().rstrip(";")
                r.apaga(a, b if texto[b - 1] != ";" else b - 1)
                r.insere(ini_de(n), "{ %s; " % decl, 0)
                r.insere(fim_comando(n), " }", 0)
        if k == "CaseStmt" and n.get("isGNURange"):
            lhs, rhs = n["inner"][0], n["inner"][1]

            def valor(x):
                t = re.sub(r"[()\s]|[uUlL]+$", "", texto[ini_de(x):fim_de(x)])
                return int(t, 0) if not t.startswith("'") else ord(t[1])
            v1, v2 = valor(lhs), valor(rhs)
            if v2 < v1 or v2 - v1 > 512:
                erro(n, "intervalo de case grande demais")
            dois = texto.index(":", fim_de(rhs))
            r.apaga(ini_de(n), dois + 1)
            r.insere(ini_de(n), " ".join("case %d:" % v for v in range(v1, v2 + 1)))
        if k == "GCCAsmStmt":
            if not re.match(r'(?:__asm__|asm)\s*(?:__volatile__|volatile)?\s*\(\s*""', texto[ini_de(n):fim_de(n)]):
                erro(n, "assembly do GCC com código")
            r.apaga(ini_de(n), fim_comando(n))
        if k == "CompoundLiteralExpr":
            literal(n, caminho)
        if k == "StmtExpr":
            expr_comando(n, caminho)
        for c in n.get("inner", []):
            if isinstance(c, dict):
                andar(c, caminho + [n])

    andar(raiz, [])
    # todo __attribute__((...)) e __asm__("nome") de declaração sai do texto; construtor não pode sumir calado
    # (o C da Microsoft não roda função antes do main: quem precisa é chamado pelo main do 360)
    m = re.search(r"__attribute__\s*\(\(\s*(constructor|destructor)", texto)
    if m:
        raise ValueError("%s: o 360 não roda isso sozinho; chame do main (port/x360/src)" % m.group(1))
    for m in re.finditer(r"\b__attribute__\b", texto):
        r.apaga(m.start(), fim_atributo(texto, m.start()))
    for m in re.finditer(r'\b__asm__\s*\(\s*"[A-Za-z_]\w*"\s*\)', texto):
        r.apaga(m.start(), m.end())
    defs = "".join("#define %s ((unsigned char *)0x%X)\n" % (n, ABSOLUTOS[n]) for n in sorted(absolutos))
    return PRELUDIO + defs + "\n".join(sorted(set(pragmas))) + "\n" + r.aplica().encode("latin-1").decode("utf-8")


FMAX_LIT = re.compile(r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'|(?<![\w.])(\d+\.\d*|\.\d+)[eE]\+?38[fF]\b')


def float_max(texto):
    """Literal float acima do maior float que arredonda para ele (3.4028235e38f): o cl 2010 recusa o decimal acima de
    FLT_MAX; vira o valor exato do float. Strings ficam como estão."""
    def troca(m):
        if m.group(1) is None:
            return m.group(0)
        v = struct.unpack(">f", struct.pack(">f", float(m.group(0)[:-1])))[0]
        if abs(v) == 3.4028234663852886e38:  # o FLT_MAX do float.h da Microsoft: abaixo do exato, mesmo float
            return "3.402823466e+38f"
        return "%.9gf" % v
    return FMAX_LIT.sub(troca, texto)


def preparar(grupo, f, flags, ee, saida):
    src = portsrc.prepare(f)[0] if f.is_relative_to(ROOT / "src") and f.parent.name != "port" else f
    texto = preprocessar(src, f, flags)
    if ee:
        texto = eeconst.transform(texto)
    texto = rebaixar(texto)
    texto = HEXF.sub(decimal, texto)
    texto = float_max(texto)
    texto = linhas(texto)
    nome = grupo + (str(f.relative_to(ROOT / "src")).replace("/", "_") if not grupo else f.name)
    (saida / nome).write_text(texto, encoding="utf-8")
    return nome


def um(args):
    grupo, f, flags, ee, saida = args
    try:
        return f, preparar(grupo, f, flags, ee, saida), None
    except Exception as e:  # noqa: BLE001 (o relatório diz qual fonte e por quê)
        return f, None, str(e)


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    saida = ROOT / a[0]
    saida.mkdir(parents=True, exist_ok=True)
    so = a[a.index("--grupo") + 1] if "--grupo" in a else None
    nomes = [x for x in a[1:] if not x.startswith("--") and x != so]
    todos = [g for g in grupos() if (so is None or g[0] == so) and (not nomes or str(g[1].relative_to(ROOT)) in nomes)]
    with concurrent.futures.ProcessPoolExecutor(os.cpu_count()) as ex:
        res = list(ex.map(um, [g + (saida,) for g in todos]))
    falhas = [(f, e) for f, n, e in res if e]
    for f, e in falhas:
        print(f"FALHOU {f.relative_to(ROOT)}: {e[:300]}")
    print(f"c89: {len(res) - len(falhas)} fontes preparados em {saida.relative_to(ROOT)}, {len(falhas)} falhas")
    sys.exit(1 if falhas else 0)


if __name__ == "__main__":
    main()
