#!/usr/bin/env python3
"""Conferência serial da fase 2, sob ~/vez.sh e com tee em log persistente.

As listas provam a seleção dos passes. Só as capturas reais provam imagem;
o término deste programa exige julgamento posterior, não aprova o D3D.
"""
import json
import os
import pathlib
import re
import shutil
import subprocess

import blur_rodada as rodada
from blur_console_2 import CASOS, CHAVES

RAIZ = rodada.RAIZ
SAIDA = pathlib.Path.home() / "blur-analise/fase2"
PC = RAIZ / "port/build/blur/pc-atual"
PADRAO = re.compile(r"^desfoque_lista: quadro (\d+) blocos (\d+) fontes (\d+) baixos (\d+) invalidos (\d+) fx (\d+)$", re.M)


def conferir_lista(texto, caso, fx=0):
    linhas = [tuple(map(int, m)) for m in PADRAO.findall(texto)]
    if len(linhas) < 1000 or len({x[0] for x in linhas}) != len(linhas):
        raise RuntimeError("lista ausente, curta ou repetida: " + caso)
    if any(x[4] != 0 or x[5] != fx for x in linhas):
        raise RuntimeError("flag fora do passe permitido ou máscara truncada: " + caso)
    somas = tuple(sum(x[i] for x in linhas) for i in (1, 2, 3))
    if caso in ("c00_base", "fx224") and any(somas):
        raise RuntimeError("o padrão armou tratamento: " + caso)
    if caso in ("c03_distancia_nativa", "c04_distancia_media", "c05_pc_completo") and not all(somas[1:]):
        raise RuntimeError("cadeia nativa não foi exercitada: " + caso)
    if caso in ("c01_blocos_macio", "c02_blocos_sharp", "c04_distancia_media", "c05_pc_completo") and not somas[0]:
        raise RuntimeError("amostragem por blocos não foi exercitada: " + caso)
    if caso == "c03_distancia_nativa" and somas[0]:
        raise RuntimeError("a redução sem média armou blocos")
    return linhas


def negativo_lista():
    texto = "\n".join("desfoque_lista: quadro %d blocos 0 fontes 0 baixos 0 invalidos 0 fx 224" % i for i in range(1001))
    conferir_lista(texto, "fx224", 224)
    for adulterado in (texto.replace("invalidos 0", "invalidos 1", 1), texto.replace("fx 224", "fx 0", 1), ""):
        try:
            conferir_lista(adulterado, "fx224", 224)
        except RuntimeError as e:
            print("negativo da lista recusado:", e, flush=True)
        else:
            raise RuntimeError("verificador aceitou lista adulterada")


def ambiente(pasta, ajustes):
    env = {"BT3_SETTINGS": "", "BT3_SOUND_TICKS": "1", "BT3_NOMOVIE": "1", "BT3_MENU_ITEM4": "0",
           "BT3_DESBLOQUEIO": "1", "BT3_UNCAPPED": "1", "BT3_NOSOUND": "1", "BT3_WIDE": "1",
           "BT3_FX_OFF": "0", "BT3_SCALE": "1", "BT3_GS": "1", "SEMMAPA": "1",
           "BT3_GS_FROM": "1000000000", "BT3_IMAGENS_JANELA": "8",
           "BT3_ROTEIRO": str(RAIZ / "port/x360/roteiros/f5_duelo.txt"), "BT3_SAVES": str(pasta / "s"),
           **{k: "0" for k in CHAVES}, **{"BT3_SEM_" + c: "0" for c in rodada.CHAVES}, **ajustes}
    return env


def pc_caso(escala, nome, ajustes, controle, quadros):
    pasta = SAIDA / "pc" / str(escala) / nome
    pasta.mkdir(parents=True)
    env = ambiente(pasta, ajustes)
    env.pop("BT3_ROTEIRO")
    # As duas cadeias têm o mesmo marcador no PC. O adaptador isola suas origens;
    # a chave global de resolução é ligada se qualquer uma delas precisar.
    env.update(BT3_PC_BRILHO_NATIVO=env["BT3_GLOW_NATIVE"],
               BT3_GLOW_NATIVE=str(int(env["BT3_GLOW_NATIVE"] == "1" or env["BT3_DISTANCIA_NATIVA"] == "1")),
               BT3_GS="gpu", BT3_GPU_API="gl", BT3_GS_THREAD="0", BT3_SCALE=str(escala), BT3_GS_FROM="0",
               BT3_INTERP="0", BT3_FRAME_SKIP="0", BT3_SHOT="1", BT3_SHOT_LISTA=",".join(quadros),
               BT3_GLOW="60", BT3_GLARE="60", BT3_PAD_PLAY=str(controle), SDL_VIDEODRIVER="offscreen")
    rodada.rodar("pc-%d-%s" % (escala, nome), [PC / "port/build/bt3_64"], env, cwd=PC, limite=1200)
    for imagem in (PC / "port/build/shots").glob("gpu_*.ppm"):
        shutil.move(str(imagem), pasta / imagem.name)
    rodada.conferir_pc(pasta, quadros, escala)


def main():
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=RAIZ).strip():
        raise RuntimeError("árvore suja antes da rodada")
    SAIDA.mkdir(parents=True, exist_ok=True)
    if (SAIDA / "INICIO").exists():
        raise RuntimeError("rodada já iniciada; preserve os resultados antes de repetir")
    (SAIDA / "INICIO").write_bytes(subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=RAIZ))
    rodada.SAIDA = SAIDA
    (SAIDA / "base-102").symlink_to(SAIDA.parent / "base-102", target_is_directory=True)
    negativo_lista()
    rodada.rodar("pc-build", ["python3", "port/x360/tools/blur_pc_build.py", PC])
    quadros = (SAIDA.parent / "quadros-console.txt").read_text().strip().split(",")
    # Primeiro resolve a falha que interrompeu a coleta anterior, antes das horas
    # de emulador. A gravação pertence a esta tarefa, não a ~/referencias.
    controle = SAIDA.parent / "imagens/base/x86/controle.bin"
    pc_caso(1, "c00_base", {}, controle, quadros)
    for alvo, extra in (("x86", []), ("x86", ["--rastreio"]), ("ppc", [])):
        rodada.rodar("final-" + alvo + ("r" if extra else ""),
                     ["python3", "port/x360/tools/headless.py", "--alvo", alvo, *extra], {"BT3_TESTE_DESFOQUE": "0"})
    rodada.objetos()
    rodada.rodar("oficial", ["bash", "port/x360/tools/roteiros.sh", "f5_duelo"],
                 {"SEMMAPA": "1", "IMG": "1", "O": str(SAIDA / "oficial"), "TP": "3600"})
    oficial = SAIDA / "oficial/f5_duelo"
    rodada.conferir_ancoras(oficial / "telas_a.txt")
    dados = oficial / "dados"
    for alvo in ("x86", "ppc"):
        for gpu in (False, True):
            rodada.rodar("build-" + alvo + ("-gpu" if gpu else "-software"),
                         ["python3", "port/x360/tools/headless.py", "--alvo", alvo, *(["--gpu", "--vu1"] if gpu else [])],
                         {"BT3_TESTE_DESFOQUE": "1"})
    provas = {}
    for nome, ajustes in CASOS[:6] + [("fx224", {"BT3_FX_OFF": "224"})]:
        for gpu in (False, True):
            if nome == "fx224" and not gpu:
                continue
            listas = []
            for alvo in ("x86", "ppc"):
                pasta = SAIDA / ("gpu" if gpu else "imagens") / nome / alvo
                pasta.mkdir(parents=True)
                env = ambiente(pasta, ajustes)
                sufixo = "bgv" if gpu else "b"
                if gpu:
                    env.update(BT3_GS="gpu", BT3_GS_FROM="0", BT3_GPU_LISTA="1", BT3_DESFOQUE_CONTA="1")
                else:
                    (pasta / "lista.txt").write_text(rodada.LISTA)
                    env["BT3_IMAGENS"] = str(pasta / "lista.txt")
                if alvo == "ppc":
                    env.update(BT3_DATA=str(dados), BT3_CONVERTIDOS=str(dados / "mods/convertidos.txt"))
                else:
                    env["BT3_PAD_REC"] = str(pasta / "controle.bin")
                exe = RAIZ / ("port/build/" + alvo + sufixo + "/bt3_" + alvo + sufixo)
                lognome = nome + "-" + alvo + ("-gpu" if gpu else "-software")
                rodada.rodar(lognome, (["qemu-ppc"] if alvo == "ppc" else []) + [exe], env, limite=3600)
                shutil.copyfile(SAIDA / (lognome + ".log"), pasta / "output.txt")
                if gpu:
                    listas.append(conferir_lista((pasta / "output.txt").read_text(), nome, 224 if nome == "fx224" else 0))
            if gpu:
                if listas[0] != listas[1]:
                    raise RuntimeError("seleção dos passes x86/PPC diferente: " + nome)
                provas[nome] = {"quadros": len(listas[0]), "blocos": sum(x[1] for x in listas[0]),
                                "fontes": sum(x[2] for x in listas[0]), "baixos": sum(x[3] for x in listas[0])}
            else:
                pasta = SAIDA / "imagens" / nome
                rodada.rodar(nome + "-compara", ["python3", "port/x360/tools/roteiros.py", "compara_imagens",
                             pasta / "x86/lista.txt", pasta / "x86", pasta / "ppc", "--saidas",
                             pasta / "x86/output.txt", pasta / "ppc/output.txt", "--telas", oficial / "telas_a.txt"])
        (SAIDA / "listas.json").write_text(json.dumps(provas, indent=2) + "\n")
    for escala in (1, 2):
        for nome, ajustes in CASOS:
            if escala == 1 and nome == "c00_base":
                continue
            pc_caso(escala, nome, ajustes, controle, quadros)
        for c in rodada.CHAVES:
            pc_caso(escala, "corte_" + c.lower(), {"BT3_SEM_" + c: "1"}, controle, quadros)
    (SAIDA / "COLETA_CONCLUIDA").write_text("Julgar alinhamento, regiões e limites do D3D antes da entrega.\n")


if __name__ == "__main__":
    try:
        main()
    except Exception as erro:
        SAIDA.mkdir(parents=True, exist_ok=True)
        (SAIDA / "FALHA").write_text(str(erro) + "\n")
        raise
