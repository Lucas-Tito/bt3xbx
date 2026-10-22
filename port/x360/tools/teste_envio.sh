#!/usr/bin/env bash
# F5, etapa 8, gravação (docs/x360/plano/F5/08-gravacao.md, seção 18): o envio de 32 bits por linhas de cache
# (BT3_ENVIO_RAPIDO) contra o vram_rw, pelo teste_envio.c, num diretório temporário (sem build do jogo, sem emulador do jogo, sem dados):
#   port/x360/tools/teste_envio.sh            (no WSL: precisa do powerpc-linux-gnu-gcc e do qemu-ppc)
# 1. o teste na x86 (gcc, -DBT3_TESTE_VU1) e na PPC de verdade (powerpc-linux-gnu-gcc -static -DPORT_BIG_ENDIAN, no
#    qemu-ppc), com -DBT3_TESTE_ENVIO (o dcbz128 vira um memset da linha): 1500 envios sorteados, as VRAMs iguais;
# 2. os controles, que têm de ser acusados nas duas e só com a chave: as linhas 1 e 2 da faixa trocadas no
#    envio_faixa32, e a linha 3 da faixa não escrita (com a chave 1 a linha de cache zerada e não escrita inteira; com a
#    2, sem o dcbz128, a VRAM antiga fica) (cópias do gs_core.c);
# 3. a medida na x86 (--mede): ns por byte do transfer_data, chave 0, 1 e 2.
# Sai com 0 só se o teste passou nas duas e todos os controles foram acusados.
set -uo pipefail
RAIZ=${RAIZ:-$(cd "$(dirname "$0")/../../.." && pwd)}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
falhas=0
falha() { echo ">> FALHA: $*"; falhas=$((falhas + 1)); }

# as cópias do gs/ com os mutantes
for m in ok linhas zera; do mkdir -p "$T/$m"; cp "$RAIZ"/port/src/gs/*.c "$RAIZ"/port/src/gs/*.h "$T/$m/"; done
python3 - "$T" <<'PY' || falha "um mutante não casou"
import sys
t = sys.argv[1]
for nome, a, b in (("linhas", "*dr = d + (r >> 1) * 16 + (r & 1) * 2;", "*dr = d + (r & 1) * 16 + (r >> 1) * 2;"),
                   ("zera", "for (r = 0; r < 4; r++) {\n            const uint8_t *sr", "for (r = 0; r < 3; r++) {\n            const uint8_t *sr")):
    p = f"{t}/{nome}/gs_core.c"
    s = open(p, encoding="utf-8").read()
    assert s.count(a) == 1, f"o mutante {nome} não casou"
    open(p, "w", encoding="utf-8").write(s.replace(a, b))
PY

compila() { # $1 x86|ppc, $2 diretório do gs, $3 saída
    local inc="-I$2 -I$RAIZ/include -I$RAIZ/port/include -I$RAIZ/port/src -I$RAIZ/port"
    if [ "$1" = x86 ]; then
        gcc -O2 -w -std=gnu89 -DBT3_TESTE_VU1 -DBT3_TESTE_ENVIO $inc "$RAIZ/port/x360/tools/teste_envio.c" -o "$3" \
            -Wl,--unresolved-symbols=ignore-all -lm -lpthread
    else
        powerpc-linux-gnu-gcc -O2 -w -std=gnu89 -static -DPORT_BIG_ENDIAN -DBT3_TESTE_ENVIO $inc "$RAIZ/port/x360/tools/teste_envio.c" -o "$3" \
            -Wl,--unresolved-symbols=ignore-all -lm -lpthread
    fi
}
roda() { if [ "$1" = x86 ]; then "$2" "${@:3}"; else qemu-ppc "$2" "${@:3}"; fi; }

for a in x86 ppc; do
    for m in ok linhas zera; do
        compila $a "$T/$m" "$T/t_${a}_$m" > "$T/c_${a}_$m.log" 2>&1 || { falha "$a $m não compilou"; cat "$T/c_${a}_$m.log"; continue; }
        roda $a "$T/t_${a}_$m" > "$T/s_${a}_$m.txt" 2>&1; rc=$?
        echo "== $a $m (saída $rc): $(tail -1 "$T/s_${a}_$m.txt")"
        if [ $m = ok ]; then
            [ $rc = 0 ] || { falha "$a: o envio por colunas difere do vram_rw"; head -9 "$T/s_${a}_$m.txt"; }
            grep -q '^1500 envios, diferentes com a chave 0: 0, 1: 0, 2: 0$' "$T/s_${a}_$m.txt" || falha "$a: a linha do resultado"
        else
            [ $rc = 1 ] || falha "$a: o mutante $m não foi acusado"
            grep -q 'diferentes com a chave 0: 0, 1: [1-9][0-9]*, 2: [1-9][0-9]*$' "$T/s_${a}_$m.txt" || falha "$a: o mutante $m não foi acusado só com a chave"
        fi
    done
done
roda x86 "$T/t_x86_ok" --mede
if [ $falhas = 0 ]; then echo "== tudo certo"; else echo "== $falhas falhas"; fi
[ $falhas = 0 ]
