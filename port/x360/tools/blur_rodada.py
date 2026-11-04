#!/usr/bin/env python3
"""Rodada serial do desfoque. Executar sob flock ~/wsl.lock, com log persistente.

Não altera fontes, referências nem serviços de outras tarefas. O término prepara
os resultados para julgamento; não declara uma causa visual automaticamente.
"""
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time

RAIZ = pathlib.Path(__file__).resolve().parents[3]
SAIDA = pathlib.Path.home() / "blur-analise"
CHAVES = ["DISTANCIA", "PANORAMICO", "RADIAL", "BRUMA", "NEVOA", "BRILHO", "CLIMA"]
LISTA = """menu 90 roteiro: boot.txt:20 ok tela 4
selecao 150 roteiro: f5_duelo.txt:19 ok tela 39
carga 1 roteiro: f5_duelo.txt:42 marca ponto carga
combate 1 roteiro: f5_duelo.txt:59 marca ponto combate
tecnica 1 roteiro: f5_duelo.txt:68 marca ponto tecnica
pausa 1 roteiro: f5_duelo.txt:73 marca ponto pausa
"""


def rodar(nome, comando, ambiente=None, cwd=RAIZ, limite=None, aceita=()):
    env = {**os.environ, **(ambiente or {})}
    env.pop("PPC", None)
    print("início:", nome, flush=True)
    log = SAIDA / (nome + ".log")
    with log.open("w") as f:
        try:
            r = subprocess.run([str(x) for x in comando], cwd=cwd, env=env,
                               stdout=f, stderr=subprocess.STDOUT, timeout=limite)
            codigo = r.returncode
        except subprocess.TimeoutExpired:
            codigo = 124
    print("fim:", nome, "código", codigo, flush=True)
    print("\n".join(log.read_text(errors="replace").splitlines()[-5:]), flush=True)
    if codigo != 0 and codigo not in aceita:
        raise RuntimeError("%s: código %d; veja %s" % (nome, codigo, log))
    return codigo


def objetos():
    base = SAIDA / "base-102/obj"
    atual = RAIZ / "port/build/x86/obj"
    iguais, diferentes = [], []
    if {p.name for p in base.glob("*.o")} != {p.name for p in atual.glob("*.o")}:
        raise RuntimeError("lista de objetos diferente da base")
    with tempfile.TemporaryDirectory() as temp:
        a, b = pathlib.Path(temp) / "a.o", pathlib.Path(temp) / "b.o"
        for p in sorted(base.glob("*.o")):
            subprocess.run(["objcopy", "--strip-debug", str(p), str(a)], check=True)
            subprocess.run(["objcopy", "--strip-debug", str(atual / p.name), str(b)], check=True)
            (iguais if a.read_bytes() == b.read_bytes() else diferentes).append(p.name)
    destino = SAIDA / "text-final.bin"
    subprocess.run(["objcopy", "-O", "binary", "--only-section=.text",
                    str(RAIZ / "port/build/x86/bt3_x86"), str(destino)], check=True)
    dados = {"objetos_iguais": len(iguais), "diferentes": diferentes,
             "text_sha1": hashlib.sha1(destino.read_bytes()).hexdigest(),
             "text_base_sha1": hashlib.sha1((SAIDA / "base-102/text.bin").read_bytes()).hexdigest()}
    (SAIDA / "objetos.json").write_text(json.dumps(dados, indent=2) + "\n")
    if set(diferentes) - {"x360_passos.o"}:
        raise RuntimeError("objetos do jogo alterados fora do diagnóstico: " + str(diferentes))
    print("objetos:", dados, flush=True)


def main():
    SAIDA.mkdir(exist_ok=True)
    # Há ferramentas antigas que não usam a trava. O serviço espera sozinho.
    while subprocess.run(["pgrep", "-x", "qemu-ppc"], stdout=subprocess.DEVNULL).returncode == 0:
        time.sleep(60)
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=RAIZ).strip():
        raise RuntimeError("árvore suja antes da rodada")
    for alvo, extra in (("x86", []), ("x86", ["--rastreio"]), ("ppc", [])):
        nome = "final-" + alvo + ("r" if extra else "")
        rodar(nome, ["python3", "port/x360/tools/headless.py", "--alvo", alvo, *extra],
              {"BT3_TESTE_DESFOQUE": "0"})
    objetos()
    rodar("oficial", ["bash", "port/x360/tools/roteiros.sh", "f5_duelo"],
          {"SEMMAPA": "1", "IMG": "1", "O": str(SAIDA / "oficial"), "TP": "3600"})
    for alvo in ("x86", "ppc"):
        rodar("diagnostico-" + alvo, ["python3", "port/x360/tools/headless.py", "--alvo", alvo],
              {"BT3_TESTE_DESFOQUE": "1"})
    oficial = SAIDA / "oficial/f5_duelo"
    dados = SAIDA / "dados"
    dados.mkdir(exist_ok=True)
    for origem in (RAIZ / "gamedata").iterdir():
        if not (dados / origem.name).exists():
            (dados / origem.name).symlink_to(origem.resolve())
    (dados / "mods").mkdir(exist_ok=True)
    lidos = (oficial / "lidos.txt").read_text().splitlines()
    rodar("conversao", [SAIDA / "oficial/conv_tool", "--mapa", oficial / "mapa.tsv", "--saida",
                         dados / "mods", "gamedata", *lidos])
    for caso in ["base", *[c.lower() for c in CHAVES]]:
        cortes = {"BT3_SEM_" + c: str(int(c.lower() == caso)) for c in CHAVES}
        for alvo in ("x86", "ppc"):
            pasta = SAIDA / "imagens" / caso / alvo
            pasta.mkdir(parents=True, exist_ok=True)
            if (pasta / "s").exists():
                raise RuntimeError("cartão já existe; preserve a rodada e use outra pasta: " + str(pasta))
            (pasta / "lista.txt").write_text(LISTA)
            ambiente = {
                "BT3_SETTINGS": "", "BT3_SOUND_TICKS": "1", "BT3_NOMOVIE": "1",
                "BT3_MENU_ITEM4": "0", "BT3_DESBLOQUEIO": "1", "BT3_UNCAPPED": "1",
                "BT3_NOSOUND": "1", "BT3_WIDE": "1", "BT3_GS": "1", "SEMMAPA": "1",
                "BT3_GS_FROM": "1000000000", "BT3_IMAGENS_JANELA": "8",
                "BT3_ROTEIRO": str(RAIZ / "port/x360/roteiros/f5_duelo.txt"),
                "BT3_SAVES": str(pasta / "s"), "BT3_IMAGENS": str(pasta / "lista.txt"), **cortes,
            }
            if alvo == "ppc":
                ambiente.update(BT3_DATA=str(dados), BT3_CONVERTIDOS=str(dados / "mods/convertidos.txt"))
            else:
                ambiente["BT3_PAD_REC"] = str(pasta / "controle.bin")
            exe = RAIZ / ("port/build/" + alvo + "b/bt3_" + alvo + "b")
            rodar(caso + "-" + alvo, (["qemu-ppc"] if alvo == "ppc" else []) + [exe], ambiente, limite=3600)
            shutil.copyfile(SAIDA / (caso + "-" + alvo + ".log"), pasta / "output.txt")
        pasta = SAIDA / "imagens" / caso
        rodar(caso + "-compara", ["python3", "port/x360/tools/roteiros.py", "compara_imagens",
                                  pasta / "x86/lista.txt", pasta / "x86", pasta / "ppc", "--saidas",
                                  pasta / "x86/output.txt", pasta / "ppc/output.txt", "--telas", oficial / "telas_a.txt"])
    # Controle negativo com uma cópia real, sem alterar as imagens de prova.
    p = SAIDA / "imagens/base"
    negativo = SAIDA / "negativo"
    negativo.mkdir(exist_ok=True)
    for origem in (p / "ppc").glob("*.ppm"):
        shutil.copyfile(origem, negativo / origem.name)
    arq = negativo / "menu.ppm"
    px = bytearray(arq.read_bytes()); px[-1] ^= 255; arq.write_bytes(px)
    codigo = rodar("imagens-mutante", ["python3", "port/x360/tools/roteiros.py", "compara_imagens",
                   p / "x86/lista.txt", p / "x86", negativo, "--saidas", p / "x86/output.txt",
                   p / "ppc/output.txt", "--telas", oficial / "telas_a.txt"], aceita=(1,))
    if codigo == 0:
        raise RuntimeError("verificador aceitou o pixel estragado")
    # O PC tem o renderizador real do upstream, com o controle gravado da mesma sessão.
    # Os logs e as capturas precisam de julgamento de alinhamento após a coleta.
    pc = RAIZ / "port/build/blur/pc-atual"
    logbase = (SAIDA / "base-x86.log").read_text()
    quadros = re.findall(r"^imagem: \S+ quadro \d+ vblank \d+ gs (\d+)", logbase, re.M)
    (SAIDA / "quadros-console.txt").write_text(",".join(quadros) + "\n")
    for escala in (1, 2):
        for caso in ["base", *[c.lower() for c in CHAVES]]:
            pasta = SAIDA / "pc" / str(escala) / caso
            pasta.mkdir(parents=True, exist_ok=True)
            ambiente = {"BT3_SETTINGS": "", "BT3_NOMOVIE": "1", "BT3_SOUND_TICKS": "1",
                "BT3_UNCAPPED": "1", "BT3_NOSOUND": "1", "BT3_GS": "gpu", "BT3_GPU_API": "gl",
                "BT3_WIDE": "1", "BT3_SCALE": str(escala), "BT3_INTERP": "0", "BT3_FRAME_SKIP": "0",
                "BT3_SHOT": "1", "BT3_SHOT_LISTA": ",".join(quadros), "BT3_FX_OFF": "0",
                "BT3_GLOW": "60", "BT3_GLARE": "60", "BT3_SAVES": str(pasta / "s"),
                "BT3_PAD_PLAY": str(p / "x86/controle.bin"), "SDL_VIDEODRIVER": "offscreen",
                **{"BT3_SEM_" + c: str(int(c.lower() == caso)) for c in CHAVES}}
            codigo = rodar("pc-%d-%s" % (escala, caso), [pc / "port/build/bt3_64"], ambiente,
                           cwd=pc, limite=900, aceita=(1, 3, 124))
            shots = pc / "port/build/shots"
            if shots.exists():
                for imagem in shots.glob("gpu_*.ppm"):
                    shutil.move(str(imagem), pasta / imagem.name)
            if codigo != 0:
                print("PC pendente de diagnóstico:", escala, caso, flush=True)
                # Falha de vídeo comum a todos os casos não justifica dezesseis tentativas.
                (SAIDA / "PC_PENDENTE").write_text("%d %s código %d\n" % (escala, caso, codigo))
                return
    (SAIDA / "COLETA_CONCLUIDA").write_text("Julgar pixels, alinhamento, tabelas e limites antes de entregar.\n")


if __name__ == "__main__":
    try:
        main()
    except Exception as erro:
        SAIDA.mkdir(exist_ok=True)
        (SAIDA / "FALHA").write_text(str(erro) + "\n")
        raise
