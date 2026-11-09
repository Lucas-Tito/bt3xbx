#!/usr/bin/env python3
"""Ambientes livres da fase 2. Não acessa o console nem altera padrões."""
import pathlib
import sys

CHAVES = ["BT3_SHARP_BLUR", "BT3_DISTANCIA_NATIVA", "BT3_DISTANCIA_MEDIA",
          "BT3_GLOW_BLOCK", "BT3_GLOW_NATIVE"]
CASOS = [
    ("c00_base", {}),
    ("c01_blocos_macio", {"BT3_GLOW_BLOCK": "1"}),
    ("c02_blocos_sharp", {"BT3_GLOW_BLOCK": "1", "BT3_SHARP_BLUR": "1"}),
    ("c03_distancia_nativa", {"BT3_DISTANCIA_NATIVA": "1"}),
    ("c04_distancia_media", {"BT3_DISTANCIA_NATIVA": "1", "BT3_DISTANCIA_MEDIA": "1"}),
    ("c05_pc_completo", {k: "1" for k in CHAVES}),
    ("c06_sem_distancia", {"BT3_SEM_DISTANCIA": "1"}),
    ("c07_repeticao", {}),
]


def preparar(destino):
    destino.mkdir(parents=True, exist_ok=True)
    for nome, ajustes in CASOS:
        valores = {
            "BT3_LOG": "1", "BT3_ISO": "game:\\bt3-x360.iso", "BT3_SETTINGS": "",
            "BT3_WIDE": "1", "BT3_FX_OFF": "0", "BT3_GLOW": "60",
            "BT3_FXAA": "0", "BT3_MSAA": "2", "BT3_AFIADO": "1",
            "BT3_ESCALA": "1.4", "BT3_ALVO_AREA": "640x480",
            "BT3_SHOT": "300", "BT3_SHOT_CHEIA": "1", "BT3_SHOT_FINAL": "1",
            "BT3_SHOT_PASTA": nome,
            **{k: "0" for k in CHAVES},
            **{"BT3_SEM_" + k: "0" for k in
               ("DISTANCIA", "PANORAMICO", "RADIAL", "BRUMA", "NEVOA", "BRILHO", "CLIMA")},
            **ajustes,
        }
        texto = "# Comparação livre do desfoque: " + nome + "\n"
        texto += "\n".join(k + "=" + v for k, v in valores.items()) + "\n"
        (destino / ("environment_" + nome + ".txt")).write_text(texto)
    (destino / "ordem.txt").write_text("\n".join(n for n, _ in CASOS) + "\n")
    print("blur_console_2: oito ambientes livres em", destino)


if __name__ == "__main__":
    preparar(pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path.home() / "blur-console-2")
