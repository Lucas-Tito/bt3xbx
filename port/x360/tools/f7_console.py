#!/usr/bin/env python3
"""Gera ambientes novos e exclusivos da sonda F7/03, sem acessar o console nem criar saves."""
import argparse
import pathlib
import re
import sys


def gerar(destino, rodada):
    if not re.fullmatch(r"[a-z0-9]{4,12}", rodada):
        raise ValueError("rodada: 4 a 12 letras minúsculas ou dígitos")
    destino = pathlib.Path(destino)
    destino.mkdir(parents=True, exist_ok=False)
    raiz = pathlib.Path(__file__).resolve().parents[3]
    comum = (raiz / "port/x360/ambientes/f7-etapa3/comum.txt").read_text()
    for nome, roteiro in (("01_simples", "f7_perfil_simples"), ("02_dividido", "f7_perfil_dividido"),
                          ("03_fluxo", "f7_dividido"), ("04_real", None)):
        # Nomes FATX curtos; a pasta é diferente em cada execução, mesmo dentro da rodada.
        pasta = f"f7e3_{rodada}_{nome}"
        s = comum + f"\nBT3_SAVES=game:\\{pasta}\nBT3_SHOT_PASTA={pasta}\n"
        if roteiro:
            s += f"BT3_DESBLOQUEIO=1\nBT3_ROTEIRO=game:\\roteiros\\{roteiro}.txt\n"
        else:
            s += "-BT3_DESBLOQUEIO\n-BT3_ROTEIRO\n"
        (destino / (nome + ".txt")).write_text(s)
    (destino / "origem.txt").write_text(f"rodada {rodada}\nsonda informativa; console ainda não medido\n")
    print(f"quatro ambientes em {destino}; conferir no console que as quatro pastas de save ainda não existem")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("destino")
    ap.add_argument("rodada")
    a = ap.parse_args()
    try:
        gerar(a.destino, a.rodada)
    except (ValueError, OSError) as e:
        print(f"FALHA: {e}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
