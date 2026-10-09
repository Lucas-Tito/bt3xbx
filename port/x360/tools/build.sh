#!/usr/bin/env bash
# Compila o .xex no Linux com a imagem Docker do XDK (receita em ~/Documentos/CollectionUI/toolchain).
# Uso: port/x360/tools/build.sh [CONFIG=Debug] [alvo]     Saída: build/x360/<CONFIG>/default.xex
#
# A pasta de libs inteira do XDK é montada por cima, só leitura (a imagem traz só um subconjunto).
# O container roda como root porque o wine recusa prefixo de outro dono; a posse volta no fim.
set -euo pipefail

IMAGEM=${IMAGEM:-collectionui-xdk:light}
XDK_DIR=${XDK_DIR:-$HOME/Documentos/CollectionUI/sdk/XDK}
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)

[ -d "$XDK_DIR/lib/xbox" ] || { echo "XDK não encontrado em $XDK_DIR (defina XDK_DIR)"; exit 1; }

# Nome e ícone do título (a imagem não tem Python).
mkdir -p "$RAIZ/build/x360"
TITULO="$RAIZ/port/x360/package/titulo.env"
python3 "$RAIZ/port/x360/tools/make_spa.py" --title-id "$(grep '^TITLE_ID=' "$TITULO" | cut -d= -f2-)" \
    --name "$(grep '^TITLE_NAME=' "$TITULO" | cut -d= -f2-)" \
    --icon "$RAIZ/port/x360/package/icone.png" --out "$RAIZ/build/x360/spa.bin"

status=0
docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/lib/xbox":/xdk/lib/xbox:ro -w /app -e WINEDEBUG=-all "$IMAGEM" \
    make -f port/x360/Makefile "$@" || status=$?
docker run --rm -v "$RAIZ":/app "$IMAGEM" chown -R "$(id -u):$(id -g)" /app/build
[ $status -eq 0 ] || exit $status

# Guarda: o bit 0x8 das image flags ("só disco XGD2") faz o console recusar o .xex.
for xex in $(find "$RAIZ/build/x360" -name '*.xex' 2>/dev/null); do
    python3 - "$xex" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
so = struct.unpack('>I', d[16:20])[0]
flags = struct.unpack('>I', d[so + 0x10C:so + 0x110])[0]
if flags & 0x8:
    print(f"ERRO: {sys.argv[1]} saiu com image flags {flags:#x} (só disco XGD2); tire <dvdxgd2/> do xex.xml")
    sys.exit(1)
PY
done
