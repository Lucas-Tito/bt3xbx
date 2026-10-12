#!/usr/bin/env bash
# F2: compila com o cl.exe do XDK cada .c preparado por c89.py e guarda a saída por arquivo.
#   port/x360/tools/cl_jogo.sh ENTRADA SAIDA [opções do cl...]
#   ex.: cl_jogo.sh build/x360/c89 build/x360/cl-zs -Zs        (só sintaxe)
#        cl_jogo.sh build/x360/c89 build/x360/obj -c -Ox ...    (objetos: o .obj sai em SAIDA)
# Depois: port/x360/tools/cl_resumo.py SAIDA
set -euo pipefail
IMAGEM=${IMAGEM:-collectionui-xdk:light}
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
ENTRADA=$1; SAIDA=$2; shift 2
JOBS=${JOBS:-12}
XDK_DIR=${XDK_DIR:-$HOME/Documentos/CollectionUI/sdk/XDK}
mkdir -p "$RAIZ/$SAIDA"
rm -f "$RAIZ/$SAIDA"/*.txt
docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/lib/xbox":/xdk/lib/xbox:ro -w /app -e ENTRADA="$ENTRADA" -e SAIDA="$SAIDA" \
    -e OPCOES="$*" -e WINEDEBUG=-all "$IMAGEM" bash -c '
  wineboot -i >/dev/null 2>&1 || true
  ls "$ENTRADA"/*.c | xargs -P '"$JOBS"' -I{} sh -c "
    f={}; n=\$(basename \$f .c)
    wine /xdk/bin/win32/cl.exe -nologo -TC -D _XBOX \$OPCOES -Fo\$SAIDA/\$n.obj \$f > \$SAIDA/\$n.txt 2>&1 || true"
  chown -R '"$(id -u):$(id -g)"' "$SAIDA"
'
