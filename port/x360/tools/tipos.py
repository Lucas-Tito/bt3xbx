#!/usr/bin/env python3
"""Decisão 8: liga cada leitura do rastreio (rastreio.c) ao tipo e campo do código que a fez.

    tipos.py PROG_X86R RASTREIO.tsv -o TIPOS.tsv [--arquivo SUBSTR]

Para cada (arquivo do disco, deslocamento) lido como número: o ponto do código (pc da chamada de
instrumentação, que vem logo antes do acesso) vira arquivo:linha:coluna pelo DWARF (llvm-symbolizer, o quadro
mais interno, para funções inline); o AST do clang do .i daquele fonte diz que expressão termina nessa coluna:
um campo de struct (MemberExpr: struct e campo), um elemento de vetor ou uma derreferência de ponteiro (com o
tipo lido). A largura em tempo de execução tem de ser o tamanho do tipo: quando não é, a linha sai marcada.

Saída: arquivo, deslocamento, larguras lidas, tipo.campo (ou "*tipo"), tipo do valor, local no código,
"ok" ou o motivo da dúvida. É o catálogo de que os conversores por formato partem.
"""
import collections, json, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
CLANG = os.environ.get("CLANG", "clang")
SYMB = os.environ.get("LLVM_SYMBOLIZER", "llvm-symbolizer")
TAM = {"char": 1, "signed char": 1, "unsigned char": 1, "u8": 1, "s8": 1, "_Bool": 1,
       "short": 2, "unsigned short": 2, "u16": 2, "s16": 2,
       "int": 4, "unsigned int": 4, "u32": 4, "s32": 4, "float": 4, "f32": 4, "long": 4, "unsigned long": 4,
       "long long": 8, "unsigned long long": 8, "u64": 8, "s64": 8, "double": 8}


def norm(p):
    """Caminho relativo à raiz do repositório (o DWARF e o AST escrevem de jeitos diferentes)."""
    p = os.path.normpath(p)
    return os.path.relpath(p, ROOT) if os.path.isabs(p) else p


def ler_rastreio(caminho, filtro=None):
    for l in open(caminho, encoding="utf-8", errors="replace"):
        c = l.rstrip("\n").split("\t")
        if filtro and filtro not in c[0]:
            continue
        pcs = [int(p, 16) for p in c[5].split(",") if int(p, 16)]
        if int(c[2]) & ~16 and pcs:
            yield c[0], int(c[1]), int(c[2]), pcs


def simbolizar(prog, pcs):
    """pc -> (arquivo, linha, coluna) do acesso (quadro mais interno)."""
    pcs = sorted(pcs)
    entrada = "".join(f"0x{pc - 1:x}\n" for pc in pcs)
    out = subprocess.run([SYMB, f"--obj={prog}", "--inlining=true"], input=entrada,
                         capture_output=True, text=True).stdout
    blocos = out.strip("\n").split("\n\n")
    res = {}
    for pc, b in zip(pcs, blocos):
        linhas = b.splitlines()
        if len(linhas) >= 2:
            m = re.match(r"(.*):(\d+):(\d+)$", linhas[1])
            if m:
                res[pc] = (norm(m.group(1)), int(m.group(2)), int(m.group(3)), linhas[0])
    return res


def i_do_fonte(objdir, fonte):
    """O .i que headless.py gerou para um fonte (jogo: src/a/b.c -> a_b.i; port: pc_x.i, gs_x.i, nl_x.i)."""
    f = fonte.split("port/build/gen/src/", 1)[-1]
    if f.startswith("src/"):
        c = objdir / (f[4:].replace("/", "_")[:-2] + ".i")
        if c.exists():
            return c
    nome = pathlib.Path(f).stem
    for pre in ("pc_", "gs_", "nl_"):
        c = objdir / f"{pre}{nome}.i"
        if c.exists():
            return c
    return None


def expressoes(caminho_i):
    """{(arquivo, linha, coluna): [(tipo de nó, descrição, tipo do valor)]} das expressões que leem memória,
    pela posição do fim (é a coluna que o clang grava no DWARF para o acesso)."""
    r = subprocess.run([CLANG, "-m32", "-std=gnu89", "-w", "-fsyntax-only", "-x", "cpp-output", "-Xclang",
                        "-ast-dump=json", str(caminho_i)], capture_output=True, text=True)
    if not r.stdout:
        return {}
    arvore = json.loads(r.stdout)
    estado = {"file": str(caminho_i), "line": 0}
    out = collections.defaultdict(list)

    def loc(d):
        """Atualiza o estado com um local (o dumper só escreve o que mudou desde o último) e o devolve."""
        if "expansionLoc" in d:
            if "spellingLoc" in d:
                loc(d["spellingLoc"])
            return loc(d["expansionLoc"])
        if "offset" not in d:
            return None
        if "file" in d:
            estado["file"] = d["file"]
        if "line" in d:
            estado["line"] = d["line"]
        if "presumedFile" in d:
            estado["pfile"] = d["presumedFile"]
        if "presumedLine" in d:
            estado["pline"] = d["presumedLine"]
        elif "line" in d and "pline" in estado and "presumedFile" not in d:
            estado.pop("pline", None)
        arq = d.get("presumedFile", estado.get("pfile", estado["file"]))
        lin = d.get("presumedLine", estado.get("pline", estado["line"]))
        return norm(arq), lin, d.get("col")

    def tipo(n):
        return n.get("type", {}).get("qualType", "?")

    def andar(n):
        """Percorre na ordem em que o dumper escreveu; devolve o local da expressão (getExprLoc do clang): o membro
        em a->b, o operador em *p, o da base em v[i]. É a coluna que o clang grava no DWARF para o acesso."""
        ini = fim = None
        filhos = []
        for k, v in n.items():
            if k == "loc" and isinstance(v, dict):
                loc(v)
            elif k == "range" and isinstance(v, dict):
                if "begin" in v:
                    ini = loc(v["begin"])
                if "end" in v:
                    fim = loc(v["end"])
            elif k == "inner":
                filhos = [andar(c) if isinstance(c, dict) else None for c in v]
            elif isinstance(v, dict) and k != "type":
                andar(v)
        kind = n.get("kind")
        v = n.get("inner", [])
        if kind == "MemberExpr":
            aqui = fim
            base = tipo(v[0]) if v else "?"
            if n.get("isArrow"):
                base = base.rstrip(" *").rstrip()
            out[aqui].append(("campo", f"{base}.{n.get('name')}", tipo(n)))
        elif kind == "ArraySubscriptExpr":
            aqui = filhos[0] if filhos else ini
            out[aqui].append(("vetor", f"{tipo(v[0])}[]", tipo(n)))
        elif kind == "UnaryOperator" and n.get("opcode") == "*":
            aqui = ini
            out[aqui].append(("ptr", f"*({tipo(v[0])})", tipo(n)))
        elif kind in ("ImplicitCastExpr",) and filhos:
            aqui = filhos[0]
        else:
            aqui = ini
        return aqui

    andar(arvore)
    return out


def tamanho(t):
    t = re.sub(r"\b(const|volatile)\b", "", t).strip()
    t = t.split(":")[0].strip()  # "u32':'unsigned int" vem como qualType simples; desugared fica de fora
    if "*" in t:
        return 4
    return TAM.get(t)


def campos(prog, pcs):
    """{pc: (descrição, tipo do valor, local)} de cada ponto de leitura; descrição "Struct.campo" quando é campo."""
    prog = pathlib.Path(prog)
    objdir = prog.parent / "obj"
    locais = simbolizar(prog, set(pcs))
    exprs, out = {}, {}
    for pc, (f, l, c, fn) in locais.items():
        i = i_do_fonte(objdir, f)
        if i is None:
            continue
        if i not in exprs:
            exprs[i] = expressoes(i)
        cand = exprs[i].get((f, l, c), [])
        if cand:
            kind, desc, t = next((x for x in cand if x[0] == "campo"), cand[0])
            out[pc] = (desc, t, f"{f.split('port/build/gen/src/')[-1]}:{l}:{c} {fn}")
    return out


def main():
    a = sys.argv[1:]
    if len(a) < 4 or "-o" not in a:
        sys.exit(__doc__)
    prog, rast, saida = pathlib.Path(a[0]), a[1], a[a.index("-o") + 1]
    filtro = a[a.index("--arquivo") + 1] if "--arquivo" in a else None
    objdir = prog.parent / "obj"
    leituras = list(ler_rastreio(rast, filtro))
    locais = simbolizar(prog, {pc for _, _, _, pcs in leituras for pc in pcs})
    por_i = collections.defaultdict(set)
    sem_i = set()
    for pc, (f, l, c, fn) in locais.items():
        i = i_do_fonte(objdir, f)
        (por_i[i].add(pc) if i else sem_i.add(f))
    exprs = {}
    for n, i in enumerate(sorted(por_i)):
        print(f"\r[{n + 1}/{len(por_i)}] {i.name:50s}", end="", file=sys.stderr, flush=True)
        exprs[i] = expressoes(i)
    print(file=sys.stderr)
    motivos = collections.Counter()
    with open(saida, "w") as fo:
        for arq, ofs, tam, pcs in leituras:
            for pc in pcs:
                if pc not in locais:
                    motivos["sem linha no DWARF"] += 1
                    fo.write(f"{arq}\t{ofs}\t{tam}\t?\t?\t{pc:08x}\tsem linha no DWARF\n")
                    continue
                f, l, c, fn = locais[pc]
                i = i_do_fonte(objdir, f)
                cand = exprs.get(i, {}).get((f, l, c), []) if i else []
                lugar = f"{f.split('port/build/gen/src/')[-1]}:{l}:{c} {fn}"
                if not cand:
                    m = "sem expressão nessa coluna" if i else "fonte sem .i (header?)"
                    motivos[m] += 1
                    fo.write(f"{arq}\t{ofs}\t{tam}\t?\t?\t{lugar}\t{m}\n")
                    continue
                kind, desc, t = next((x for x in cand if x[0] == "campo"), cand[0])
                w = tamanho(t)
                m = "ok" if w and tam & ~16 == w else f"largura lida {tam & ~16} != tipo {w}"
                motivos[m if m == "ok" else "largura diferente do tipo"] += 1
                fo.write(f"{arq}\t{ofs}\t{tam}\t{desc}\t{t}\t{lugar}\t{m}\n")
    print(f"{saida}: " + ", ".join(f"{k} {v}" for k, v in motivos.most_common()))
    if sem_i:
        print(f"fontes sem .i: {', '.join(sorted(sem_i)[:8])}")


if __name__ == "__main__":
    main()
