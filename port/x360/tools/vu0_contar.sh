#!/usr/bin/env bash
# Executar sob ~/vez.sh. LISTA é lidos.txt de uma rodada aceita de f5_duelo.
# Não mede tempo de CPU: os incrementos contam chamadas inclusive após inlining.
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$RAIZ"
D=${1:?uso: vu0_contar.sh PASTA LISTA}
LISTA=${2:?informe lidos.txt do f5_duelo}
mkdir -p "$D"
exec > >(tee "$D/contar.log") 2>&1
trap 'rc=$?; echo "$rc" > "$D/resultado.rc"; exit "$rc"' EXIT
pgrep -af 'qemu|roteiros' > "$D/processos-iniciais.txt" || true
export DEBUGINFOD_URLS= BT3_TESTE_VU0_NATIVO=1 BT3_VU0_CONTA=1 BT3_WIDE=1
# A PPC gera o mapa das tabelas pelos .i da x86 normal, não pelos da x86vc.
env -u BT3_VU0_CONTA -u BT3_TESTE_VU0_NATIVO python3 port/x360/tools/headless.py --alvo x86 2>&1 | tee "$D/build-x86.log"
python3 port/x360/tools/headless.py --alvo x86 2>&1 | tee "$D/build-x86vc.log"
python3 port/x360/tools/headless.py --alvo ppc 2>&1 | tee "$D/build-ppcvc.log"
cp "$LISTA" "$D/lidos.txt"
mkdir -p "$D/dados/mods"
for d in "$RAIZ"/gamedata/*; do ln -s "$d" "$D/dados/$(basename "$d")"; done
gcc -std=c89 -O2 -w -o "$D/conv_tool" $(find port/x360/conv -name '*.c' ! -name conv_jogo.c)
mapfile -t entradas < "$D/lidos.txt"
"$D/conv_tool" --saida "$D/dados/mods" gamedata "${entradas[@]}" > "$D/conversao.log"
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
export BT3_ROTEIRO="$RAIZ/port/x360/roteiros/f5_duelo.txt"
for lado in 1 0; do
    export BT3_VU0_NATIVO=$lado
    for alvo in x86 ppc; do
        export BT3_SAVES="$D/saves-$alvo-$lado" BT3_HASH="$D/hash-$alvo-$lado.txt"
        mkdir "$BT3_SAVES"
        unset BT3_DATA BT3_CONVERTIDOS
        comando=("port/build/${alvo}vc/bt3_${alvo}vc")
        if [ "$alvo" = ppc ]; then
            export BT3_DATA="$D/dados" BT3_CONVERTIDOS="$D/dados/mods/convertidos.txt"
            comando=(qemu-ppc "${comando[@]}")
        fi
        timeout 1200 "${comando[@]}" > "$D/conta-$alvo-$lado.txt" 2>&1
        grep -q '^roteiro: .* fim ' "$D/conta-$alvo-$lado.txt"
        if grep -q FALHA "$D/conta-$alvo-$lado.txt"; then exit 1; fi
        python3 port/x360/tools/vu0_conta_resumo.py "$D/conta-$alvo-$lado.txt" --json "$D/conta-$alvo-$lado.json" > "$D/resumo-$alvo-$lado.txt"
        python3 port/x360/tools/roteiros.py telas "$D/conta-$alvo-$lado.txt" > "$D/telas-$alvo-$lado.txt"
        python3 port/x360/tools/roteiros.py compara "$D/telas-x86-1.txt" "$D/telas-$alvo-$lado.txt"
        echo "Fim $alvo nativo=$lado $(date -Is)"
    done
done
python3 - "$D" <<'PY'
import json,pathlib,sys
p=pathlib.Path(sys.argv[1]);rs={}
for a in ['x86','ppc']:
 for n in [1,0]:rs[a,n]=json.loads((p/f'conta-{a}-{n}.json').read_text())
for n in [1,0]:
 assert rs['x86',n]==rs['ppc',n],f'contagem x86/PPC diferente, nativo={n}'
for a in ['x86','ppc']:
 for n in [1,0]:
  r=rs[a,n];assert r['passos']>2000
  alvo=r['totais']['vu0_nativo/Vn_MtxApply']
  assert (alvo[1]>0)==bool(n)
  if not n:assert alvo[3]==alvo[0]
print('tudo certo: quatro rastros iguais, contagens x86/PPC iguais nas duas posições')
PY
