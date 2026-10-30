#!/usr/bin/env python3
"""Julga os arquivos preservados por save_mc.sh, sem rodar o emulador."""
from pathlib import Path
import re
import sys


def conferir(pasta):
    def texto(nome):
        return (pasta / nome).read_text(errors="replace")

    def exige(condicao, motivo):
        if not condicao:
            raise ValueError(motivo)

    for alvo in ("x86", "ppc"):
        ensaio = texto(f"t_{alvo}.txt")
        exige(ensaio.endswith("ok: 0 falha(s)\n") and "FALHOU" not in ensaio,
              f"teste_mc {alvo} sem aprovação")
    for nome in ("x86_1", "x86_2", "gdb_A", "gdb_F", "gdb_B1", "gdb_B2"):
        log = texto(nome + ".txt")
        exige(re.search(r"Breakpoint \d+, MainMenu_Init", log) and
              re.search(r"^\$1 = [0-9]+$", log, re.M), f"{nome} não chegou ao menu")
    for nome, rodada in (("A", 1), ("B1", 2), ("B2", 2)):
        estados = texto(f"estados_{nome}.txt")
        exige(estados.strip() and estados == texto(f"estados_x86_{rodada}.txt"),
              f"estados {nome} diferentes da x86")
    exige("ESTADO 11 " in texto("estados_F.txt"), "F sem estado de erro de escrita")
    exige("FALHOU (escrita (teste: BT3_MC_FALHA))" in texto("ppc_F.txt"),
          "F sem falha de escrita injetada")
    dados = "BASLUS-21678DBZT3"
    rel = Path("card1") / dados / dados
    anterior = (pasta / "c_A" / rel).read_bytes()
    exige(len(anterior) == 16384, "A sem save completo")
    exige(f"mc: gravou {dados}/{dados} (16384 bytes): ok" in texto("ppc_A.txt"),
          "A sem gravação confirmada")
    for nome in ("B1", "B2"):
        arquivo = pasta / f"c_{nome}" / rel
        exige(arquivo.read_bytes() == anterior, f"{nome} não recuperou o save de A")
        exige(not Path(str(arquivo) + ".tmp").exists() and
              not Path(str(arquivo) + ".new").exists(), f"{nome} deixou temporário")
    exige(not (pasta / "c_F" / rel).exists(), "F deixou save após falha de escrita")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("uso: confere_save_mc.py PASTA_SAVE_MC")
    try:
        conferir(Path(sys.argv[1]))
    except (OSError, ValueError) as erro:
        sys.exit(f"confere_save_mc: FALHA: {erro}")
    print("confere_save_mc: tudo certo")
