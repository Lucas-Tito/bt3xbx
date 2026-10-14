#!/usr/bin/env bash
# F4, etapa 8 (no WSL, na raiz): o save entre builds, a x86 (little-endian, o port de PC) e a PPC (big-endian, o 360
# no qemu). Pede as builds x86 e ppc (headless.py) do commit de agora, sem nada fora de commit (o CARIMBO), e a árvore
# sem alteração; DESENVOLVIMENTO=1 roda assim mesmo e o fim diz "NÃO VALE COMO CRITÉRIO".
#   1. cada build cria o seu save do cartão vazio pelo roteiro opcoes (New Game, som em mono, salvar pelo McFlow; a PPC
#      sem mapa, só com os conversores, como no roteiros.sh SEMMAPA=1), e os dois se comparam campo a campo
#      (save_entre.py campos: os mesmos valores, cada um na ordem da sua build);
#   2. cada caso põe um cartão (o save de uma build, o da outra, ou um estragado: save_entre.py estraga) e roda uma
#      build com o roteiro que o resultado previsto pede: save_mono (som 1) ou save_zerado (som 0) se o save carrega,
#      save_danificado se não. A previsão sai do McCard_Checksum (save_entre.py preve: as palavras na ordem da build e,
#      na PPC, a regra do cabeçalho), e o rastro tem de cumpri-la sem queda (save_entre.py julga). No danificado, o
#      save que o New Game grava por cima tem de ser da ordem da build e carregar nela;
#   3. os casos de mesmo resultado nas duas builds têm de dar o mesmo rastro (roteiros.py compara).
# Os arquivos que a x86 lê nas rodadas (o gdb, linhas LEU) vão convertidos para a PPC (conv_tool --saida), como no
# save_mc.sh. Um emulador por vez (CLAUDE.md). Os saves ficam em $O (no WSL, nunca no git: são dado do usuário).
#   CARTAO=pasta   só a PPC com o save_mono sobre o cartão de outra origem (pasta com card1/BASLUS-21678DBZT3, como a
#                  que o save_console.sh leva-wsl monta com o save do console); pede os convertidos de uma rodada
#                  inteira em $O/dados
#   REUSA=1        não roda o jogo: refaz previsões e julgamentos sobre as saídas guardadas (para ver o verificador
#                  falhar numa cópia estragada: O=cópia)
#   O=pasta        as saídas (port/build/save_entre);   TP=1800   segundos de cada PPC no qemu
set -uo pipefail
B=port/build; O=${O:-$B/save_entre}; P=port/x360/tools/save_entre.py; R=port/x360/roteiros
DADOS=BASLUS-21678DBZT3
FALHAS=()
falhou() { FALHAS+=("$*"); echo "   >> FALHA: $*"; }
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0
mkdir -p $O

# as builds do commit de agora (como no roteiros.sh)
AGORA=$(git rev-parse --short HEAD); ORIGEM=()
for b in x86 ppc; do
    c=$(head -1 $B/$b/CARIMBO 2>/dev/null | cut -d' ' -f1,2)
    [ "$c" = "$AGORA diff:da39a3ee5e" ] || ORIGEM+=("$b é de \"${c:-sem carimbo}\", não do commit de agora ($AGORA) sem diferença")
done
[ -z "$(git status --porcelain --untracked-files=normal)" ] || ORIGEM+=("a árvore tem alterações fora de commit")
echo "x86 $(cat $B/x86/CARIMBO) | ppc $(cat $B/ppc/CARIMBO)"
if [ ${#ORIGEM[@]} -gt 0 ]; then
    printf '   >> origem: %s\n' "${ORIGEM[@]}"
    [ -n "${DESENVOLVIMENTO:-}" ] || { echo "== builds de outra origem: refaça x86 e ppc do HEAD (ou DESENVOLVIMENTO=1)"; exit 1; }
fi

roda() {  # build roteiro cartão saída [VAR=valor...]: uma rodada; o código de saída vai para saída.codigo
    local b=$1 rot=$2 c=$3 s=$4; shift 4
    if [ $b = x86 ]; then
        env "$@" BT3_ROTEIRO=$R/$rot.txt BT3_SAVES=$c timeout 900 $B/x86/bt3_x86 > $s 2>&1 < /dev/null
    else
        env "$@" BT3_DATA=$O/dados BT3_CONVERTIDOS=$O/dados/mods/convertidos.txt BT3_ROTEIRO=$R/$rot.txt BT3_SAVES=$c \
            timeout ${TP:-1800} qemu-ppc $B/ppc/bt3_ppc > $s 2>&1 < /dev/null
    fi
    echo $? > $s.codigo
    rm -f bt3_crash.txt
}
leu() {  # roteiro cartão: os arquivos que a x86 lê nessa rodada (gdb), para a conversão da PPC
    BT3_DESBLOQUEIO=1 BT3_ROTEIRO=$R/$1.txt BT3_SAVES=$2 timeout 900 gdb -batch -nx -ex 'set pagination off' \
        -ex 'dprintf Port_FileReadAt,"LEU %s\n", rel' -ex run --args $B/x86/bt3_x86 2>&1 < /dev/null |
        grep -a '^LEU ' | cut -d' ' -f2 | grep -v '^disc/' >> $O/lidos_todos.txt
}
roteiro_de() {  # previsão -> roteiro
    case "$1" in
        "carrega"*"som 1") echo save_mono ;;
        "carrega"*"som 0") echo save_zerado ;;
        danificado*) echo save_danificado ;;
        *) echo "" ;;
    esac
}
espera_de() { case "$1" in carrega*) echo carrega ;; *) echo danificado ;; esac; }

if [ -n "${CARTAO:-}" ]; then
    [ -s $O/dados/mods/convertidos.txt ] || { echo "sem $O/dados: rode antes o save_entre.sh inteiro"; exit 1; }
    k=$O/cartao; rm -rf $k; mkdir -p $k; cp -r "$CARTAO" $k/antes; cp -r $k/antes $k/cartao
    arq=$k/antes/card1/$DADOS/$DADOS
    python3 $P analisa $arq | sed 's/^/   /'
    prev=$(python3 $P preve $arq --build ppc); echo "   previsão na ppc: $prev"
    [ -n "$(roteiro_de "$prev")" ] || { echo "   >> previsão sem roteiro: $prev"; exit 1; }
    roda ppc "$(roteiro_de "$prev")" $k/cartao $k/saida.txt
    python3 $P julga $k/saida.txt --espera "$(espera_de "$prev")" --codigo "$(cat $k/saida.txt.codigo)" | sed 's/^/   /'
    exit ${PIPESTATUS[0]}
fi

# 1. cada build cria o seu save (roteiro opcoes, do cartão vazio)
CASOS=(
    "ppc_ppc ppc ppc -"            # o save da PPC na PPC (do 360 no 360)
    "x86_x86 x86 x86 -"            # a referência: o save da x86 na x86
    "x86_ppc ppc x86 -"            # o save de PC no big-endian
    "ppc_x86 x86 ppc -"            # o save da PPC no port de PC (só registra: a x86 não muda)
    "soma0_x86 x86 x86 soma0"      # o save de PC cuja soma tem o byte baixo 0: válido na x86...
    "soma0_ppc ppc x86 soma0"      # ...e recusado na PPC (a regra do cabeçalho; antes dela, carregava trocado)
    "truncado_ppc ppc ppc truncado" "truncado_x86 x86 x86 truncado"
    "zerado_ppc ppc ppc zerado"     "zerado_x86 x86 x86 zerado"
    "byte_ppc ppc ppc byte"         "byte_x86 x86 x86 byte"
    "bit_ppc ppc ppc bit"           "bit_x86 x86 x86 bit"
)
echo "== 1. os saves das duas builds (roteiro opcoes, do cartão vazio)"
if [ -z "${REUSA:-}" ]; then
    rm -rf $O/c_x86 $O/c_ppc $O/dados $O/lidos_todos.txt $O/k_*
    roda x86 opcoes $O/c_x86 $O/cria_x86.txt BT3_DESBLOQUEIO=1
    # os arquivos que a x86 lê: no opcoes e nos três roteiros de carga (com o seu save, zerado e o da PPC: o caminho
    # do danificado), do mesmo jeito que a PPC vai ler
    leu opcoes $O/l_vazio
    cp -r $O/c_x86 $O/l_mono; leu save_mono $O/l_mono
    python3 $P estraga $O/c_x86 $O/l_zero zerado > /dev/null; leu save_zerado $O/l_zero
    python3 $P estraga $O/c_x86 $O/l_dan truncado > /dev/null; leu save_danificado $O/l_dan
    rm -rf $O/l_*
    sort -u $O/lidos_todos.txt > $O/lidos.txt
    gcc -std=c89 -O2 -w -o $O/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
    mkdir -p $O/dados/mods; for d in gamedata/*; do ln -s "$(realpath "$d")" $O/dados/; done
    $O/conv_tool --saida $O/dados/mods gamedata $(cat $O/lidos.txt) > $O/conv.txt; r=$?
    touch $O/dados/mods/convertidos.txt
    [ $r = 0 ] || falhou "conv_tool --saida saiu com $r"
    roda ppc opcoes $O/c_ppc $O/cria_ppc.txt BT3_DESBLOQUEIO=1
fi
echo "   convertidos para a PPC: $(wc -l < $O/dados/mods/convertidos.txt) de $(wc -l < $O/lidos.txt) arquivos lidos pela x86"
for b in x86 ppc; do
    s=$O/cria_$b.txt
    echo "   $b: saiu com $(cat $s.codigo); $(grep -a '^roteiro: .* fim \|FALHA' $s | head -1); $(grep -ac '^mc: gravou ' $s) linhas \"mc: gravou\""
    { [ "$(cat $s.codigo)" = 0 ] && grep -aq '^roteiro: .* fim ' $s && ! grep -aq FALHA $s; } || falhou "$b não criou o save pelo opcoes"
    echo "   cartão da $b: $(cd $O/c_$b/card1 2>/dev/null && find . -type f -printf '%P:%s ' | sort | tr '\n' ' ')"
    [ -s $O/c_$b/card1/$DADOS/$DADOS ] || falhou "$b sem o arquivo de dados"
done
python3 port/x360/tools/roteiros.py telas $O/cria_x86.txt > $O/cria_x86.telas
python3 port/x360/tools/roteiros.py telas $O/cria_ppc.txt > $O/cria_ppc.telas
echo "   rastro do opcoes, PPC contra x86: $(python3 port/x360/tools/roteiros.py compara $O/cria_x86.telas $O/cria_ppc.telas | tr '\n' ' ')"
python3 port/x360/tools/roteiros.py compara $O/cria_x86.telas $O/cria_ppc.telas > /dev/null || falhou "opcoes: rastro da PPC diferente"
for b in x86 ppc; do
    python3 $P analisa $O/c_$b/card1/$DADOS/$DADOS | sed -n '2,5p' | sed "s/^/   $b /"
done
echo "== 2. formato: o save da x86 e o da PPC campo a campo"
python3 $P campos $O/c_x86 $O/c_ppc | sed 's/^/   /'
[ ${PIPESTATUS[0]} = 0 ] || falhou "campos: o save da PPC com outros valores que o da x86"

echo "== 3. os casos"
declare -A RES
for caso in "${CASOS[@]}"; do
    read -r n b fonte estrago <<< "$caso"
    k=$O/k_$n
    if [ -z "${REUSA:-}" ]; then
        rm -rf $k; mkdir -p $k
        if [ "$estrago" = - ]; then cp -r $O/c_$fonte $k/antes; else python3 $P estraga $O/c_$fonte $k/antes $estrago > /dev/null; fi
        cp -r $k/antes $k/cartao
    fi
    arq=$k/antes/card1/$DADOS/$DADOS
    prev=$(python3 $P preve $arq --build $b); rot=$(roteiro_de "$prev"); esp=$(espera_de "$prev")
    RES[$n]=$esp
    rotulo="o save da $fonte"; [ "$estrago" = - ] || rotulo="$rotulo, $estrago"
    echo "-- $n ($rotulo, na $b): previsto $prev"
    [ -n "$rot" ] || { falhou "$n: previsão sem roteiro ($prev)"; continue; }
    [ -n "${REUSA:-}" ] || roda $b $rot $k/cartao $k/saida.txt
    python3 $P julga $k/saida.txt --espera $esp --codigo "$(cat $k/saida.txt.codigo 2>/dev/null || echo 999)" | sed 's/^/   /'
    [ ${PIPESTATUS[0]} = 0 ] || falhou "$n: o rastro não cumpre a previsão ($esp) ou caiu"
    grep -a '^mc: leu ' $k/saida.txt | head -1 | sed 's/^/   /'
    python3 port/x360/tools/roteiros.py telas $k/saida.txt > $k/telas.txt
    if [ "$esp" = danificado ]; then
        # o New Game gravou por cima um save da ordem desta build, que ela mesma carrega
        novo=$k/cartao/card1/$DADOS/$DADOS
        dep=$(python3 $P preve $novo --build $b)
        quem=$(python3 $P analisa $novo | grep 'gravado por' | sed 's/.*gravado por: //')
        echo "   save gravado por cima pelo New Game: $quem; na $b: $dep"
        case "$b:$quem" in x86:little*|ppc:big*) ;; *) falhou "$n: o New Game gravou um save de outra ordem ($quem)" ;; esac
        [[ "$dep" == carrega* ]] || falhou "$n: o save novo não carregaria ($dep)"
    fi
done

echo "== 4. o mesmo resultado, o mesmo rastro"
PARES=("x86_x86 ppc_ppc" "ppc_x86 x86_ppc" "x86_ppc soma0_ppc")
for e in truncado zerado byte bit; do
    [ "${RES[${e}_x86]:-}" != "${RES[${e}_ppc]:-}" ] || PARES+=("${e}_x86 ${e}_ppc")
    [ "${RES[${e}_x86]:-}" = "${RES[${e}_ppc]:-}" ] || echo "   $e: x86 ${RES[${e}_x86]:-?}, ppc ${RES[${e}_ppc]:-?} (somas diferentes: sem par)"
done
for par in "${PARES[@]}"; do
    read -r a c <<< "$par"
    out=$(python3 port/x360/tools/roteiros.py compara $O/k_$a/telas.txt $O/k_$c/telas.txt); r=$?
    echo "   $a contra $c: $(tr '\n' ' ' <<< "$out")"
    [ $r = 0 ] || falhou "rastros diferentes: $a e $c"
done
rm -f bt3_crash.txt
if [ ${#FALHAS[@]} -gt 0 ]; then
    echo "== ${#FALHAS[@]} falha(s):"; printf '   %s\n' "${FALHAS[@]}"
    [ ${#ORIGEM[@]} -eq 0 ] || echo "== NÃO VALE COMO CRITÉRIO: $(printf '%s; ' "${ORIGEM[@]}")"
    exit 1
fi
[ ${#ORIGEM[@]} -eq 0 ] || { echo "== tudo certo, mas NÃO VALE COMO CRITÉRIO (DESENVOLVIMENTO=1): $(printf '%s; ' "${ORIGEM[@]}")"; exit 0; }
echo "== tudo certo"
