#!/usr/bin/env python3
"""Prepara os ambientes da rodada do desfoque, sem acessar o console."""
import pathlib
import shutil
import sys

RAIZ = pathlib.Path(__file__).resolve().parents[3]
CHAVES = ["DISTANCIA", "PANORAMICO", "RADIAL", "BRUMA", "NEVOA", "BRILHO"]
CASOS = [("b00_base", {})] + [("b%02d_%s" % (i + 1, n.lower()), {"BT3_SEM_" + n: "1"})
                              for i, n in enumerate(CHAVES)] + [
    ("b07_sem_msaa", {"BT3_MSAA": "0"}),
    ("b08_escala1", {"BT3_ESCALA": "1", "BT3_ALVO_AREA": "640x480"}),
    ("b09_escala133", {"BT3_ESCALA": "1.33", "BT3_ALVO_AREA": "648x456"}),
    ("b10_fxaa", {"BT3_FXAA": "1"}),
    ("b11_copia_ponto", {"BT3_COPIA_PONTO": "1"}),
    ("b12_copia_afiada", {"BT3_AFIADO": "3"}),
    ("b13_sem_efeitos", {"BT3_SEM_" + n: "1" for n in CHAVES}),
    ("b14_repeticao", {}),
]
QUADROS = "2900,2990,3350,3450,4600,4673,4800,4975,5335,5355,5416,5450"


def preparar(destino):
    destino.mkdir(parents=True, exist_ok=True)
    for nome in ("boot.txt", "f5_duelo.txt"):
        shutil.copyfile(RAIZ / "port/x360/roteiros" / nome, destino / nome)
    for nome, ajustes in CASOS:
        ambiente = {
            "BT3_ISO": "game:\\bt3-x360.iso", "BT3_ROTEIRO": "game:\\f5_duelo.txt",
            "BT3_SAVES": "game:\\sv_blur_" + nome, "BT3_SETTINGS": "",
            "BT3_NOMOVIE": "1", "BT3_NOSOUND": "1", "BT3_SOUND_TICKS": "1",
            "BT3_DESBLOQUEIO": "1", "BT3_MENU_ITEM4": "0", "BT3_WIDE": "1",
            "BT3_FX_OFF": "0", "BT3_GLOW": "60", "BT3_FXAA": "0", "BT3_MSAA": "2",
            "BT3_ESCALA": "1.4", "BT3_ALVO_AREA": "640x480", "BT3_AFIADO": "1",
            "BT3_SHOT_LISTA": QUADROS, "BT3_SHOT_CHEIA": "1", "BT3_SHOT_FINAL": "1",
            "BT3_SHOT_PASTA": nome, "BT3_TEMPOS": "0",
            **{"BT3_SEM_" + n: "0" for n in CHAVES}, **ajustes,
        }
        texto = "# Rodada do desfoque: " + nome + "\n"
        texto += "\n".join(k + "=" + v for k, v in ambiente.items()) + "\n"
        (destino / ("environment_" + nome + ".txt")).write_text(texto)
    (destino / "ordem.txt").write_text("\n".join(n for n, _ in CASOS) + "\n")
    shutil.copyfile(destino / "environment_b00_base.txt", destino / "environment.txt")
    print("blur_console: %d ambientes em %s" % (len(CASOS), destino))


if __name__ == "__main__":
    preparar(pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path.home() / "blur-console"))
