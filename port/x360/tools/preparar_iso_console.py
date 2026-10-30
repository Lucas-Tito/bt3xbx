#!/usr/bin/env python3
"""Prepara no host o selo versão 4 idêntico ao do instalador PC desta build.

Conserve build/x360/iso/conv_tool para reproduzir a marca no PC com --conv-tool.
Não lê a ISO nem dados do jogo. O XDK não precisa executar programas nativos.
"""
import argparse
import os
from pathlib import Path
import subprocess
import convert_iso


def preparar(pasta):
    pasta.mkdir(parents=True, exist_ok=True)
    tool = pasta / "conv_tool"
    fontes = sorted((convert_iso.RAIZ / "port/x360/conv").glob("*.c"))
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c89", "-O2", "-Wall", "-o", str(tool),
                    *(str(p) for p in fontes if p.name != "conv_jogo.c")], check=True)
    selo = convert_iso.marca(tool)
    (pasta / "iso_selo.h").write_text(
        "/* Gerado no host, sem dados do jogo. Mesmo selo do convert_iso.py. */\n"
        "static const unsigned char sIsoSelo[512] = {\n" +
        ",\n".join("    " + ",".join(f"0x{x:02x}" for x in selo[i:i+16]) for i in range(0,512,16)) + "\n};\n")
    (pasta / "selo.bin").write_bytes(selo)
    print(f"selo versão 4 e ferramenta PC: {pasta}", flush=True)
    return tool


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--saida", type=Path, default=convert_iso.RAIZ / "build/x360/iso")
    preparar(p.parse_args().saida)
