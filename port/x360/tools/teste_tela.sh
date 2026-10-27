#!/usr/bin/env bash
# O teste da tela por imagem (BT3_PICOS_TELA): recorta do gs_d3d.cpp os anéis e o tela_* e compila o teste_tela.cpp com
# eles, no g++ daqui. Uso: port/x360/tools/teste_tela.sh [gs_d3d.cpp]  (um mutante: passe uma cópia mudada; sai com 1)
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
FONTE=${1:-$RAIZ/port/x360/src/gs/gs_d3d.cpp}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
sed -n '/^\/\* (BT3_PICOS_TELA, 08-gravacao.md/,/^static void WINAPI vblank_cb/p' "$FONTE" | sed '$d' > "$T/tela_h.inc"
sed -n '/^\/\* ---- a tela por imagem/,/^\/\* ---- tempos por passada/p' "$FONTE" | sed '$d' > "$T/tela_c.inc"
[ -s "$T/tela_h.inc" ] && [ -s "$T/tela_c.inc" ] || { echo "teste_tela: os trechos não foram achados em $FONTE"; exit 1; }
g++ -O1 -Wall -Wno-unused-function -Wno-unused-variable -DTELA_H="\"$T/tela_h.inc\"" -DTELA_C="\"$T/tela_c.inc\"" \
    -o "$T/teste" "$RAIZ/port/x360/tools/teste_tela.cpp"
"$T/teste"
