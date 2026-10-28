#!/usr/bin/env bash
# Prepara e dispara a rodada final no WSL. Não envia nada ao console.
# Uso: f5_final.sh COMMIT BASE DESTINO_NOVO_ABSOLUTO [--plano]
# O modo --plano só imprime a chamada do serviço. Não cria árvores nem executa testes.
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
SERVICO=0
if [ "${1:-}" = --servico ]; then SERVICO=1; shift; fi
[ $# -ge 3 ] && [ $# -le 4 ] || { echo 'uso: f5_final.sh COMMIT BASE DESTINO_NOVO_ABSOLUTO [--plano]'; exit 2; }
FINAL=$(git -C "$RAIZ" rev-parse --verify "$1^{commit}")
BASE=$(git -C "$RAIZ" rev-parse --verify "$2^{commit}")
I=$3
[[ "$I" = "$HOME/"* ]] && [ ! -e "$I" ] || { echo 'FALHA: destino deve ser novo e ficar sob HOME'; exit 1; }
UNIDADE=bt3-f5-final-${FINAL:0:8}
if [ "$SERVICO" = 0 ]; then
    [ "$(git -C "$RAIZ" rev-parse HEAD)" = "$FINAL" ] || { echo "FALHA: executar com HEAD no commit final"; exit 1; }
    [ -z "$(git -C "$RAIZ" status --porcelain)" ] || { echo 'FALHA: árvore com alterações'; exit 1; }
    CMD=(systemd-run --user --unit="$UNIDADE" -p MemoryMax=5G --collect
         bash "$RAIZ/port/x360/tools/f5_final.sh" --servico "$FINAL" "$BASE" "$I")
    if [ "${4:-}" = --plano ]; then printf '%q ' "${CMD[@]}"; printf '\n'; exit 0; fi
    [ $# = 3 ] || exit 2
    "${CMD[@]}"
    echo "Resumo: $I/provas.resumo; log: $I/rodada.log; unidade: $UNIDADE"
    exit 0
fi
mkdir -p "$I"
exec > >(tee "$I/rodada.log") 2>&1
P=$I/provas.resumo
pa() { echo "$*" | tee -a "$P"; }
termina() {
    local rc=$?
    trap - EXIT
    if [ "$rc" = 0 ]; then pa 'tudo certo no WSL; console e aceite da F5 continuam pendentes';
    else pa "FALHA RC=$rc; interrompido, consultar o último passo e rodada.log"; fi
    pa FIM
    exit "$rc"
}
trap termina EXIT
# Ambiente herdado não pode ligar reuso, mapa, sabotagem, som ou desenho escondidos.
while IFS= read -r v; do
    case "$v" in BT3_*|PPC|SEMMAPA|IMG|REF|REFS|SOM|REUSA|DESENVOLVIMENTO|MEM|O|T|CARTAO|ALVO|TP|JANELA|GUARDA_SOM|RAPIDO) unset "$v";; esac
done < <(compgen -e)
export DEBUGINFOD_URLS= BT3_WIDE=1
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
livre() {
    pgrep -af 'qemu|roteiros' >> "$I/processos.log" || true
    if pgrep -x qemu-ppc || pgrep -f '(^|/|[[:space:]])roteiros[.]sh([[:space:]]|$)'; then
        echo 'FALHA: outro ciclo de emulador em execução'; return 1
    fi
}
# Trava por etapa: teste_clut16.sh tem a própria trava, nunca aninhar os dois flock.
passo() {
    local nome=$1; shift
    pa "INÍCIO $nome: $(date -Is)"
    (
        flock -n 9 || { echo 'FALHA: trava do WSL ocupada'; exit 1; }
        livre
        "$@"
    ) 9>>"$HOME/wsl.lock" 2>&1 | tee "$I/$nome.log"
    pa "OK $nome: $(date -Is)"
}
arvore() {
    local nome=$1 sha=$2 w=$I/$1
    git -C "$RAIZ" worktree add --detach "$w" "$sha"
    for d in asm assets disc gamedata; do ln -s "/srv/bt3/dados/$d" "$w/$d"; done
    mkdir -p "$w/port/build/gen"
    cp -r "$RAIZ/port/build/gen/data" "$RAIZ/port/build/gen/gs" "$w/port/build/gen/"
}
pa "final $FINAL; base $BASE; início $(date -Is); WSL, BT3_WIDE=1"
arvore base "$BASE"
cd "$I/base"
passo build-base python3 port/x360/tools/headless.py --alvo x86
arvore final "$FINAL"
cd "$I/final"
for args in 'x86' 'x86 --rastreio' 'ppc' 'x86 --gravador' 'ppc --gravador'; do
    read -ra a <<< "$args"
    passo "build-${args// /-}" python3 port/x360/tools/headless.py --alvo "${a[@]}"
done
for b in x86 x86r ppc x86a ppca; do
    grep -qx "$(git rev-parse --short HEAD) diff:da39a3ee5e" "port/build/$b/CARIMBO"
done
for nome in base final; do
    objcopy -O binary --only-section=.text "$I/$nome/port/build/x86/bt3_x86" "$I/text-$nome.bin"
    pa ".text $nome: $(sha1sum "$I/text-$nome.bin")"
done
passo text python3 port/x360/tools/f5_final_confere.py text "$I"
passo objetos python3 port/x360/tools/f5_final_confere.py objetos "$I/base/port/build/x86" port/build/x86
passo inventario python3 port/x360/tools/unpacks.py
passo cores python3 port/x360/tools/cores.py
# layout.py faz parte da build PPC; a auditoria AST usa os .i da x86.
passo pacotes64 python3 port/x360/tools/pacotes64.py port/build/x86/obj
passo disco bash -c 'gcc -std=c89 -O2 -w -o port/build/conv_tool $(find port/x360/conv -name "*.c" ! -name conv_jogo.c | sort) && port/build/conv_tool --disco gamedata'
passo iso python3 port/x360/tools/teste_iso_convertida.py /srv/bt3/dados/iso/bt3.iso --dados gamedata --trabalho "$I/iso"
passo leitor-iso port/x360/tools/teste_iso.sh "$I/iso/convertida.iso"
# Um único comando para preservar também a soma de todas as chamadas contra o inventário.
export O=$I/roteiros REFS=$I/referencias REF=1 SEMMAPA=1 IMG=1 SOM=1
cp -r "$HOME/referencias" "$REFS"
passo roteiros port/x360/tools/roteiros.sh f5_duelo f5_treino duelo duelo_time treino opcoes historia historia_novo historia_perde ultimate missao sobrevivencia simdragon discfusion torneio evolution datacenter referencia shenlong
for n in f5_duelo f5_treino; do
    while read -r t v; do
        [[ "$t" = ponto_* ]] || continue
        m=$O/$n/mem_$t
        passo "negativos-$n-$t" python3 port/x360/tools/teste_memoria_luta.py "$m/hm.txt.dump" "$m/hq.txt.dump" port/build/x86/bt3_x86 port/build/ppc/bt3_ppc "$m/donos"
    done < "$O/$n/mem_telas.txt"
done
unset O REFS REF IMG SOM
# Rede de listas do caminho de shaders, com e sem cache e divisão entre threads.
passo build-x86gv python3 port/x360/tools/headless.py --alvo x86 --gpu --vu1
passo build-ppcg python3 port/x360/tools/headless.py --alvo ppc --gpu
mkdir -p "$I/dados/mods"
for d in gamedata/*; do ln -s "$(realpath "$d")" "$I/dados/$(basename "$d")"; done
passo dados-listas port/build/conv_tool --disco --saida "$I/dados/mods" gamedata
lista() {
    local alvo=$1 n=$2 chave=$3 threads=$4
    local log=$I/lista-$alvo-$n-$chave-$threads.txt
    (
        export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
        export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_NORMAL=2 BT3_TEX_ORIGEM=$chave
        export BT3_GS_THREAD=$threads BT3_GS_DESENHO=$threads BT3_ROTEIRO=port/x360/roteiros/$n.txt
        export BT3_SAVES=$I/save-lista-$alvo-$n-$chave-$threads
        if [ "$alvo" = ppcg ]; then
            export BT3_DATA=$I/dados BT3_CONVERTIDOS=$I/dados/mods/convertidos.txt BT3_LEU=1
            timeout 3600 qemu-ppc port/build/ppcg/bt3_ppcg
        else
            timeout 900 port/build/x86gv/bt3_x86gv
        fi
    ) > "$log" 2>&1
    grep -q '^roteiro: .* fim ' "$log"
    grep -q '^gpu_falso: [0-9]* quadros, hash do jogo' "$log"
    ! grep -q 'FALHA\|CRASH\|race:' "$log"
    tail -1 "$log"
}
for alvo in x86gv ppcg; do
    for n in f5_duelo f5_treino shenlong; do
        for par in '0 0' '1 0' '1 1'; do
            read -r chave threads <<< "$par"
            passo "lista-$alvo-$n-$chave-$threads" lista "$alvo" "$n" "$chave" "$threads"
            if [ "$par" != '0 0' ]; then
                passo "compara-$alvo-$n-$chave-$threads" python3 port/x360/tools/divergencias.py \
                    "$I/lista-$alvo-$n-0-0.txt" "$I/lista-$alvo-$n-$chave-$threads.txt" --normal
            fi
        done
    done
done
passo save-mc port/x360/tools/save_mc.sh
passo save-mc-julgamento python3 port/x360/tools/f5_final_confere.py save-mc port/build/save_mc
passo save-entre env O="$I/save-entre" port/x360/tools/save_entre.sh
passo som-ambiente python3 port/x360/tools/som_ambiente.py --mutantes
passo som-amostras env T="$I/som-amostras" port/x360/tools/som.sh amostras
passo som-fluxo env T="$I/som-fluxo" port/x360/tools/som.sh fluxo
passo som-controles env O="$I/roteiros" T="$I/som-controles" port/x360/tools/som.sh controles f5_duelo
passo som360 port/x360/tools/teste_som360.sh
passo diario port/x360/tools/teste_diario.sh --mutantes
passo envio port/x360/tools/teste_envio.sh
passo tela port/x360/tools/teste_tela.sh
passo filmes port/x360/tools/teste_blocos.sh
modelo() {
    local nome=$1; shift
    gcc -std=c89 -pedantic -Wall -Wextra -Werror -Iport/x360/src/gs "port/x360/tools/teste_$nome.c" -lm -o "$I/teste_$nome"
    "$I/teste_$nome"
    local rc=0
    "$I/teste_$nome" --estraga > "$I/$nome-estragado.log" 2>&1 || rc=$?
    [ "$rc" = 1 ]
    if [ "$nome" = afiado ]; then
        rc=0
        "$I/teste_$nome" --sem-caixa > "$I/afiado-sem-caixa.log" 2>&1 || rc=$?
        [ "$rc" = 1 ]
    fi
}
for nome in escala afiado fxaa; do passo "modelo-$nome" modelo "$nome"; done
for alvo in x86 ppc; do
    livre
    pa "INÍCIO clut16-$alvo (trava própria)"
    ALVO=$alvo port/x360/tools/teste_clut16.sh 2>&1 | tee "$I/clut16-$alvo.log"
    pa "OK clut16-$alvo"
done
for w in "$I/base" "$I/final"; do
    [ -z "$(git -C "$w" status --porcelain)" ]
done
pa 'Árvores limpas; resultados preservados fora do git. Nenhum aceite do console foi inferido.'
