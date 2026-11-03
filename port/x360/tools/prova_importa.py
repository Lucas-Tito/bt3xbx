#!/usr/bin/env python3
"""Prova host da tela SAVE IMPORT: compila o desenho real (f2/importa_tela_desenho.h) com o D3D falso do prova_tela
(que pinta num buffer), confere fonte, colunas, paleta, seleção, janela da lista e os textos do resultado, e grava PNGs
1280x720 de doze estados. Cópias estragadas do desenho precisam falhar com saída 1.

    prova_importa.py [--png PASTA]      (sem --png, só as conferências e os mutantes)
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from prova_tela import png  # noqa: E402

RAIZ = Path(__file__).resolve().parents[3]
DESENHO = RAIZ / "port/x360/src/f2/importa_tela_desenho.h"
BASE = RAIZ / "port/x360/src/f2/iso_tela_desenho.h"
PROVA = RAIZ / "port/x360/src/teste_instalar/prova"
CONV = RAIZ / "port/x360/conv"
SRC = RAIZ / "port/x360/src"


def compilar(pasta, desenho, mutante=False):
    objs = []
    for n, d in (("save_importa", SRC), ("save_conv", CONV), ("psu", CONV), ("save_larguras", CONV)):
        o = pasta / (n + ".o")
        subprocess.run(["gcc", "-std=c89", "-c", "-I" + str(CONV), "-I" + str(SRC), str(d / (n + ".c")), "-o", str(o)], check=True)
        objs.append(str(o))
    exe = pasta / "prova_importa"
    # o desenho e a fonte vêm de cópias (os mutantes) nas pastas próprias; os demais headers, do repositório
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++03", "-O1", "-Wall", "-Wextra", "-Werror"] + (["-Wno-error"] if mutante else []) + [
                    "-Wno-unused-function", "-Wno-unused-parameter", "-I" + str(desenho.parent), "-I" + str(PROVA),
                    "-I" + str(RAIZ / "port/x360/src"), "-I" + str(CONV), str(PROVA / "prova_importa.cpp")] + objs
                   + ["-o", str(exe)], check=True)
    return exe


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--png", type=Path)
    args = p.parse_args()
    versao = next(l.split("=", 1)[1].strip() for l in (RAIZ / "port/x360/package/versao.env").read_text().splitlines()
                  if l.startswith("VERSION="))
    with tempfile.TemporaryDirectory(prefix="prova-importa-") as tmp:
        pasta = Path(tmp)
        # uma cópia do desenho e da fonte lado a lado: o include "iso_tela_desenho.h" acha a cópia do mesmo diretório
        def copia(nome, trocas=()):
            d = pasta / nome
            d.mkdir()
            for f in (DESENHO, BASE):
                t = f.read_text()
                for de, para, alvo in trocas:
                    if alvo == f.name:
                        if de not in t:
                            raise SystemExit(f"FALHA: o mutante '{nome}' não achou {de!r}")
                        t = t.replace(de, para)
                (d / f.name).write_text(t)
            return d
        d0 = copia("original")
        exe = compilar(d0, d0 / DESENHO.name)
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
        D, B = DESENHO.name, BASE.name
        mutantes = [
            ("nome longo sem reticências", "if (n <= IMP_NOME) {", "if (1) {", D, "nome longo não foi truncado"),
            ("sem janela da lista", "if (n <= IMP_JANELA)\n        return 0;", "return 0;\n    if (0)\n        return 0;", D, "a janela não acompanha"),
            ("aviso do backup sempre", "    if (l.ha_save)\n        texto(dev, ISO_IMP_T_AVISO", "    if (1)\n        texto(dev, ISO_IMP_T_AVISO", D, "aviso do backup não depende"),
            ("a ação não aparece", "telaBotoes(buf, ISO_IMP_T_IMPORTAR, ISO_IMP_T_PULAR, acao);", "telaBotoes(buf, ISO_IMP_T_IMPORTAR, ISO_IMP_T_PULAR, 0);", D, "a seleção IMPORT/SKIP não aparece"),
            ("recusado em verde", "l.item[i].erro ? TELA_FALHA : TELA_OK", "TELA_OK", D, "sem OK verde ou sem motivo vermelho"),
            ("o selecionado não se destaca", "unsigned cor = i == arq ? IMP_SEL : TELA_TEXTO;", "unsigned cor = TELA_TEXTO;", D, "não se destaca"),
            ("sem aviso do recusado", "    if (aviso)\n        texto(dev, ISO_IMP_T_RECUSADO", "    if (0)\n        texto(dev, ISO_IMP_T_RECUSADO", D, "aviso do recusado não aparece"),
            ("sem _ na fonte", "case '_': return 50;", "case '_': return -1;", B, "sem desenho na fonte"),
            ("sem ( na fonte", "case '(': return 51;", "case '(': return -1;", B, "sem desenho na fonte"),
            ("a lista não rola", "int i, ini = impInicio(l.n, arq), linha = 0;", "int i, ini = 0, linha = 0;", D, "a lista longa não rola"),
            ("falha de troca sem o backup", "        if (f.backup) {\n            sprintf(b, ISO_IMP_T_COPIA, f.backup);", "        if (0) {\n            sprintf(b, ISO_IMP_T_COPIA, f.backup);", D, "não aponta o backup"),
            ("falha de gravação sem dizer que o save ficou", "impAdd(m, f.passo == SI_P_TROCA ? ISO_IMP_T_RECOLOCADO : ISO_IMP_T_INTACTO);", "if (f.passo == SI_P_TROCA) impAdd(m, ISO_IMP_T_RECOLOCADO);", D, "não diz que o save ficou intacto"),
            ("lista fora das colunas", "#define IMP_NOME 28", "#define IMP_NOME 40", D, None),
        ]
        for i, (nome, de, para, alvo, acusacao) in enumerate(mutantes):
            d = copia(f"m{i}", [(de, para, alvo)])
            r = subprocess.run([str(compilar(d, d / D, True)), "checar"], capture_output=True, text=True)
            if r.returncode != 1 or "FALHA:" not in r.stderr or (acusacao and acusacao not in r.stderr):
                raise SystemExit(f"FALHA: mutante '{nome}' não foi recusado (código {r.returncode}): {r.stderr}")
            print(f"ok: mutante '{nome}' recusado com saída 1: {r.stderr.strip()}", flush=True)
    print("prova_importa: tudo certo", flush=True)


if __name__ == "__main__":
    main()
