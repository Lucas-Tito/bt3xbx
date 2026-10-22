#!/usr/bin/env python3
"""F6, etapa 1, rede 1 (docs/x360/plano/F6/01.md): o ambiente do som no 360, no WSL e no PC.

Cada getenv e cada ramo de janela (GsGpu_Enabled) e de rede (Port_NetSession, Port_NetVoiceAlt) dos arquivos que
mexem no som e nos filmes, e o caminho que cada um dá em cada lugar. O 360 lê o ambiente de uma tabela
(port/x360/src/f2/posix360.c: os padrões do Ambiente_Carregar e por cima o game:\\ambiente.txt); o que não está
nela devolve NULL em silêncio (a lição da F6, 00-escopo.md seção 1.1). O WSL roda os roteiros com o export do
port/x360/tools/roteiros.sh. O PC (o programa de release) começa sem nenhuma variável e com a janela.

    som_ambiente.py [--raiz DIR] [--ambiente ARQ]...
    som_ambiente.py --mutantes     (o controle negativo: estraga cópias de cinco jeitos e exige a falha de cada um)

Sai com 1 quando:
  - um arquivo da lista tem um getenv, um GsGpu_Enabled() ou uma chamada de rede que o inventário (INVENTARIO,
    RAMOS) não conhece, ou deixou de ter um que ele conhece (a conta por arquivo tem de bater);
  - um caminho do som (CAMINHOS) dá um valor no 360 e outro no WSL, fora das divergências esperadas (ESPERADAS),
    em qualquer cenário: o 360 sem desenho (os padrões e os ambiente.txt dados), o 360 com desenho (BT3_GS=gpu) e
    os roteiros do WSL;
  - o 360 sem desenho perde uma variável que os roteiros do WSL definem e que o inventário diz que muda o som.
--raiz aponta outra cópia dos arquivos (o controle negativo: uma cópia estragada num diretório temporário).
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]

# Os arquivos que tocam o som e os filmes, do port e do jogo (o jogo não tem getenv no som; entram para a conta).
ARQUIVOS = [
    "port/src/gs/snd_adx.c", "port/src/gs/snd_se.c", "port/src/plat_sndstate.c", "port/src/plat_file.c",
    "port/src/plat_movie.c", "port/src/plat_songs.c", "port/src/plat_extras.c", "port/src/plat_stub.c",
    "src/sys/adx.c", "src/sys/movie.c", "src/sys/file.c",
]

# Cada variável lida nesses arquivos: (classe, o que faz). Classe "estado" muda o que o jogo vê (e por isso o
# rastro); "tempo" muda só o ritmo real; "saida" só o que se ouve ou o log; "dados" de onde os arquivos vêm;
# "fora" não é do som (plat_stub.c lê também a tela e o controle gravado).
INVENTARIO = {
    "BT3_SOUND_TICKS": ("estado", "snd_adx.c by_ticks: o estado dos players contado em vblanks pela duração do arquivo"),
    "BT3_SYNCTEST": ("estado", "snd_adx.c by_ticks: o mesmo, ligado pelo teste de salvar e restaurar"),
    "BT3_SOUND": ("saida", "snd_adx.c device: tenta abrir o dispositivo sem janela (testes com som)"),
    "BT3_NOSOUND": ("saida", "snd_adx.c device: nunca abre o dispositivo"),
    "BT3_SND_VERBOSE": ("saida", "snd_adx.c, snd_se.c: log de início, parada, pausa e volume"),
    "BT3_SE_GAIN": ("saida", "snd_se.c: o ganho da mistura dos efeitos"),
    "BT3_NOMOVIE": ("estado", "plat_movie.c: o filme não toca (movie_start volta na hora)"),
    "BT3_PAD_PLAY": ("estado", "plat_movie.c: o filme toca sem janela com o controle gravado; plat_stub.c: o controle "
                     "sai do arquivo"),
    "BT3_PACED": ("tempo", "plat_stub.c Port_VBlank: espera o vblank real sem janela"),
    "BT3_UNCAPPED": ("tempo", "plat_stub.c Port_VBlank: nunca espera o vblank"),
    "BT3_DATA": ("dados", "plat_file.c, plat_songs.c, plat_extras.c: a raiz dos dados (no 360, a ISO por baixo)"),
    "BT3_FILE_ALIAS": ("dados", "plat_file.c: um arquivo do jogo servido de outro lugar (também os fluxos de som)"),
    "BT3_TEST_BGM": ("estado", "plat_songs.c: toda luta com a música dada"),
    "BT3_SONGS": ("dados", "plat_extras.c (indireto, PortExtras_Dir de plat_songs.c): a pasta das músicas de fora"),
    "BT3_STAGES": ("fora", "plat_extras.c (indireto, de plat_stages.c): a pasta dos estágios de fora"),
    "BT3_GS_VERBOSE": ("fora", "plat_stub.c: log do ritmo do GS"),
    "BT3_PAD_REC": ("fora", "plat_stub.c: grava o controle"),
    "BT3_PAD_TABLE": ("fora", "plat_stub.c: tabela do controle por vblank"),
    "BT3_ASPECT": ("fora", "plat_stub.c: proporção da tela"),
    "BT3_WIDE": ("fora", "plat_stub.c: tela larga"),
    "BT3_WINDOW": ("fora", "plat_stub.c: tamanho da janela"),
}
# getenv com o nome numa variável: arquivo -> nomes que chegam a ele
INDIRETOS = {"port/src/plat_extras.c": ["BT3_SONGS", "BT3_STAGES"]}

# Ramos de janela e de rede por arquivo: a conta de cada chamada e o que ela decide no som.
RAMOS = {
    "port/src/gs/snd_adx.c": {"GsGpu_Enabled()": 2, "Port_NetSession()": 1, "Port_NetVoiceAlt()": 1},
    "port/src/gs/snd_se.c": {"GsGpu_Enabled()": 1, "Port_NetVoiceAlt()": 1},
    "port/src/plat_movie.c": {"GsGpu_Enabled()": 1, "Port_NetSession()": 1},
    "port/src/plat_stub.c": {"GsGpu_Enabled()": 3, "Port_NetSession()": 1},
}


def tem(amb, nome):
    return nome in amb  # getenv != NULL (o valor vazio conta, como no C)


# Os caminhos do som, como o código os decide (arquivo e função de cada um no texto). p: "360", "wsl" ou "pc";
# gpu: GsGpu_Enabled(). As builds do 360 e do WSL têm a SDL de mentira (port/x360/headless/SDL3/SDL.h: o
# SDL_InitSubSystem devolve false) e a rede de mentira (port/x360/headless/sem_janela.c: Port_NetSession 0).
def caminhos(amb, gpu, p):
    sdl = p == "pc"
    tenta = gpu or tem(amb, "BT3_SOUND")
    disp = sdl and tenta and not tem(amb, "BT3_NOSOUND")
    ticks = tem(amb, "BT3_SYNCTEST") or tem(amb, "BT3_SOUND_TICKS")
    return {
        "snd_adx.c by_ticks (estado dos players em vblanks)": ticks,
        "snd_adx.c ADXT_GetStat devolve": "vblanks" if ticks else ("dispositivo" if disp else "0 sempre"),
        "snd_adx.c device (tenta abrir)": tenta,
        "snd_adx.c device (abre)": disp,
        "snd_se.c mistura ligada (Port_AudioDevice)": disp,
        "snd_adx.c/snd_se.c voz alternativa da rede": False,
        "plat_movie.c o filme toca": (gpu or tem(amb, "BT3_PAD_PLAY")) and not tem(amb, "BT3_NOMOVIE"),
        "plat_stub.c Port_VBlank espera o vblank real": (gpu or tem(amb, "BT3_PACED")) and not tem(amb, "BT3_UNCAPPED"),
        "plat_stub.c Port_VBlank imagens intermediárias (InterpPump)": gpu,
        "plat_songs.c música de teste (BT3_TEST_BGM)": amb.get("BT3_TEST_BGM", "") or "nenhuma",
        "plat_file.c apelidos de arquivo (BT3_FILE_ALIAS)": amb.get("BT3_FILE_ALIAS", "") or "nenhum",
    }


# As divergências 360 contra WSL que se esperam, com o porquê. Qualquer outra é falha.
ESPERADAS = {
    ("360 com desenho", "plat_movie.c o filme toca"):
        "no console o filme de abertura toca (o Lucas pula com START); os roteiros do WSL rodam com BT3_NOMOVIE=1 "
        "e sem janela. Sem som tocando de verdade; o ADX do filme também é contado em vblanks",
    ("360 com desenho", "plat_stub.c Port_VBlank espera o vblank real"):
        "o console com desenho anda em tempo real; só o ritmo, o jogo conta vblanks",
    ("360 com desenho", "plat_stub.c Port_VBlank imagens intermediárias (InterpPump)"):
        "o backend D3D9 do console; não toca no som",
    ("360 com desenho", "snd_adx.c device (tenta abrir)"):
        "GsGpu_Enabled() no console: o device() tenta, e a SDL de mentira recusa (abre: não, igual ao WSL)",
}


def le(raiz, rel):
    return (raiz / rel).read_text(encoding="utf-8", errors="replace")


def sem_comentarios(texto):
    texto = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), texto, flags=re.S)
    return re.sub(r"//[^\n]*", "", texto)


def padroes_360(raiz):
    """Os padrões do Ambiente_Carregar (posix360.c), na ordem."""
    t = sem_comentarios(le(raiz, "port/x360/src/f2/posix360.c"))
    m = re.search(r"void Ambiente_Carregar\([^)]*\)\s*\{(.*?)\n\}", t, re.S)
    if m is None:
        sys.exit("som_ambiente: Ambiente_Carregar não achado em posix360.c")
    amb = {}
    for nome, valor in re.findall(r'definir\("([A-Z0-9_]+)",\s*"((?:[^"\\]|\\.)*)"\)', m.group(1)):
        amb[nome] = valor.replace("\\\\", "\\")
    return amb


def ambiente_txt(amb, caminho):
    """Um game:\\ambiente.txt por cima (NOME=VALOR, -NOME tira, # comentário), como o Ambiente_Carregar."""
    amb = dict(amb)
    for linha in pathlib.Path(caminho).read_text(encoding="utf-8").splitlines():
        linha = linha.rstrip("\r\n")
        if linha.startswith("-") and len(linha) > 1:
            amb.pop(linha[1:], None)
        elif "=" in linha and not linha.startswith("#"):
            n, v = linha.split("=", 1)
            amb[n] = v
    return amb


def ambiente_wsl(raiz):
    """O export do roteiros.sh (o ambiente de toda rodada dos roteiros no WSL)."""
    amb = {}
    for linha in sem_comentarios(le(raiz, "port/x360/tools/roteiros.sh")).splitlines():
        m = re.match(r"\s*export\s+(.*)$", linha)
        if m:
            for nome, valor in re.findall(r"(BT3_[A-Z0-9_]+)=(\S*)", m.group(1)):
                amb[nome] = valor
    if not amb:
        sys.exit("som_ambiente: nenhum export BT3_ no roteiros.sh")
    return amb


def gpu_de(amb, p):
    """gs_core.c gs_mode(): BT3_GS 'none', 'off' ou '0' desliga; sem a variável o programa de release (o PC) abre a
    janela; no 360 (main360.cpp) só 'gpu' liga o backend."""
    g = amb.get("BT3_GS")
    if g is None:
        return p == "pc"
    return g == "gpu"


# Cada mutante: (arquivo, texto a trocar, troca, ambiente.txt ou None). Todos têm de sair com 1.
MUTANTES = [
    ("port/x360/src/f2/posix360.c", 'definir("BT3_SOUND_TICKS", "1");', "", None),
    ("port/src/gs/snd_adx.c", "static int by_ticks(void) {",
     'static int by_ticks(void) {\n    if (getenv("BT3_SOM_NOVO") != NULL) { return 0; }', None),
    ("port/src/plat_movie.c", "extern int GsGpu_Enabled(void);",
     "extern int GsGpu_Enabled(void);\nstatic int novo(void) { return GsGpu_Enabled(); }", None),
    ("port/x360/tools/roteiros.sh", "BT3_SOUND_TICKS=1 ", "", None),
    (None, None, None, "-BT3_SOUND_TICKS\n"),
]


def mutantes():
    import shutil
    import subprocess
    import tempfile
    falhas = 0
    for i, (rel, de, para, amb) in enumerate(MUTANTES):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            for r in ARQUIVOS + ["port/x360/src/f2/posix360.c", "port/x360/tools/roteiros.sh"]:
                (d / r).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(ROOT / r, d / r)
            args = [sys.executable, __file__, "--raiz", str(d)]
            if rel is not None:
                t = (d / rel).read_text(encoding="utf-8")
                if t.count(de) != 1:
                    print(f"mutante {i}: o texto a trocar não está uma vez só em {rel}")
                    falhas += 1
                    continue
                (d / rel).write_text(t.replace(de, para), encoding="utf-8")
            if amb is not None:
                (d / "ambiente.txt").write_text(amb, encoding="utf-8")
                args += ["--ambiente", str(d / "ambiente.txt")]
            r = subprocess.run(args, capture_output=True, text=True)
            motivo = [x for x in r.stdout.splitlines() if x.startswith(">> ") and "FALHA" not in x]
            print(f"mutante {i} ({rel or 'ambiente.txt'}): saída {r.returncode}; {motivo[0] if motivo else ''}")
            if r.returncode != 1:
                falhas += 1
    print("todos os mutantes acusados" if falhas == 0 else f">> FALHA: {falhas} mutante(s) não acusado(s)")
    return 1 if falhas else 0


def main():
    if "--mutantes" in sys.argv:
        return mutantes()
    ap = argparse.ArgumentParser()
    ap.add_argument("--raiz", default=str(ROOT))
    ap.add_argument("--ambiente", action="append", default=[],
                    help="ambiente.txt de uma rodada do console (vale para o 360 sem e com desenho)")
    a = ap.parse_args()
    raiz = pathlib.Path(a.raiz)
    erros = []

    # 1. o inventário cobre todo getenv e todo ramo dos arquivos
    print("== getenv e ramos por arquivo")
    vistos = set()
    for rel in ARQUIVOS:
        t = sem_comentarios(le(raiz, rel))
        nomes = re.findall(r'getenv\("([A-Z0-9_]+)"\)', t)
        indiretos = len(re.findall(r"getenv\((?!\")", t))
        conta = {k: t.count(k) for k in ("GsGpu_Enabled()", "Port_NetSession()", "Port_NetVoiceAlt()") if t.count(k)}
        print(f"{rel}: getenv {sorted(set(nomes)) or '-'}; indiretos {indiretos}; ramos {conta or '-'}")
        for n in nomes:
            vistos.add(n)
            if n not in INVENTARIO:
                erros.append(f"{rel}: getenv(\"{n}\") fora do inventário")
        if indiretos != (1 if rel in INDIRETOS else 0):
            erros.append(f"{rel}: {indiretos} getenv com o nome numa variável (o inventário espera "
                         f"{1 if rel in INDIRETOS else 0})")
        vistos.update(INDIRETOS.get(rel, []))
        if conta != RAMOS.get(rel, {}):
            erros.append(f"{rel}: ramos {conta}, o inventário espera {RAMOS.get(rel, {})}")
    for n in INVENTARIO:
        if n not in vistos:
            erros.append(f"{n}: no inventário, mas nenhum arquivo o lê mais")

    # 2. os valores em cada lugar
    base360 = padroes_360(raiz)
    s360 = base360
    for arq in a.ambiente:
        s360 = ambiente_txt(s360, arq)
    c360 = dict(s360)
    c360["BT3_GS"] = "gpu"
    wsl = ambiente_wsl(raiz)
    pc = {}
    print("\n== variáveis: 360 (padrões" + (" + " + ", ".join(a.ambiente) if a.ambiente else "") +
          ") | WSL (roteiros.sh) | PC")
    for n, (cls, oque) in sorted(INVENTARIO.items()):
        v = lambda amb: ("=" + amb[n] if amb[n] != "" else "= (vazio)") if n in amb else "NULL"  # noqa: E731
        print(f"{n:16} {cls:7} 360 {v(s360):22} WSL {v(wsl):22} PC {v(pc):6}  {oque}")
        if cls == "estado" and (n in wsl) != (n in s360) and n not in ("BT3_NOMOVIE",):
            erros.append(f"{n}: muda o som e está {'só no WSL' if n in wsl else 'só no 360'}")

    # 3. os caminhos
    cen = {
        "360 sem desenho": caminhos(s360, gpu_de(s360, "360"), "360"),
        "360 com desenho": caminhos(c360, gpu_de(c360, "360"), "360"),
    }
    cw = caminhos(wsl, gpu_de(wsl, "wsl"), "wsl")
    cp = caminhos(pc, gpu_de(pc, "pc"), "pc")
    print("\n== caminhos: 360 sem desenho | 360 com desenho | WSL roteiros | PC")
    for k in cw:
        print(f"{k:62} {str(cen['360 sem desenho'][k]):9} {str(cen['360 com desenho'][k]):9} {str(cw[k]):9} {cp[k]}")
    print()
    for nome, c in cen.items():
        for k in cw:
            if c[k] != cw[k]:
                motivo = ESPERADAS.get((nome, k))
                if motivo is None:
                    erros.append(f"{nome}: '{k}' é {c[k]} no 360 e {cw[k]} no WSL")
                else:
                    print(f"esperada ({nome}): '{k}' {c[k]} contra {cw[k]}: {motivo}")
            elif (nome, k) in ESPERADAS:
                erros.append(f"{nome}: '{k}' deixou de divergir; tire-o de ESPERADAS")
    if erros:
        print("\n>> FALHA")
        for e in erros:
            print(">> " + e)
        return 1
    print("tudo certo: o som segue o mesmo caminho no 360 e nos roteiros do WSL, fora as divergências esperadas")
    return 0


if __name__ == "__main__":
    sys.exit(main())
