#!/usr/bin/env bash
# Rodada integral F6, sem console. Uma trava cobre toda a fila, sem passos em segundo plano.
# Uso: f6_final.sh COMMIT BASE DESTINO_NOVO [--plano]
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
MODO=${1:-}
if [ "$MODO" = --servico ] || [ "$MODO" = --travado ]; then shift; fi
[ $# -ge 3 ] && [ $# -le 4 ] || { echo 'uso: f6_final.sh COMMIT BASE DESTINO_NOVO [--plano]'; exit 2; }
FINAL=$(git -C "$RAIZ" rev-parse --verify "$1^{commit}")
BASE=$(git -C "$RAIZ" rev-parse --verify "$2^{commit}")
I=$3
[[ "$I" = "$HOME/"* ]] && [ ! -e "$I" ] || { echo 'FALHA: destino deve ser novo, sob HOME'; exit 1; }
UNIDADE=bt3-f6-final-${FINAL:0:8}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
if [ "$MODO" != --servico ] && [ "$MODO" != --travado ]; then
    [ "$(git -C "$RAIZ" rev-parse HEAD)" = "$FINAL" ]
    [ -z "$(git -C "$RAIZ" status --porcelain)" ] || { echo 'FALHA: árvore alterada'; exit 1; }
    CMD=(systemd-run --user --unit="$UNIDADE" --property=MemoryMax=5G --collect
         bash "$RAIZ/port/x360/tools/f6_final.sh" --servico "$FINAL" "$BASE" "$I")
    if [ "${4:-}" = --plano ]; then printf '%q ' "${CMD[@]}"; printf '\n'; exit 0; fi
    [ $# = 3 ]
    "${CMD[@]}"
    echo "unidade $UNIDADE; saída $I; esperar pelo shell, ver 09.md"
    exit 0
fi
if [ "$MODO" = --servico ]; then
    # flock em primeiro plano espera a fila inteira. O processo seguinte só nasce depois da trava.
    exec flock "$HOME/wsl.lock" bash "$RAIZ/port/x360/tools/f6_final.sh" --travado "$FINAL" "$BASE" "$I"
fi
mkdir -p "$I"
exec > >(tee "$I/rodada.log") 2>&1
trap 'rc=$?; echo "FIM rc=$rc" | tee -a "$I/provas.resumo"' EXIT
pgrep -af 'qemu|roteiros' > "$I/processos-iniciais.log" || true
if pgrep -x qemu-ppc || pgrep -f '(^|/|[[:space:]])roteiros[.]sh([[:space:]]|$)'; then
    echo 'FALHA: outro ciclo em execução apesar da trava'; exit 1
fi
while IFS= read -r v; do
    case "$v" in BT3_*|PPC|SEMMAPA|IMG|REF|REFS|SOM|REUSA|DESENVOLVIMENTO|MEM|O|T|CARTAO|ALVO|TP|JANELA|GUARDA_SOM|RAIZ|CC|OUT)
        # RAIZ é local ao script e será restaurada abaixo, não usar ambiente herdado.
        [ "$v" != RAIZ ] || continue
        unset "$v";;
    esac
done < <(compgen -e)
export DEBUGINFOD_URLS= BT3_WIDE=1
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
python3 - "$I" "$FINAL" "$BASE" <<'PY'
import json, sys
from pathlib import Path
p = Path(sys.argv[1])
(p/'origem.json').write_text(json.dumps(dict(commit=sys.argv[2], base=sys.argv[3], curto=sys.argv[2][:8])))
(p/'casos.json').write_text('[]')
PY
arvore() {
    local nome=$1 sha=$2 w=$I/$1
    git -C "$RAIZ" worktree add --detach "$w" "$sha"
    for d in asm assets disc gamedata; do ln -s "/srv/bt3/dados/$d" "$w/$d"; done
    mkdir -p "$w/port/build/gen"
    cp -r "$RAIZ/port/build/gen/data" "$RAIZ/port/build/gen/gs" "$w/port/build/gen/"
}
arvore base "$BASE"
arvore final "$FINAL"
cd "$I/final"
C=$PWD/port/x360/tools/f6_final_confere.py
# A origem curta é obtida pelo git: core.abbrev pode variar entre clones.
python3 - "$I/origem.json" "$(git rev-parse --short HEAD)" <<'PY'
import json, sys
from pathlib import Path
p=Path(sys.argv[1]); d=json.loads(p.read_text()); d['curto']=sys.argv[2]; p.write_text(json.dumps(d))
PY
passo() {
    local nome=$1; shift
    local inicio fim rc
    inicio=$(date +%s)
    echo "INÍCIO $nome $(date -Is)" | tee -a "$I/provas.resumo"
    set +e
    (set -e; "$@") 2>&1 | tee "$I/$nome.log"
    rc=${PIPESTATUS[0]}
    set -e
    fim=$(date +%s)
    python3 - "$I" "$nome" "$rc" "$((fim-inicio))" <<'PY'
import hashlib, json, sys
from pathlib import Path
p=Path(sys.argv[1]); nome=sys.argv[2]; f=p/'casos.json'; casos=json.loads(f.read_text())
casos.append(dict(nome=nome, rc=int(sys.argv[3]), segundos=int(sys.argv[4]),
                  sha256=hashlib.sha256((p/(nome+'.log')).read_bytes()).hexdigest()))
f.write_text(json.dumps(casos, indent=2))
PY
    echo "FIM $nome rc=$rc segundos=$((fim-inicio))" | tee -a "$I/provas.resumo"
}
build_base() { (cd "$I/base"; python3 port/x360/tools/headless.py --alvo x86); }
build_final() {
    for args in 'x86' 'x86 --rastreio' 'ppc' 'x86 --gravador' 'ppc --gravador'; do
        read -ra a <<< "$args"
        python3 port/x360/tools/headless.py --alvo "${a[@]}"
    done
    for b in x86 x86r ppc x86a ppca; do
        grep -qx "$(git rev-parse --short HEAD) diff:da39a3ee5e" "port/build/$b/CARIMBO"
    done
}
texto() {
    for nome in base final; do
        objcopy -O binary --only-section=.text "$I/$nome/port/build/x86/bt3_x86" "$I/text-$nome.bin"
        sha1sum "$I/text-$nome.bin"
    done
    python3 "$C" text "$I"
}
disco() {
    gcc -std=c89 -O2 -w -o "$I/conv_tool" $(find port/x360/conv -name '*.c' ! -name conv_jogo.c | sort)
    "$I/conv_tool" --disco gamedata
}
filme_build() {
    gcc -m32 -std=gnu99 -O2 -Wall -Wextra port/x360/tools/f6_filme_hash.c -o "$I/filme-x86"
    powerpc-linux-gnu-gcc -static -std=gnu99 -O2 -Wall -Wextra port/x360/tools/f6_filme_hash.c -o "$I/filme-ppc"
    echo 'hash RGBA: x86 e PPC compilados'
}
referencia_upstream() {
    # Não há mpeg2test neste clone. Não substituir por outro nome do mesmo decodificador.
    echo 'FALHA: mpeg2test do upstream ausente neste clone; comparação independente pendente na revisão Linux'
    return 1
}
soma() {
    python3 port/x360/tools/roteiros.py soma "$I"/roteiros/*/gdb.txt --prog port/build/x86/bt3_x86
}
limpeza() {
    for w in "$I/base" "$I/final"; do [ -z "$(git -C "$w" status --porcelain)" ]; done
    ! pgrep -x qemu-ppc
    echo 'árvores limpas; nenhum emulador restante'
}
passo build-base build_base
passo build-final build_final
passo objetos python3 "$C" objetos "$I/base/port/build/x86" port/build/x86
passo text texto
passo negativos python3 "$C" negativos
passo inventario python3 port/x360/tools/unpacks.py
passo cores python3 port/x360/tools/cores.py
passo pacotes64 python3 port/x360/tools/pacotes64.py port/build/x86/obj
passo disco disco
passo iso python3 port/x360/tools/teste_iso_convertida.py /srv/bt3/dados/iso/bt3.iso --dados gamedata --trabalho "$I/iso"
passo leitor-iso port/x360/tools/teste_iso.sh "$I/iso/convertida.iso"
passo som-ambiente python3 port/x360/tools/som_ambiente.py --mutantes
passo som360 port/x360/tools/teste_som360.sh
passo som-amostras env T="$I/som-amostras" port/x360/tools/som.sh amostras
passo som-fluxo env T="$I/som-fluxo" port/x360/tools/som.sh fluxo
passo filme-testes env OUT="$I/filmes" port/x360/tools/teste_filme.sh
passo filme-blocos port/x360/tools/teste_blocos.sh
passo filme-build filme_build
for tipo in op ed; do
    case "$tipo" in op) pss=ZS3USOP;; ed) pss=ZS3USED;; esac
    arquivo=/srv/bt3/dados/gamedata/disc/DATA/$pss.PSS
    passo filme-$tipo-x86 timeout 1800 "$I/filme-x86" "$arquivo" "$tipo"
    passo filme-$tipo-ppc timeout 3600 qemu-ppc "$I/filme-ppc" "$arquivo" "$tipo"
    passo filme-$tipo-compara python3 "$C" filmes "$I/filme-$tipo-x86.log" "$I/filme-$tipo-ppc.log" "$tipo"
done
passo filme-referencia-upstream referencia_upstream
passo deriva python3 port/x360/tools/som_deriva.py --autoteste
export O=$I/roteiros REFS=$I/referencias SEMMAPA=1 IMG=1 SOM=1
cp -r "$HOME/referencias" "$REFS"
mapfile -t NOMES < <(python3 -c 'import sys; sys.path.insert(0,"port/x360/tools"); from f6_final_confere import ROTEIROS; print("\n".join(ROTEIROS))')
for n in "${NOMES[@]}"; do
    passo roteiro-$n port/x360/tools/roteiros.sh "$n"
done
passo soma soma
passo som-controles env T="$I/som-controles" port/x360/tools/som.sh controles f5_duelo
unset O REFS SEMMAPA IMG SOM
passo save-mc port/x360/tools/save_mc.sh
passo save-mc-julgamento python3 "$C" save-mc port/build/save_mc
passo save-entre env O="$I/save-entre" port/x360/tools/save_entre.sh
passo limpeza limpeza
python3 "$C" rodada "$I" | tee "$I/julgamento.md"
