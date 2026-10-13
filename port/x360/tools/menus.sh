#!/usr/bin/env bash
# F3, etapa 3 (no WSL, na raiz): os menus sem BT3_DEMO em x86 e na PPC sem mapa, com a memória comparada num quadro e as
# lutas das demos comparadas quadro a quadro. Pede as builds x86 e ppc (headless.py --alvo x86|ppc).
#   Q=3000     vblank do dump de memória (no título; a primeira demo começa no 4433)
#   ATE=30000  até onde a x86 roda para listar os arquivos lidos (os que a PPC precisa convertidos)
#   TP=1800    segundos da PPC no qemu (uns 20 vblanks por segundo)
set -uo pipefail
B=port/build; Q=${Q:-3000}; ATE=${ATE:-30000}
# BT3_MENU_ITEM4=0: o Dragon Net Battle fora do menu principal, como no 360 (posix360.c), para os roteiros de
# controle andarem igual nas três builds (decisão 22)
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p $B/menus
rm -f $B/menus/q_x86.txt* $B/menus/q_ppc.txt* $B/menus/run_ppc.txt $B/menus/abertos.txt
# os arquivos que a x86 lê na rodada inteira (o rastreio para antes: a segunda demo sorteia outra tela de carregamento,
# e um arquivo fora da lista chegaria cru à PPC). gdb com dprintf em Port_FileReadAt (o WSL não tem strace)
printf 'set pagination off\nset confirm off\ndprintf Port_FileReadAt,"LEU %%s\\n", rel\nbreak Port_StateLog if vblank >= %d\nrun\nkill\nquit\n' "$ATE" > $T/lista.gdb
BT3_SAVES=$T/l BT3_HASH=$T/l.txt timeout 900 gdb -batch -nx -x $T/lista.gdb --args $B/x86/bt3_x86 2>/dev/null |
    grep '^LEU ' | cut -d' ' -f2 | grep -v '^disc/' | sort -u > $B/menus/abertos.txt
echo "x86 leu $(wc -l < $B/menus/abertos.txt) arquivos até o vblank $ATE"
BT3_SAVES=$T/x BT3_HASH_AT=$Q BT3_HASH=$B/menus/q_x86.txt timeout ${TX:-120} $B/x86/bt3_x86 > $B/menus/run_x86.txt 2>&1
gcc -std=c89 -O2 -w -o $B/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1  # (conv_jogo.c é a ponte do jogo)
rm -rf $B/dados_menus $B/menus/marcas && mkdir -p $B/dados_menus/mods $B/menus/marcas
for d in gamedata/*; do ln -s "$(realpath "$d")" $B/dados_menus/; done
$B/conv_tool --saida $B/dados_menus/mods gamedata $(cat $B/menus/abertos.txt) | grep -v "sem conversor$" |
    grep -v ': convertido; cobertura [0-9]* de [0-9]* bytes (100.0%)'
touch $B/dados_menus/mods/convertidos.txt
echo "convertidos: $(wc -l < $B/dados_menus/mods/convertidos.txt)"
BT3_SAVES=$T/p BT3_HASH_AT=$Q BT3_DATA=$B/dados_menus BT3_CONVERTIDOS=$B/dados_menus/mods/convertidos.txt \
    BT3_HASH=$B/menus/q_ppc.txt timeout ${TP:-1800} qemu-ppc $B/ppc/bt3_ppc > $B/menus/run_ppc.txt 2>&1
echo "ppc saiu com $? (124: tempo esgotado) no vblank $(tail -2 $B/menus/q_ppc.txt | head -1 | cut -d' ' -f1)"
grep -v -e '^inverter: [0-9]* arquivos' -e 'sem mapa' $B/menus/run_ppc.txt | tail -3
python3 port/x360/tools/compara.py $B/menus/q_x86.txt $B/menus/q_ppc.txt --big | head -3
# memória no quadro Q, número a número nos blocos que guardam arquivo. Os índices dos blocos são os do vblank 3000
# (campos.py --donos diz quem alocou cada um); em outro quadro, confira antes
M=$B/menus/marcas
$B/conv_tool --marcas $M gamedata pzs3us1/00001.bin pzs3us1/00002.bin pzs3us1/00003.bin pzs3us1/00004.bin \
    pzs3us1/00449.bin 'pzs3us1/00449.bin@0x40#bpe' pzs3us1/00477.bin 'pzs3us1/00477.bin@0x0#bpe' > /dev/null
A=()
[ "$Q" = 3000 ] && A=(--marcas "heap#13=$M/pzs3us1_00004.bin.marcas" --marcas "heap#14=$M/pzs3us1_00001.bin.marcas"
    --marcas "heap#15=$M/pzs3us1_00002.bin.marcas" --marcas "heap#16=$M/pzs3us1_00003.bin.marcas"
    --marcas "heap#31=$M/pzs3us1_00449.bin.marcas" --marcas "heap#33=$M/pzs3us1_00449.bin@0x40#bpe.marcas"
    --marcas "heap#69=$M/pzs3us1_00477.bin.marcas" --marcas "heap#70=$M/pzs3us1_00477.bin@0x0#bpe.marcas")
# sem os bytes (o memdiff sem --resumo imprime hexa): só regiões, deslocamentos e classes
python3 port/x360/tools/memdiff.py $B/menus/q_x86.txt.dump $B/menus/q_ppc.txt.dump $B/x86/bt3_x86 $B/ppc/bt3_ppc \
    "${A[@]}" --max 400 | sed 's/  x86 .*//' > $B/menus/memdiff.txt
grep -E '^(heap|globais|região|total)' $B/menus/memdiff.txt
