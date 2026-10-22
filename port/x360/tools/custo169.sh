#!/usr/bin/env bash
# F8, 16:9, etapa 1 (plano/F8/16x9.md; no WSL ou no Deck, na raiz): o custo do 16:9 por quadro na x86gv
# (headless.py --alvo x86 --gpu --vu1, do commit de agora), 4:3 contra 16:9, nos roteiros pedidos (padrão: f5_duelo
# f5_treino). Cada roteiro duas vezes na x86gv, sem a divisão, uma com BT3_WIDE=0 e outra com BT3_WIDE=1, e o
# custo169.py sobre os dois logs. Uns 30 s por rodada, sem emulador. Sai com 1 se alguma conferência falhar.
#   O=pasta    onde ficam os logs (port/build/custo169)
#   REUSA=1    não roda o jogo: só o custo169.py sobre os logs guardados (para vê-lo falhar numa cópia estragada)
set -uo pipefail
B=port/build; O=${O:-$B/custo169}; RC=0
NOMES=("$@"); [ ${#NOMES[@]} -gt 0 ] || NOMES=(f5_duelo f5_treino)
mkdir -p $O
echo "x86gv $(head -1 $B/x86gv/CARIMBO 2>/dev/null)"
for r in "${NOMES[@]}"; do
    echo "== $r"
    if [ -z "${REUSA:-}" ]; then
        for f in 43:0 169:1; do
            rm -rf $O/sv_$r; mkdir -p $O/sv_$r
            env -u BT3_GS_THREAD -u BT3_GS_DESENHO BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 \
                BT3_DESBLOQUEIO=1 BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_BYTES=1 BT3_GPU_PIVOS=1 \
                BT3_WIDE=${f#*:} BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$O/sv_$r \
                timeout 900 $B/x86gv/bt3_x86gv > $O/${r}_${f%:*}.log 2>&1
            echo "   x86gv ${f%:*} RC=$?"
        done
        rm -rf $O/sv_$r
    fi
    python3 port/x360/tools/custo169.py $O/${r}_43.log $O/${r}_169.log | tee $O/$r.custo
    [ ${PIPESTATUS[0]} -eq 0 ] || { echo "   >> FALHA: $r"; RC=1; }
done
[ $RC -eq 0 ] && echo "== tudo certo" || echo "== com falhas"
exit $RC
