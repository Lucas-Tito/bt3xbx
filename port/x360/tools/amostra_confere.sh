#!/usr/bin/env bash
# O teste do amostrador da thread do jogo no host, sem console (F8, docs/x360/plano/F8/08-explosao-planeta.md):
#   port/x360/tools/amostra_confere.sh MAPA [OUTRO_MAPA]
# MAPA: o default.map da build; OUTRO_MAPA: o de outra build (o resolvedor tem de recusá-lo).
#   1. amostra_hist.c (o mesmo arquivo do 360) com o gcc em C89, e os casos de pilha montados à mão (teste pilha);
#   2. um roteiro sintético com contagens conhecidas (amostra_perfil.py sintetico) passa pelo histograma e pelo despejo
#      do C, e o resolvedor tem de devolver as mesmas somas por função;
#   3. as sabotagens, cada uma numa cópia, que têm de falhar: uma contagem mudada no log, o .map de outra build, e duas
#      mutações no amostra_hist.c (o chamador sem o Lr, o quadro de armadilha sem o elo).
# Sai com 1 se algo não confere ou se alguma sabotagem passa.
set -uo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
MAPA=${1:?uso: amostra_confere.sh MAPA [OUTRO_MAPA]}
OUTRO=${2:-}
T=$(mktemp -d -t amostra-XXXXXX)
trap 'rm -rf "$T"' EXIT
PY="python3 $RAIZ/port/x360/tools/amostra_perfil.py"
falhas=0

compilar() { # fonte do amostra_hist.c, saída
  gcc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-overlength-strings -O2 -I"$RAIZ/port/x360/src/f2" \
    "$RAIZ/port/x360/tools/teste_amostra/teste.c" "$1" -o "$2"
}

echo "== 1. a pilha"
compilar "$RAIZ/port/x360/src/f2/amostra_hist.c" "$T/teste" || exit 1
"$T/teste" pilha || falhas=$((falhas + 1))

echo "== 2. o roteiro sintético"
$PY sintetico "$MAPA" "$T" || exit 1
"$T/teste" hist < "$T/roteiro.txt" > "$T/bt3xbx.log" || exit 1
echo "   $(grep -c '^amostra: d1' "$T/bt3xbx.log") linhas no despejo"
$PY resolver "$T/bt3xbx.log" "$MAPA" --json "$T/obtido.json" > "$T/relatorio.txt" || { cat "$T/relatorio.txt"; exit 1; }
$PY conferir "$T/obtido.json" "$T/esperado.json" || falhas=$((falhas + 1))

echo "== 3. as sabotagens (cada uma tem de falhar)"
deve_falhar() { # nome, comando...
  local nome=$1; shift
  if "$@" > "$T/sabotagem.txt" 2>&1; then
    echo "   SABOTAGEM NÃO ACUSADA: $nome"; cat "$T/sabotagem.txt"; falhas=$((falhas + 1))
  else
    echo "   $nome: acusada ($(grep -m1 -E 'FALHA|diferenças|recusado|error' "$T/sabotagem.txt" | head -c 160))"
  fi
}
# uma contagem a mais na primeira linha de folhas do lento
sed -E '0,/^amostra: d1 lento folha ([0-9A-F]{8}):([0-9]+)/s//amostra: d1 lento folha \1:\2 0/' "$T/bt3xbx.log" \
  | sed -E '0,/^(amostra: d1 lento folha [0-9A-F]{8}:[0-9]+) 0/s//\11/' > "$T/estragado.log"
cmp -s "$T/bt3xbx.log" "$T/estragado.log" && { echo "   a sabotagem do log não mudou nada"; falhas=$((falhas + 1)); }
deve_falhar "contagem mudada no log" bash -c "$PY resolver $T/estragado.log $MAPA --json $T/estragado.json >/dev/null && $PY conferir $T/estragado.json $T/esperado.json"
if [ -n "$OUTRO" ]; then
  deve_falhar ".map de outra build" $PY resolver "$T/bt3xbx.log" "$OUTRO"
fi
mutar() { # nome, antes, depois
  local nome=$1 antes=$2 depois=$3 f="$T/mutante.c"
  python3 - "$RAIZ/port/x360/src/f2/amostra_hist.c" "$f" "$antes" "$depois" <<'EOF' || { falhas=$((falhas + 1)); return; }
import sys
t = open(sys.argv[1]).read()
if t.count(sys.argv[3]) != 1:
    sys.exit("mutação sem alvo único: " + sys.argv[3])
open(sys.argv[2], "w").write(t.replace(sys.argv[3], sys.argv[4]))
EOF
  compilar "$f" "$T/mutante" || { falhas=$((falhas + 1)); return; }
  deve_falhar "$nome" "$T/mutante" pilha
}
mutar "chamador sem o Lr" 'u->chamador = fl != 0 && fl != u->folha ? fl : primeiro;' 'u->chamador = primeiro;'
mutar "quadro de armadilha sem o elo" 'if (elo != 0 ? *r1 != elo :' 'if (elo != 0 ? 0 :'

if [ "$falhas" != 0 ]; then
  echo "amostra_confere: $falhas falhas"
  exit 1
fi
echo "amostra_confere: tudo certo"
