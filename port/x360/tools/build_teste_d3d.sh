#!/usr/bin/env bash
# F3, etapa 6: o teste do D3D9 do 360 (port/x360/src/teste_d3d), num .xex à parte.
#   port/x360/tools/build_teste_d3d.sh   ->  build/x360/teste_d3d/default.xex
# Os shaders saem do fxc do XDK no build (headers com o microcódigo do Xenos), nunca compilados no boot.
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$RAIZ"
IMAGEM=${IMAGEM:-collectionui-xdk:light}
XDK_DIR=${XDK_DIR:-$HOME/Documentos/CollectionUI/sdk/XDK}
OUT=build/x360/teste_d3d
mkdir -p $OUT
TITLE_ID=$(grep '^TITLE_ID=' port/x360/package/titulo.env | cut -d= -f2-)
TITLE_NAME=$(grep '^TITLE_NAME=' port/x360/package/titulo.env | cut -d= -f2-)
python3 port/x360/tools/make_spa.py --title-id "$TITLE_ID" --name "$TITLE_NAME" --icon port/x360/package/icone.png --out $OUT/spa.bin
docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/lib/xbox":/xdk/lib/xbox:ro -w /app -e WINEDEBUG=-all "$IMAGEM" bash -c '
  set -e
  wineboot -i >/dev/null 2>&1 || true
  O='"$OUT"'
  wine /xdk/bin/win32/fxc.exe /nologo /T vs_3_0 /E vs /Fh $O/teste_vs.h /Vn kVs port/x360/src/teste_d3d/teste.hlsl > $O/fxc.txt 2>&1
  wine /xdk/bin/win32/fxc.exe /nologo /T ps_3_0 /E ps /Fh $O/teste_ps.h /Vn kPs port/x360/src/teste_d3d/teste.hlsl >> $O/fxc.txt 2>&1
  export INCLUDE="Z:/xdk/include/xbox;Z:/xdk/include/crt" LIB="Z:/xdk/lib/xbox"
  F="-nologo -c -D _XBOX -Ox -GS- -MT -W3 -Zi -Iport/x360/src -I$O -Fd$O/"
  wine /xdk/bin/win32/cl.exe $F -TP -EHsc -GR- -Fo$O/teste_d3d.obj port/x360/src/teste_d3d/teste_d3d.cpp > $O/cl.txt 2>&1
  wine /xdk/bin/win32/cl.exe $F -TC -Fo$O/diario.obj port/x360/src/diario.c >> $O/cl.txt 2>&1
  wine /xdk/bin/win32/link.exe -NOLOGO -DEBUG -PDB:$O/default.pdb -RELEASE -XEX:NO -MAP:$O/default.map -OPT:REF,NOICF \
    -OUT:$O/default.exe $O/teste_d3d.obj $O/diario.obj d3d9.lib xgraphics.lib xboxkrnl.lib xapilib.lib > $O/link.txt 2>&1
  wine /xdk/bin/win32/imagexex.exe -nologo -config:port/x360/package/xex.xml -titleid:'"$TITLE_ID"' \
    -section:'"${TITLE_ID#0x}"'=$O/spa.bin -out:$O/default.xex $O/default.exe > $O/imagexex.txt 2>&1
  chown -R '"$(id -u):$(id -g)"' $O' || { cat $OUT/fxc.txt $OUT/cl.txt $OUT/link.txt $OUT/imagexex.txt 2>/dev/null | grep -iE "error|erro" | head -20; exit 1; }
grep -iE "warning" $OUT/cl.txt | head -5 || true
ls -la $OUT/default.xex
