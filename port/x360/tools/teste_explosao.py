#!/usr/bin/env python3
"""F8, item B (docs/x360/plano/F8/11-cpu-vu0.md): prova, sem o jogo, que o EftBurst_DrawModel com a tabela por vértice
(o ramo PORT de src/battle/eft_d_b.c) manda para a lista de desenho os mesmos bits que o original (o ramo #else do
mesmo arquivo), e deixa a VU0 emulada no mesmo estado.

    python3 port/x360/tools/teste_explosao.py [--mutantes] [--identidade] [--original REV] [--pasta DIR]

Monta port/x360/tools/teste_explosao.c com o arquivo do jogo duas vezes (velho: o fonte sem os blocos PORT da função;
novo: o fonte como está), a VU0 do port e o modelo de float de cada build, e roda:
  x86      gcc -m32 com as opções do port de PC (port/tools/undefined.py): o jogo em float por software (o modelo do PS2,
           port/src/softfloat_ps2.c), a VU0 com REF_VU0_EXTERN_ARITH e os caminhos rápidos (plat_fastvec.c);
  x86-360  o modelo do 360 na x86: o jogo e a VU0 em float de hardware (SSE), a soma e a multiplicação da VU0 pela FPU
           (port/x360/src/hw/softfloat_ps2_inl.h) e a divisão de port/x360/src/f2/float360.c, tudo em truncamento
           como o _controlfp(_RC_CHOP) do console (sem o cl: as fusões em fmadds não entram aqui);
  ppc      só onde houver powerpc-linux-gnu-gcc e qemu-ppc (o WSL): as opções do headless.py.
O binário da x86 é ligado sem libc (port/x360/tools/teste_explosao_rt.c): esta máquina não tem a de 32 bits.
--mutantes: cada mutante de MUTANTES (um erro plausível na tabela) tem de ser recusado; sai com 1 se algum passar.
--identidade: confere 1.0 * q == q para todos os 2^32 padrões (sem ela, um a cada 257).
--original REV: confere que o ramo velho é, letra por letra, o arquivo de REV (padrão f5-alfa, se existir).
"""
import argparse, os, pathlib, re, shutil, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/tools"))
import eeconst, portsrc  # noqa: E402

FONTE = ROOT / "src/battle/eft_d_b.c"
FERR = ROOT / "port/x360/tools"
WRAP = ["Mtx_ProjectPointStq", "IVec4_ToFloat12", "IVec4_ToFloat", "Vec4_Scale", "Vec4_Sub", "Vec4_Add", "Vec4_Clamp",
        "Vec4_ToInt", "Vec3_Dot", "Port_sqrtf", "Vec4_Copy", "IVec4_Set"]

# Erros plausíveis na tabela; o teste tem de pegar cada um. (nome, trecho do ramo PORT, troca)
MUTANTES = [
    ("tabela sem zerar por grupo (ignora a escala e o vetor do grupo)", "if (++sEftBurstStamp == 0) {",
     "if (sEftBurstStamp == 0 && ++sEftBurstStamp) {"),
    ("Q da tabela errado", "c->q = stq[2];", "c->q = stq[2] * 1.0001f;"),
    ("cor do canto guardada por vértice", "IVec4_ToFloat(colF, rgba);\n                        rgbaq[k][1]",
     "{ static EftFVec cc[EFT_BURST_VTX_MAX]; static u32 cs[EFT_BURST_VTX_MAX]; IVec4_ToFloat(colF, rgba);"
     " if ((u32)idx < EFT_BURST_VTX_MAX) { if (cs[idx] != sEftBurstStamp) { memcpy(cc[idx], colF, 16);"
     " cs[idx] = sEftBurstStamp; } memcpy(colF, cc[idx], 16); } }\n                        rgbaq[k][1]"),
    ("ordem trocada (a normal convertida depois da cor: a VU0 sai diferente)",
     "IVec4_ToFloat(colF, rgba);\n                        rgbaq[k][1]",
     "IVec4_ToFloat(colF, rgba); IVec4_ToFloat12(nrm[k], ang);\n                        rgbaq[k][1]"),
    ("tabela sem o limite (índice negativo vira 0)", "if ((u32)idx < EFT_BURST_VTX_MAX) {",
     "if (idx < 0) idx = 0; if ((u32)idx < EFT_BURST_VTX_MAX) {"),
    ("luz sem o perto (len < dist)", "if (c->near) {\n                            Vec4_Add",
     "if (1) {\n                            Vec4_Add"),
]


def velho(texto):
    """O fonte sem os blocos `#ifdef PORT` (fica o `#else`) dentro do `#if 0` da tentativa do EftBurst_DrawModel."""
    linhas = texto.split("\n")
    ini = next(i for i, l in enumerate(linhas) if l.startswith("#if 0 /* ATTEMPT: not matching: 597 instructions"))
    prof, fim = 0, None
    for i in range(ini, len(linhas)):
        if linhas[i].startswith("#if"):
            prof += 1
        elif linhas[i].startswith("#endif"):
            prof -= 1
            if prof == 0:
                fim = i
                break
    saida, pilha, solto = linhas[:ini + 1], [], False  # pilha: (é PORT, no ramo #else)
    for l in linhas[ini + 1:fim]:
        if solto and l == "" and saida[-1] == "":  # a linha em branco que separava o bloco PORT tirado
            solto = False
            continue
        solto = False
        if l.startswith("#if"):
            pilha.append([l.split("/*")[0].strip() == "#ifdef PORT", False])
            if pilha[-1][0]:
                continue
        elif l.startswith("#else") and pilha and pilha[-1][0]:
            pilha[-1][1] = True
            continue
        elif l.startswith("#endif") and pilha:
            p = pilha.pop()
            if p[0]:
                solto = not p[1]
                continue
        if any(p[0] and not p[1] for p in pilha):
            continue
        saida.append(l)
    return "\n".join(saida + linhas[fim:])


def run(cmd, **kw):
    r = subprocess.run([str(c) for c in cmd], cwd=ROOT, capture_output=True, text=True, **kw)
    if r.returncode:
        raise SystemExit("falhou: " + " ".join(str(c) for c in cmd)[:300] + "\n" + (r.stderr or r.stdout)[-1500:])
    return r


class Alvo:
    def __init__(self, nome, pasta):
        self.nome, self.b = nome, pasta / nome
        self.b.mkdir(parents=True, exist_ok=True)
        base = ["-O2", "-g", "-fno-pic", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w",
                "-ffunction-sections", "-fdata-sections"]
        if nome == "ppc":
            self.cc, self.livre = ["powerpc-linux-gnu-gcc"], False
            self.soft = ["-msoft-float", "-mno-gnu-attribute", "-fsigned-char", "-DPORT_BIG_ENDIAN", "-frounding-math"]
            self.hard = ["-fsigned-char", "-DPORT_BIG_ENDIAN"]
            self.exe = ["qemu-ppc"]
        else:
            # sem libc de 32 bits: os headers da libc pedem gnu/stubs-32.h, que fica vazio aqui
            shim = self.b / "shim/gnu"
            shim.mkdir(parents=True, exist_ok=True)
            (shim / "stubs-32.h").write_text("")
            multi = subprocess.run(["gcc", "-print-multiarch"], capture_output=True, text=True).stdout.strip()
            self.cc, self.livre = ["gcc", "-m32", "-fno-stack-protector", f"-I{self.b / 'shim'}"] + \
                ([f"-idirafter/usr/include/{multi}"] if multi else []), True
            if nome == "x86":  # port/tools/undefined.py
                self.soft = ["-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double"]
                self.hard = ["-malign-double"]
            else:  # x86-360
                self.soft = self.hard = ["-msse2", "-mfpmath=sse", "-malign-double", "-DMODELO_360"]
            self.exe = []
        self.base = base
        self.jogo = self.cc + ["-std=gnu89", "-c"] + base + ["-fcommon"] + self.soft + \
            ["-include", "port_libm.h", "-Iinclude", "-Iport/include", "-include", "port_compat.h", "-Isrc/battle"]

    def ee(self, cmd, texto_ou_fonte, o):
        """Como port/tools/undefined.py compile_ee: pré-processa, aplica as constantes do PS2 e compila."""
        if isinstance(texto_ou_fonte, str):
            src = self.b / (o.stem + ".c")
            src.write_text(texto_ou_fonte)
        else:
            src = texto_ou_fonte
        r = run([a for a in cmd if a != "-c"] + ["-E", src])
        i = o.with_suffix(".i")
        i.write_text(eeconst.transform(r.stdout))
        run(cmd + ["-fpreprocessed", i, "-o", o])
        return o

    def comuns(self):
        """A VU0, o float, a newlib (sqrtf) e o teste: o mesmo para o velho e o novo."""
        objs = []
        nomes = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"]
        porta = self.cc + ["-std=gnu99", "-c"] + self.base + ["-Iinclude", "-Iport/include"]
        if self.nome == "x86-360":
            for f in ["src/port/vu0_a.c", "src/port/vu0_b.c"]:  # o softfloat_ps2_inl.h do 360 antes do de PC
                o = self.b / ("pc_" + pathlib.Path(f).stem + ".o")
                run(porta + self.hard + ["-Iport/x360/src/hw", "-Iport/src"] + nomes + [f, "-o", o])
                objs.append(o)
            o = self.b / "float360.o"
            run(porta + self.hard + ["port/x360/src/f2/float360.c", "-o", o])
            objs.append(o)
        else:
            soft = ["-DSF_FAST_CALLS", "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
            for f in ["src/port/vu0_a.c", "src/port/vu0_b.c"]:
                o = self.b / ("pc_" + pathlib.Path(f).stem + ".o")
                run(porta + ["-Iport/src"] + self.soft + soft + nomes + [f, "-o", o])
                objs.append(o)
            objs.append(self.ee(porta + ["-Iport/src"] + self.soft + soft, ROOT / "port/src/softfloat_ps2.c",
                                self.b / "pc_softfloat_ps2.o"))
            for f in ["plat_libm", "plat_fastvec"]:
                o = self.b / ("pc_" + f + ".o")
                run(porta + ["-Iport/src"] + self.hard + [f"port/src/{f}.c", "-o", o])
                objs.append(o)
        nl = ROOT / "port/third_party/newlib_libm"
        for f in ["ef_sqrt", "wrappers"]:
            objs.append(self.ee(self.cc + ["-std=gnu89", "-c"] + self.base + self.soft +
                                ["-Iport/include", f"-I{nl}", "-include", "newlib_shim.h"], nl / f"{f}.c",
                                self.b / f"nl_{f}.o"))
        t = self.b / "teste.o"
        run(self.jogo + (["-DTESTE_LIBC"] if not self.livre else []) + [FERR / "teste_explosao.c", "-o", t])
        objs.append(t)
        if self.livre:
            o = self.b / "rt.o"
            run(self.cc + ["-c", "-O2", "-ffreestanding", "-fno-builtin", FERR / "teste_explosao_rt.c", "-o", o])
            objs.append(o)
        return objs

    def jogo_obj(self, texto, nome):
        """O eft_d_b.c como o port o compila (as tentativas ligadas), com o EftBurst_DrawModel renomeado e o resto
        fraco (as duas cópias trazem as mesmas outras funções; as que o desenho chama vêm do teste)."""
        t, _, _ = portsrc.enable_attempts(texto)
        o = self.ee(self.jogo, t, self.b / f"eft_{nome}.o")
        # só os definidos ficam fracos: um --weaken geral deixaria fraca também a referência a uma função que falta,
        # que o link estático liga ao endereço 0 sem aviso
        defs = run(["nm", "-g", "--defined-only", o]).stdout.split()
        defs = [d for d in defs[2::3] if d != "EftBurst_DrawModel"]
        run(["objcopy", f"--redefine-sym=EftBurst_DrawModel=EftBurst_DrawModel_{nome.split('_')[0].capitalize()}"] +
            [f"--weaken-symbol={d}" for d in defs] + [o])
        return o

    def liga_roda(self, objs, nome, args):
        exe = self.b / f"teste_{nome}"
        wrap = [f"-Wl,--wrap={w}" for w in WRAP]
        if self.livre:
            cmd = self.cc + ["-nostdlib", "-static", "-no-pie", "-Wl,--gc-sections", "-Wl,-e,_start"]
        else:
            cmd = self.cc + ["-static", "-Wl,--gc-sections"]
        run(cmd + wrap + ["-o", exe] + objs + ([] if self.livre else ["-lm"]))
        r = subprocess.run(self.exe + [str(exe)] + args, cwd=ROOT, capture_output=True, text=True, timeout=3600)
        return r.returncode, r.stdout + r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mutantes", action="store_true")
    ap.add_argument("--identidade", action="store_true")
    ap.add_argument("--original", default="f5-alfa")
    ap.add_argument("--pasta")
    ap.add_argument("--alvos", default="x86,x86-360,ppc")
    a = ap.parse_args()

    novo = FONTE.read_text()
    v = velho(novo)
    orig = subprocess.run(["git", "show", f"{a.original}:src/battle/eft_d_b.c"], cwd=ROOT, capture_output=True, text=True)
    if orig.returncode == 0:
        if orig.stdout != v:
            print(f"FALHOU: o ramo original do fonte não é o arquivo de {a.original}")
            return 1
        print(f"o ramo original (sem os blocos PORT da função) é, letra por letra, o eft_d_b.c de {a.original}")
    else:
        print(f"(sem {a.original} no git: o ramo original não foi conferido contra ele)")

    pasta = pathlib.Path(a.pasta) if a.pasta else pathlib.Path(tempfile.mkdtemp(prefix="teste-explosao."))
    falhou = 0
    for nome in a.alvos.split(","):
        if nome == "ppc" and not (shutil.which("powerpc-linux-gnu-gcc") and shutil.which("qemu-ppc")):
            print("ppc: sem powerpc-linux-gnu-gcc / qemu-ppc nesta máquina (roda no WSL): não rodado")
            continue
        alvo = Alvo(nome, pasta)
        comuns = alvo.comuns() + [alvo.jogo_obj(v, "velho")]
        rc, out = alvo.liga_roda(comuns + [alvo.jogo_obj(novo, "novo")], "novo", ["i"] if a.identidade else [])
        print(f"== {nome}: " + ("igual" if rc == 0 else "DIFERE"))
        print("\n".join("   " + l for l in out.strip().split("\n") if not l.startswith("cena ")))
        falhou |= rc != 0
        if a.mutantes:
            for i, (m, de, para) in enumerate(MUTANTES):
                if novo.count(de) != 1:
                    print(f"   mutante '{m}': trecho não achado uma vez só no fonte")
                    falhou = 1
                    continue
                o = alvo.jogo_obj(novo.replace(de, para), f"novo_m{i}")
                rc, out = alvo.liga_roda(comuns + [o], f"m{i}", [])
                motivo = next((l for l in out.split("\n") if l.startswith("DIFERE") or l.startswith("1.0")), "")
                print(f"   mutante '{m}': " + (f"recusado ({motivo[:110]})" if rc != 0 else "ACEITO (o teste não o pega)"))
                falhou |= rc == 0
    if not a.pasta:
        shutil.rmtree(pasta, ignore_errors=True)
    print("tudo certo" if not falhou else "FALHOU")
    return 1 if falhou else 0


if __name__ == "__main__":
    sys.exit(main())
