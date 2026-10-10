#!/usr/bin/env python3
"""F1: builds sem janela do port de PC, para comparar a ordem dos bytes.

    --alvo ppc   PowerPC 32 bits big-endian (Linux), roda no qemu-ppc: mesmo processador e mesma ordem de
                 bytes do Xbox 360                                        -> port/build/ppc/bt3_ppc
    --alvo x86   x86 32 bits (clang -m32): a referência little-endian, com ponteiros de 32 bits como o 360
                                                                          -> port/build/x86/bt3_x86
    --chamadas   registra a sequência de funções do jogo chamadas (port/x360/headless/chamadas.c), para
                 comparar as duas builds com port/x360/tools/chamadas.py   -> port/build/<alvo>c/
    --rastreio   (x86) instrumenta as leituras de memória: port/x360/headless/rastreio.c diz quais bytes de
                 cada arquivo do disco o jogo lê como número          -> port/build/x86r/bt3_x86r

As duas usam a aritmética em software da build de PC (-msoft-float chama port/src/softfloat_ps2.c): uma
diferença entre elas só pode vir da ordem dos bytes. Reaproveita portsrc e eeconst do upstream sem mudá-los;
janela, controle e rede são trocados por port/x360/headless/sem_janela.c.

Uso (no WSL, na raiz do repositório, depois do install.py):
    port/x360/tools/headless.py --alvo ppc
    BT3_DEMO=1 BT3_GS=none BT3_SOUND_TICKS=1 BT3_SETTINGS= qemu-ppc port/build/ppc/bt3_ppc
"""
import argparse, collections, concurrent.futures, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/tools"))
import eeconst, portsrc  # noqa: E402
sys.path.insert(0, str(ROOT / "port/x360/tools"))
import bitfields, elfdados  # noqa: E402

# Sem janela: desenho na GPU, controle, rede e a janela de opções ficam de fora, trocados por
# port/x360/headless/sem_janela.c (o que eles devolvem rodando sem janela). O som e o checksum de estado
# (state.c) entram com a SDL de mentira de port/x360/headless/SDL3/SDL.h. O que ainda faltar vira stub
# que avisa e para.
GS_SEM_SDL = {"gs_core", "gs_sony", "gs_vu1", "namefont", "state", "snd_adx", "snd_se"}


class Alvo:
    def __init__(self, nome, rastreio, chamadas=False):
        self.nome, self.rastreio, self.chamadas = nome, rastreio, chamadas
        self.be = nome == "ppc"  # big-endian: campos de bits na ordem do PS2 (bitfields.py)
        # -O2 nas duas (em -O0 o jogo se comporta diferente: há comportamento indefinido no código e a luta
        # diverge no quadro 2755).
        self.opt = "-O2"
        pasta = nome + ("r" if rastreio else "") + ("c" if chamadas else "")
        self.b = ROOT / "port/build" / pasta
        self.obj = self.b / "obj"
        self.exe = self.b / f"bt3_{pasta}"
        if nome == "ppc":
            p = "powerpc-linux-gnu-"
            self.cc, self.ld = [p + "gcc"], [p + "gcc", "-static"]
            self.asm = [p + "as", "-a32", "-mbig"]
            # -mno-gnu-attribute: na PowerPC o linker recusa juntar objetos marcados "float em software" com os
            # de float em hardware (a libc, plat_libm.c, o GS). A mistura é segura pelo mesmo motivo que no x86:
            # as pontes entre os dois lados (plat_libm.c, plat_fastvec.c) só passam inteiros. Ressalva: printf
            # de float a partir de código em software sai errado.
            # PORT_BIG_ENDIAN: os #ifdef do código do jogo para host big-endian (o mesmo símbolo no 360)
            # -fsigned-char: no PS2, na x86 e no MSVC o char tem sinal; no GCC de PowerPC, não
            self.soft = ["-msoft-float", "-mno-gnu-attribute", "-fsigned-char", "-DPORT_BIG_ENDIAN"]
            self.hard = ["-fsigned-char", "-DPORT_BIG_ENDIAN"]
        else:
            self.cc, self.ld = ["clang", "-m32"], ["clang", "-m32", "-static"]
            self.asm = ["as", "--32"]
            # -Wno-error=return-mismatch: o clang trata `return;` em função não-void como erro (3 arquivos do jogo)
            self.soft = ["-msoft-float", "-mno-sse", "-mno-mmx", "-mno-x87", "-malign-double", "-Wno-error=return-mismatch"]
            self.hard = ["-msse2", "-mfpmath=sse"]
        # Rastreio: uma chamada antes de cada leitura/escrita (rastreio.c), cópias de estrutura via memcpy.
        # As leituras de variáveis locais também: o jogo copia um vetor do arquivo para a pilha e lê um
        # componente dali (o ASan pularia por "provar" o acesso seguro).
        self.instr = ["-fsanitize=kernel-address", "-mllvm", "-asan-instrumentation-with-call-threshold=0",
                      "-mllvm", "-asan-stack=0", "-mllvm", "-asan-globals=0",
                      "-mllvm", "-asan-skip-promotable-allocas=0", "-mllvm", "-asan-use-stack-safety=0",
                      # sem isto o clang junta leituras de bytes vizinhos numa só (um RGBA de u8 vira um load de 4) e
                      # a largura anotada deixa de ser a do tipo no código
                      "-mllvm", "-aggressive-instcombine-max-scan-instrs=0",
                      # nem o vetorizador (o código do port, em SSE2, juntava leituras de int32 numa de 8)
                      "-fno-slp-vectorize", "-fno-vectorize"] if rastreio else []


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, capture_output=True, text=True, **kw)


def compile_ee(cmd, src, o, ee=True, be=False):
    """Como undefined.compile_ee: pré-processa, aplica as constantes do compilador do PS2 (ee), na build
    big-endian põe os campos de bits na ordem do PS2 (be, bitfields.py), compila."""
    r = run([a for a in cmd if a != "-c"] + ["-E", src])
    if r.returncode:
        return r
    i = pathlib.Path(str(o)[:-2] + ".i")
    texto = eeconst.transform(r.stdout) if ee else r.stdout
    i.write_text(bitfields.lsb(texto) if be else texto)
    c, skip = [], False
    for a in cmd:  # os -include já entraram no pré-processamento
        if skip:
            skip = False
        elif a == "-include":
            skip = True
        else:
            c.append(a)
    return run(c + ["-x", "cpp-output", i, "-o", o])


def jogo(args):
    alvo, f = args
    o = alvo.obj / (str(f.relative_to(ROOT / "src")).replace("/", "_")[:-2] + ".o")
    src, _, _ = portsrc.prepare(f)
    # -fno-eliminate-unused-debug-types: todas as structs no DWARF (o clang só grava as que alguma variável usa), para
    # layout.py comparar todas
    cmd = alvo.cc + ["-std=gnu89", "-c", alvo.opt, "-g", "-fno-eliminate-unused-debug-types", "-fno-strict-aliasing", "-ffp-contract=off", "-fcommon", "-w",
                     "-fno-pic", "-fno-builtin"] + alvo.soft + alvo.instr + (["-finstrument-functions"] if alvo.chamadas else []) + \
        ["-include", "port_libm.h", "-Iinclude", "-Iport/include", "-include", "port_compat.h", f"-I{f.parent}"]
    return o, f, compile_ee(cmd, src, o, be=alvo.be)


def construir(alvo):
    alvo.obj.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ProcessPoolExecutor(os.cpu_count()) as ex:
        res = list(ex.map(jogo, [(alvo, f) for f in portsrc.sources()]))
    falhas = [(f, r) for o, f, r in res if r.returncode]
    objs = [o for o, f, r in res if not r.returncode]

    def junta(f, o, r):
        (falhas.append((f, r)) if r.returncode else objs.append(o))

    # tabelas de dados do jogo (do disco do usuário, gen_data.py); na PowerPC o assembler grava big-endian
    dados = alvo.b / "obj_data"
    dados.mkdir(exist_ok=True)
    # No big-endian, cada diretiva com a largura que o jogo lê (elfdados.py, a partir do rastreio da x86).
    larg_elf = {}
    if alvo.be:
        rastreio = pathlib.Path(os.environ.get("BT3_RASTREIO_ELF", ROOT / "port/build/x86r/rastreio.tsv"))
        if rastreio.exists():
            larg_elf = elfdados.larguras_elf(rastreio)
            import hashlib
            (alvo.b / "ENTRADAS").write_text(f"rastreio {rastreio} sha1:{hashlib.sha1(rastreio.read_bytes()).hexdigest()[:10]}\n")
        else:
            print(f"AVISO: sem {rastreio}: tabelas de dados do executável com os tipos do splat (erradas no big-endian)")
    trocas, conflitos = collections.Counter(), []
    for s in sorted((ROOT / "port/build/gen/data").glob("*.s")):
        o = dados / (s.name[:-2] + ".o")
        fonte = s
        if alvo.be:
            texto, t, c = elfdados.retipar(s.read_text(), larg_elf.get(s.name[:-2], {}))
            trocas.update(t)
            conflitos += [f"{s.name[:-2]}\t{ofs}\t{d}\t{m}" for ofs, d, m in c]
            fonte = alvo.b / "gen_data" / s.name
            fonte.parent.mkdir(exist_ok=True)
            fonte.write_text(texto)
        junta(s, o, run(alvo.asm + [fonte, "-o", o]))
    if trocas:
        print("tabelas do executável retipadas: " + ", ".join(f"{k} {v}" for k, v in trocas.most_common()))
        (alvo.b / "elfdados.conflitos").write_text("".join(c + "\n" for c in conflitos))

    # biblioteca matemática do PS2 (software)
    for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c")):
        o = alvo.obj / ("nl_" + f.stem + ".o")
        junta(f, o, compile_ee(alvo.cc + ["-std=gnu89", "-c", alvo.opt, "-g", "-fno-pic"] + alvo.soft +
                               ["-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w", "-Iport/include",
                                f"-I{f.parent}", "-include", "newlib_shim.h"], f, o, be=alvo.be))

    # o GS do port, o som e o checksum de estado, sem janela (float em hardware, como na build de PC)
    for f in sorted((ROOT / "port/src/gs").glob("*.c")):
        if f.stem in GS_SEM_SDL:
            o = alvo.obj / ("gs_" + f.stem + ".o")
            junta(f, o, compile_ee(alvo.cc + ["-std=gnu99", "-c", alvo.opt, "-g", "-fno-pic", "-fno-strict-aliasing", "-w"] +
                                   alvo.hard + (alvo.instr if f.stem != "state" else []) +  # o checksum lê tudo em blocos de 8
                                   ["-Iport/x360/headless", f"-I{ROOT / 'port/build/gen/gs'}"], f, o, ee=False, be=alvo.be))

    # a camada do port: aritmética do PS2 em software, menos as duas pontes com a FPU (interface só de inteiros)
    for f in [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"] + sorted((ROOT / "port/src").glob("*.c")):
        o = alvo.obj / ("pc_" + f.stem + ".o")
        nomes = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"] if f.parent.name == "port" and f.parent.parent.name == "src" else []
        ponte = f.name in ("plat_libm.c", "plat_fastvec.c")
        soft = alvo.hard if ponte else \
            alvo.soft + ["-DSF_FAST_CALLS", "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
        cmd = alvo.cc + ["-std=gnu99", "-c", alvo.opt, "-g", "-fno-pic", "-fno-strict-aliasing", "-ffp-contract=off",
                         "-fno-builtin", "-w", "-Iinclude", "-Iport/include", "-Iport/src", "-Iport/x360/headless"] + \
            soft + nomes + alvo.instr + \
            (["-finstrument-functions"] if alvo.chamadas and f.stem == "plat_sys" else [])  # rand/srand do jogo
        junta(f, o, compile_ee(cmd, f, o, ee=not ponte and not nomes, be=alvo.be))

    extras = ["sem_janela"] + (["rastreio"] if alvo.rastreio else []) + (["inverter"] if alvo.nome == "ppc" else []) + \
        (["chamadas"] if alvo.chamadas else [])
    for nome in extras:
        o = alvo.obj / f"x360_{nome}.o"
        junta(nome, o, run(alvo.cc + ["-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-w"] + alvo.hard +
                           [f"port/x360/headless/{nome}.c", "-o", o]))

    for f, r in falhas:
        erro = next((l for l in r.stderr.splitlines() if "rror" in l), r.stderr.strip()[:200])
        print("FALHOU", pathlib.Path(str(f)).name, erro[:220])
    print(f"{alvo.nome}{' (rastreio)' if alvo.rastreio else ''}: {len(objs)} objetos, {len(falhas)} falhas")
    if falhas:
        sys.exit(1)
    ligar(alvo, objs)


def ligar(alvo, objs):
    # 0x20000000: longe das regiões do PS2 (0x100000.., 0x10000000, 0x12000000, 0x70000000), como a build de 64 bits.
    base = alvo.ld + ["-no-pie", "-w", "-o", alvo.exe] + objs
    resto = ["-lm", "-lpthread", "-Wl,-Ttext-segment=0x20000000", "-Wl,--defsym=D_3BE71C=0x3BE71C",
             "-Wl,--wrap=Progress_Main", f"-Wl,-Map={alvo.exe}.map"]
    if alvo.rastreio:
        resto += ["-Wl,--wrap=memcpy", "-Wl,--wrap=memmove", "-Wl,--wrap=memset"]
    if alvo.rastreio or alvo.nome == "ppc":  # o rastreio marca a origem; a PowerPC inverte os bytes (inverter.c)
        resto += ["-Wl,--wrap=Port_FileReadAt", "-Wl,--wrap=Bpe_Decode"]
    r = run(base + resto)
    faltam = sorted(set(re.findall(r"undefined reference to `(\w+)'", r.stderr)) - {"__real_memcpy", "__real_memmove", "__real_memset"})
    stubs = alvo.b / "stubs.c"
    stubs.write_text("\n".join(
        ['/* Gerado por port/x360/tools/headless.py: um stub por símbolo que a build sem janela não tem. */',
         'extern int printf(const char *, ...);', 'extern void exit(int);',
         'static void falta(const char *n) { printf("bt3: not implemented yet: %s\\n", n); exit(3); }'] +
        [f'void {s}(void) {{ falta("{s}"); }}' for s in faltam]) + "\n")
    (alvo.b / "stubs.txt").write_text("\n".join(faltam) + "\n")
    so = alvo.obj / "stubs.o"
    run(alvo.cc + ["-c", "-O1", "-w", "-fno-pic", stubs, "-o", so])
    r = run(base + [so] + resto)
    erros = [l for l in r.stderr.splitlines() if "multiple definition" in l or "undefined reference" in l or "rror" in l]
    print(f"{len(faltam)} stubs; link {'OK -> ' + str(alvo.exe.relative_to(ROOT)) if r.returncode == 0 else 'FALHOU'}")
    for l in erros[:15]:
        print("  " + l[-200:])
    if r.returncode:
        sys.exit(1)
    # onde cada objeto de dados do executável ficou (rastreio.c marca esses bytes com a origem "ELF:...")
    faixas = []
    for m in re.finditer(r"^ (\.data|\.bss|\.rodata)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\S*/obj_data/(\S+)\.o$",
                         pathlib.Path(str(alvo.exe) + ".map").read_text(errors="replace"), re.M):
        if int(m.group(3), 16):
            faixas.append(f"{m.group(4)}{m.group(1)} {m.group(2)} {m.group(3)}")
    pathlib.Path(str(alvo.exe) + ".elfdata").write_text("\n".join(faixas) + "\n")
    # as variáveis globais do jogo no programa, para o checksum e o dump de estado (state.c)
    r = run([sys.executable, ROOT / "port/x360/tools/mem.py", alvo.exe])
    if r.returncode:
        print((r.stdout + r.stderr).strip()[-300:])
        sys.exit(1)
    # carimbo: de que commit (e diferença) este programa saiu, para não comparar builds de origens diferentes
    (alvo.b / "CARIMBO").write_text(carimbo())
    # big-endian: as structs do jogo têm de ter o mesmo layout da referência x86 (bitfields.py, #ifdef do jogo)
    if alvo.be:
        ref = ROOT / "port/build/x86/bt3_x86"
        if not ref.exists():
            print("AVISO: sem port/build/x86/bt3_x86: layout das structs não conferido")
            return
        if (ref.parent / "CARIMBO").read_text() != carimbo():
            print("AVISO: a referência x86 é de outra origem: refaça-a antes de confiar no layout")
        r = run([sys.executable, ROOT / "port/x360/tools/layout.py", ref, alvo.exe])
        print(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()[-300:])
        if r.returncode:
            print("\n".join(r.stdout.strip().splitlines()[:-1][:60]))
            sys.exit(1)


def carimbo():
    try:
        return pathlib.Path(ROOT / "port/build/ENVIADO").read_text()
    except OSError:
        h = run(["git", "rev-parse", "--short", "HEAD"]).stdout.strip()
        d = run(["git", "diff", "HEAD"]).stdout
        novos = run(["git", "ls-files", "-o", "--exclude-standard"]).stdout.split()
        import hashlib
        sha = hashlib.sha1(d.encode())
        for n in novos:
            sha.update((ROOT / n).read_bytes())
        return f"{h} diff:{sha.hexdigest()[:10]}\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--alvo", choices=["ppc", "x86"], required=True)
    ap.add_argument("--rastreio", action="store_true")
    ap.add_argument("--chamadas", action="store_true", help="registra a sequência de funções do jogo (BT3_CHAMADAS)")
    a = ap.parse_args()
    if a.rastreio and a.alvo != "x86":
        sys.exit("o rastreio roda na x86 (é a build em que o jogo já funciona)")
    construir(Alvo(a.alvo, a.rastreio, a.chamadas))


main()
