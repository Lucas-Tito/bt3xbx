#!/usr/bin/env bash
# F5, etapa 8 (no WSL, na raiz): o perfil da gravação da VU1 (o tempo dentro do GsVu1_Call) e a prova de que uma mudança
# nela não mudou a lista que o backend recebe. Pede as builds com --gpu (headless.py --gpu: x86g e ppcg) do commit de agora
# e, para a PPC, a pasta de dados do roteiro (port/build/roteiros/ROTEIRO/dados, que o roteiros.sh deixa).
# FONTE=arquivo é o gs_draw.c de que a build ppcg guardada saiu (git show COMMIT:port/src/gs/gs_draw.c), para as linhas do perfil.
# X86=x86gv usa a build com --vu1 (a otimização na x86) em vez da x86g.
#   vu1_perfil.sh x86 ROTEIRO [SAIDA]        a rodada na x86g: família, tiques e amostras por função/linha (vu1_perfil.py x86)
#   vu1_perfil.sh lista x86|ppc ROTEIRO LOG  a rodada que escreve a lista de cada quadro (hash por seção; BT3_GPU_LISTA=1)
#   vu1_perfil.sh ppc ROTEIRO A B [SAIDA]    as instruções executadas pela PPC no qemu nos quadros do GS de A a B-1
#                                            (-d exec, uma instrução por bloco, só as funções de vu1_perfil.py faixas)
#   vu1_perfil.sh compara LOG1 LOG2          as linhas "gpu_falso: quadro" de dois logs: iguais ou onde divergem
#                                            (SECOES=2,4,5,6 compara só as seções da VU1, contadas de 0: a lista de desenhos, os
#                                            vértices, os índices e as constantes da VU1; a 3, a dos vértices de
#                                            CPU, fica de fora)
set -uo pipefail
export DEBUGINFOD_URLS=
B=port/build; V=${V:-$HOME/perfil}; P=${PPCG:-$B/ppcg} # PPCG=pasta de uma build ppcg guardada (a "antes" de uma otimização)
mkdir -p $V # (sem ela o BT3_SAVES aponta para uma pasta que não se cria e o boot para em "600 quadros sem mcflow 15", guia, seção 6)
amb() { export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1 BT3_UNCAPPED=1 BT3_NOSOUND=1; }
modo=$1; shift
case $modo in
x86)
    r=$1; out=${2:-$V/x86g_perfil.txt}; amb
    rm -rf $V/sv_x86
    BT3_PERFIL=${PERFIL:-3} BT3_PERFIL_US=${US:-50} BT3_PERFIL_SAIDA=$out.bruto BT3_ROTEIRO=port/x360/roteiros/$r.txt \
        BT3_SAVES=$V/sv_x86 timeout 900 $B/${X86:-x86g}/bt3_${X86:-x86g} > $out.log 2>&1
    rm -rf $V/sv_x86
    python3 port/x360/tools/vu1_perfil.py x86 $out.bruto --linhas ${LINHAS:-25} | tee $out ;;
lista)
    b=$1; r=$2; out=$3; amb; export BT3_GPU_LISTA=1
    rm -rf $V/sv_l
    if [ $b = x86 ]; then
        BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$V/sv_l timeout 900 $B/${X86:-x86g}/bt3_${X86:-x86g} > $out 2>&1
    else
        D=$B/roteiros/$r
        BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_LEU=1 BT3_ROTEIRO=port/x360/roteiros/$r.txt \
            BT3_SAVES=$V/sv_l timeout ${TP:-3600} qemu-ppc $P/bt3_ppcg > $out 2>&1
    fi
    rm -rf $V/sv_l
    grep -a '^gpu_falso: [0-9]* quadros' $out; grep -a 'FALHA' $out | head -3 ;;
ppc)
    r=$1; A=$2; Z=$3; out=${4:-$V/ppcg_contagem.txt}; amb
    F=$(python3 port/x360/tools/vu1_perfil.py faixas --prog $P/bt3_ppcg)
    D=$B/roteiros/$r
    rm -rf $V/sv_p $V/fifo; mkfifo $V/fifo
    # (o log do qemu é uma linha por instrução: conta por endereço em quem lê, sem guardar o log)
    awk '{ if (match($0, /\[[0-9a-f]+\/[0-9a-f]+\//)) { s = substr($0, RSTART, RLENGTH); sub(/^\[[0-9a-f]+\//, "", s); sub(/\/$/, "", s); c[s]++ } }
         END { for (k in c) print k, c[k] }' < $V/fifo > $out &
    AW=$!
    BT3_PERFIL_JANELA=$A,$Z BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_LEU=1 \
        BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$V/sv_p timeout ${TP:-7200} qemu-ppc -d exec,nochain -one-insn-per-tb \
        -dfilter "$F" -D $V/fifo $P/bt3_ppcg > $V/ppcg_contagem.log 2>&1
    wait $AW; rm -rf $V/sv_p $V/fifo
    grep -a 'gpu_falso: fim da janela' $V/ppcg_contagem.log || { echo "a janela não terminou"; exit 1; }
    python3 port/x360/tools/vu1_perfil.py ppc $out --prog $P/bt3_ppcg ${FONTE:+--fonte $FONTE} --linhas ${LINHAS:-30} ;;
compara)
    a=$1; b=$2
    f() { grep -a '^gpu_falso: quadro' $1 | if [ -n "${SECOES:-}" ]; then
              awk -v s="$SECOES" 'BEGIN{n=split(s,k,",")} {o=$3; for(i=1;i<=n;i++) o=o" "$(NF-6+k[i]); print o}'; else cat; fi; }
    na=$(f $a | wc -l); nb=$(f $b | wc -l)
    if [ "$na" -gt 0 ] && [ "$na" = "$nb" ] && diff <(f $a) <(f $b) > /dev/null; then echo "iguais: $na quadros"; else
        echo "DIFERENTES: $na e $nb quadros; os primeiros:"; diff <(f $a) <(f $b) | head -6 | cut -c1-200; exit 1; fi ;;
*) echo "modo?"; exit 2 ;;
esac
