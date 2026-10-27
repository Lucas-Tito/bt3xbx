#!/usr/bin/env bash
# Tempo do roteiro completo na x86gv, sem marcas finas nem listas por quadro.
# Uso: cortes_medir.sh SAIDA [REPETICOES]. A trava inclui todas as execuções.
set -euo pipefail
R=$(cd "$(dirname "$0")/../../.." && pwd)
I=$(realpath -m "$1")
N=${2:-3}
mkdir -p "$I"
cd "$R"
exec 9>>"$HOME/wsl.lock"
flock 9
pgrep -af 'qemu|roteiros' > "$I/processos.log" || true
if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu'; exit 1; fi
export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_TEX_ORIGEM=1 BT3_TEMPOS=0 BT3_CORTES=0 BT3_GS_THREAD=0 BT3_GS_DESENHO=0
export BT3_ROTEIRO=port/x360/roteiros/f5_duelo.txt BT3_SAVES=$I/save
unset BT3_VU_CONST_CONFERE BT3_GPU_LISTA BT3_GPU_NORMAL BT3_WIDE BT3_MAPA BT3_GUARDA
trap 'rm -rf "$I/save"' EXIT
for ((rep=1; rep<=N; rep++)); do
    if ((rep % 2)); then modos='00 01 11 10 20 21'; else modos='21 20 10 11 01 00'; fi
    for modo in $modos; do
        export BT3_VU_CONSTANTES=${modo:0:1} BT3_VU_FECHO=${modo:1:1}
        rm -rf "$BT3_SAVES"
        /usr/bin/time -f '%e %U %S' -o "$I/$rep-$modo.tempo" timeout 900 "${BT3_BINARIO:-port/build/x86gv/bt3_x86gv}" > "$I/$rep-$modo.log" 2>&1
        grep -q '^roteiro: .* fim ' "$I/$rep-$modo.log"
        grep -q '^gpu_falso: 8380 quadros' "$I/$rep-$modo.log"
        if grep -q 'FALHA\|CRASH\|^gpu_falso: quadro\|^gpu_falso: normal quadro\|^cortes:' "$I/$rep-$modo.log"; then exit 1; fi
        echo "$rep $modo: $(cat "$I/$rep-$modo.tempo")"
    done
done
echo FIM
