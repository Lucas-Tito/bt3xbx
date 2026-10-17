#!/usr/bin/env python3
"""F1/F3: compara um bloco de memória das duas builds campo a campo, pelo tipo do DWARF (complemento do memdiff.py).

    campos.py X86.dump PPC.dump PROG_X86 TIPO ENDEREÇO[:ENDEREÇO_PPC] [N]
    campos.py --donos PROG_X86 VBLANK [--tipos SAIDA]
    campos.py --globais PROG_X86 SAIDA [MAPA_ELFTIPOS]

A classificação do memdiff.py é por palavra de 4 bytes, e uma palavra com campos de larguras diferentes (dois u8 e um
u16, um u16 desalinhado) sai "diferente" mesmo convertida certo. Aqui cada folha do tipo (pelo gdb: structs, uniões
abertas pelo primeiro membro, arrays elemento a elemento) se compara pela própria largura: byte igual, número com os
bytes invertidos. Mostra só caminho, deslocamento, largura e classe, nunca valores.
  - TIPO: nome do typedef ou da struct (gdb lookup_type; com mais de um tipo do mesmo nome o gdb escolhe um, então
    confira o tamanho impresso);
  - ENDEREÇO: no heap é o mesmo nas duas builds; numa global dê o da ppc depois de ':' (o memdiff.py imprime os dois);
  - N: quantos elementos seguidos (array).
Ponteiro de função difere sempre (endereço do código do port) e é contado à parte.

--donos roda a x86 no gdb até o VBLANK e diz, para cada bloco do heap do jogo vivo naquele momento, quem o alocou
(Heap_Alloc com três chamadores e o vblank). Rode na raiz, com as mesmas variáveis BT3_* da rodada comparada. Com
--tipos SAIDA, para cada bloco cuja chamada pede sizeof(T) (na linha do chamador direto ou do seguinte), a largura de
cada byte de T pelo DWARF, com os códigos de --globais: SAIDA.tsv (endereço dos dados, T, tamanho, início em SAIDA.bin,
quem alocou) e SAIDA.bin. O memdiff.py --donos compara esses blocos campo a campo (T repetido pelo tamanho pedido).

--globais grava a largura de cada byte de cada variável global da x86 pelo tipo do DWARF (F4, revisão da etapa 5), para
o memdiff.py --tipos comparar as globais campo a campo em vez de palavra a palavra: SAIDA.tsv (nome, endereço, tamanho,
início em SAIDA.bin) e SAIDA.bin (um byte por byte da variável). Os códigos: 1 byte (char, u8, bool), 2/4/8 inteiro
dessa largura, 0x44/0x48 float/double, 0x84 ponteiro (de dado ou de função), 0x80 continuação, 0 enchimento, 0xFF
fora da comparação (tabela que o port não usa, morta no elftipos.py). União:
o primeiro membro (como acima); campo de bits: a unidade inteira, como inteiro da largura do tipo dela. As variáveis
do jogo definidas nos .s (port/build/gen/data: tabelas e .bss do executável), que o DWARF não tipa, entram pela largura
que o elftipos.py tirou das declarações do C (MAPA_ELFTIPOS, port/build/ppc/elftipos/mapa.tsv por padrão); o resto
fica de fora e o memdiff o compara por palavra.
"""
import os, pathlib, re, struct, subprocess, sys, tempfile
from collections import Counter

FOLHAS = r'''
import gdb
def folhas(t, base, cam, out):
    t = t.strip_typedefs()
    c = t.code
    if c in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
        campos = t.fields()
        if c == gdb.TYPE_CODE_UNION:
            campos = campos[:1]
        for f in campos:
            if f.bitsize:
                out.append((base + f.bitpos // 8, f.type.sizeof, "bits", cam + "." + f.name))
                continue
            folhas(f.type, base + f.bitpos // 8, cam + "." + (f.name or "?"), out)
    elif c == gdb.TYPE_CODE_ARRAY:
        el = t.target()
        for i in range(t.sizeof // max(el.sizeof, 1)):
            folhas(el, base + i * el.sizeof, "%s[%d]" % (cam, i), out)
    elif c == gdb.TYPE_CODE_PTR:
        out.append((base, 4, "fptr" if t.target().strip_typedefs().code == gdb.TYPE_CODE_FUNC else "ptr", cam))
    else:
        out.append((base, t.sizeof, "float" if c == gdb.TYPE_CODE_FLT else "int", cam))
t = gdb.lookup_type(TIPO)
out = []
folhas(t, 0, TIPO, out)
print("TOTAL", t.sizeof)
for o, s, k, c in out:
    print("FOLHA", o, s, k, c)
'''

MARCAS_PY = r'''
import gdb, os
cache = {}
# vetores de u8 da decomp que guardam um pacote da GPU montado pelo código em palavras u32 do host (decisão 19): os
# trechos de palavra (o resto do vetor é byte). GfxClut_InitPacket (src/sys/gfxm_b_b.c) grava w[0..27] e w[0x470/4..]
# e deixa a CLUT, bytes RGBA, em 0x70..0x46F; o upload/ref/end são as etiquetas de DMA, u32
PACOTES_U8 = {("GfxPostClut", "packet"): [(0, 0x70), (0x470, 0x490)], ("GfxClutWork", "packet"): [(0, 0x70), (0x470, 0x490)],
              ("GfxPostClut", "upload"): [(0, 0x20)], ("GfxClutWork", "ref"): [(0, 0x10)], ("GfxClutWork", "end"): [(0, 0x10)]}
def marcas(t):
    t = t.strip_typedefs()
    # (um tipo anônimo, "union {...}", não entra no cache: dois diferentes teriam a mesma chave)
    k = (str(t), t.sizeof) if "{...}" not in str(t) else None
    if k in cache:
        return cache[k]
    n = t.sizeof
    m = bytearray(n)
    c = t.code
    if c in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
        campos = t.fields()
        if c == gdb.TYPE_CODE_UNION:
            campos = campos[:1]
        for f in campos:
            if not hasattr(f, "bitpos") or f.type is None:
                continue
            if f.bitsize:
                w = f.type.strip_typedefs().sizeof
                o = f.bitpos // (8 * w) * w
                if 0 <= o and o + w <= n and m[o] == 0:
                    m[o] = w if w in (2, 4, 8) else 1
                    for j in range(1, w):
                        m[o + j] = 0x80 if w in (2, 4, 8) else 1
                continue
            o = f.bitpos // 8
            sub = marcas(f.type)
            m[o:o + len(sub)] = sub[:max(0, n - o)]
            for a, b in PACOTES_U8.get((t.name or "", f.name or ""), ()):
                for j in range(o + a, min(o + b, n), 4):
                    m[j:j + 4] = b"\x84\x80\x80\x80"  # (palavra de pacote: número ou o endereço de uma etiqueta)
            # campo opaco da decomp (unk*, pad*: vetor de bytes ou de u64 cujo conteúdo ela não conhece; o código grava
            # ali por deslocamento): 0xFE, comparado por palavra no memdiff
            if (f.name or "").startswith(("unk", "pad")) and f.type.strip_typedefs().code == gdb.TYPE_CODE_ARRAY:
                m[o:min(o + f.type.sizeof, n)] = b"\xfe" * (min(o + f.type.sizeof, n) - o)
    elif c == gdb.TYPE_CODE_ARRAY:
        el = t.target()
        if el.sizeof == 1 and el.strip_typedefs().code in (gdb.TYPE_CODE_INT, gdb.TYPE_CODE_CHAR):
            # vetor de bytes: texto, pixels, ou um armazém que o código usa por deslocamento com outros tipos (o
            # charaWork do BtlObjWork, o pool3.nodes): opaco (0xFE), comparado por palavra no memdiff
            m = bytearray(b"\xfe" * n)
        elif el.sizeof and n:
            m = bytearray(marcas(el) * (n // el.sizeof))
            m += bytearray(n - len(m))
    elif c == gdb.TYPE_CODE_PTR:
        if n == 4:
            m = bytearray([0x84, 0x80, 0x80, 0x80])
    elif c == gdb.TYPE_CODE_FLT:
        if n in (4, 8):
            m = bytearray([0x40 + n] + [0x80] * (n - 1))
    elif n == 1:
        m = bytearray([1])
    elif n in (2, 4, 8):
        m = bytearray([n] + [0x80] * (n - 1))
    if k is not None:
        cache[k] = bytes(m)
    return bytes(m)
'''

GLOBAIS = MARCAS_PY + r'''
SAIDA = os.environ["CAMPOS_SAIDA"]
nomes = [l.split() for l in open(SAIDA + ".nomes")]
tsv, binf = open(SAIDA + ".tsv", "w"), open(SAIDA + ".bin", "wb")
pos = 0
for nome, ender in nomes:
    ender = int(ender, 16)
    cand = []
    s = gdb.lookup_global_symbol(nome)
    if s is not None:
        cand.append(s)
    try:
        cand += list(gdb.lookup_static_symbols(nome))
    except Exception:
        pass
    sym = None
    for s in cand:
        try:
            if s.is_variable and int(s.value().address) == ender:
                sym = s
                break
        except Exception:
            continue
    if sym is None:
        continue
    m = marcas(sym.type)
    tsv.write("%s\t%x\t%x\t%x\n" % (nome, ender, len(m), pos))
    binf.write(m)
    pos += len(m)
tsv.close(); binf.close()
print("GLOBAIS", len(nomes), pos)
# as declarações extern dos .h e .c (o objeto é definido num .s, sem DWARF): a largura de um elemento do tipo declarado
tsv, binf, pos = open(SAIDA + ".decl.tsv", "w"), open(SAIDA + ".decl.bin", "wb"), 0
vistos_decl = set()
for l in open(SAIDA + ".decl"):
    nome, tipo, nptr = l.split("\t")[:3]
    if nome in vistos_decl:
        continue
    vistos_decl.add(nome)
    try:
        t = gdb.lookup_type(tipo)
    except gdb.error:
        continue
    for _ in range(int(nptr)):
        t = t.pointer()
    m = marcas(t)
    if m:
        tsv.write("%s\t%s\t%x\t%x\n" % (nome, tipo + "*" * int(nptr), len(m), pos))
        binf.write(m)
        pos += len(m)
tsv.close(); binf.close()
'''

POOL_PY = r'''
import json
pool_vivos = {}
pool_tipos = {}
# A linha do chamador e o tipo do destino são evidência, nunca o tamanho sozinho.
def pool_tipo(fr):
    fr.select()
    sal = fr.find_sal()
    if not sal.symtab:
        return None
    texto = open(sal.symtab.fullname(), errors="replace").read().splitlines()
    linha = " ".join(texto[sal.line - 1:sal.line + 1])
    m = re.search(r"sizeof\s*\(\s*((?:struct\s+)?\w+)\s*\)", linha)
    if m:
        try: return gdb.lookup_type(m.group(1))
        except gdb.error: pass
    # Tipos provados pelo leitor das alocações em bytes, identificados pelo dono e pelo destino.
    conhecidos = {
        ("EftEmit_LoadSet", "set->array[0]"): "EftAeTexSet32",
        ("EftEmit_LoadSet", "set->array[1]"): "EftAeTexSet16",
        ("EftEmit_LoadSet", "set->array[2]"): "EftAeTexSet8",
        ("EftEmit_LoadSet", "set->array[3]"): "EftAeTexSet4",
        ("EftBoltMgr_Init", "gEftBoltPool->pool"): "EftBoltSeg",
        ("EftChainMgr_Init", "gEftChain->buf"): "EftArcNode",
        ("EftBlastObjMgr_Init", "mgr"): "s32",
    }
    for (dono, destino), nome in conhecidos.items():
        if fr.name() == dono and destino + " = BtlPool_Alloc" in linha:
            return gdb.lookup_type(nome)
    if fr.name() == "EftAuraMgr_Init" and "gPool->tasks = BtlPool_Alloc" in linha:
        return gdb.lookup_type("EftNTask").pointer()
    m = re.search(r"([\w>.*()\[\]-]+)\s*=\s*BtlPool_Alloc", linha)
    if m:
        try:
            t = gdb.parse_and_eval(m.group(1)).type.strip_typedefs().target().strip_typedefs()
            if t.code != gdb.TYPE_CODE_VOID and t.sizeof > 1: return t
        except (gdb.error, RuntimeError, KeyError): pass
    if m:
        raiz = m.group(1).split("->")[0]
        for nome, nptr, ndim in DECL.get(raiz, []):
            try:
                t = gdb.lookup_type(nome)
                for _ in range(nptr): t = t.pointer()
                partes = m.group(1).split("->")
                for parte in partes[1:]: t = t.strip_typedefs().target()[parte].type
                t = t.strip_typedefs().target().strip_typedefs()
                if t.code != gdb.TYPE_CODE_VOID: return t
            except (gdb.error, RuntimeError, KeyError): pass
    return None
class PoolFim(gdb.FinishBreakpoint):
    def __init__(self, fr, info):
        super().__init__(fr, internal=True); self.info = info
    def stop(self):
        if self.return_value is not None:
            pool_vivos[int(self.return_value) & 0xffffffff] = self.info
        return False
class PoolAloc(gdb.Breakpoint):
    def stop(self):
        fr = gdb.newest_frame()
        slot, size = int(gdb.parse_and_eval("slot")), int(gdb.parse_and_eval("size"))
        caller = fr.older()
        if caller.name() == "__wrap_BtlPool_Alloc": caller = caller.older()
        t = pool_tipo(caller)
        if caller.name() == "EftShotFxMgr_Init":
            sal0 = caller.find_sal()
            linha0 = open(sal0.symtab.fullname()).read().splitlines()[sal0.line-1]
            if "gEftShotFx->tex =" in linha0: t = gdb.lookup_type("EftAeTexSet4")
            if "gEftShotFx->lines =" in linha0: t = gdb.lookup_type("EftShotFxLine")
            if "sizeof(EftTShotFxMgr)" in linha0: t = gdb.lookup_type("EftShotFxWork")
        sal = caller.find_sal()
        if t:
            mt = bytearray(marcas(t))
            if caller.name() == "EftDiscMgr_Init" and size == 0x660:
                # As duas vistas se completam: recursos em eft_o.h, fila de discos em eft_p.h.
                fila = marcas(gdb.lookup_type("EftDiscMgr"))
                mt += b"\x00" * (size-len(mt))
                mt[0x554:len(fila)] = fila[0x554:]
                mesh = marcas(gdb.lookup_type("EftMesh"))
                mt[0x420:0x4b0] = mesh
                mt[0x4b0:0x540] = mesh
            pool_tipos[str(t)] = mt.hex()
        info = (slot, size, caller.name(), sal.line, str(t) if t else None)
        fr.select()
        PoolFim(fr, info)
        return False
class PoolReset(gdb.Breakpoint):
    def stop(self):
        slot = int(gdb.parse_and_eval("slot"))
        mgr = int(gdb.parse_and_eval("*(unsigned*)&gBtlPool"))
        if not mgr or not (int(gdb.parse_and_eval("*(unsigned*)%d" % (mgr+0x98))) & (2 << slot)):
            return False
        for a in list(pool_vivos):
            if pool_vivos[a][0] == slot: del pool_vivos[a]
        return False
class PoolLivre(gdb.Breakpoint):
    def stop(self):
        slot = int(gdb.parse_and_eval("slot"))
        mgr = int(gdb.parse_and_eval("*(unsigned*)&gBtlPool"))
        if not mgr or not (int(gdb.parse_and_eval("*(unsigned*)%d" % (mgr+0x98))) & (2 << slot)):
            pool_vivos.pop(int(gdb.parse_and_eval("(unsigned)ptr")),None)
        return False
class PoolLimpa(gdb.Breakpoint):
    def stop(self):
        pool_vivos.clear(); return False

def pool_gravar():
    tipos = dict(pool_tipos)
    def tipo(nome):
        if nome and nome not in tipos:
            try:
                t = gdb.lookup_type(nome)
                tipos[nome] = marcas(t).hex()
            except gdb.error: pass
    for info in pool_vivos.values(): tipo(info[-1])
    for nome in ("BtlScriptWork", "BtlScriptLine", "BtlScriptStart", "BtlCam", "DemoCam", "GscTask", "GscFile", "EftHitSphere", "EftHitBox", "BtlScriptCmdWork", "EftSet", "EftAeTexSet32", "EftPGlowMgr", "EftAbTexEntry", "StgRigidPool", "BtlObjWork", "BtlScriptCmdWindow"):
        tipo(nome)
    # Tipos de trabalho dos callbacks da árvore: o próprio C entrega task->work a um T*.
    import pathlib
    trabalhos, gsc_trabalhos = {}, {}
    for arq in pathlib.Path("src/battle").glob("*.c"):
        texto = arq.read_text()
        for fun in re.finditer(r"^\w[\w *]*?\b(\w+)\([^;{}]*\)\s*\{", texto, re.M):
            inicio = fim = fun.end()
            nivel = 1
            while fim < len(texto) and nivel:
                nivel += (texto[fim] == "{") - (texto[fim] == "}")
                fim += 1
            corpo = texto[inicio:fim-1]
            if fun.group(1).startswith("BtlScriptCmd_"):
                mw = re.search(r"\b(\w+)\s*\*\s*work\s*=\s*taskWork\s*;", corpo)
                if mw:
                    nome = mw.group(1)
                    tipo(nome)
                    if nome in tipos: gsc_trabalhos[fun.group(1)] = nome
                elif "taskWork" not in corpo:
                    gsc_trabalhos[fun.group(1)] = None
            m = re.search(r"\b(\w+)\s*\*\s*\w+\s*=\s*(?:\([^;]+?\))?\s*task->(?:work|data)\s*;", corpo)
            if m:
                nome = m.group(1)
                tipo(nome)
                if nome in tipos: trabalhos[fun.group(1)] = nome
    json.dump({"vblank": vb[0], "alocacoes": pool_vivos, "tipos": tipos, "trabalhos": trabalhos, "gsc_trabalhos": gsc_trabalhos},
              open(SAIDA + ".pool.json", "w"), sort_keys=True)
PoolAloc("BtlPool_Alloc"); PoolReset("BtlPool_Reset"); PoolLivre("BtlPool_Free"); PoolLimpa("BtlPool_Init"); PoolLimpa("BtlPool_Term")
'''

DONOS = MARCAS_PY + r'''
import re
ATE = int(os.environ["CAMPOS_ATE"])
SAIDA = os.environ.get("CAMPOS_SAIDA")
vivos, vb, linhas = {}, [0], {}
def pilha():
    out, fr = [], gdb.newest_frame().older()
    while fr is not None and len(out) < 3:
        sal = fr.find_sal()
        out.append((fr.name() or hex(fr.pc())) + ("@%s:%d" % (os.path.basename(sal.symtab.filename), sal.line) if sal.symtab else ""))
        if sal.symtab is not None and len(out) <= 2:
            linhas.setdefault(out[-1], (sal.symtab.fullname(), sal.line))
        fr = fr.older()
    return " < ".join(out)
# o primeiro argumento da alocação tem de ser só sizeof(T), ou sizeof(T) vezes uma contagem: com soma ou alinhamento
# (o Ot_Init pede o pacote e as entradas juntos) o bloco não é um vetor de T
SIZEOF = re.compile(r"Heap_Alloc\s*\(\s*(?:[\w.>-]+\s*\*\s*)?sizeof\s*\(\s*((?:struct\s+)?\w+)\s*\)(?:\s*\*\s*[\w.>()-]+)?\s*,")
DESTINO = re.compile(r"\b(g[A-Za-z0-9_]+)((?:\s*\[[^\]]*\]|\s*->\s*\w+|\s*\.\s*\w+)*)\s*=\s*(?:\([^()]*\)\s*)?Heap_Alloc\s*\(")
DECL = {}
if os.environ.get("CAMPOS_DECL"):
    for l in open(os.environ["CAMPOS_DECL"]):
        q = l.rstrip("\n").split("\t")
        DECL.setdefault(q[0], []).append((q[1], int(q[2]), int(q[3]) if len(q) > 3 else 0))
def tipo_do_dono(info):
    """o tipo pedido na linha da chamada (Heap_Alloc(sizeof(T), ...)), do chamador direto ou do seguinte"""
    for quadro in info.split(" < ")[:2]:
        if quadro not in linhas:
            continue
        arq, n = linhas[quadro]
        try:
            texto = open(arq, errors="replace").read().splitlines()
        except OSError:
            continue
        linha = " ".join(texto[n - 1:n + 1])
        m = SIZEOF.search(linha)
        if m:
            try:
                return m.group(1), gdb.lookup_type(m.group(1))
            except gdb.error:
                return None
        # sem sizeof (tamanho literal: gCharSel = Heap_Alloc(0x3A4C, ...)): o tipo apontado por quem recebe o bloco,
        # uma global declarada em C (extern T *g; os .s não têm DWARF) seguida de [i] e ->campo
        m = DESTINO.search(linha)
        for tipo, nptr, ndim in (DECL.get(m.group(1), []) if m else []):
            try:
                t = gdb.lookup_type(tipo)
                for _ in range(nptr):
                    t = t.pointer()
                for passo in re.findall(r"\[[^\]]*\]|->\s*\w+|\.\s*\w+", m.group(2) or ""):
                    if passo.startswith("[") and ndim:
                        ndim -= 1  # (o índice do vetor declarado: extern T *g[2]; g[i] é um T *)
                    elif passo.startswith("["):
                        t = t.strip_typedefs().target()
                    else:
                        if passo.startswith("->"):
                            t = t.strip_typedefs().target()
                        t = t.strip_typedefs()[passo.lstrip("->. ")].type
                t = t.strip_typedefs()
                if t.code == gdb.TYPE_CODE_PTR and t.target().strip_typedefs().code in (gdb.TYPE_CODE_STRUCT,
                                                                                       gdb.TYPE_CODE_UNION):
                    return str(t.target()) + " (de " + m.group(0).split("=")[0].strip() + ")", t.target()
            except (gdb.error, KeyError, RuntimeError):
                continue
    return None
def tipar():
    tsv, binf, pos = open(SAIDA + ".tsv", "w"), open(SAIDA + ".bin", "wb"), 0
    for a in sorted(vivos):
        t = tipo_do_dono(vivos[a].split(": ", 1)[1])
        if t is None or t[1].sizeof == 0:
            continue
        m = marcas(t[1])
        # O contexto distingue objetos do mesmo tipo alocados pelo mesmo helper (as raízes do Flash, por exemplo).
        tsv.write("%08x\t%s\t%x\t%x\t%s\n" % (a, t[0], len(m), pos, vivos[a].split(": ", 1)[1]))
        binf.write(m)
        pos += len(m)
    tsv.close(); binf.close()
class Fim(gdb.FinishBreakpoint):
    def __init__(self, fr, info):
        super().__init__(fr, internal=True); self.info = info
    def stop(self):
        if self.return_value is not None:
            vivos[int(self.return_value) & 0xffffffff] = "vb %d: %s" % (vb[0], self.info)
        return False
class Aloc(gdb.Breakpoint):
    def stop(self):
        Fim(gdb.newest_frame(), pilha()); return False
class Livre(gdb.Breakpoint):
    def stop(self):
        vivos.pop(int(gdb.parse_and_eval("(unsigned)ptr")) & 0xffffffff, None); return False
class Vb(gdb.Breakpoint):
    def stop(self):
        vb[0] = int(gdb.parse_and_eval("vblank"))
        if vb[0] >= ATE:
            for a in sorted(vivos):
                print("DONO %08x %s" % (a, vivos[a]))
            if SAIDA:
                tipar()
                pool_gravar()
            gdb.execute("kill"); gdb.execute("quit")
        return False
gdb.execute("set pagination off"); gdb.execute("set confirm off")
Aloc("Heap_Alloc"); Livre("Heap_Free"); Vb("Port_StateLog")
exec(POOL_PY)
gdb.execute("run")
'''


def gdb_py(prog, codigo, pre=(), args=()):
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
        f.write(codigo)
    try:
        cmd = ["gdb", "-batch", "-nx"] + [x for p in pre for x in ("-ex", p)] + ["-x", f.name, "--args", prog] + list(args)
        r = subprocess.run(cmd, capture_output=True, text=True)
        return r.stdout + r.stderr
    finally:
        os.unlink(f.name)


def ler(caminho):
    d = open(caminho, "rb").read()
    out, o = [], 0
    while o < len(d):
        e = d.index(b"\n", o)
        nome, a, n = d[o:e].split()
        a, n = int(a, 16), int(n, 16)
        out.append((nome.decode(), a, d[e + 1:e + 1 + n]))
        o = e + 1 + n
    return out


def pega(regs, a, n):
    for _, ra, d in regs:
        if ra <= a and a + n <= ra + len(d):
            return d[a - ra:a - ra + n]
    return None


def comparar(dx, dp, prog, tipo, ender, n):
    saida = gdb_py(prog, FOLHAS, pre=[f"py TIPO = {tipo!r}"])
    total = next(int(l.split()[1]) for l in saida.splitlines() if l.startswith("TOTAL"))
    folhas = [l.split(None, 4)[1:] for l in saida.splitlines() if l.startswith("FOLHA")]
    ax, _, ap = ender.partition(":")
    ax = int(ax, 16)
    ap = int(ap, 16) if ap else ax
    X, P = ler(dx), ler(dp)
    cont = Counter()
    print(f"{tipo}: {total} bytes, {len(folhas)} folhas, {n} elemento(s)")
    for i in range(n):
        for o, s, k, cam in folhas:
            o, s = int(o), int(s)
            bx, bp = pega(X, ax + i * total + o, s), pega(P, ap + i * total + o, s)
            if bx is None or bp is None:
                cont["fora do dump"] += 1
                continue
            if k == "bits":
                cont["campo de bits (não comparado)"] += 1
                continue
            if (bx == bp) if s == 1 else (bx == bp[::-1]):
                cont["igual"] += 1
                continue
            cls = "ponteiro de função" if k == "fptr" else "diferente"
            cont[cls] += 1
            print(f"  [{i}] {cam} +0x{o:x} ({s} bytes, {k}): {cls}")
    print("  " + ", ".join(f"{k} {v}" for k, v in cont.most_common()))


DECL = re.compile(r"^\s*extern\s+(?:(?:const|volatile)\s+)*((?:struct\s+|union\s+)?\w+)\s*(\**)\s*(\w+)\s*((?:\[[^\]]*\])*)\s*;")


def escrever_decl(caminho):
    """nome, tipo, número de * e de [] de cada `extern T [*]nome[...];` de include/ e src/, em ordem (as de include/
    primeiro); a mesma variável pode ter visões diferentes em arquivos diferentes (gBtlChars é BtlCharMgr e
    BtlFlagRoster): quem usa escolhe"""
    vistos = []
    for base in ("include", "src"):
        for arq in sorted(pathlib.Path(base).rglob("*.[ch]")):
            for l in arq.read_text(errors="replace").splitlines():
                m = DECL.match(l)
                if m and (m.group(3), m.group(1), len(m.group(2)), m.group(4).count("[")) not in vistos:
                    vistos.append((m.group(3), m.group(1), len(m.group(2)), m.group(4).count("[")))
    with open(caminho, "w") as f:
        for nome, tipo, nptr, ndim in vistos:
            f.write(f"{nome}\t{tipo}\t{nptr}\t{ndim}\n")


def globais_do_s(nm, saida, mapa):
    """As variáveis do jogo definidas nos .s de port/build/gen/data (as tabelas e o .bss do executável, sem tipo no
    DWARF: o clang não grava a declaração extern) pela largura que o elftipos.py tirou do código C (mapa.tsv: objeto,
    deslocamento, largura, tipo), acrescentadas a SAIDA.tsv/.bin com os mesmos códigos. Posição que o C não tipa fica 0
    (o memdiff exige igual)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import elftipos
    ender = {q[2]: int(q[0], 16) for q in (l.split() for l in nm) if len(q) == 3 and q[1] in "bBdDrR"}
    tipos = {}
    for l in open(mapa):
        q = l.rstrip("\n").split("\t")
        if len(q) >= 4:
            tipos.setdefault(q[0], []).append((int(q[1]), int(q[2]), q[3]))
    # o alcance no port, como o elftipos.py (passo 5): rótulo que um objeto C da build x86 referencia (nm -u) e o que se
    # alcança dele por `.long Rótulo`
    usados = set()
    objs = sorted(str(f) for f in pathlib.Path("port/build/x86/obj").glob("*.o"))
    for j in range(0, len(objs), 200):
        for l in subprocess.run(["nm", "-u"] + objs[j:j + 200], capture_output=True, text=True).stdout.splitlines():
            q = l.split()
            if q and not l.endswith(":"):
                usados.add(q[-1])
    objetos = [elftipos.Objeto(arq) for arq in sorted(pathlib.Path("port/build/gen/data").glob("*.s"))]
    ext, ptrs = {}, {}
    for o in objetos:
        rot = sorted(o.rotulos.items(), key=lambda x: x[1])
        for j, (r, d) in enumerate(rot):
            ext[r] = (o, d, next((x[1] for x in rot[j + 1:] if x[1] > d), o.tam))
        ptrs[o.nome] = [(d.ofs, d.sim[0]) for d in o.dirs if d.tipo == "ptr" and d.sim]
    vivos = {r for r in ext if r in usados}
    fila = list(vivos)
    while fila:
        o, ini, fim = ext[fila.pop()]
        for ofs, alvo in ptrs[o.nome]:
            if ini <= ofs < fim and alvo in ext and alvo not in vivos:
                vivos.add(alvo)
                fila.append(alvo)
    ja = {l.split("\t")[0] for l in open(saida + ".tsv")}
    bdecl = open(saida + ".decl.bin", "rb").read()
    decl = {}
    for l in open(saida + ".decl.tsv"):
        q = l.split("\t")
        decl[q[0]] = bdecl[int(q[3], 16):int(q[3], 16) + int(q[2], 16)]
    tsv, binf = open(saida + ".tsv", "a"), open(saida + ".bin", "ab")
    pos = os.path.getsize(saida + ".bin")
    n = 0
    for o in objetos:
        m = bytearray(o.tam)
        for ofs, w, k in tipos.get(o.nome, []):
            if ofs + w > o.tam:
                continue
            c = {"ptr": 0x84, "float": 0x40 + w}.get(k, w if w in (2, 4, 8) else 1)
            if k == "bits" or w == 1:
                c = w if w in (2, 4, 8) else 1
            m[ofs] = c
            for j in range(1, w):
                m[ofs + j] = 0x80 if c != 1 else 1
        # depois, o que o próprio .s diz: `.long Símbolo` é ponteiro, sempre (o ligador o põe na ordem de cada build,
        # mesmo onde o C declara um vetor de u8); onde o C não tipa, `.float` é float e texto é byte (a conversão da
        # carga deixa esses como o .s diz, elftipos.py, passo 6)
        for d in o.dirs:
            if d.tipo == "ptr" and d.tam == 4:
                m[d.ofs:d.ofs + 4] = b"\x84\x80\x80\x80"
            elif d.tipo == "float" and d.tam == 4 and not any(m[d.ofs:d.ofs + 4]):
                m[d.ofs:d.ofs + 4] = b"\x44\x80\x80\x80"
            elif d.tipo == "texto" and not any(m[d.ofs:d.ofs + d.tam]):
                m[d.ofs:d.ofs + d.tam] = b"\x01" * d.tam
        # no .bss (zero na carga, gravado só pelo código, na ordem do host) o elftipos não tipa: o tipo da declaração
        # extern, repetido pelo alcance do rótulo (um vetor declarado com tamanho simbólico)
        if o.nome.endswith((".bss", ".sbss")):
            for r, d in o.rotulos.items():
                if r in decl and r in ext:
                    _, ini, fim = ext[r]
                    el = decl[r]
                    rep = (fim - ini) // len(el) if (fim - ini) % len(el) == 0 else 1
                    m[ini:ini + min(fim - ini, len(el) * rep)] = (el * rep)[:fim - ini]
        # e o que o port não usa (nenhum objeto C o referencia nem o alcança por ponteiro): o valor não importa no 360
        # e a carga não o converte, então fica de fora da comparação (0xFF)
        vivo = bytearray(o.tam)
        for r in o.rotulos:
            if r in vivos:
                _, ini, fim = ext[r]
                vivo[ini:fim] = b"\x01" * (fim - ini)
        for b in range(o.tam):
            if not vivo[b]:
                m[b] = 0xFF
        rot = sorted((d, r) for r, d in o.rotulos.items())
        for i, (d, r) in enumerate(rot):
            fim = rot[i + 1][0] if i + 1 < len(rot) else o.tam
            if r in ja or r not in ender or fim <= d:
                continue
            tsv.write("%s\t%x\t%x\t%x\n" % (r, ender[r], fim - d, pos))
            binf.write(m[d:fim])
            pos += fim - d
            n += 1
    tsv.close(); binf.close()
    print("GLOBAIS DO .s", n)


def main():
    a = sys.argv[1:]
    if a[:1] == ["--donos"] and len(a) >= 3:
        if "--tipos" in a:
            os.environ["CAMPOS_SAIDA"] = a[a.index("--tipos") + 1]
            del a[a.index("--tipos"):a.index("--tipos") + 2]
            escrever_decl(os.environ["CAMPOS_SAIDA"] + ".decl")
            os.environ["CAMPOS_DECL"] = os.environ["CAMPOS_SAIDA"] + ".decl"
        os.environ["CAMPOS_ATE"] = a[2]
        codigo = DONOS.replace("exec(POOL_PY)", POOL_PY)
        for l in gdb_py(a[1], codigo).splitlines():
            if l.startswith("DONO "):
                print(l[5:])
            elif "Exception" in l or "Error" in l or "Traceback" in l:
                sys.exit(l)
        if "CAMPOS_SAIDA" in os.environ and not pathlib.Path(os.environ["CAMPOS_SAIDA"] + ".pool.json").exists():
            sys.exit("sem contexto de memória do vblank pedido")
        return
    if a[:1] == ["--globais"] and len(a) >= 3:
        # as variáveis de dado do programa (nm), com o endereço: o gdb escolhe entre estáticas do mesmo nome por ele
        nm = subprocess.run(["nm", "-n", a[1]], capture_output=True, text=True).stdout.splitlines()
        with open(a[2] + ".nomes", "w") as f:
            for l in nm:
                q = l.split()
                if len(q) == 3 and q[1] in "bBdDrR":
                    f.write(f"{q[2]} {q[0]}\n")
        escrever_decl(a[2] + ".decl")
        os.environ["CAMPOS_SAIDA"] = a[2]
        print("\n".join(l for l in gdb_py(a[1], GLOBAIS).splitlines() if l.startswith(("GLOBAIS", "Traceback", "  ",
                                                                                         "gdb.", "Python"))))
        globais_do_s(nm, a[2], a[3] if len(a) > 3 else "port/build/ppc/elftipos/mapa.tsv")
        return
    if len(a) < 5:
        sys.exit(__doc__)
    comparar(a[0], a[1], a[2], a[3], a[4], int(a[5]) if len(a) > 5 else 1)


if __name__ == "__main__":
    main()
