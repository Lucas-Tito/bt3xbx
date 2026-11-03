#!/usr/bin/env python3
"""O fluxo da tela SAVE IMPORT (f2/importa_tela.cpp) sem GPU nem console: o D3D e o XInput falsos de
teste_instalar/ui, a lógica falsa. Confere a entrada (borda de descida, SKIP ao abrir, B pula, IMPORT recusado em arquivo
inválido, os limites da lista, a apresentação a cada volta). Mutantes em cópias: cada um tem de reprovar (saída 1)."""
import os
from pathlib import Path
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
UI = RAIZ / "port/x360/src/teste_instalar/ui"
FONTE = RAIZ / "port/x360/src/f2/importa_tela.cpp"
CONV = RAIZ / "port/x360/conv"
SRC = RAIZ / "port/x360/src"
CASOS = ("nenhum", "skip_ao_abrir", "import_valido", "ja_apertado", "b_pula", "b_pula_sem_botao_mudar", "recusado",
         "abre_no_primeiro_bom", "cima_baixo", "borda_descida", "limites", "analogico", "falha_importar", "falha_pular",
         "apresenta")


def compilar(pasta, fonte):
    objs = []
    for n, d in (("save_importa", SRC), ("save_conv", CONV), ("psu", CONV), ("save_larguras", CONV)):
        o = pasta / (n + ".o")
        subprocess.run(["gcc", "-std=c89", "-c", "-I" + str(CONV), "-I" + str(SRC), str(d / (n + ".c")), "-o", str(o)], check=True)
        objs.append(str(o))
    exe = pasta / "importa"
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++03", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-I" + str(UI), "-I" + str(RAIZ / "port/x360/src"), "-I" + str(RAIZ / "port/x360/src/f2"),
                    "-I" + str(CONV), str(fonte), str(UI / "teste_importa.cpp")] + objs + ["-o", str(exe)], check=True)
    return exe


def rodar(exe, caso, esperado=0):
    r = subprocess.run([str(exe), caso], capture_output=True, text=True, errors="replace", timeout=30)
    if r.returncode != esperado or (esperado and "FALHA:" not in r.stderr):
        raise ValueError(f"{caso}: código {r.returncode}: {r.stdout}{r.stderr}")
    print(f"ok: {caso}", flush=True)


MUTANTES = [
    ("IMPORT vale em arquivo recusado", "else if (lista.item[arq].erro != 0)\n                aviso = true;", "else if (0)\n                aviso = true;", "recusado"),
    ("não abre com SKIP selecionado", "acao = 1, i, r;", "acao = 0, i, r;", "skip_ao_abrir"),
    ("B não pula", "if (b & PAD_B) {\n            acao = 1;\n            fim = true;", "if (0) {\n            acao = 1;\n            fim = true;", "b_pula"),
    ("sem borda de descida", "borda |= agora & e.armado[p];", "borda |= agora;", "borda_descida"),
    ("o botão apertado na abertura vale", "e.armado[p] = ~padLer(p);", "e.armado[p] = ~0u;", "ja_apertado"),
    ("a tela não é apresentada", "            telaImportar(dev, kVersaoImp, lista, arq, acao, aviso);\n            dev->Present(NULL, NULL, NULL, NULL);",
     "            telaImportar(dev, kVersaoImp, lista, arq, acao, aviso);", "apresenta"),
    ("não abre no primeiro arquivo bom", "if (lista.item[i].erro == 0) {", "if (0) {", "abre_no_primeiro_bom"),
    ("sem tela de resultado", "impResultado(m, r, falha, false);\n        mensagem(dev, m);", "impResultado(m, r, falha, false);", "falha_importar"),
    ("a lista passa do fim", "if (arq < n - 1)\n                arq++;", "arq++;", "limites"),
    ("a lista não chega ao primeiro", "if (arq > 0)\n                arq--;", "if (arq > 1)\n                arq--;", "cima_baixo"),
    ("SKIP que falha não avisa", "                impResultado(m, r, falha, true);\n                mensagem(dev, m);\n", "", "falha_pular"),
    ("o analógico para baixo não vale", "e.Gamepad.sThumbLY < -PAD_LIMIAR", "0", "analogico"),
]


def main():
    with tempfile.TemporaryDirectory(prefix="importa-tela-") as tmp:
        pasta = Path(tmp)
        exe = compilar(pasta, FONTE)
        for caso in CASOS:
            rodar(exe, caso)
        for i, (desc, de, para, caso) in enumerate(MUTANTES):
            d = pasta / f"m{i}"
            d.mkdir()
            texto = FONTE.read_text()
            if de not in texto:
                raise ValueError(f"mutação '{desc}' não encontrou {de!r}")
            f = d / "importa_tela.cpp"
            f.write_text(texto.replace(de, para))
            print(f"mutante {i} ({desc}): ", end="")
            rodar(compilar(d, f), caso, 1)
    print("ok: tela SAVE IMPORT e", len(MUTANTES), "mutantes reprovados")


if __name__ == "__main__":
    main()
