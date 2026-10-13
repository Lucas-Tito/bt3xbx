#!/usr/bin/env python3
"""Decisão 19: confere que toda gravação de 64 bits num pacote da GPU passa por PS2_PKT64 (include/types.h).

    pacotes64.py [OBJDIR] [--lista]     (no WSL, na raiz, depois de headless.py --alvo x86)

Pelo AST do clang dos .i da build x86 (OBJDIR, padrão port/build/x86/obj), cada acesso de 64 bits à memória (por
ponteiro, vetor, campo ou global; variável local não conta) vira uma linha: gravação, gravação composta (|=...),
cópia de memória para memória, leitura, ou gravação num campo de bits. O alvo é PACOTE ou NÚMERO:
  - pacote: ponteiro u64/u32 local ou parâmetro que não esteja em NATIVO_VAR (os pacotes montados com
    Dma_BeginDirect, gOtCur...), e campo de struct cujo tipo case com PACOTE_TIPO (EftTriPkt.prim, GsQword.d,
    TexBltPacket.bitbltbuf, sceGsDrawEnv1.frame1...);
  - número: o resto (flags de 64 bits, SaveData.charaBits, o TEX0 guardado nas structs de textura, que o conversor
    troca como u64 e o código lê como número).
Exige PS2_PKT64 no lado direito de toda gravação em pacote, de toda cópia entre pacote e número (nos dois sentidos)
e em volta de toda leitura de pacote usada como número. Cópia de pacote para pacote fica como está. Gravação em campo
de bits de pacote pede PS2_PKT64_FIELDS na struct (não se confere aqui: layout.py confere a permutação).

A conferência é no texto do fonte, na linha que o AST aponta (o .i já não tem os macros): o lado direito tem de
começar por "PS2_PKT64(". Gravação feita dentro de um macro de função (EFT_SURF_FILL_PKT...) não aparece no texto da
linha que o usa: sai como "em macro", para conferir à mão. Sai com 1 se alguma gravação em pacote não tiver a macro.
As listas NATIVO_VAR e PACOTE_TIPO são a classificação feita na etapa 4 da F3 (docs/x360/formatos/gpu.md).
"""
import collections, concurrent.futures, json, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
CLANG = os.environ.get("CLANG", "clang")
U64 = {"unsigned long long", "long long"}
MAC = "PS2_PKT64("
# ponteiros e vetores de 64 bits que guardam números, não pacotes (pelo nome da variável)
NATIVO_VAR = {"alpha", "bits", "blastPos", "buf", "col", "ctx", "gPortRandNext", "gSaveData", "rgba", "tex0", "tex",
              "t", "pairTex", "IntegerLiteral", "CallExpr", "g", "sPaceNext", "sPacePrev", "sPaceMax", "sPaceMin",
              "gPortSleptNs"}
# structs que são pacote da GPU (ou parte de um), pelo nome do tipo
PACOTE_TIPO = re.compile(r"(Pkt|PktHead|RegPkt)$|^(GsQword|u128|Reg|EftTriGs|CbFontOut|TexBltPacket|TexChainEnt|"
                         r"TexChunkHdr|ScrXfade|sceGsDrawEnv1|sceGsClear)$")


def norm(p):
    p = os.path.normpath(p)
    p = os.path.relpath(p, ROOT) if os.path.isabs(p) else p
    return p.replace("port/build/gen/src/", "")


def tipo_de(n):
    t = n.get("type", {})
    return re.sub(r"\b(const|volatile)\b", "", t.get("desugaredQualType", t.get("qualType", ""))).strip()


def raiz(n):
    """De onde vem o lvalue: 'Struct.campo' para campo, 'var:tipo' para ponteiro, vetor ou global."""
    k, v = n.get("kind"), n.get("inner", [])
    if k in ("ImplicitCastExpr", "ParenExpr", "CStyleCastExpr", "ArraySubscriptExpr", "UnaryOperator",
             "BinaryOperator"):
        return raiz(v[0]) if v else "?"
    if k == "MemberExpr":
        t = v[0].get("type", {}).get("qualType", "?") if v else "?"
        return f"{t.rstrip(' *').rstrip() if n.get('isArrow') else t}.{n.get('name')}"
    if k == "DeclRefExpr":
        d = n.get("referencedDecl", {})
        return f"{d.get('name')}:{d.get('type', {}).get('qualType', '?')}"
    return k


def eh_pacote(r):
    if r.startswith(("struct (", "union (")):
        return False
    if "." in r:
        return bool(PACOTE_TIPO.search(r.split(".")[0]))
    return r.split(":")[0] not in NATIVO_VAR


def memoria(n):
    k = n.get("kind")
    if k == "ParenExpr":
        return memoria(n["inner"][0])
    return k in ("ArraySubscriptExpr", "MemberExpr") or (k == "UnaryOperator" and n.get("opcode") == "*") or \
        (k == "DeclRefExpr" and n.get("referencedDecl", {}).get("kind") == "VarDecl")


def analisar(caminho_i):
    r = subprocess.run([CLANG, "-m32", "-std=gnu89", "-w", "-fsyntax-only", "-x", "cpp-output", "-Xclang",
                        "-ast-dump=json", str(caminho_i)], capture_output=True, text=True)
    if not r.stdout:
        return []
    arvore = json.loads(r.stdout)
    fonte = open(caminho_i, "rb").read().decode("latin-1")
    estado = {"file": str(caminho_i), "line": 0}
    out, bits, locais = [], set(), set()

    def loc(d):
        """O dumper só escreve o que mudou desde o último local: o estado acompanha a ordem de escrita."""
        if "expansionLoc" in d:
            if "spellingLoc" in d:
                loc(d["spellingLoc"])
            return loc(d["expansionLoc"])
        if "offset" not in d:
            return None
        for a, b in (("file", "file"), ("line", "line"), ("presumedFile", "pfile"), ("presumedLine", "pline")):
            if a in d:
                estado[b] = d[a]
        if "line" in d and "presumedLine" not in d and "presumedFile" not in d:
            estado.pop("pline", None)
        return (norm(d.get("presumedFile", estado.get("pfile", estado["file"]))),
                d.get("presumedLine", estado.get("pline", estado["line"])), d.get("col"))

    def texto(n):
        b, e = n.get("range", {}).get("begin", {}), n.get("range", {}).get("end", {})
        return " ".join(fonte[b["offset"]:e["offset"] + e.get("tokLen", 0)].split()) if "offset" in b and \
            "offset" in e else "?"

    def local(n):
        while n.get("kind") == "ParenExpr":
            n = n["inner"][0]
        return n.get("kind") == "DeclRefExpr" and n.get("referencedDecl", {}).get("id") in locais

    def nu(n):
        while n.get("kind") in ("ParenExpr", "ImplicitCastExpr"):
            n = n["inner"][0]
        return n

    def andar(n, func, em_func):
        k = n.get("kind")
        if k == "FunctionDecl":
            func, em_func = n.get("name", func), "inner" in n
        if k == "FieldDecl" and n.get("isBitfield"):
            bits.add(n.get("id"))
        if k == "VarDecl" and em_func and n.get("storageClass") not in ("static", "extern"):
            locais.add(n.get("id"))
        ini = None
        filhos = n.get("inner", [])
        for chave, v in n.items():
            if chave == "loc" and isinstance(v, dict):
                loc(v)
            elif chave == "range" and isinstance(v, dict):
                ini = loc(v["begin"]) if "begin" in v else ini
                if "end" in v:
                    loc(v["end"])
            elif chave == "inner":
                for c in v:
                    if isinstance(c, dict):
                        andar(c, func, em_func)
            elif isinstance(v, dict) and chave not in ("type", "referencedDecl"):
                andar(v, func, em_func)
        if ini is None:
            return
        onde = f"{ini[0]}:{ini[1]}:{ini[2]}"
        op = n.get("opcode", "")
        if k in ("BinaryOperator", "CompoundAssignOperator") and len(filhos) > 1 and op.endswith("=") and \
                op not in ("==", "!=", "<=", ">="):
            l = filhos[0]
            if tipo_de(l) in U64 and memoria(l) and not local(l):
                bf = nu(l).get("kind") == "MemberExpr" and nu(l).get("referencedMemberDecl") in bits
                rv = filhos[1]
                while rv.get("kind") in ("ImplicitCastExpr", "ParenExpr", "CStyleCastExpr") and \
                        rv.get("castKind") != "LValueToRValue":
                    rv = rv["inner"][0]
                orig = ""
                if op == "=" and rv.get("castKind") == "LValueToRValue" and tipo_de(rv["inner"][0]) in U64 and \
                        memoria(rv["inner"][0]) and not local(rv["inner"][0]):
                    orig = raiz(rv["inner"][0])
                out.append(("bits" if bf else "grav" if op == "=" else "comp", onde, func, raiz(l), orig,
                            texto(l), op, texto(filhos[1])))
        elif k == "ImplicitCastExpr" and n.get("castKind") == "LValueToRValue" and filhos and \
                tipo_de(filhos[0]) in U64 and memoria(filhos[0]) and not local(filhos[0]):
            out.append(("leit", onde, func, raiz(filhos[0]), "", texto(filhos[0]), "", ""))

    andar(arvore, "?", False)
    return out


def conferir(acs):
    """[(situação, acesso)]: 'ok', 'falta', 'em macro', 'numero', 'copia-pacote', 'bits'."""
    linhas = {}

    def linha(arq, lin):
        if arq not in linhas:
            p = ROOT / arq
            linhas[arq] = p.read_text(errors="replace").split("\n") if p.exists() else []
        ls = linhas[arq]
        return ls[lin - 1] if 0 < lin <= len(ls) else ""

    macros = {}

    def corpo_macros(arq):
        """{nome: corpo} dos #define de função do arquivo (com as linhas de continuação)."""
        if arq not in macros:
            texto = (ROOT / arq).read_text(errors="replace") if (ROOT / arq).exists() else ""
            macros[arq] = {m.group(1): m.group(2) for m in
                           re.finditer(r"^#define\s+(\w+)\(.*?\)((?:.*\\\n)*.*)$", texto, re.M)}
        return macros[arq]

    def no_macro(arq, l, lhs, op):
        """A gravação vem de um macro usado na linha, e o corpo dele já tem a macro no lado direito."""
        for nome, corpo in corpo_macros(arq).items():
            if re.search(r"\b" + nome + r"\s*\(", l) and \
                    re.search(re.escape(lhs) + r"\s*" + re.escape(op) + r"\s*" + re.escape(MAC), corpo):
                return True
        return False

    copias_rhs = {(a[1].rsplit(":", 1)[0], a[7].strip("() ")) for a in acs if a[0] == "grav" and a[4]}
    out = []
    for a in acs:
        tipo, onde, func, r, orig, lhs, op, rhs = a
        arq, lin, _ = onde.split(":")
        if arq.startswith("port/src/gs/") and not arq.endswith("gs_sony.c") or "third_party" in arq:
            continue
        pac = eh_pacote(r)
        if tipo == "bits":
            out.append(("bits", a))  # a ordem dos campos (PS2_PKT64_FIELDS) quem confere é o layout.py
            continue
        if tipo == "leit":
            if pac and (onde.rsplit(":", 1)[0], lhs.strip("() ")) not in copias_rhs:
                l = linha(arq, int(lin))
                k = l.find(lhs.strip("() "))
                out.append(("ok" if 0 <= l.find(MAC) < k else "falta", a))
            continue
        precisa = pac != eh_pacote(orig) if orig else pac
        if not precisa:
            out.append(("copia-pacote" if orig and pac else "numero", a))
            continue
        if tipo == "comp" and op not in ("|=", "&=", "^="):
            out.append(("falta", a))  # aritmética sobre metades trocadas: reescrever à mão
            continue
        l = linha(arq, int(lin))
        m = [x for x in re.finditer(re.escape(lhs) + r"\s*" + re.escape(op), l)]
        if not m:
            out.append(("ok" if no_macro(arq, l, lhs, op) else "em macro", a))
            continue
        resto = l[m[0].end():].strip() or linha(arq, int(lin) + 1).strip()  # (o lado direito na linha seguinte)
        out.append(("ok" if resto.startswith(MAC) else "falta", a))
    return out


def main():
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    objdir = pathlib.Path(a[0]) if a else ROOT / "port/build/x86/obj"
    acs, vistos = [], set()
    with concurrent.futures.ProcessPoolExecutor() as ex:
        for res in ex.map(analisar, sorted(objdir.glob("*.i"))):
            for t in res:
                if not t[1].startswith(("/usr", "<", "..")) and t not in vistos:
                    vistos.add(t)
                    acs.append(t)
    res = conferir(acs)
    cont = collections.Counter((s, x[0]) for s, x in res)
    print("pacotes64: " + ", ".join(f"{s}/{t} {n}" for (s, t), n in sorted(cont.items())))
    for s, x in res:
        if s in ("falta", "em macro") or "--lista" in sys.argv:
            print(f"  {s}: {x[1]} {x[2]}: {x[5]} {x[6]} {x[7][:80]}")
    sys.exit(1 if any(s == "falta" for s, _ in res) else 0)


main()
