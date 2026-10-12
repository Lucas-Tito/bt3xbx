#!/usr/bin/env bash
# Envia a build sem janela ao console: o .xex SEMPRE com o .dat e o .swap da mesma build. Os dois dependem dos
# endereços do .map; um .dat de outra build copia as tabelas do disco para os lugares errados, em silêncio.
#   port/x360/tools/subir_f2.sh [--ip IP] [arquivos extras, como build/x360/f2/ambiente.txt]
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$RAIZ/build/x360/f2
IP=()
if [ "${1:-}" = "--ip" ]; then IP=(--ip "$2"); shift 2; fi
python3 "$RAIZ/port/x360/tools/subir.py" "${IP[@]}" "$OUT/default.xex" "$OUT/default.dat" "$OUT/default.swap" "$@"
