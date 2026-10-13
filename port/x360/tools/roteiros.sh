#!/usr/bin/env bash
# F4, etapa 4 (no WSL, na raiz): os roteiros de controle (port/x360/roteiros, BT3_ROTEIRO) contra as referências e
# contra o inventário. Pede as builds x86 e x86r (headless.py --alvo x86 [--rastreio]); com PPC=1, também a ppc.
#   roteiros.sh [NOME...]     (sem nomes: os do caminho essencial: duelo duelo_time treino opcoes; "todos": também
#                             historia ultimate torneio evolution datacenter referencia shenlong, da etapa 9)
# Para cada roteiro:
#   1. a x86 duas vezes: o rastro (as trocas de tela e as esperas cumpridas, com o vblank) tem de sair igual nas duas
#      e igual à referência guardada em ~/referencias/NOME.telas (fora do repositório; a primeira rodada a grava, e
#      REF=1 a regrava);
#   2. a x86 sob o gdb: cada Sprite_Unpack (vblank, modo, endereço de retorno), casado com a lista do unpacks.py;
#   3. a x86r com o rastreio (BT3_RASTREIO_BITS=24), o mapa (rastreio.py) e o conv_tool --mapa sobre o que a rodada
#      leu: o "sem conversor" por conteúdo tem de ser o das chamadas pendentes alcançadas (roteiros.py confere);
#   4. com PPC=1: a PPC no qemu com os arquivos que têm conversor convertidos, o mapa no resto (BT3_MAPA, como o
#      F1_COM_MAPA do f1.sh) e a guarda só anotando (BT3_GUARDA=anota): o rastro tem de ser o começo do da x86, e a
#      guarda nomeia a chamada sem conversor. Hoje a PPC para no Flash_Create do menu principal (o mapa não inverte os
#      registros do Flash, lidos com duas larguras: guia, seção 6): até o conversor dele (etapa 5), o passo 4 confere
#      do boot ao MainMenu_Init.
# No fim, as chamadas de todos os roteiros juntos contra o inventário (roteiros.py soma).
#   TP=3600  segundos da PPC no qemu
set -uo pipefail
B=port/build; O=$B/roteiros; REFS=~/referencias
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
NOMES=("$@"); [ ${#NOMES[@]} -gt 0 ] || NOMES=(duelo duelo_time treino opcoes)
[ "${NOMES[0]}" != todos ] || NOMES=(duelo duelo_time treino opcoes historia ultimate torneio evolution datacenter \
    referencia shenlong)
mkdir -p $O $REFS
gcc -std=c89 -O2 -w -o $O/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
cat > $O/unpack.gdb <<'G'
set pagination off
set confirm off
dprintf Port_FileReadAt,"LEU %s\n", rel
break *Sprite_Unpack
commands
silent
printf "UNPACK %u %d 0x%x\n", gPortVBlanks, *(int*)(*(int*)&gProgress + 0x18), *(int*)$esp
continue
end
run
G
echo "x86 $(cat $B/x86/CARIMBO) | x86r $(cat $B/x86r/CARIMBO)"
LOGS=()
for n in "${NOMES[@]}"; do
    D=$O/$n; rm -rf $D; mkdir -p $D
    R=port/x360/roteiros/$n.txt
    echo "== $n"
    for v in a b; do
        BT3_ROTEIRO=$R BT3_SAVES=$D/s$v BT3_HASH=$D/h$v.txt timeout 900 $B/x86/bt3_x86 > $D/x86_$v.txt 2>&1
        echo "   x86 ($v) saiu com $?: $(grep -a '^roteiro: .* fim \|FALHA' $D/x86_$v.txt | head -1)"
        python3 port/x360/tools/roteiros.py telas $D/x86_$v.txt > $D/telas_$v.txt
    done
    echo "   repetição: $(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_b.txt | tr '\n' ' ')"
    if [ ! -s $REFS/$n.telas ] || [ -n "${REF:-}" ]; then
        cp $D/telas_a.txt $REFS/$n.telas
        echo "   referência gravada: $REFS/$n.telas ($(grep -c '^tela ' $REFS/$n.telas) trocas de tela)"
    else
        echo "   referência: $(python3 port/x360/tools/roteiros.py compara $REFS/$n.telas $D/telas_a.txt | tr '\n' ' ')"
    fi
    grep '^tela ' $D/telas_a.txt | cut -d' ' -f2,4 | tr '\n' ' ' | sed 's/^/   telas (vblank): /'; echo
    BT3_ROTEIRO=$R BT3_SAVES=$D/sg BT3_HASH=$D/hg.txt timeout 900 gdb -batch -nx -x $O/unpack.gdb --args $B/x86/bt3_x86 \
        > $D/gdb.txt 2>&1
    LOGS+=($D/gdb.txt)
    echo "   Sprite_Unpack alcançados:"; python3 port/x360/tools/roteiros.py chamadas $D/gdb.txt --prog $B/x86/bt3_x86
    BT3_RASTREIO_BITS=24 BT3_ROTEIRO=$R BT3_SAVES=$D/sr BT3_HASH=$D/hr.txt BT3_RASTREIO=$D/rastreio.tsv \
        timeout 3600 setarch -R $B/x86r/bt3_x86r > $D/x86r.txt 2>&1
    echo "   x86r saiu com $?: $(grep -a '^rastreio:' $D/x86r.txt)"
    python3 port/x360/tools/roteiros.py telas $D/x86r.txt > $D/telas_r.txt
    echo "   x86r contra x86: $(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_r.txt | tr '\n' ' ')"
    # (o rastreio.py guarda tudo na memória: uns 5 GB com os 13 milhões de posições da Ultimate Battle; um roteiros.sh
    # por vez no WSL de 8 GB)
    if ! python3 port/x360/tools/rastreio.py mapa $D/rastreio.tsv -o $D/mapa.tsv --prog $B/x86r/bt3_x86r > $D/mapa.txt 2>&1; then
        echo "   FALHA: rastreio.py mapa saiu com erro (memória?): $(tail -1 $D/mapa.txt)"; continue
    fi
    # o que o rastreio viu ler como número e todo arquivo aberto (gdb): a PPC do passo 4 lê todos, e um que tem
    # conversor e chegasse sem conversão pararia o inverter.c (00001 a 00005 só se leem como número na luta)
    { cut -f1 $D/rastreio.tsv; grep -a '^LEU ' $D/gdb.txt | cut -d' ' -f2; } | grep -v -e '^ELF:' -e '^disc/' | sort -u > $D/lidos.txt
    # (com --saida: os arquivos que têm conversor, convertidos para a PPC do passo 4, como no f1.sh)
    mkdir -p $D/dados/mods; for d in gamedata/*; do ln -s "$(realpath "$d")" $D/dados/; done
    $O/conv_tool --mapa $D/mapa.tsv --saida $D/dados/mods gamedata $(cat $D/lidos.txt) > $D/conv.txt
    touch $D/dados/mods/convertidos.txt
    echo "   conv_tool --mapa saiu com $? ($(wc -l < $D/lidos.txt) arquivos e conteúdos lidos)"
    grep -v -e "sem conversor$" -e ': convertido; cobertura [0-9]* de [0-9]* bytes (100.0%)' \
        -e ' iguais, 0 com outra largura, 0 não trocadas$' $D/conv.txt | sed 's/^/   /' | head -10
    python3 port/x360/tools/roteiros.py confere $D/gdb.txt --prog $B/x86/bt3_x86 --conv $D/conv.txt | sed 's/^/   /'
    if [ -n "${PPC:-}" ]; then
        BT3_MAPA=$D/mapa.tsv BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_GUARDA=anota \
            BT3_ROTEIRO=$R BT3_SAVES=$D/sp BT3_HASH=$D/hp.txt timeout ${TP:-3600} qemu-ppc $B/ppc/bt3_ppc > $D/ppc.txt 2>&1
        echo "   ppc com mapa saiu com $?: $(grep -a '^roteiro: .* fim \|FALHA\|abort' $D/ppc.txt | head -1)"
        python3 port/x360/tools/roteiros.py telas $D/ppc.txt > $D/telas_p.txt
        echo "   ppc contra x86: $(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_p.txt | tr '\n' ' ')"
        grep -a '^guarda: ' $D/ppc.txt | sed 's/ (retorno.*//' | sort | uniq -c | sed 's/^/   /'
    fi
    rm -f bt3_crash.txt
done
echo "== todos juntos"
python3 port/x360/tools/roteiros.py soma "${LOGS[@]}" --prog $B/x86/bt3_x86
