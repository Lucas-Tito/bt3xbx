#!/usr/bin/env python3
"""Fluxo C++ real das caixas, sem GPU, console ou emulador. Mutantes em cópias."""
import os
from pathlib import Path
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
UI = RAIZ / "port/x360/src/teste_instalar/ui"
FONTE = RAIZ / "port/x360/src/f2/iso_tela.cpp"


def compilar(pasta, fonte):
    exe = pasta / "tela"
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++03", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-I" + str(UI), "-I" + str(RAIZ / "port/x360/src"),
                    "-I" + str(RAIZ / "port/x360/src/f2"), "-I" + str(RAIZ / "port/x360/conv"),
                    str(fonte), str(UI / "teste_tela.cpp"), "-o", str(exe)], check=True)
    return exe


def rodar(exe, caso, esperado=0):
    r = subprocess.run([str(exe), caso], capture_output=True, text=True)
    if r.returncode != esperado or (esperado and "FALHA:" not in r.stderr):
        raise ValueError(f"{caso}: código {r.returncode}: {r.stdout}{r.stderr}")
    print(f"ok: {caso}", flush=True)


def main():
    with tempfile.TemporaryDirectory(prefix="iso-tela-") as tmp:
        pasta = Path(tmp)
        exe = compilar(pasta, FONTE)
        for caso in ("sucesso", "esquerda_direita", "analogico", "analogico_fraco", "cancelar", "cancelar_b",
                     "direita_cancela", "ja_apertado", "preso", "outra_porta", "apresenta", "sucesso_b", "limpeza",
                     "thread", "espaco", "destino", "falha", "falha_b"):
            rodar(exe, caso)
        for i, (de, para, caso) in enumerate([
            ("XSetThreadProcessor(h, 2)", "XSetThreadProcessor(h, 4)", "sucesso"),
            ("WaitForSingleObject(h, 0)", "WaitForSingleObject(h, 1)", "sucesso"),
            ("r = trabalhar(dev, limpar);", "r = IC_OK;", "limpeza"),
            ("int sel = dois ? 1 : 0,", "int sel = 0,", "cancelar"),
            ("borda = agora & armado[p]", "borda = agora | (armado[p] & 0)", "ja_apertado"),
            ("borda = agora & armado[p]", "borda = agora | (armado[p] & 0)", "preso"),
            ("} else if (borda & PAD_B) {", "} else if (0) {", "cancelar_b"),
            ("escolha = sel;", "escolha = 1;", "sucesso"),
            ("(borda & PAD_ESQ) && sel != 0", "(borda & PAD_ESQ) && 0", "sucesso"),
            ("sThumbLX < -PAD_LIMIAR", "sThumbLX < -5000", "analogico_fraco"),
            ("        dev->Present(NULL, NULL, NULL, NULL);\n    }\n    Diario_Escrever(\"tela: pergunta '%s' escolheu",
             "    }\n    Diario_Escrever(\"tela: pergunta '%s' escolheu", "apresenta"),
            ("    Diario_Escrever(\"tela: pergunta '%s' aberta",
             "    XShowMessageBoxUI(0, titulo, msg, 0, 0, 0, 0, 0, 0);\n    Diario_Escrever(\"tela: pergunta '%s' aberta", "sucesso"),
        ]):
            d = pasta / str(i)
            d.mkdir()
            texto = FONTE.read_text()
            if de not in texto:
                raise ValueError(f"mutação não encontrou {de}")
            f = d / "iso_tela.cpp"
            f.write_text(texto.replace(de, para))
            rodar(compilar(d, f), caso, 1)
        print("teste_iso_tela: tudo certo", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, subprocess.SubprocessError) as e:
        raise SystemExit(f"FALHA: {e}")
