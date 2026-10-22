#!/usr/bin/env bash
# F6, etapa 3: o teste do núcleo portátil da camada de áudio do 360 (port/x360/src/som/som_nucleo.c), sem emulador nem
# console, num diretório temporário (não mexe no clone nem nas builds):
#   port/x360/tools/teste_som360.sh            (aqui ou no WSL: cat ... | ssh bt3win "wsl ... bash -s" com RAIZ=)
# 1. o núcleo em C89 estrito (-std=c89 -pedantic -Werror, o que o cl.exe do XDK aceita) e o teste
#    (port/x360/src/som/teste/teste_nucleo.c) com gcc; o teste tem de passar;
# 2. os mutantes: cópias do núcleo estragadas de seis jeitos (corte, interpolação, canal direito, arredondamento do
#    ganho, fase inicial, tamanho do pedido ao retorno); cada troca tem de casar uma vez só e o teste tem de falhar.
# Sai com 0 só se o teste passou e todos os mutantes foram acusados.
set -uo pipefail
RAIZ=${RAIZ:-$(cd "$(dirname "$0")/../../.." && pwd)}
SOM=$RAIZ/port/x360/src/som
CC=${CC:-gcc}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/teste"
cp "$SOM/som_nucleo.h" "$SOM/som_nucleo.c" "$T/"
cp "$SOM/teste/teste_nucleo.c" "$T/teste/"
falhas=0

compila() { # $1 = o diretório com som_nucleo.c
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -c "$1/som_nucleo.c" -o "$1/som_nucleo.o" &&
  $CC -std=c99 -Wall -O2 -I"$1" "$T/teste/teste_nucleo.c" "$1/som_nucleo.o" -lm -o "$1/prog"
}

echo "== $($CC --version | head -1)"
if compila "$T"; then
  "$T/prog"; r=$?
  [ $r -eq 0 ] || { echo ">> FALHA: o teste do núcleo saiu com $r"; falhas=$((falhas + 1)); }
else
  echo ">> FALHA: o núcleo não compila em C89 estrito"; falhas=$((falhas + 1))
fi

# os mutantes: "o que troca" -> "pelo quê"
MUT=(
  'v = 32767;'                                   'v = 32766;'
  '* frac) >> 15);'                              '* frac) >> 14);'
  's->b[1] = s->canais == 2 ? q[1] : q[0];'      's->b[1] = q[0];'
  '(v * g + 32768) >> 16'                        '(v * g) >> 16'
  's->f = 2 * SOM_TAXA;'                         's->f = 1 * SOM_TAXA;'
  '(precisa - s->n) * tam,'                       '(precisa - s->n) * tam - tam,'
)
for ((i = 0; i < ${#MUT[@]}; i += 2)); do
  de=${MUT[i]}; para=${MUT[i+1]}; d=$T/m$i
  mkdir -p "$d"; cp "$T/som_nucleo.h" "$d/"
  n=$(grep -cF -- "$de" "$T/som_nucleo.c")
  if [ "$n" != 1 ]; then
    echo "mutante $((i / 2)): '$de' aparece $n vez(es) no núcleo (tem de ser 1)"; falhas=$((falhas + 1)); continue
  fi
  python3 -c 'import sys; t=open(sys.argv[1]).read(); open(sys.argv[2],"w").write(t.replace(sys.argv[3], sys.argv[4]))' \
    "$T/som_nucleo.c" "$d/som_nucleo.c" "$de" "$para"
  if compila "$d" >/dev/null 2>&1 && "$d/prog" > "$d/saida.txt"; then
    echo "mutante $((i / 2)) ($para): NÃO acusado"; falhas=$((falhas + 1))
  else
    echo "mutante $((i / 2)) ($para): acusado; $(grep -m1 '^FALHA' "$d/saida.txt" 2>/dev/null || echo 'não compilou')"
  fi
done
if [ $falhas -eq 0 ]; then
  echo "teste_som360: núcleo certo e os 6 mutantes acusados"
else
  echo ">> FALHA: teste_som360 com $falhas falha(s)"
fi
exit $((falhas > 0))
