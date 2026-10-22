#!/usr/bin/env python3
"""F1: builds sem janela do port de PC, para comparar a ordem dos bytes.

    --alvo ppc   PowerPC 32 bits big-endian (Linux), roda no qemu-ppc: mesmo processador e mesma ordem de
                 bytes do Xbox 360                                        -> port/build/ppc/bt3_ppc
                 As tabelas do executável seguem o caminho do produto (decisão 13): esqueleto port/data, valores
                 copiados do SLUS na partida (bt3_ppc.dat) e trocados pelo mapa estático (bt3_ppc.swap, de
                 elftipos.py sobre os .i da build x86, que tem de existir e ser da mesma origem).
    --alvo x86   x86 32 bits (clang -m32): a referência little-endian, com ponteiros de 32 bits como o 360
                                                                          -> port/build/x86/bt3_x86
    --chamadas   registra a sequência de funções do jogo chamadas (port/x360/headless/chamadas.c), para
                 comparar as duas builds com port/x360/tools/chamadas.py   -> port/build/<alvo>c/
    --rastreio   (x86) instrumenta as leituras de memória: port/x360/headless/rastreio.c diz quais bytes de
                 cada arquivo do disco o jogo lê como número          -> port/build/x86r/bt3_x86r
    --gpu        (F5, etapa 8) o GS em modo gpu com um backend falso (port/x360/headless/gpu_falso.c): o gs_draw.c de
                 verdade grava a lista do quadro, os programas da VU1 vão pelo caminho dos shaders (hle_program*) como
                 no 360, e nada se desenha. Só para medir e provar o custo da gravação da VU1 (BT3_GS=gpu)
                                                                      -> port/build/<alvo>g/bt3_<alvo>g
    --vu1        (x86 com --gpu) compila também com -DBT3_TESTE_VU1: a otimização da gravação da VU1, que na PPC vem
                 com o PORT_BIG_ENDIAN, para medir e provar a mesma mudança na x86 -> port/build/x86gv/bt3_x86gv
    --gravador   (F6, etapa 2, rede 4) o som com o dispositivo gravador (port/x360/headless/gravador.c) no lugar dos stubs
                 da SDL: o snd_adx.c e o snd_se.c do port dentro de grava_adx.c e grava_se.c, com -DBT3_GRAVADOR; com
                 BT3_SOUND=1 o dispositivo abre e BT3_GRAVADOR=arquivo grava o PCM de cada fluxo por vblank
                                                                      -> port/build/<alvo>a/bt3_<alvo>a
    --teste-som  (F6, etapa 2, rede 3) só o teste das amostras (port/x360/headless/teste_som.c), sem o jogo: os
                 decodificadores do port como o gravador os compila    -> port/build/<alvo>t/teste_som
O rastro de som (port/x360/headless/som.c, F6 rede 2: BT3_SOM=arquivo) entra em todas as builds, por --wrap.

As duas usam a aritmética em software da build de PC (-msoft-float chama port/src/softfloat_ps2.c): uma
diferença entre elas só pode vir da ordem dos bytes. Reaproveita portsrc e eeconst do upstream sem mudá-los;
janela, controle e rede são trocados por port/x360/headless/sem_janela.c.

Uso (no WSL, na raiz do repositório, depois do install.py):
    port/x360/tools/headless.py --alvo x86 && port/x360/tools/headless.py --alvo ppc
    BT3_DEMO=1 BT3_GS=none BT3_SOUND_TICKS=1 BT3_SETTINGS= qemu-ppc port/build/ppc/bt3_ppc
"""
import argparse, bisect, collections, concurrent.futures, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/tools"))
import eeconst, portsrc  # noqa: E402
sys.path.insert(0, str(ROOT / "port/x360/tools"))
import bitfields  # noqa: E402

# Sem janela: desenho na GPU, controle, rede e a janela de opções ficam de fora, trocados por
# port/x360/headless/sem_janela.c (o que eles devolvem rodando sem janela). O som e o checksum de estado
# (state.c) entram com a SDL de mentira de port/x360/headless/SDL3/SDL.h. O que ainda faltar vira stub
# que avisa e para.
GS_SEM_SDL = {"gs_core", "gs_sony", "gs_vu1", "namefont", "state", "snd_adx", "snd_se"}
# Fora do rastreio (compilados sem a instrumentação na x86r): o checksum de estado (state.c), que lê tudo em blocos de
# 8, e as letras dos nomes de estágio e de música (namefont.c, F4 etapa 5b), código do port que copia os pixels e a
# paleta das folhas de nomes do CharSel e do TeamSel em blocos de 4 bytes: o rastreio via essas cópias como leitura de
# número, e a origem delas ia junto para a pilha, onde o FlashDraw_TransformQuad lia um FlashQuad local "do arquivo".
# Os números que o namefont lê (o cabeçalho do TexEntry e do pacote de imagem) o jogo também lê
SEM_RASTREIO = {"state", "namefont"}


class Alvo:
    def __init__(self, nome, rastreio, chamadas=False, gpu=False, vu1=False, gravador=False, teste_som=False):
        self.nome, self.rastreio, self.chamadas, self.gpu, self.vu1 = nome, rastreio, chamadas, gpu, vu1
        self.gravador, self.teste_som = gravador, teste_som
        self.be = nome == "ppc"  # big-endian: campos de bits na ordem do PS2 (bitfields.py)
        # -O2 nas duas (em -O0 o jogo se comporta diferente: há comportamento indefinido no código e a luta
        # diverge no quadro 2755).
        self.opt = "-O2"
        pasta = nome + ("r" if rastreio else "") + ("c" if chamadas else "") + ("g" if gpu else "") + ("v" if vu1 else "") + \
            ("a" if gravador else "") + ("t" if teste_som else "")
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
            if vu1:  # (F5, etapa 8) a otimização da gravação da VU1, que na PPC já vem com o PORT_BIG_ENDIAN
                self.soft += ["-DBT3_TESTE_VU1"]
                self.hard += ["-DBT3_TESTE_VU1"]
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


# (F6, etapa 2) o som do port com o dispositivo gravador: os dois arquivos dentro dos que os expõem aos testes
GRAVA = {"snd_adx": "port/x360/headless/grava_adx.c", "snd_se": "port/x360/headless/grava_se.c"}


def flags_som(alvo):
    """As do som do port nesta build (as do laço do port/src/gs abaixo), com o gravador."""
    fma = ["-ffp-contract=off"] if alvo.be else []
    return alvo.cc + ["-std=gnu99", "-c", alvo.opt, "-g", "-fno-pic", "-fno-strict-aliasing", "-w", "-DBT3_GRAVADOR"] + \
        alvo.hard + fma + ["-Iport/x360/headless", f"-I{ROOT / 'port/build/gen/gs'}"]


def construir_teste_som(alvo):
    """(F6, etapa 2, rede 3) port/build/<alvo>t/teste_som: o snd_adx.c e o snd_se.c como a build --gravador os compila,
    o gravador, o estado dos players (plat_sndstate.c) e o teste, sem o jogo."""
    alvo.obj.mkdir(parents=True, exist_ok=True)
    objs, falhas = [], []
    for nome, src in [("gs_snd_adx", GRAVA["snd_adx"]), ("gs_snd_se", GRAVA["snd_se"]),
                      ("x360_gravador", "port/x360/headless/gravador.c"), ("pc_plat_sndstate", "port/src/plat_sndstate.c"),
                      ("x360_teste_som", "port/x360/headless/teste_som.c")]:
        o = alvo.obj / f"{nome}.o"
        r = compile_ee(flags_som(alvo), src, o, ee=False, be=alvo.be)
        (falhas.append((src, r)) if r.returncode else objs.append(o))
    for f, r in falhas:
        print("FALHOU", f, next((l for l in r.stderr.splitlines() if "rror" in l), r.stderr.strip()[:200])[:220])
    if falhas:
        sys.exit(1)
    exe = alvo.b / "teste_som"
    r = run(alvo.ld + ["-no-pie", "-o", exe] + objs + ["-lm"])
    if r.returncode:
        print("link FALHOU:", r.stderr.strip()[-400:])
        sys.exit(1)
    (alvo.b / "CARIMBO").write_text(carimbo())
    print(f"teste_som: {len(objs)} objetos; link OK -> {exe.relative_to(ROOT)}")


def construir(alvo):
    if alvo.teste_som:
        return construir_teste_som(alvo)
    alvo.obj.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ProcessPoolExecutor(os.cpu_count()) as ex:
        res = list(ex.map(jogo, [(alvo, f) for f in portsrc.sources()]))
    falhas = [(f, r) for o, f, r in res if r.returncode]
    objs = [o for o, f, r in res if not r.returncode]

    def junta(f, o, r):
        (falhas.append((f, r)) if r.returncode else objs.append(o))

    # tabelas de dados do jogo. x86: com os valores (port/build/gen/data, do gen_data.py sobre o disco do usuário).
    # PowerPC: o caminho do produto (decisão 13), o esqueleto versionado do upstream (port/data, make_skeleton.py:
    # rótulos, `.space` onde entram os valores, `.long Símbolo`); na partida Port_LoadGameData copia os valores do SLUS
    # pela lista <programa>.dat, confere o hash e troca a ordem de cada posição viva pelo mapa <programa>.swap
    # (esqueleto(), depois da ligação).
    dados = alvo.b / "obj_data"
    dados.mkdir(exist_ok=True)
    origem = ROOT / ("port/data" if alvo.be else "port/build/gen/data")
    if alvo.be:  # restos de quando a PowerPC retipava gen/data pelo rastreio (elfdados.py)
        import shutil
        shutil.rmtree(alvo.b / "gen_data", ignore_errors=True)
        (alvo.b / "elfdados.conflitos").unlink(missing_ok=True)
    for s in sorted(origem.glob("*.s")):
        o = dados / (s.name[:-2] + ".o")
        junta(s, o, run(alvo.asm + [s, "-o", o]))

    # biblioteca matemática do PS2 (software)
    for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c")):
        o = alvo.obj / ("nl_" + f.stem + ".o")
        junta(f, o, compile_ee(alvo.cc + ["-std=gnu89", "-c", alvo.opt, "-g", "-fno-pic"] + alvo.soft +
                               ["-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w", "-Iport/include",
                                f"-I{f.parent}", "-include", "newlib_shim.h"], f, o, be=alvo.be))

    # o GS do port, o som e o checksum de estado, sem janela (float em hardware, como na build de PC). Na PPC sem
    # multiplicação-soma fundida: o GCC junta a * b + c num fmadd, que arredonda uma vez só, e o rasterizador de
    # referência saía com pixels de borda diferentes da x86 (que não tem FMA) em 4 de 125 quadros do título
    fma = ["-ffp-contract=off"] if alvo.be else []
    for f in sorted((ROOT / "port/src/gs").glob("*.c")):
        # (--gpu: o gs_draw.c de verdade, com o pedaço de SDL do 360 que o compila sem janela, como o c89.py faz)
        if f.stem in GS_SEM_SDL or (alvo.gpu and f.stem == "gs_draw"):
            o = alvo.obj / ("gs_" + f.stem + ".o")
            sdl360 = ["-Iport/x360/src/gs/sdl"] if f.stem == "gs_draw" else []
            # (--gpu: -DBT3_GPU_FALSO no gs_core.c, a medida dos bytes da cadeia por quadro, e no gs_draw.c, as chaves de teste
            # da divisão do degrau 2; as builds sem --gpu não a têm)
            if alvo.gravador and f.stem in GRAVA:  # (F6, rede 4: o mesmo arquivo, dentro do que o expõe ao gravador)
                junta(f, o, compile_ee(flags_som(alvo) + alvo.instr, GRAVA[f.stem], o, ee=False, be=alvo.be))
                continue
            junta(f, o, compile_ee(alvo.cc + ["-std=gnu99", "-c", alvo.opt, "-g", "-fno-pic", "-fno-strict-aliasing", "-w"] +
                                   (["-DBT3_GPU_FALSO"] if alvo.gpu and f.stem in ("gs_core", "gs_draw") else []) + alvo.hard + fma + (alvo.instr if f.stem not in SEM_RASTREIO else []) + sdl360 +
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

    extras = ["sem_janela", "roteiro", "imagens", "ponte_pow", "passos", "memoria", "som"] + (["rastreio"] if alvo.rastreio else []) + (["inverter"] if alvo.nome == "ppc" else []) + \
        (["chamadas"] if alvo.chamadas else []) + (["gpu_falso"] if alvo.gpu else []) + (["gravador"] if alvo.gravador else [])
    for nome in extras:
        o = alvo.obj / f"x360_{nome}.o"
        # (o roteiro lê o save do jogo pela estrutura dele, include/sys/save.h, com os tipos do jogo: port_compat.h)
        tipos_jogo = ["-Iinclude", "-Iport/include", "-include", "port_compat.h"] if nome == "roteiro" else []
        # (F5, etapa 3: a medida de tempo diz no log de que commit a build saiu, o mesmo CARIMBO desta pasta)
        if nome == "passos":
            tipos_jogo = [f'-DBT3_CARIMBO="{carimbo().strip()}"']
        # (--gpu: o sem_janela.c sem as entradas GsGpu_* vazias, que vêm do gs_draw.c, como na build do 360)
        gpu_def = ["-DX360_GS_D3D"] if alvo.gpu and nome == "sem_janela" else []
        # (F6, rede 4: o rastro de som com o relógio do gravador e os estragos de propósito; o gravador com a SDL de teste)
        if alvo.gravador and nome in ("som", "gravador"):
            gpu_def = ["-DBT3_GRAVADOR", "-Iport/x360/headless"]
        junta(nome, o, run(alvo.cc + ["-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-w"] + alvo.hard + gpu_def + tipos_jogo +
                           [f"port/x360/headless/{nome}.c", "-o", o]))
    # big-endian: a conversão por formato (port/x360/conv), que o jogo chama em tempo de jogo (Port_ConvertAnim)
    if alvo.be:
        for f in sorted((ROOT / "port/x360/conv").glob("*.c")):
            if f.stem == "conv_tool":
                continue
            o = alvo.obj / f"conv_{f.stem}.o"
            junta(f, o, run(alvo.cc + ["-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-Wall"] + alvo.hard + [f, "-o", o]))

    for f, r in falhas:
        erro = next((l for l in r.stderr.splitlines() if "rror" in l), r.stderr.strip()[:200])
        print("FALHOU", pathlib.Path(str(f)).name, erro[:220])
    print(f"{alvo.nome}{' (rastreio)' if alvo.rastreio else ''}: {len(objs)} objetos, {len(falhas)} falhas")
    if falhas:
        sys.exit(1)
    ligar(alvo, objs)


def ligar(alvo, objs):
    # O pow de double do replay (port/x360/headless/ponte_pow.c, F4 tarefa 9c-2): o ligador recebe uma cópia do objeto
    # do menu_za_d.c com a chamada renomeada para a ponte de inteiros; o objeto compilado (o que se compara com a base)
    # não muda, e os outros que chamam pow, de float em hardware (o som do GS), continuam com o da libm, por isso não um
    # --wrap=pow, que desviaria todos.
    za = alvo.obj / "menu_menu_za_d.o"
    if za in objs:
        copia = alvo.b / "ligar" / za.name  # (fora de obj/, que se compara com a base)
        copia.parent.mkdir(exist_ok=True)
        objcopy = "powerpc-linux-gnu-objcopy" if alvo.nome == "ppc" else "objcopy"
        r = run([objcopy, "--redefine-sym", "pow=Port_PowDuplo", za, copia])
        if r.returncode:
            print("FALHOU objcopy do menu_za_d.o:", r.stderr.strip()[:200])
            sys.exit(1)
        objs = [copia if o == za else o for o in objs]
    # 0x20000000: longe das regiões do PS2 (0x100000.., 0x10000000, 0x12000000, 0x70000000), como a build de 64 bits.
    base = alvo.ld + ["-no-pie", "-w", "-o", alvo.exe] + objs
    resto = ["-lm", "-lpthread", "-Wl,-Ttext-segment=0x20000000", "-Wl,--defsym=D_3BE71C=0x3BE71C",
             "-Wl,--wrap=BtlPool_Alloc", "-Wl,--wrap=BtlPool_Free", "-Wl,--wrap=BtlPool_Reset", "-Wl,--wrap=BtlPool_Init",
             "-Wl,--wrap=BtlPool_Term", "-Wl,--wrap=Port_StateLog",
             "-Wl,--wrap=Progress_Main", f"-Wl,-Map={alvo.exe}.map",
             # as imagens das telas (port/x360/headless/imagens.c, F4 etapa 6): a lista de cada quadro e as cargas fora
             # dela passam por lá, que desenha só as janelas antes de cada imagem e grava a imagem
             "-Wl,--wrap=Port_GsVif1Chain", "-Wl,--wrap=Port_GsGifChannel",
             # o golpe do passo "termina luta" dos roteiros (port/x360/headless/roteiro.c, F4 etapa 9a): no quadro da
             # luta, depois do giro dos eventos e antes do Battle_Update, onde o jogo dá os seus
             "-Wl,--wrap=BtlChars_SampleInput",
             # as ações do roteiro de evento da luta que começam, no rastro dos roteiros (roteiro.c, F4 etapa 9b): os
             # gatilhos do btl_script.c, a ação principal do battle_work.c e a da carga do battle_load.c
             "-Wl,--wrap=Gsc_StartAction", "-Wl,--wrap=Gsc_StartMain", "-Wl,--wrap=Gsc_RunAction"]
    # o rastro de som (port/x360/headless/som.c, F6 etapa 2, rede 2): as chamadas do jogo à CRI e ao SOUNDS.IRX que o
    # snd_adx.c e o snd_se.c atendem, e o fim de cada vblank
    resto += [f"-Wl,--wrap={s}" for s in ("ADXT_Create", "ADXT_StartAfs", "ADXT_StartFname", "ADXT_Stop", "ADXT_Pause",
                                          "ADXT_SetOutVol", "ADXT_SetOutPan", "ADXT_SetOutputMono", "ADXT_GetStat",
                                          "sceSifCallRpc", "sceSifSetDma", "func_002967C0", "Port_AdxTick",
                                          "IopHeap_Free")]
    if alvo.gpu:  # a medida da VU1 (gpu_falso.c, BT3_PERFIL): passam por lá o GsVu1_Call, o Unpack, os três desenhos e o Gs_PageHash
        resto += [f"-Wl,--wrap={s}" for s in ("GsVu1_Call", "GsVu1_Unpack", "GsGpu_Draw", "GsGpu_DrawVu0", "GsGpu_DrawVu4", "GsGpu_DrawVu6", "Gs_PageHash")]
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
    if alvo.be:
        esqueleto(alvo)
    # as variáveis globais do jogo no programa, para o checksum e o dump de estado (state.c)
    r = run([sys.executable, ROOT / "port/x360/tools/mem.py", alvo.exe])
    if r.returncode:
        print((r.stdout + r.stderr).strip()[-300:])
        sys.exit(1)
    # carimbo: de que commit (e diferença) este programa saiu, para não comparar builds de origens diferentes
    (alvo.b / "CARIMBO").write_text(carimbo())
    # big-endian: as structs do jogo têm de ter o mesmo layout da referência x86 (bitfields.py, #ifdef do jogo)
    if alvo.be:
        # as cores byte a byte dos pacotes da GPU contra o inventário (F5, etapa 4: plano/F5/inventario.md seção 1)
        r = run([sys.executable, ROOT / "port/x360/tools/cores.py"])
        print(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr.strip()[-300:])
        if r.returncode:
            print("\n".join(r.stdout.strip().splitlines()[:-1][:60]))
            sys.exit(1)
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


def esqueleto(alvo):
    """PowerPC, ligada do esqueleto port/data (decisão 13): grava ao lado do programa o que o produto leva junto.
      <programa>.dat   a lista de make_dat.py do upstream (endereço no programa, arquivo do disco, deslocamento,
                       tamanho de cada `.space`) e o hash de port/data/index.txt; little-endian, o formato do upstream;
      <programa>.swap  o mapa estático do host big-endian: endereço e largura de cada posição viva de número que vem do
                       disco (elftipos.py --trocas, das declarações C da decomp sobre os .i da build x86), que
                       Port_LoadGameData troca de ordem depois da cópia e da conferência do hash.
    Nenhum dos dois tem byte do jogo. O programa é marcado (gPortDataStripped = 1) como um que não roda sem a lista."""
    import hashlib, struct
    mapa_ld = pathlib.Path(str(alvo.exe) + ".map").read_text(errors="replace")
    lugar = {}
    for m in re.finditer(r"^\s*\.data\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\S*/obj_data/(\S+)\.o\s*$", mapa_ld, re.M):
        if int(m.group(2), 16):
            lugar[m.group(3)] = (int(m.group(1), 16), int(m.group(2), 16))
    # a lista, como make_dat.py
    onde, checksum = {}, None
    for l in (ROOT / "port/data/index.txt").read_text().splitlines():
        p = l.split()
        if len(p) == 3 and not l.startswith("#"):
            onde[p[0]] = (int(p[1]), int(p[2], 16))
        elif len(p) == 2 and p[0] == "checksum":
            checksum = int(p[1], 16)
    regs = []
    for nome in sorted(onde):
        if nome not in lugar:
            sys.exit(f"{nome}: fora do mapa do linker (o programa não saiu de port/data?)")
        qual, so = onde[nome]
        ini, tam = lugar[nome]
        off = 0
        for l in (ROOT / "port/data" / (nome + ".s")).read_text().splitlines():
            p = l.split()
            if not p or p[0].startswith("#"):
                continue
            if p[0] == ".space":
                regs.append((ini + off, qual, so + off, int(p[1])))
                off += int(p[1])
            elif p[0] == ".zero":
                off += int(p[1])
            elif p[0] == ".long":
                off += 4
        if not off <= tam < off + 16:
            sys.exit(f"{nome}: {off} bytes em port/data, {tam} no programa")
    dat = pathlib.Path(str(alvo.exe) + ".dat")
    dat.write_bytes(struct.pack("<4sIQ", b"BT3D", len(regs), checksum) + b"".join(struct.pack("<IIII", *r) for r in regs))
    # a marca, na ordem do host (big-endian)
    secs, linhas = [], run(["powerpc-linux-gnu-objdump", "-h", alvo.exe]).stdout.splitlines()
    for k, l in enumerate(linhas):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", l)
        if m and k + 1 < len(linhas) and "CONTENTS" in linhas[k + 1]:
            secs.append((int(m.group(3), 16), int(m.group(5), 16), int(m.group(2), 16)))
    marca = next((int(l.split()[0], 16) for l in run(["powerpc-linux-gnu-nm", alvo.exe]).stdout.splitlines()
                  if l.endswith(" gPortDataStripped")), None)
    pos = next((o + marca - va for va, o, t in secs if marca is not None and va <= marca < va + t), None)
    if pos is None:
        sys.exit("gPortDataStripped não está nos dados do programa")
    img = bytearray(alvo.exe.read_bytes())
    img[pos:pos + 4] = struct.pack(">I", 1)
    alvo.exe.write_bytes(img)
    # o mapa das trocas: do código (a build x86 dá os .i e os símbolos que o port usa), sem disco
    x86 = ROOT / "port/build/x86"
    if not any((x86 / "obj").glob("*.i")):
        sys.exit("sem port/build/x86/obj/*.i: o mapa das tabelas (elftipos.py) sai da build x86; faça-a antes")
    if (x86 / "CARIMBO").read_text() != carimbo():
        sys.exit("a build x86 é de outra origem: refaça-a antes (o mapa das tabelas sai dos .i dela)")
    sai = alvo.b / "elftipos"
    r = run([sys.executable, ROOT / "port/x360/tools/elftipos.py", "--dados", ROOT / "port/data", "--obj", x86 / "obj",
             "--sem-rastreio", "--saida", sai, "--trocas", sai / "trocas.tsv"])
    for l in r.stdout.splitlines():
        if l.startswith(("trocas:", "ERRO")):
            print(l)
    if r.returncode:
        print(r.stderr.strip()[-600:])
        sys.exit(1)
    # cada posição tem de cair inteira no que a lista copia (os `.long Símbolo` o linker já escreveu big-endian)
    faixas = []
    for a, _, _, n in sorted(regs):
        if faixas and a <= faixas[-1][1]:
            faixas[-1][1] = max(faixas[-1][1], a + n)
        else:
            faixas.append([a, a + n])
    inicios = [f[0] for f in faixas]
    pos = []
    for l in (sai / "trocas.tsv").read_text().splitlines():
        nome, ofs, w = l.split("\t")
        if nome not in lugar:
            sys.exit(f"trocas: {nome} não está no mapa do linker")
        a, w = lugar[nome][0] + int(ofs), int(w)
        i = bisect.bisect_right(inicios, a) - 1
        if i < 0 or a + w > faixas[i][1]:
            sys.exit(f"trocas: {nome}+{int(ofs):#x} [{w}] fora do que a lista copia")
        pos.append((a, w))
    pos.sort()
    corpo = b"".join(struct.pack("<II", a, w) for a, w in pos)
    pathlib.Path(str(alvo.exe) + ".swap").write_bytes(struct.pack("<4sI", b"BT3S", len(pos)) + corpo)
    (alvo.b / "ENTRADAS").write_text(f"esqueleto port/data {checksum:#018x}, {len(regs)} trechos; "
                                     f"trocas {len(pos)} posições sha1:{hashlib.sha1(corpo).hexdigest()[:10]}\n")
    print(f"tabelas: {len(lugar)} do esqueleto, {sum(r[3] for r in regs)} bytes do disco em {len(regs)} trechos; "
          f"{len(pos)} posições a trocar ({', '.join(f'{w} bytes {n}' for w, n in sorted(collections.Counter(w for _, w in pos).items()))})")


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
    ap.add_argument("--gpu", action="store_true", help="GS em modo gpu com backend falso (F5, etapa 8: custo da VU1)")
    ap.add_argument("--vu1", action="store_true", help="(x86 com --gpu) liga na x86 a otimização da gravação da VU1 que a PPC já tem (BT3_TESTE_VU1)")
    ap.add_argument("--gravador", action="store_true", help="(F6, rede 4) o som com o dispositivo gravador")
    ap.add_argument("--teste-som", action="store_true", help="(F6, rede 3) só o teste das amostras dos decodificadores")
    a = ap.parse_args()
    if (a.gravador or a.teste_som) and (a.rastreio or a.chamadas or a.gpu):
        sys.exit("--gravador e --teste-som não se combinam com --rastreio, --chamadas nem --gpu")
    if a.rastreio and a.alvo != "x86":
        sys.exit("o rastreio roda na x86 (é a build em que o jogo já funciona)")
    if a.gpu and (a.rastreio or a.chamadas):
        sys.exit("--gpu não se combina com --rastreio nem com --chamadas")
    if a.vu1 and not (a.gpu and a.alvo == "x86"):
        sys.exit("--vu1 só vale com --alvo x86 --gpu (na PPC a otimização já vem com o PORT_BIG_ENDIAN)")
    construir(Alvo(a.alvo, a.rastreio, a.chamadas, a.gpu, a.vu1, a.gravador, a.teste_som))


main()
