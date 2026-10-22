#!/usr/bin/env bash
# F6, etapa 6 (docs/x360/plano/F6/06.md, seção 5): a leitura do filme em blocos (port/x360/src/f2/blocos.c) contra a
# leitura direta, pelo decodificador do port sobre os .PSS de verdade, num diretório temporário (sem build, sem
# emulador, sem mexer no clone):
#   port/x360/tools/teste_blocos.sh [arquivo.PSS ...]
#   (no WSL: o tar do worktree em base64 dentro do script, RAIZ= a cópia; os filmes padrão são os de /srv/bt3/dados)
# 1. blocos.c em C89 estrito (o que o cl.exe do XDK aceita) e o teste (teste_blocos.c) com gcc;
# 2. por filme, a saída da leitura direta (stdio, a mesma sequência de leituras que a ISO via antes) contra a dos blocos
#    com a thread (256 KB), sem thread, e com 64 KB e atrasos aleatórios na thread: as mesmas leituras (posição, pedido,
#    entregue, hash dos bytes) e as mesmas imagens (hash do RGBA de cada uma);
# 3. o caminho do 360 (Mpeg2_NextPlanes) pelas duas leituras: os mesmos planos, e a conversão do ps_filme refeita em
#    float de 32 bits igual à conta inteira do Mpeg2_Next em todos os pixels;
# 4. os controles, que têm de ser acusados: um byte trocado na leitura (--estraga), uma cópia do blocos.c que entrega
#    do lugar errado depois do primeiro bloco, e uma cópia do teste com uma constante do shader trocada.
# Sai com 0 só se tudo bateu e todos os controles foram acusados.
set -uo pipefail
RAIZ=${RAIZ:-$(cd "$(dirname "$0")/../../.." && pwd)}
CC=${CC:-gcc}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
FILMES=("$@")
[ ${#FILMES[@]} -gt 0 ] || FILMES=(/srv/bt3/dados/gamedata/disc/DATA/ZS3USOP.PSS /srv/bt3/dados/gamedata/disc/DATA/ZS3USED.PSS)
falhas=0
falha() { echo ">> FALHA: $*"; falhas=$((falhas + 1)); }

mkdir -p "$T/ok" "$T/mut" "$T/sh"
cp "$RAIZ/port/x360/src/f2/blocos.h" "$RAIZ/port/x360/src/f2/blocos.c" "$T/ok/"
cp "$RAIZ/port/x360/src/f2/blocos.h" "$T/mut/"
cp "$RAIZ/port/x360/tools/teste_blocos.c" "$T/"
# o mutante: depois do primeiro bloco, a cópia sai um byte adiante
sed 's|b->buf\[k\] + (pos - b->ini\[k\])|b->buf[k] + (pos - b->ini[k] + (b->ini[k] > 0))|' "$T/ok/blocos.c" > "$T/mut/blocos.c"
cmp -s "$T/ok/blocos.c" "$T/mut/blocos.c" && falha "o mutante do blocos.c não casou"
sed 's|39061.0f|39062.0f|' "$T/teste_blocos.c" > "$T/sh/teste_blocos.c"
cmp -s "$T/teste_blocos.c" "$T/sh/teste_blocos.c" && falha "o mutante do shader não casou"

compila() { # $1 = diretório do blocos.c, $2 = o teste, $3 = a saída
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -c "$1/blocos.c" -o "$1/blocos.o" &&
  $CC -std=c99 -O2 -Wall -DPORT_X360 -I"$1" -I"$RAIZ/port/src" "$2" "$1/blocos.o" -lpthread -lm -o "$3"
}
echo "== $($CC --version | head -1)"
compila "$T/ok" "$T/teste_blocos.c" "$T/tb" || { falha "não compila"; exit 1; }
compila "$T/mut" "$T/teste_blocos.c" "$T/tb_mut" || falha "o mutante do blocos.c não compila"
compila "$T/ok" "$T/sh/teste_blocos.c" "$T/tb_sh" || falha "o mutante do shader não compila"

for f in "${FILMES[@]}"; do
  [ -r "$f" ] || { falha "não leio $f"; continue; }
  n=$(basename "$f")
  echo "== $n ($(stat -c %s "$f") bytes, md5 $(md5sum < "$f" | cut -c1-12))"
  "$T/tb" "$f" "$T/a.txt" arquivo || falha "$n: leitura direta"
  for modo in "blocos 256" "sincrono 256" "blocos 64 --atraso"; do
    "$T/tb" "$f" "$T/b.txt" $modo || falha "$n: $modo"
    if cmp -s "$T/a.txt" "$T/b.txt"; then
      echo "   $modo: igual à leitura direta ($(grep -c '^L' "$T/a.txt") leituras, $(grep -c '^I' "$T/a.txt") imagens)"
    else
      falha "$n: $modo difere da leitura direta: $(diff "$T/a.txt" "$T/b.txt" | head -2 | tr '\n' ' ')"
    fi
  done
  "$T/tb" "$f" "$T/pa.txt" arquivo --planos || falha "$n: planos, leitura direta"
  "$T/tb" "$f" "$T/pb.txt" blocos 256 --planos || falha "$n: planos, blocos"
  cmp -s "$T/pa.txt" "$T/pb.txt" && echo "   planos: iguais pelas duas leituras" || falha "$n: os planos diferem"
  c=$(tail -1 "$T/pb.txt")
  [ "$(echo "$c" | awk '{print $3}')" = "0" ] && echo "   shader: $c (imagens, pixels diferentes da conta inteira)" || falha "$n: shader $c"
  [ "$(grep -c '^I' "$T/pa.txt")" = "$(grep -c '^I' "$T/a.txt")" ] || falha "$n: o número de imagens dos planos difere do RGBA"
  # os controles
  "$T/tb" "$f" "$T/e.txt" blocos 256 --estraga >/dev/null
  cmp -s "$T/a.txt" "$T/e.txt" && falha "$n: o byte trocado passou" ||
    echo "   controle --estraga acusado: $(diff "$T/a.txt" "$T/e.txt" | grep -c '^>') linhas diferentes"
  "$T/tb_mut" "$f" "$T/m.txt" blocos 256 >/dev/null 2>&1
  cmp -s "$T/a.txt" "$T/m.txt" && falha "$n: o blocos.c mutante passou" ||
    echo "   controle blocos.c mutante acusado: $(diff "$T/a.txt" "$T/m.txt" | grep -c '^>') linhas diferentes"
  "$T/tb_sh" "$f" "$T/s.txt" blocos 256 --planos >/dev/null
  c=$(tail -1 "$T/s.txt" | awk '{print $3}')
  [ "$c" != "0" ] && echo "   controle do shader acusado: $c pixels diferentes" || falha "$n: o shader mutante passou"
done
echo "== falhas: $falhas"
[ $falhas -eq 0 ]
