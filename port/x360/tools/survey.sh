#!/usr/bin/env bash
# Compila só a sintaxe (-Zs) de cada fonte do jogo com o cl.exe do XDK e guarda os erros por arquivo.
# Uso: port/x360/tools/survey.sh SAIDA [opções do cl...]   ex.: survey.sh build/x360/survey-tc -TC
# CL=/jul12/bin/win32/cl.exe usa o compilador 16.10 do TechPreview.
# Depois: python3 port/x360/tools/survey_tally.py SAIDA
set -euo pipefail

IMAGEM=${IMAGEM:-collectionui-xdk:light}
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
SAIDA=$1; shift
CL=${CL:-/xdk/bin/win32/cl.exe}
JOBS=${JOBS:-8}
XDK_DIR=${XDK_DIR:-$HOME/Documentos/CollectionUI/sdk/XDK}

mkdir -p "$RAIZ/$SAIDA"
docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/TechPreview/Jul12Compiler":/jul12:ro -w /app -e SAIDA="$SAIDA" -e CL="$CL" -e OPCOES="$*" -e FORCAR="${FORCAR:-}" "$IMAGEM" bash -c '
  wineboot -i >/dev/null 2>&1 || true
  find src -name "*.c" | sort | xargs -P '"$JOBS"' -I{} sh -c "
    f={}; o=\"\$SAIDA/\$(echo \$f | tr / _).txt\"
    INCLUDE=\"Z:/xdk/include/xbox;Z:/xdk/include/crt\" wine \"\$CL\" -nologo -Zs -D _XBOX \$OPCOES \
      -Iinclude -Iport/include -Iport/x360/include $FORCAR -FIport_compat.h \$f > \$o 2>&1 || true"
  chown -R '"$(id -u):$(id -g)"' "$SAIDA"
'
