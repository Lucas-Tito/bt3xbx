#!/usr/bin/env python3
"""F5, etapa 8: o perfil da gravação da VU1 (o tempo dentro do GsVu1_Call) nas builds --gpu (headless.py --gpu).

    vu1_perfil.py x86 PERFIL.txt [--prog port/build/x86g/bt3_x86g] [--linhas N]
        PERFIL.txt é o BT3_PERFIL_SAIDA de uma rodada com BT3_PERFIL=2 (gpu_falso.c): por família de programa, as
        chamadas, os tiques (rdtsc) e os desenhos; e as amostras do contador de instruções dentro do GsVu1_Call, que
        aqui viram, pelo DWARF do programa (addr2line), o tempo por função (a de fora e a de dentro, com as
        funções em linha) e por linha de código.
    vu1_perfil.py ppc LOG.txt --prog port/build/ppcg/bt3_ppcg
        LOG.txt: "endereço contagem" por instrução executada, do -d exec do qemu-ppc com -one-insn-per-tb e -dfilter nos
        trechos do gs_vu1.c, do gs_draw.c e do que eles chamam (vu1_perfil.sh ppc). Conta as instruções por função e, pelo
        código de máquina do programa, quantas são leituras e quantas são gravações na memória. O tempo da PPC emulada
        não é o do Xenon: são contagens, para comparar o antes e o depois e ver onde está a massa.
"""
import argparse, collections, os, re, subprocess, sys


def run(cmd):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True).stdout


def x86(a):
    fams, unpack, pcs = [], None, {}
    for l in open(a.perfil, errors="replace"):
        if l.startswith("perfil: fam"):
            p = [x.strip() for x in l[len("perfil: "):].split("|")]
            d = {k: int(v) for k, v in (x.split() for x in p[2:])}
            fams.append((p[1], d))
        elif l.startswith("perfil: unpack"):
            unpack = {k: int(v) for k, v in (x.split() for x in l.split("|")[1:])}
        elif l.startswith("perfil: pc"):
            _, _, pc, n = l.split()
            pcs[int(pc, 16)] = int(n)
    tot = sum(d["tiques"] for _, d in fams)
    print(f"== por família de programa (rdtsc; o total do GsVu1_Call é {tot / 1e9:.2f} G tiques)")
    print(f"{'família':44} {'chamadas':>10} {'tiq/chamada':>12} {'%':>6} {'desenhos':>10} {'tiq/desenho':>12} {'% no desenho':>13} {'vért/desenho':>13} {'índ/desenho':>12}")
    for nome, d in fams:
        dd = d.get("desenhos", 0)
        print(f"{nome:44} {d['chamadas']:10d} {d['tiques'] / d['chamadas']:12.0f} {100 * d['tiques'] / tot:6.1f} {dd:10d} "
              f"{(d['tiques_desenho'] / dd if dd else 0):12.0f} {(100 * d['tiques_desenho'] / d['tiques'] if d['tiques'] else 0):13.1f} "
              f"{(d['vertices'] / dd if dd else 0):13.1f} {(d['indices'] / dd if dd else 0):12.1f}")
    if unpack:
        print(f"GsVu1_Unpack (fora do GsVu1_Call): {unpack['chamadas']} chamadas, {unpack['tiques'] / 1e9:.2f} G tiques "
              f"({unpack['tiques'] / max(unpack['chamadas'], 1):.0f} por chamada)")
    total = sum(pcs.values())
    if not total:
        return
    pcl = sorted(pcs)
    # addr2line -a -f -i: para cada endereço, a linha do endereço e os pares (função, arquivo:linha), da mais interna
    # para a mais externa
    sai = run(["addr2line", "-a", "-f", "-i", "-C", "-e", a.prog] + [hex(p) for p in pcl]).splitlines()
    blocos, atual = [], None
    for l in sai:
        if re.match(r"^0x[0-9a-f]+$", l):
            atual = []
            blocos.append(atual)
        else:
            atual.append(l)
    por_dentro, por_fora = collections.Counter(), collections.Counter()
    for pc, b in zip(pcl, blocos):
        n = pcs[pc]
        pares = [(b[i], b[i + 1]) for i in range(0, len(b) - 1, 2)]
        if not pares:
            continue
        por_dentro[pares[0][0]] += n
        por_fora[pares[-1][0]] += n
    print(f"\n== amostras do contador de programa dentro do GsVu1_Call: {total}")
    print("-- por função de fora (a que o GsVu1_Call chama, contando as em linha dentro dela)")
    for f, n in por_fora.most_common(a.linhas):
        print(f"  {100 * n / total:6.1f} %  {f}")
    print("-- por função de dentro (a que tem o código, as em linha à parte)")
    for f, n in por_dentro.most_common(a.linhas):
        print(f"  {100 * n / total:6.1f} %  {f}")
    lin = collections.Counter()
    # (uma linha de arquivo:linha por endereço, sem -f, sem -i: a linha do primeiro par)
    sai3 = run(["addr2line", "-e", a.prog] + [hex(p) for p in pcl]).splitlines()
    for pc, l in zip(pcl, sai3):
        lin[l.split("/")[-1].split(" ")[0]] += pcs[pc]
    print("-- por linha de código (as mais quentes)")
    for l, n in lin.most_common(a.linhas):
        print(f"  {100 * n / total:6.1f} %  {l}")


# ------------------------------------------------------------------------------------------------ PPC (contagem)
def ppc(a):
    nm = run(["powerpc-linux-gnu-nm", "-n", a.prog]).splitlines()
    sim = []
    for l in nm:
        p = l.split()
        if len(p) == 3 and p[1] in "tTwW":
            sim.append((int(p[0], 16), p[2]))
    ends = [s[0] for s in sim]

    import bisect

    def funcao(pc):
        i = bisect.bisect_right(ends, pc) - 1
        return sim[i][1] if i >= 0 else "?"

    # as instruções de cada endereço, para dizer leitura ou gravação (o código de máquina, do ELF)
    ops = {}
    for l in run(["powerpc-linux-gnu-objdump", "-d", a.prog]).splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\s+([0-9a-f]{2}) ([0-9a-f]{2}) ([0-9a-f]{2}) ([0-9a-f]{2})\s+(\S+)", l)
        if m:
            ops[int(m.group(1), 16)] = m.group(6)
    cont = collections.Counter()
    for l in open(a.perfil, errors="replace"):  # "PC contagem" (vu1_perfil.sh ppc)
        p = l.split()
        if len(p) == 2:
            cont[int(p[0], 16)] += int(p[1])
    # a função mais interna de cada PC (com as em linha: a texture_get entra na draw_state), e a parte da texture_get
    # que decodifica uma textura nova (do primeiro uso depois do cache até o fim dela) fica à parte: é rara em regime, e a
    # janela do qemu, que parte do zero, decodifica as que a luta usa, o que não é o custo de um quadro
    fonte = open(a.fonte or os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../src/gs/gs_draw.c"), errors="replace").read().splitlines()
    ini_dec = next(i for i, l in enumerate(fonte, 1) if "GsDraw_X360Abre(tw, th, 0)" in l)
    fim_tex = next(i for i, l in enumerate(fonte, 1) if i > ini_dec and l.startswith("}"))
    pcs = sorted(cont)
    interno = {}
    if pcs:
        saida = run(["powerpc-linux-gnu-addr2line", "-a", "-f", "-i", "-e", a.prog] + [hex(p) for p in pcs]).splitlines()
        atual, ant = None, []
        for l in saida + ["0x00000000"]:
            if re.fullmatch(r"0x[0-9a-f]{8,16}", l):
                if atual is not None and len(ant) >= 2:
                    interno[atual] = (ant[0], ant[1])  # (a função mais interna, arquivo:linha)
                atual, ant = int(l, 16), []
            else:
                ant.append(l)
    total = sum(cont.values())
    por_f, leit, grav = collections.Counter(), collections.Counter(), collections.Counter()
    cls = collections.Counter()
    for pc, n in cont.items():
        f = funcao(pc)
        if pc in interno:
            nome, loc = interno[pc]
            m = re.search(r"gs_draw\.c:(\d+)", loc)
            if nome == "texture_get" and m:
                f = "texture_get (decodifica textura nova)" if ini_dec <= int(m.group(1)) <= fim_tex else "texture_get (laço do gen)"
            elif f == "draw_state":
                f = "draw_state" if nome == "draw_state" else f"draw_state/{nome}"
        por_f[f] += n
        op = ops.get(pc, "")
        if re.match(r"^(l[bhwdaf]|lm|lsw|lvx|lwarx|lwbrx|lhbrx|lfs|lfd)", op):
            leit[f] += n
            cls["leitura"] += n
        elif re.match(r"^(st[bhwdf]|stm|stsw|stvx|stwcx|stwbrx|sthbrx|dcbz)", op):
            grav[f] += n
            cls["gravação"] += n
        else:
            cls["outras"] += n
    dec = por_f.get("texture_get (decodifica textura nova)", 0)
    print(f"== instruções executadas (PPC no qemu, dentro do filtro): {total}, das quais {dec} decodificam textura nova "
          f"(rara em regime, fora da conta de regime: {total - dec})")
    print(f"   leituras {cls['leitura']} ({100 * cls['leitura'] / total:.1f} %), gravações {cls['gravação']} "
          f"({100 * cls['gravação'] / total:.1f} %), outras {cls['outras']}")
    print(f"{'função':36} {'instruções':>14} {'%':>6} {'leituras':>12} {'gravações':>12}")
    for f, n in por_f.most_common(a.linhas):
        print(f"{f:36} {n:14d} {100 * n / total:6.1f} {leit[f]:12d} {grav[f]:12d}")


# O que o -dfilter do qemu deixa passar: só as funções da gravação da VU1 (as do gs_vu1.o, do gs_draw.o e do gpu_falso.o).
# Ficam de fora, de propósito, tudo o que o jogo também chama fora da VU1 (memcpy/memset/memcmp da libc, Gs_Gif e o resto
# do gs_core.o): com elas o log tem centenas de milhões de linhas por quadro e uma rodada leva horas (uma tentativa levou
# 30 min para 237 quadros). O custo das cópias fica pela conta: tamanho e número de chamadas, que o código mostra.
# Fora também, e contados à parte, o hash de página (Gs_PageHash, a conta é recálculos x o custo de um) e a decodificação
# de uma textura nova (texture_get e as leituras da VRAM), rara em regime.
# Do gs_draw.o ficam só as que o caminho da VU1 usa (o resto, como GsGpu_FrameEnd e as interp_*, roda por quadro sem ser
# a gravação, e o gpu_falso.o é o harness).
GS_DRAW_VU1 = {"pipeline_get", "GsDraw_TargetGet", "depth_clut", "draw_state", "same_state", "date_snapshot", "blend_key",
               "tex_bucket", "GsGpu_DrawVu0", "GsGpu_DrawVu4", "GsGpu_DrawVu6", "put"}
# Do gs_vu1.o ficam só o GsVu1_Call, o GsVu1_Unpack e o que o Call chama (hle_*): as outras (UnpackWords, SetOffset, Program,
# FrameEnd...) o jogo chama também antes da janela, a cada pacote, e o log encheria; elas não fazem parte da gravação.
VU1_FORA = {"GsVu1_UnpackWords", "GsVu1_FrameEnd", "GsVu1_Program", "GsVu1_SetOffset", "GsVu1_SetBase", "GsVu1_SetCycle",
            "GsVu1_SetItop", "GsVu1_SetMode", "GsVu1_SetMask", "GsVu1_SetRow", "GsVu1_SetCol", "GsVu1_StateKeep"}
DENTRO_CORE = set()
FORA = {"texture_get", "vram_rw", "Gs_VramRead", "Gs_Expand", "Gs_PageHash", "clut_hash", "clut_texture", "__wrap_Gs_PageHash"}


def faixas(a):
    mapa = open(a.prog + ".map", errors="replace").read()
    objs = []
    for nome in ("gs_gs_vu1.o", "gs_gs_draw.o"):
        m = re.search(r"^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) \S*/" + re.escape(nome) + "$", mapa, re.M)
        objs.append((nome, int(m.group(1), 16), int(m.group(1), 16) + int(m.group(2), 16)))
    out = []
    for l in run(["powerpc-linux-gnu-nm", "-S", "-n", a.prog]).splitlines():
        p = l.split()
        if len(p) != 4 or p[2] not in "tTwW":
            continue
        ini, tam, nome = int(p[0], 16), int(p[1], 16), p[3]
        if nome in FORA or tam == 0:
            continue
        if any(o[1] <= ini < o[2] and ((o[0] == "gs_gs_draw.o" and nome in GS_DRAW_VU1) or (o[0] == "gs_gs_vu1.o" and nome not in VU1_FORA)) for o in objs):
            out.append(f"0x{ini:x}+0x{tam:x}")
    print(",".join(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("modo", choices=["x86", "ppc", "faixas"])
    ap.add_argument("perfil", nargs="?", default="")
    ap.add_argument("--prog", default=None)
    ap.add_argument("--fonte", default=None, help="o gs_draw.c de que a build saiu (as linhas do addr2line), se não for o da árvore")
    ap.add_argument("--linhas", type=int, default=25)
    a = ap.parse_args()
    a.prog = a.prog or ("port/build/" + os.environ.get("X86", "x86g") + "/bt3_" + os.environ.get("X86", "x86g") if a.modo == "x86" else "port/build/ppcg/bt3_ppcg")
    {"x86": x86, "ppc": ppc, "faixas": faixas}[a.modo](a)


main()
