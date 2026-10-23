#!/usr/bin/env bash
# F2: o jogo para o Xbox 360 (port/x360/src/f2/main360.cpp), da preparação ao .xex. Sem janela por padrão; com
# BT3_GS=gpu no ambiente.txt, desenhando pelo backend D3D9 (port/x360/src/gs/gs_d3d.cpp, F3, etapa 7).
#   port/x360/tools/build_f2.sh          ->  build/x360/f2/default.xex (+ .map, .pdb)
# Passos:
#   1. c89.py (imagem bt3xbx-ferramentas): cada fonte do jogo e do port vira C89 para o cl;
#   2. fxc.exe e cl.exe (imagem do XDK): os shaders do backend (gs_d3d.hlsl, em headers com o microcódigo do Xenos),
#      esses fontes e os do 360 (port/x360/src/f2, port/x360/src/gs, port/x360/src/diario.c);
#   3. coff.py: as tabelas do executável (esqueleto port/data, sem bytes do jogo) como objetos COFF;
#   4. link.exe: o que ainda faltar vira stub que diz o nome no log e encerra (build/x360/f2/stubs.txt), como a
#      build PowerPC da F1; depois imagexex com o Title ID e o SPA;
#      chamadores_bl.py: cada `bl Sprite_Unpack` do .exe cai numa função da tabela CHAMADORES da guarda;
#   5. dat360.py: default.dat (de onde vem cada valor das tabelas no disco) e default.swap (o que trocar de ordem),
#      pelo .map. As trocas (elftipos.py --trocas) saem da build x86, que só existe no WSL:
#      wsl.sh rodar 'cat port/build/ppc/elftipos/trocas.tsv' > build/x360/f2/elftipos/trocas.tsv
# Nenhum dado do jogo passa por aqui: as tabelas do disco entram na partida (Port_LoadGameData, decisão 13).
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$RAIZ"
IMAGEM=${IMAGEM:-collectionui-xdk:light}
XDK_DIR=${XDK_DIR:-$HOME/Documentos/CollectionUI/sdk/XDK}
OUT=build/x360/f2
mkdir -p $OUT/c89 $OUT/obj $OUT/dados $OUT/x360 $OUT/shaders
TITLE_ID=$(grep '^TITLE_ID=' port/x360/package/titulo.env | cut -d= -f2-)
TITLE_NAME=$(grep '^TITLE_NAME=' port/x360/package/titulo.env | cut -d= -f2-)

echo "== 1. preparação (c89.py)"
rm -f $OUT/c89/*.c
# (F5, etapa 3) o carimbo do commit no log do console (port/x360/headless/passos.c: "passos: origem ..."), no formato
# do headless.py: o commit e o sha1 da diferença para ele, com os arquivos novos (da39a3ee5e: sem diferença)
CARIMBO="$(git rev-parse --short HEAD) diff:$( { git diff HEAD; git ls-files -o --exclude-standard -z | xargs -0 -r cat; } | sha1sum | cut -c1-10)"
echo "   carimbo: $CARIMBO"
docker run --rm --user "$(id -u):$(id -g)" -e BT3_CARIMBO="$CARIMBO" -v "$RAIZ":/app -w /app bt3xbx-ferramentas python3 port/x360/tools/c89.py $OUT/c89 | tail -2

echo "== 2. compilação (cl.exe)"
CLFLAGS="-c -Ox -Oi -fp:precise -fp:except- -GS- -Gy -GF -MT -W3 -Zi"
port/x360/tools/cl_jogo.sh $OUT/c89 $OUT/obj $CLFLAGS -Fd$OUT/obj/ >/dev/null
python3 port/x360/tools/cl_resumo.py $OUT/obj | head -1
python3 port/x360/tools/cl_resumo.py $OUT/obj | grep -q "^$(ls $OUT/c89/*.c | wc -l) de" || { python3 port/x360/tools/cl_resumo.py $OUT/obj --arquivos | tail -20; exit 1; }
# os fontes do 360: com os headers do XDK (xtl.h), C89 e C++
docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/lib/xbox":/xdk/lib/xbox:ro -w /app -e WINEDEBUG=-all "$IMAGEM" bash -c '
  wineboot -i >/dev/null 2>&1 || true
  ok=0
  # os shaders: compilados aqui, nunca no boot
  H=port/x360/src/gs/gs_d3d.hlsl
  for e in vs_gs:vs_3_0:kGsVs:gs_vs ps_gs:ps_3_0:kGsPs:gs_ps ps_gsz:ps_3_0:kGsZPs:gsz_ps ps_tabela:ps_3_0:kTabelaPs:tabela_ps ps_contorno:ps_3_0:kContornoPs:contorno_ps ps_filme:ps_3_0:kFilmePs:filme_ps vs_copia:vs_3_0:kCopiaVs:copia_vs ps_copia:ps_3_0:kCopiaPs:copia_ps ps_zcor:ps_3_0:kZCorPs:zcor_ps ps_zprof:ps_3_0:kZProfPs:zprof_ps vs_vu0:vs_3_0:kVu0Vs:vu0_vs vs_vu4:vs_3_0:kVu4Vs:vu4_vs vs_vu6:vs_3_0:kVu6Vs:vu6_vs ps_gs_triv:ps_3_0:kGsTrivPs:gs_triv_ps ps_gsz_triv:ps_3_0:kGsZTrivPs:gsz_triv_ps ps_gs_tex:ps_3_0:kGsTexPs:gs_tex_ps ps_gs_sem:ps_3_0:kGsSemPs:gs_sem_ps ps_gsz_tex:ps_3_0:kGsZTexPs:gsz_tex_ps ps_gsz_sem:ps_3_0:kGsZSemPs:gsz_sem_ps ps_tabela_z:ps_3_0:kTabelaZPs:tabela_z_ps ps_tabela_ids:ps_3_0:kTabelaIdsPs:tabela_ids_ps ps_gs_af:ps_3_0:kGsAfPs:gs_af_ps vs_gs_af:vs_3_0:kGsAfVs:gs_af_vs ps_copia_af:ps_3_0:kCopiaAfPs:copia_af_ps ps_fxaa:ps_3_0:kFxaaPs:fxaa_ps; do
    IFS=: read n t v a <<< "$e"
    wine /xdk/bin/win32/fxc.exe /nologo /T $t /E $n /Fh '"$OUT"'/shaders/$a.h /Vn $v $H > '"$OUT"'/shaders/$a.txt 2>&1 || { cat '"$OUT"'/shaders/$a.txt; ok=1; }
  done
  # o backend do GS: com os headers do GS do port (C) e o stdint.h que o cl não tem
  for f in port/x360/src/gs/*.cpp; do
    n=$(basename ${f%.*})
    INCLUDE="Z:/xdk/include/xbox;Z:/xdk/include/crt" wine /xdk/bin/win32/cl.exe -nologo -D _XBOX -D PORT_X360 -D PORT_BIG_ENDIAN \
      '"$CLFLAGS"' -TP -EHsc -GR- -Iport/x360/src -Iport/src/gs -Iport/x360/src/gs/cl -I'"$OUT"'/shaders \
      -Fo'"$OUT"'/x360/$n.obj -Fd'"$OUT"'/x360/ $f > '"$OUT"'/x360/$n.txt 2>&1 || { cat '"$OUT"'/x360/$n.txt; ok=1; }
  done
  for f in port/x360/src/diario.c port/x360/src/iso360.c port/x360/src/f2/*.c port/x360/src/f2/*.cpp port/x360/src/som/*.c port/x360/src/som/*.cpp $(ls port/x360/conv/*.c | grep -v conv_tool); do
    n=$(basename ${f%.*}); t=-TC; case $f in *.cpp) t="-TP -EHsc -GR-";; esac
    INCLUDE="Z:/xdk/include/xbox;Z:/xdk/include/crt" wine /xdk/bin/win32/cl.exe -nologo -D _XBOX '"$CLFLAGS"' $t \
      -Iport/x360/src -Iport/x360/conv -Fo'"$OUT"'/x360/$n.obj -Fd'"$OUT"'/x360/ $f > '"$OUT"'/x360/$n.txt 2>&1 || { cat '"$OUT"'/x360/$n.txt; ok=1; }
  done
  chown -R '"$(id -u):$(id -g)"' '"$OUT"'
  exit $ok'

echo "== 3. tabelas do executável (coff.py)"
python3 port/x360/tools/coff.py --absoluto D_3BE71C=0x3BE71C --saida $OUT/dados port/data/*.s | tail -1

echo "== 4. ligação (link.exe)"
rm -f $OUT/x360/stubs.* $OUT/default.exe
LIBS="d3d9.lib xgraphics.lib xboxkrnl.lib xapilib.lib xaudio2.lib xmcore.lib" # (o xaudio2 exige o xmcore: F6, etapa 3)
LDFLAGS="-NOLOGO -DEBUG -PDB:$OUT/default.pdb -STACK:262144,262144 -RELEASE -XEX:NO -MAP:$OUT/default.map -OPT:REF,NOICF"
ligar() {
  docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/lib/xbox":/xdk/lib/xbox:ro -w /app -e WINEDEBUG=-all "$IMAGEM" bash -c "
    wineboot -i >/dev/null 2>&1 || true
    ls $OUT/obj/*.obj $OUT/x360/*.obj $OUT/dados/*.obj > $OUT/objetos.txt
    LIB='Z:/xdk/lib/xbox' wine /xdk/bin/win32/link.exe $LDFLAGS -OUT:$OUT/default.exe @$OUT/objetos.txt $LIBS > $OUT/link.txt 2>&1
    r=\$?; chown -R $(id -u):$(id -g) $OUT; exit \$r" || true
}
ligar
# o que faltar: stub que diz o nome e encerra
grep -oE "unresolved external symbol [^ ]+" $OUT/link.txt | awk '{print $4}' | sort -u > $OUT/stubs.txt || true
if [ -s $OUT/stubs.txt ]; then
  { echo "/* Gerado por port/x360/tools/build_f2.sh: um stub por símbolo que a build sem janela do 360 ainda não tem. */"
    echo "void Diario_Escrever(const char *, ...); void Port_Sair(int);"
    while read s; do echo "void $s(void) { Diario_Escrever(\"não implementado no 360: $s\"); Port_Sair(4); }"; done < $OUT/stubs.txt
  } > $OUT/x360/stubs.c
  docker run --rm -v "$RAIZ":/app -w /app -e WINEDEBUG=-all "$IMAGEM" bash -c "
    wineboot -i >/dev/null 2>&1 || true
    INCLUDE='Z:/xdk/include/xbox;Z:/xdk/include/crt' wine /xdk/bin/win32/cl.exe -nologo -c -TC -D _XBOX -Fo$OUT/x360/stubs.obj $OUT/x360/stubs.c > $OUT/x360/stubs.txt 2>&1
    chown -R $(id -u):$(id -g) $OUT"
  ligar
fi
echo "$(wc -l < $OUT/stubs.txt) stubs: $(head -c 400 $OUT/stubs.txt | tr '\n' ' ')"
if [ ! -s $OUT/default.exe ] || grep -q "error LNK" $OUT/link.txt; then
  grep -E "error|warning LNK4" $OUT/link.txt | head -20
  exit 1
fi

# a guarda do Sprite_Unpack nomeia quem chamou pela tabela CHAMADORES (conv_jogo.c): toda chamada no binário tem de
# cair numa função da tabela (o inlining do cl podia mudar o nome sem aviso)
python3 port/x360/tools/chamadores_bl.py $OUT/default.exe $OUT/default.map || exit 1

echo "== 5. lista e trocas das tabelas (dat360.py)"
[ -s $OUT/elftipos/trocas.tsv ] || { echo "falta $OUT/elftipos/trocas.tsv (ver o cabeçalho)"; exit 1; }
python3 port/x360/tools/dat360.py $OUT/default.map $OUT/elftipos/trocas.tsv $OUT/default

echo "== 6. imagexex"
python3 port/x360/tools/make_spa.py --title-id "$TITLE_ID" --name "$TITLE_NAME" --icon port/x360/package/icone.png --out build/x360/spa.bin
docker run --rm -v "$RAIZ":/app -w /app -e WINEDEBUG=-all "$IMAGEM" bash -c "
  wineboot -i >/dev/null 2>&1 || true
  wine /xdk/bin/win32/imagexex.exe -nologo -config:port/x360/package/xex.xml -titleid:$TITLE_ID \
    -section:${TITLE_ID#0x}=build/x360/spa.bin -out:$OUT/default.xex $OUT/default.exe > $OUT/imagexex.txt 2>&1
  r=\$?; chown -R $(id -u):$(id -g) $OUT build/x360/spa.bin; exit \$r" || { cat $OUT/imagexex.txt; exit 1; }
ls -la $OUT/default.xex
