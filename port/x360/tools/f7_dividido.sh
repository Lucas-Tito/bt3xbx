#!/usr/bin/env bash
# F7/03: uma fila oficial no WSL, builds do HEAD e saídas próprias fora do git.
# F7_SAIDA=~/f7e3-rodada BASE_X86=~/f7e3/base bash port/x360/tools/f7_dividido.sh [--servico]
# --servico arma um serviço de usuário que espera a trava sozinho. Não cria worktrees nem usa o Deck.
set -euo pipefail
cd "$(dirname "$0")/../../.."
: "${F7_SAIDA:?defina uma pasta nova fora do repositório}"
: "${BASE_X86:?defina a build x86 preservada da base 0a9cccb1}"
S=$(realpath -m "$F7_SAIDA")
BASE_X86=$(realpath "$BASE_X86")
case "$S/" in "$(pwd)/"*) echo 'saída dentro do repositório recusada'; exit 1;; esac
[ ! -e "$S" ] || { echo 'a pasta da rodada já existe'; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo 'árvore fora de commit'; exit 1; }
H=$(git rev-parse HEAD)
if [ "${1:-}" = --servico ]; then
    export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
    systemd-run --user --collect --unit="bt3-f7e3-${H:0:8}" -p MemoryMax=5G \
        --setenv="F7_SAIDA=$S" --setenv="BASE_X86=$BASE_X86" --setenv="F7_COMMIT=$H" \
        bash "$(pwd)/port/x360/tools/f7_dividido.sh"
    echo "aguardar o serviço bt3-f7e3-${H:0:8}; resumo: $S/resumo.txt"
    exit 0
fi
[ "${F7_COMMIT:-$H}" = "$H" ] || { echo 'HEAD mudou enquanto o serviço esperava'; exit 1; }
mkdir -p "$S"
exec > >(tee "$S/rodada.log") 2>&1
fim() {
    local rc=$?
    trap - EXIT
    echo "FIM RC=$rc" | tee -a "$S/resumo.txt"
    exit "$rc"
}
trap fim EXIT
# Não herdar reuso, mapa, gravação, cartão ou sabotagens de outra rodada.
while IFS= read -r v; do
    case "$v" in BT3_*|PPC|SEMMAPA|IMG|REF|REFS|SOM|REUSA|DESENVOLVIMENTO|MEM|O|T|CARTAO|ALVO|TP|JANELA|GUARDA_SOM) unset "$v";; esac
done < <(compgen -e)
export DEBUGINFOD_URLS= BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1 BT3_WIDE=1
passo() {
    local n=$1; shift
    echo "INÍCIO $n $(date -Is)" | tee -a "$S/resumo.txt"
    "$@" 2>&1 | tee "$S/$n.log" > /dev/null
    echo "OK $n $(date -Is)" | tee -a "$S/resumo.txt"
}
echo "$H" > "$S/commit.txt"
# Uma única espera e uma única trava para todos os passos dependentes.
exec 9> "$HOME/wsl.lock"
echo 'aguardando a trava do WSL' | tee -a "$S/resumo.txt"
flock 9
pgrep -af 'qemu|roteiros' > "$S/processos-antes.txt" || true
if pgrep -x qemu-ppc || pgrep -f '(^|/|[[:space:]])roteiros[.]sh([[:space:]]|$)'; then
    echo 'emulador ocupado sem a trava'; exit 1
fi
[ "$(git rev-parse HEAD)" = "$H" ] && [ -z "$(git status --porcelain)" ]
for args in 'x86' 'x86 --rastreio' 'ppc' 'x86 --gpu --vu1' 'ppc --gpu'; do
    read -ra a <<< "$args"
    passo "build-${args// /_}" python3 port/x360/tools/headless.py --alvo "${a[@]}"
done
for b in x86 x86r ppc x86gv ppcg; do
    grep -qx "$(git rev-parse --short HEAD) diff:da39a3ee5e" "port/build/$b/CARIMBO"
done
passo objetos python3 port/x360/tools/f5_final_confere.py objetos "$BASE_X86" port/build/x86
objcopy --dump-section .text="$S/text.bin" port/build/x86/bt3_x86
sha1sum "$S/text.bin" > "$S/text.sha1"
passo inventario python3 port/x360/tools/unpacks.py
passo novos env BT3_DOIS=1 SEMMAPA=1 IMG=1 O="$S/roteiros" REFS="$S/referencias" \
    bash port/x360/tools/roteiros.sh f7_dividido f7_simples
for ponto in cheia dividido_volta; do
    M=$S/roteiros/f7_dividido/mem_ponto_$ponto
    passo "memoria-negativos-$ponto" python3 port/x360/tools/teste_memoria_luta.py \
        "$M/hm.txt.dump" "$M/hq.txt.dump" port/build/x86/bt3_x86 port/build/ppc/bt3_ppc "$M/donos"
done
passo regressao env SEMMAPA=1 IMG=1 O="$S/regressao" bash port/x360/tools/roteiros.sh duelo
# Gravação efetiva da entrada no scePad2Read, independente do log do Roteiro_Pad.
for b in x86 ppc; do
    mkdir -p "$S/entrada-$b"
    A=(env BT3_GS=none BT3_DOIS=1 BT3_PAD_REC="$S/entrada-$b/pad.bin" \
       BT3_ROTEIRO=port/x360/roteiros/f7_dividido.txt BT3_SAVES="$S/entrada-$b/cartao")
    if [ "$b" = x86 ]; then
        A+=(timeout 900 port/build/x86/bt3_x86)
    else
        D=$S/roteiros/f7_dividido/dados
        A+=(BT3_DATA="$D" BT3_CONVERTIDOS="$D/mods/convertidos.txt" timeout 3600 qemu-ppc port/build/ppc/bt3_ppc)
    fi
    passo "entrada-$b" "${A[@]}"
    passo "confere-entrada-$b" python3 port/x360/tools/f7_dividido.py entrada "$S/entrada-$b.log" "$S/entrada-$b/pad.bin"
done
cmp "$S/entrada-x86/pad.bin" "$S/entrada-ppc/pad.bin"
# A lista e as duas metades, no caminho dos shaders, sem rasterizar no backend falso.
for r in f7_simples f7_dividido; do
    for b in x86gv ppcg; do
        A=(env BT3_UNCAPPED=1 BT3_GS=gpu BT3_SCALE=1 BT3_DOIS=1 BT3_GPU_LISTA=1 \
           BT3_ROTEIRO="port/x360/roteiros/$r.txt" BT3_SAVES="$S/gpu-$r-$b-cartao")
        if [ "$b" = x86gv ]; then
            A+=(BT3_PERFIL=1 timeout 900 port/build/x86gv/bt3_x86gv)
        else
            D=$S/roteiros/$r/dados
            A+=(BT3_DATA="$D" BT3_CONVERTIDOS="$D/mods/convertidos.txt" timeout 3600 qemu-ppc port/build/ppcg/bt3_ppcg)
        fi
        passo "gpu-$r-$b" "${A[@]}"
    done
    passo "listas-$r" python3 port/x360/tools/divergencias.py "$S/gpu-$r-x86gv.log" "$S/gpu-$r-ppcg.log"
done
for b in x86gv ppcg; do
    passo "vistas-$b" python3 port/x360/tools/f7_dividido.py vistas "$S/gpu-f7_dividido-$b.log"
done
for b in x86 ppc; do
    passo "metades-imagem-$b" python3 port/x360/tools/f7_dividido.py imagens \
        "$S/roteiros/f7_dividido/img/$b/marca_ponto_dividido.ppm"
done
passo perfil python3 port/x360/tools/f7_dividido.py perfil "$S/gpu-f7_simples-x86gv.log" "$S/gpu-f7_dividido-x86gv.log"
passo negativos python3 port/x360/tools/f7_dividido.py negativos "$S/entrada-x86.log" "$S/entrada-x86/pad.bin" \
    "$S/gpu-f7_dividido-x86gv.log" "$S/gpu-f7_simples-x86gv.log" \
    "$S/roteiros/f7_dividido/img/x86/marca_ponto_dividido.ppm" "$S/negativos"
# Os próprios roteiros da sonda curta também chegam às três janelas nas duas ordens de bytes.
for r in simples dividido; do
    for b in x86 ppc; do
        A=(env BT3_GS=none BT3_UNCAPPED=1 BT3_DOIS=1 BT3_ROTEIRO="port/x360/roteiros/f7_perfil_$r.txt"
           BT3_SAVES="$S/sonda-$r-$b-cartao")
        if [ "$b" = x86 ]; then
            A+=(timeout 900 port/build/x86/bt3_x86)
        else
            D=$S/roteiros/f7_$r/dados
            A+=(BT3_DATA="$D" BT3_CONVERTIDOS="$D/mods/convertidos.txt" timeout 3600 qemu-ppc port/build/ppc/bt3_ppc)
        fi
        passo "sonda-$r-$b" "${A[@]}"
        passo "sonda-confere-$r-$b" python3 port/x360/tools/passos.py "$S/sonda-$r-$b.log" \
            --carimbo "$(git rev-parse --short HEAD) diff:da39a3ee5e" --minimo 600
        python3 port/x360/tools/roteiros.py telas "$S/sonda-$r-$b.log" > "$S/sonda-$r-$b.telas"
    done
    passo "sonda-rastro-$r" python3 port/x360/tools/roteiros.py compara "$S/sonda-$r-x86.telas" "$S/sonda-$r-ppc.telas"
done
passo save-mc bash port/x360/tools/save_mc.sh
passo confere-save-mc python3 port/x360/tools/f5_final_confere.py save-mc port/build/save_mc
passo save-entre env O="$S/save_entre" bash port/x360/tools/save_entre.sh
# O gerador é exercitado, mas nenhum ambiente é enviado ao console.
passo ambientes python3 port/x360/tools/f7_console.py "$S/console" "${H:0:8}"
if python3 port/x360/tools/f7_console.py "$S/console" "${H:0:8}" > "$S/ambiente-repetido.txt" 2>&1; then exit 1; fi
if python3 port/x360/tools/f7_console.py "$S/console-invalido" ../ruim > "$S/ambiente-invalido.txt" 2>&1; then exit 1; fi
[ -z "$(git status --porcelain)" ] && [ "$(git rev-parse HEAD)" = "$H" ]
echo 'tudo certo no WSL; console não medido nem aprovado' | tee -a "$S/resumo.txt"
