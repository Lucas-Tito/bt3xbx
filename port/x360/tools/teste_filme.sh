#!/usr/bin/env bash
# Executar dentro de flock ~/wsl.lock, com tee para ~/; toda saída com imagens fica fora do git.
# O padrão produz as referências dos dois filmes em ~/f6-etapa7/referencia/{op,ed}.
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=${OUT:-$HOME/f6-etapa7}
SOM=$RAIZ/port/x360/src/som
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$OUT/referencia/op" "$OUT/referencia/ed"
python3 "$RAIZ/port/x360/tools/teste_filme_envio.py"
cp "$SOM/som_nucleo.c" "$SOM/som_nucleo.h" "$SOM/filme_tempo.h" "$T/"
compila() {
    gcc -std=c89 -pedantic -Wall -Wextra -Werror -O2 -I"$1" "$1/som_nucleo.c" "$SOM/teste/teste_filme.c" -o "$1/teste"
}
compila "$T"
"$T/teste"
# A prova tem de falhar por asserção, não por não compilar.
python3 - "$T" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
for nome, arq, de, para in (
    ('relogio', 'som_nucleo.c', 's->filme.quadros++;', 's->filme.quadros += 2;'),
    ('fila', 'filme_tempo.h', '*posicao = b->inicio +', '*posicao = b->inicio -'),
    ('silencio', 'filme_tempo.h', 'deslocamento < b->dados ? deslocamento : b->dados', 'deslocamento'),
    ('identidade', 'som_nucleo.c', 's->user == user) {\n            *r = s->filme;', 's->user != user) {\n            *r = s->filme;'),  # (o relógio da deriva tem a mesma comparação)
    ('versao', 'filme_tempo.h', 'antes == depois && !(antes & 1u)', 'antes == depois'),
):
    d = p / nome; d.mkdir()
    for f in ('som_nucleo.c', 'som_nucleo.h', 'filme_tempo.h'):
        texto = (p / f).read_text()
        if f == arq:
            assert texto.count(de) == 1
            texto = texto.replace(de, para)
        (d / f).write_text(texto)
PY
ulimit -c 0
for mut in relogio fila silencio identidade versao; do
    compila "$T/$mut"
    if "$T/$mut/teste" >"$T/$mut.log" 2>&1; then
        echo "FALHA: mutante $mut não foi acusado"; exit 1
    fi
    echo "controle do núcleo $mut: recusado"
done
python3 "$RAIZ/port/x360/tools/filme.py" controles
gcc -m32 -std=gnu99 -O2 -Wall -Wextra "$RAIZ/port/x360/tools/filme_ref.c" -o "$T/filme_ref"
for tipo in op ed; do
    case "$tipo" in op) nome=ZS3USOP;; ed) nome=ZS3USED;; esac
    "$T/filme_ref" "/srv/bt3/dados/gamedata/disc/DATA/$nome.PSS" "$OUT/referencia/$tipo" "$tipo"
    head -c 524288 "/srv/bt3/dados/gamedata/disc/DATA/$nome.PSS" > "$T/curto.PSS"
    mkdir -p "$T/curto-$tipo"
    if "$T/filme_ref" "$T/curto.PSS" "$T/curto-$tipo" "$tipo"; then r=0; else r=$?; fi
    if [ "$r" != 1 ]; then
        echo "FALHA: extrator de $tipo truncado saiu com $r, esperado 1"; exit 1
    fi
    echo "controle do extrator $tipo truncado: saída 1 confirmada"
done
echo "tudo certo: núcleo, controles e referências x86 dos dois filmes"
