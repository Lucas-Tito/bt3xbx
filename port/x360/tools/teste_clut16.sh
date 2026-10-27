#!/usr/bin/env bash
# F5: prova sem dados do jogo, na x86 de 32 bits e na PPC real; vê a chave antiga falhar.
# Rodar da raiz. ALVO=x86|ppc (padrão x86); respeita o bloqueio dos ciclos do WSL.
set -euo pipefail
RAIZ=$(pwd)
T=$(mktemp -d "$HOME/clut16-teste.XXXXXX")
trap 'rm -rf "$T"' EXIT
exec 9>>"$HOME/wsl.lock"
flock 9
cp port/src/gs/gs_draw.c "$T/gs_draw.c"
python3 port/x360/tools/clut16.py extrai "$T/gs_draw.c" "$T/clut16_hash.h"
if [ "${ALVO:-x86}" = ppc ]; then
    CC=(powerpc-linux-gnu-gcc -static -DPORT_BIG_ENDIAN)
    RODA=(qemu-ppc)
    # O pgrep também acha este script no comando do chamador; só processos qemu contam aqui.
    if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu em execução'; exit 1; fi
else
    CC=(clang -m32 -DBT3_TESTE_VU1)
    RODA=()
fi
compila() {
    "${CC[@]}" -O1 -w -std=gnu89 -ffunction-sections -fdata-sections -I"$T" -Iport/src/gs -Iport/x360/headless \
        -Iport/include -Iport/src -Iinclude -Iport port/x360/tools/teste_clut16.c -Wl,--gc-sections -lm -lpthread -o "$T/teste"
}
compila
"${RODA[@]}" "$T/teste" > "$T/certo.log" 2>&1
cat "$T/certo.log"
python3 port/x360/tools/clut16_confere.py "$T/certo.log" --sintetico
python3 port/x360/tools/clut16.py sabota "$T/gs_draw.c"
python3 port/x360/tools/clut16.py extrai "$T/gs_draw.c" "$T/clut16_hash.h"
compila
if "${RODA[@]}" "$T/teste" > "$T/errado.log" 2>&1; then
    echo 'FALHA: a chave antiga passou no teste'; exit 1
fi
tail -n 2 "$T/errado.log"
if python3 port/x360/tools/clut16_confere.py "$T/errado.log" --sintetico; then
    echo 'FALHA: o diagnóstico aceitou as colisões'; exit 1
fi
python3 port/x360/tools/clut16_confere.py "$T/errado.log" --sintetico --colisoes 144
# O verificador também precisa recusar um relatório truncado e um número de ocorrências adulterado.
sed '/^clut16: leituras/d' "$T/certo.log" > "$T/truncado.log"
sed 's/colisoes 0/colisoes 1/' "$T/certo.log" > "$T/adulterado.log"
for l in truncado adulterado; do
    if python3 port/x360/tools/clut16_confere.py "$T/$l.log" --sintetico; then
        echo "FALHA: $l passou"; exit 1
    fi
done
echo "tudo certo: ${ALVO:-x86}, chave antiga e relatórios estragados acusados"
