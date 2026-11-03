#!/usr/bin/env python3
"""Lê o mapa de larguras do save (port/x360/conv/save_larguras.c) compilando e rodando save_larguras_tool.c: a tabela
fica só em C (uma fonte), as ferramentas Python a recebem pronta.

    from save_larguras import carrega
    mapa = carrega()                      # {"BASLUS-21678DBZT3": [Faixa, ...], "icon.sys": [...], "dbzsm.ico": [...]}
    for f in mapa["icon.sys"]: f.desl, f.qtd, f.larg, f.cat, f.nome

    save_larguras.py                      imprime a tabela (uma linha por faixa)
"""
import pathlib, subprocess, sys, tempfile
from collections import namedtuple

RAIZ = pathlib.Path(__file__).resolve().parents[3]
FERRAMENTA = RAIZ / "port/x360/tools/save_larguras_tool.c"
CONV = RAIZ / "port/x360/conv"
CATEGORIAS = ("tipado", "resolvido", "sem acesso", "disco")
Faixa = namedtuple("Faixa", "desl qtd larg cat nome")


def compila(destino, conv=CONV):
    """gcc -std=c89 da ferramenta com a tabela de `conv`; devolve o executável. Levanta RuntimeError se não compilar."""
    exe = pathlib.Path(destino) / "save_larguras_tool"
    r = subprocess.run(["gcc", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", f"-I{conv}", "-o", str(exe),
                        str(FERRAMENTA), str(pathlib.Path(conv) / "save_larguras.c")], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("compilação falhou:\n" + r.stderr)
    return exe


def le_faixas(exe):
    mapa = {}
    r = subprocess.run([str(exe), "imprime"], capture_output=True, text=True, check=True)
    for linha in r.stdout.splitlines():
        arq, desl, qtd, larg, cat, nome = linha.split(" ", 5)
        mapa.setdefault(arq, []).append(Faixa(int(desl, 16), int(qtd), int(larg), int(cat), nome))
    return mapa


def carrega(conv=CONV):
    with tempfile.TemporaryDirectory() as d:
        return le_faixas(compila(d, conv))


def numeros(faixas):
    """as faixas planas: [(desl, larg, cat)], um por número"""
    return [(f.desl + i * f.larg, f.larg, f.cat) for f in faixas for i in range(f.qtd)]


if __name__ == "__main__":
    for arq, fs in carrega().items():
        for f in fs:
            print(f"{arq} {f.desl:#06x} x{f.qtd} de {f.larg} byte(s) [{CATEGORIAS[f.cat]}] {f.nome}")
