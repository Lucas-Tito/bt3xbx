#!/usr/bin/env python3
"""Prova host da tela de conversão do 360: compila o desenho real (f2/iso_tela_desenho.h) com um D3D falso que pinta
num buffer, confere o giro (por batida, nunca por tempo), as cores e a falha, e grava PNGs 1280x720 de seis estados.
Cópias estragadas do desenho precisam falhar com saída 1.

    prova_tela.py [--png PASTA]      (sem --png, só as conferências e os mutantes)
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

RAIZ = Path(__file__).resolve().parents[3]
DESENHO = RAIZ / "port/x360/src/f2/iso_tela_desenho.h"
PROVA = RAIZ / "port/x360/src/teste_instalar/prova"


def compilar(pasta, desenho):
    exe = pasta / "prova_tela"
    # O desenho vem de uma cópia (o mutante) na pasta própria; os demais headers, do repositório.
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++03", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-Wno-unused-parameter", "-I" + str(desenho.parent), "-I" + str(PROVA),
                    "-I" + str(RAIZ / "port/x360/src"), "-I" + str(RAIZ / "port/x360/conv"),
                    str(PROVA / "prova_tela.cpp"), "-o", str(exe)], check=True)
    return exe


def png(ppm, destino):
    dados = ppm.read_bytes()
    cab, resto = dados.split(b"255\n", 1)
    assert cab.startswith(b"P6\n1280 720\n")
    larg, alt = 1280, 720
    linhas = b"".join(b"\0" + resto[y * larg * 3:(y + 1) * larg * 3] for y in range(alt))

    def bloco(tipo, d):
        return struct.pack(">I", len(d)) + tipo + d + struct.pack(">I", zlib.crc32(tipo + d))
    destino.write_bytes(b"\x89PNG\r\n\x1a\n" + bloco(b"IHDR", struct.pack(">IIBBBBB", larg, alt, 8, 2, 0, 0, 0))
                        + bloco(b"IDAT", zlib.compress(linhas, 6)) + bloco(b"IEND", b""))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--png", type=Path)
    args = p.parse_args()
    versao = next(l.split("=", 1)[1].strip() for l in (RAIZ / "port/x360/package/versao.env").read_text().splitlines()
                  if l.startswith("VERSION="))
    with tempfile.TemporaryDirectory(prefix="prova-tela-") as tmp:
        pasta = Path(tmp)
        exe = compilar(pasta, DESENHO)
        r = subprocess.run([str(exe), "checar"], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f"FALHA: a tela real foi recusada: {r.stderr}")
        print("ok: tela real aprovada nas conferências", flush=True)
        if args.png:
            args.png.mkdir(parents=True, exist_ok=True)
            bruto = pasta / "ppm"
            bruto.mkdir()
            subprocess.run([str(exe), "gravar", str(bruto), versao], check=True)
            for f in sorted(bruto.glob("*.ppm")):
                png(f, args.png / (f.stem + ".png"))
                print(f"png: {args.png / (f.stem + '.png')}", flush=True)
        texto = DESENHO.read_text()
        mutantes = [
            ("giro por tempo em vez de batida", "telaGiro(e.c.batidas)", "telaGiro(e.ms / 250)", "giro mudou com o tempo"),
            ("giro parado", "[batidas & 3]", "[0]", "giro não avançou"),
            ("FAILED sem vermelho", "ISO_CONV_T_FALHOU, TELA_FALHA", "ISO_CONV_T_FALHOU, TELA_TEXTO", "sem FAILED vermelho"),
            ("giro em toda linha", "gira_agora = !ok && e.fase == IC_CONVERTER && e.c.atual == i - 1;", "gira_agora = !ok;", "fora da linha da categoria atual"),
            ("OK antes da conferência", "ok = e.fase >= IC_COPIAR;", "ok = true;", "OK verde antes"),
        ]
        for i, (nome, de, para, acusacao) in enumerate(mutantes):
            if de not in texto:
                raise SystemExit(f"FALHA: o mutante '{nome}' não achou o alvo")
            d = pasta / f"m{i}"
            d.mkdir()
            (d / DESENHO.name).write_text(texto.replace(de, para))
            r = subprocess.run([str(compilar(d, d / DESENHO.name)), "checar"], capture_output=True, text=True)
            if r.returncode != 1 or "FALHA:" not in r.stderr or (acusacao and acusacao not in r.stderr):
                if acusacao is None and r.returncode == 0:
                    print(f"aviso: o mutante '{nome}' não é pego pelas conferências (só a imagem mostra)", flush=True)
                    continue
                raise SystemExit(f"FALHA: mutante '{nome}' não foi recusado (código {r.returncode}): {r.stderr}")
            print(f"ok: mutante '{nome}' recusado com saída 1: {r.stderr.strip()}", flush=True)
    print("prova_tela: tudo certo", flush=True)


if __name__ == "__main__":
    main()
