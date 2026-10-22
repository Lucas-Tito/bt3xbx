#!/usr/bin/env bash
# F6, etapa 3: o teste do núcleo portátil da camada de áudio do 360 (port/x360/src/som/som_nucleo.c), sem emulador nem
# console, num diretório temporário (não mexe no clone nem nas builds):
#   port/x360/tools/teste_som360.sh            (aqui ou no WSL: cat ... | ssh bt3win "wsl ... bash -s" com RAIZ=)
# 1. o núcleo em C89 estrito (-std=c89 -pedantic -Werror, o que o cl.exe do XDK aceita) e o teste
#    (port/x360/src/som/teste/teste_nucleo.c) com gcc; o teste tem de passar;
# 2. os mutantes: cópias do núcleo estragadas de seis jeitos (corte, interpolação, canal direito, arredondamento do
#    ganho, fase inicial, tamanho do pedido ao retorno); cada troca tem de casar uma vez só e o teste tem de falhar;
# 3. (F6, etapa 5) a leitura em blocos (port/x360/src/som/fluxo.c) em C89 estrito e o teste dela
#    (port/x360/src/som/teste/teste_fluxo.c, com pthreads) três vezes seguidas; e cinco mutantes do fluxo.c (sem o
#    adiantado do começo do laço, sem a sobra do bloco, um byte deslocado, a tabela que não se esvazia, o arquivo que não
#    existe dado como pronto), cada um acusado.
# 4. (F6, a thread do jogo sem esperar) o teste da leitura em blocos roda pelo caminho do console: o fluxo.c com
#    -DFLUXO_PELO_PORT e o port/x360/src/iso360.c de verdade sobre o xtl.h do teste (teste/xtl), numa ISO feita pelo
#    teste, com a leitura lenta (50 ms dentro do ReadFile); o fluxo.c sem a chave (o do WSL) também compila em C89. E
#    quatro mutantes do iso360.c que fazem a thread do jogo esperar a leitura de outra thread (a leitura dos fluxos pela
#    trava comum, o controle negativo pedido; o afs(), o novo() e o Iso_Fechar com a trava comum), cada um acusado.
# Sai com 0 só se os testes passaram e todos os mutantes foram acusados.
set -uo pipefail
RAIZ=${RAIZ:-$(cd "$(dirname "$0")/../../.." && pwd)}
SOM=$RAIZ/port/x360/src/som
CC=${CC:-gcc}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/teste"
cp "$SOM/som_nucleo.h" "$SOM/som_nucleo.c" "$T/"
cp "$SOM/teste/teste_nucleo.c" "$SOM/teste/teste_fluxo.c" "$T/teste/"
cp "$SOM/fluxo.h" "$SOM/fluxo.c" "$RAIZ/port/x360/src/iso360.c" "$T/"
mkdir -p "$T/xtl"; cp "$SOM/teste/xtl/xtl.h" "$T/xtl/"
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
# 3. a leitura em blocos
compila_fluxo() { # $1 = o diretório com fluxo.c e iso360.c
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -DFLUXO_PELO_PORT -c "$1/fluxo.c" -o "$1/fluxo.o" &&
  $CC -std=gnu99 -Wall -Wextra -Wno-stringop-truncation -Werror -O2 -I"$T/xtl" -I"$RAIZ/port/x360/src" \
    -I"$RAIZ/port/x360/conv" -c "$1/iso360.c" -o "$1/iso360.o" &&
  $CC -std=c99 -Wall -O2 -I"$1" "$T/teste/teste_fluxo.c" "$1/fluxo.o" "$1/iso360.o" -lpthread -o "$1/prog_fluxo"
}
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -c "$T/fluxo.c" -o "$T/fluxo_wsl.o" ||
  { echo ">> FALHA: o fluxo.c sem FLUXO_PELO_PORT não compila em C89 estrito"; falhas=$((falhas + 1)); }
if compila_fluxo "$T"; then
  for i in 1 2 3; do
    "$T/prog_fluxo" "$T" > "$T/fluxo_$i.txt"; r=$?
    [ $i = 1 ] && grep -v '^teste_fluxo' "$T/fluxo_1.txt"
    [ $r -eq 0 ] || { echo ">> FALHA: o teste da leitura em blocos saiu com $r (vez $i): $(grep -m1 FALHA "$T/fluxo_$i.txt")"; falhas=$((falhas + 1)); }
  done
else
  echo ">> FALHA: o fluxo.c não compila em C89 estrito"; falhas=$((falhas + 1))
fi
MUTF=(
  'return f->lacoIni >= b->ini ? -1 : f->lacoIni;'  'return -1;'
  'b->n = sBloco + FLUXO_SOBRA;'                     'b->n = sBloco;'
  'r = b->dados + (pos - b->ini);'                   'r = b->dados + (pos - b->ini) + 1;'
  'solta->usado = 0;'                                'solta->usado = 1;'
  'f->estado = ok ? FLUXO_PRONTO : FLUXO_ERRO;'      'f->estado = FLUXO_PRONTO;'
)
for ((i = 0; i < ${#MUTF[@]}; i += 2)); do
  de=${MUTF[i]}; para=${MUTF[i+1]}; d=$T/f$i
  mkdir -p "$d"; cp "$T/fluxo.h" "$T/iso360.c" "$d/"
  n=$(grep -cF -- "$de" "$T/fluxo.c")
  if [ "$n" != 1 ]; then
    echo "mutante do fluxo $((i / 2)): '$de' aparece $n vez(es) (tem de ser 1)"; falhas=$((falhas + 1)); continue
  fi
  python3 -c 'import sys; t=open(sys.argv[1]).read(); open(sys.argv[2],"w").write(t.replace(sys.argv[3], sys.argv[4]))' \
    "$T/fluxo.c" "$d/fluxo.c" "$de" "$para"
  if compila_fluxo "$d" >/dev/null 2>&1 && timeout 120 "$d/prog_fluxo" "$d" > "$d/saida.txt"; then
    echo "mutante do fluxo $((i / 2)) ($para): NÃO acusado"; falhas=$((falhas + 1))
  else
    echo "mutante do fluxo $((i / 2)) ($para): acusado; $(grep -m1 '^FALHA' "$d/saida.txt" 2>/dev/null || echo 'não compilou ou não terminou')"
  fi
done
# 4. os mutantes do iso360.c: a thread do jogo esperando a leitura de outra thread
TRAVA='EnterCriticalSection(&sTrava); LeaveCriticalSection(&sTrava); '
MUTI=(
  'sTemLeitora = 1;'                                     'sTemLeitora = 0;'
  '    if (a->lido) { /* já lida: sem trava */'          "    ${TRAVA}if (a->lido) { /* já lida: sem trava */"
  'EnterCriticalSection(&sTravaTabela); /* um lugar'     "${TRAVA}EnterCriticalSection(&sTravaTabela); /* um lugar"
  'EnterCriticalSection(&sTravaTabela); /* o lugar volta' "${TRAVA}EnterCriticalSection(&sTravaTabela); /* o lugar volta"
)
for ((i = 0; i < ${#MUTI[@]}; i += 2)); do
  de=${MUTI[i]}; para=${MUTI[i+1]}; d=$T/i$i
  mkdir -p "$d"; cp "$T/fluxo.h" "$T/fluxo.c" "$d/"
  n=$(grep -cF -- "$de" "$T/iso360.c")
  if [ "$n" != 1 ]; then
    echo "mutante do iso360 $((i / 2)): '$de' aparece $n vez(es) (tem de ser 1)"; falhas=$((falhas + 1)); continue
  fi
  python3 -c 'import sys; t=open(sys.argv[1]).read(); open(sys.argv[2],"w").write(t.replace(sys.argv[3], sys.argv[4]))' \
    "$T/iso360.c" "$d/iso360.c" "$de" "$para"
  if compila_fluxo "$d" >/dev/null 2>&1 && timeout 120 "$d/prog_fluxo" "$d" > "$d/saida.txt"; then
    echo "mutante do iso360 $((i / 2)) ($para): NÃO acusado"; falhas=$((falhas + 1))
  else
    echo "mutante do iso360 $((i / 2)) ($para): acusado; $(grep '^FALHA' "$d/saida.txt" 2>/dev/null | grep -m1 esperou || echo 'não compilou ou não terminou')"
  fi
done
if [ $falhas -eq 0 ]; then
  echo "teste_som360: núcleo certo e os 6 mutantes acusados; leitura em blocos certa e os 5 mutantes acusados; a thread do jogo sem esperar e os 4 mutantes do iso360.c acusados"
else
  echo ">> FALHA: teste_som360 com $falhas falha(s)"
fi
exit $((falhas > 0))
