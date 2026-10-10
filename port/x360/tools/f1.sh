#!/usr/bin/env bash
# F1, ciclo inteiro (no WSL, na raiz do repositório): referência x86, rastreio, mapa, PowerPC com o mapa, e as
# comparações. Cada passo para o ciclo se falhar.
#   F1_QUADRO=4428   quadro do dump de memória comparado no fim (o último da luta de demonstração)
#   F1_SEM_BUILD=1   não recompila (só roda e compara)
#   F1_SEM_RASTREIO=1  reaproveita o rastreio e o mapa que já estão em port/build (o rastreio leva ~1 h)
set -euo pipefail
export BT3_DEMO=1 BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1
Q=${F1_QUADRO:-4428}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
B=port/build
if [ -z "${F1_SEM_BUILD:-}" ]; then
    python3 port/x360/tools/headless.py --alvo x86 | grep -E "FALHOU|objetos|link|AVISO"
    if [ -z "${F1_SEM_RASTREIO:-}" ]; then
        rm -f $B/x86r/rastreio.tsv
        python3 port/x360/tools/headless.py --alvo x86 --rastreio | grep -E "FALHOU|objetos|link|AVISO"
    fi
fi
# referência; o rastreio com o endereço da pilha fixo (setarch -R), para o resultado não depender do ASLR
BT3_SAVES=$T/x86 BT3_HASH=$B/x86/hash.txt timeout 900 $B/x86/bt3_x86 > $B/x86/run.txt 2>&1
if [ -z "${F1_SEM_RASTREIO:-}" ]; then
    BT3_SAVES=$T/x86r BT3_HASH=$B/x86r/hash.txt BT3_VISOES=$B/x86r/visoes.tsv BT3_RASTREIO=$B/x86r/rastreio.tsv \
        timeout 5400 setarch -R $B/x86r/bt3_x86r > $B/x86r/run.txt 2>&1
    grep -E '^(rastreio|visões):' $B/x86r/run.txt
    python3 port/x360/tools/rastreio.py mapa $B/x86r/rastreio.tsv -o $B/mapa.tsv --prog $B/x86r/bt3_x86r
fi
[ -n "${F1_SEM_BUILD:-}" ] || python3 port/x360/tools/headless.py --alvo ppc
# conversores por formato (port/x360/conv): os arquivos do registro, convertidos e conferidos contra o mapa, numa
# raiz de dados só da PPC (links para gamedata + mods/ com os convertidos; a x86 segue lendo o original)
gcc -std=c89 -O2 -Wall -o $B/conv_tool port/x360/conv/*.c  # conv_jogo.c entra junto; não atrapalha
rm -rf $B/dados_ppc && mkdir -p $B/dados_ppc/mods
for d in gamedata/*; do ln -s "$(realpath "$d")" $B/dados_ppc/; done
# os arquivos que a luta lê (do rastreio) e que algum conversor conhece (conv_tool diz "sem conversor" aos outros)
$B/conv_tool --mapa $B/mapa.tsv --saida $B/dados_ppc/mods gamedata \
    $(cut -f1 $B/x86r/rastreio.tsv | grep -v -e '#bpe' -e '^ELF:' | sort -u) | grep -v "sem conversor$" ||
    echo "CONVERSORES: falha ou discordância"
touch $B/dados_ppc/mods/convertidos.txt
export BT3_CONVERTIDOS=$B/dados_ppc/mods/convertidos.txt
echo "carimbos: x86 $(cat $B/x86/CARIMBO) | x86r $(cat $B/x86r/CARIMBO) | ppc $(cat $B/ppc/CARIMBO) | $(cat $B/ppc/ENTRADAS)"
echo "mapa sha1:$(sha1sum < $B/mapa.tsv | cut -c1-10)"
BT3_SAVES=$T/ppc BT3_DATA=$B/dados_ppc BT3_MAPA=$B/mapa.tsv BT3_HASH=$B/ppc/hash.txt timeout 1800 qemu-ppc $B/ppc/bt3_ppc > $B/ppc/run.txt 2>&1 ||
    { echo "ppc saiu com $?"; tail -5 $B/ppc/run.txt; }
python3 port/x360/tools/compara.py $B/x86/hash.txt $B/ppc/hash.txt --big --ref-rastreio $B/x86r/hash.txt || true
# memória no último quadro
# memória no último quadro (os dumps ficam em port/build/<alvo>/q.txt.dump para olhar com memdiff.py sem --resumo)
BT3_SAVES=$T/mx BT3_HASH_AT=$Q BT3_HASH=$B/x86/q.txt timeout 900 $B/x86/bt3_x86 > /dev/null 2>&1
BT3_SAVES=$T/mp BT3_HASH_AT=$Q BT3_DATA=$B/dados_ppc BT3_MAPA=$B/mapa.tsv BT3_HASH=$B/ppc/q.txt timeout 1800 qemu-ppc $B/ppc/bt3_ppc > /dev/null 2>&1 || true
python3 port/x360/tools/memdiff.py $B/x86/q.txt.dump $B/ppc/q.txt.dump $B/x86/bt3_x86 $B/ppc/bt3_ppc --resumo
python3 port/x360/tools/visoes.py $B/x86r/visoes.tsv $B/x86r/bt3_x86r --linhas > $B/x86r/visoes.txt
echo "visões: $(wc -l < $B/x86r/visoes.txt) combinações em $B/x86r/visoes.txt"
