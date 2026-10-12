#!/usr/bin/env python3
"""F1/F3: compara um bloco de memória das duas builds campo a campo, pelo tipo do DWARF (complemento do memdiff.py).

    campos.py X86.dump PPC.dump PROG_X86 TIPO ENDEREÇO[:ENDEREÇO_PPC] [N]
    campos.py --donos PROG_X86 VBLANK [env...]

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
(Heap_Alloc com três chamadores e o vblank). Rode na raiz, com as mesmas variáveis BT3_* da rodada comparada.
"""
import os, struct, subprocess, sys, tempfile
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

DONOS = r'''
import gdb, os
ATE = int(os.environ["CAMPOS_ATE"])
vivos, vb = {}, [0]
def pilha():
    out, fr = [], gdb.newest_frame().older()
    while fr is not None and len(out) < 3:
        sal = fr.find_sal()
        out.append((fr.name() or hex(fr.pc())) + ("@%s:%d" % (os.path.basename(sal.symtab.filename), sal.line) if sal.symtab else ""))
        fr = fr.older()
    return " < ".join(out)
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
            gdb.execute("kill"); gdb.execute("quit")
        return False
gdb.execute("set pagination off"); gdb.execute("set confirm off")
Aloc("Heap_Alloc"); Livre("Heap_Free"); Vb("Port_StateLog")
gdb.execute("run")
'''


def gdb_py(prog, codigo, pre=(), args=()):
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
        f.write(codigo)
    try:
        cmd = ["gdb", "-batch", "-nx"] + [x for p in pre for x in ("-ex", p)] + ["-x", f.name, "--args", prog] + list(args)
        return subprocess.run(cmd, capture_output=True, text=True).stdout
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


def main():
    a = sys.argv[1:]
    if a[:1] == ["--donos"] and len(a) >= 3:
        os.environ["CAMPOS_ATE"] = a[2]
        for l in gdb_py(a[1], DONOS).splitlines():
            if l.startswith("DONO "):
                print(l[5:])
        return
    if len(a) < 5:
        sys.exit(__doc__)
    comparar(a[0], a[1], a[2], a[3], a[4], int(a[5]) if len(a) > 5 else 1)


main()
