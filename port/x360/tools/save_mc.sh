#!/usr/bin/env bash
# F4, etapa 3 (no WSL, na raiz): a gravação atômica do cartão (port/src/plat_mc.c, PORT_MC_ATOMIC, decisão 22).
# Pede as builds x86 e ppc (headless.py --alvo x86|ppc).
#   1. teste_mc (port/x360/headless/teste_mc.c) na x86 e na PPC (qemu): os casos da camada, com quedas simuladas;
#   2. a x86 do jogo até o MainMenu_Init, com um controle sintético que responde "sim" ao "criar o save?" do New Game
#      (BT3_PAD_PLAY: START, ESQUERDA e X num ciclo de 120 leituras; o de 90 do guarda_menu.sh chega ao "sim" tarde e
#      segue sem save): cartão vazio (cria o save) e de novo com esse save (o boot o carrega); os arquivos lidos nas
#      duas vão para a conversão da PPC. O gdb registra a sequência de estados do McFlow (gMcFlow->state e msg);
#   3. a PPC no qemu, sem mapa, com o gdb do qemu parado no MainMenu_Init e a mesma sequência de estados, que tem de
#      ser igual à da x86 (A com a rodada 1, B1 e B2 com a rodada 2):
#      A  cartão vazio: cria o save pelo caminho atômico (as linhas "mc: gravou" no log);
#      F  cartão vazio com BT3_MC_FALHA=escrita no arquivo de dados: a gravação falha, o jogo mostra o erro do cartão
#         (estado 0x11) e segue, sem queda;
#      B1 o save de A com um .tmp truncado ao lado: o boot carrega o save (estados 0x13 e 0x14) e apaga o .tmp;
#      B2 o save de A como se a energia caísse entre apagar e trocar o nome (só o .new e um .tmp): carrega o save.
#   TP=2400  segundos de cada PPC no qemu
set -uo pipefail
B=port/build; G=$B/save_mc
DADOS=BASLUS-21678DBZT3
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0
rm -rf $G && mkdir -p $G/dados/mods

echo "== 1. teste_mc"
gcc -std=gnu99 -O2 -w -DPORT_MC_ATOMIC -Iport/src -o $G/teste_x86 port/src/plat_mc.c port/x360/headless/teste_mc.c || exit 1
powerpc-linux-gnu-gcc -static -std=gnu99 -O2 -w -fsigned-char -DPORT_BIG_ENDIAN -Iport/src -o $G/teste_ppc \
    port/src/plat_mc.c port/x360/headless/teste_mc.c || exit 1
for a in x86 ppc; do
    mkdir -p $G/t_$a
    if [ $a = x86 ]; then $G/teste_x86 $G/t_$a > $G/t_$a.txt 2> $G/t_$a.log; else qemu-ppc $G/teste_ppc $G/t_$a > $G/t_$a.txt 2> $G/t_$a.log; fi
    echo "$a: $(tail -1 $G/t_$a.txt) em $(grep -c '' $G/t_$a.txt | awk '{print $1 - 1}') casos"; grep FALHOU $G/t_$a.txt
done

echo "== 2. x86 do jogo"
python3 - "$G/pad.bin" <<'PY2'
import sys
ocioso = bytes([0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80] + [0] * 12)
start, esq, x = (bytes([b0, b1]) + ocioso[2:] for b0, b1 in ((0xF7, 0xFF), (0x7F, 0xFF), (0xFF, 0xBF)))
out = bytearray()
for r in range(200000):
    k = r % 120
    out += start if k < 6 else esq if 90 <= k < 96 else x if 110 <= k < 116 else ocioso
open(sys.argv[1], "wb").write(out)
PY2
# a sequência de estados do McFlow (sem as repetições e sem o estado parado) e o vblank do MainMenu_Init
GDBCMD=(-ex 'set pagination off' -ex 'set confirm off'
        -ex 'dprintf McFlow_Start,"FLUXO %d\n", mode'
        -ex 'dprintf McFlow_Update,"ESTADO %x msg %x\n", gMcFlow->state, gMcFlow->msg'
        -ex 'dprintf Port_FileReadAt,"LEU %s\n", rel'
        -ex 'break MainMenu_Init')
FIM=(-ex 'print gPortVBlanks' -ex kill)
estados() { grep -a '^FLUXO\|^ESTADO' "$1" | grep -v '^ESTADO 0 \|^ESTADO 26 ' | uniq | tr '\n' ' '; }
for v in 1 2; do  # 1: cartão vazio; 2: o save da rodada 1
    BT3_PAD_PLAY=$G/pad.bin BT3_SAVES=$G/sx BT3_HASH=$G/hx$v.txt timeout 900 gdb -batch -nx "${GDBCMD[@]}" -ex run "${FIM[@]}" \
        --args $B/x86/bt3_x86 > $G/x86_$v.txt 2>&1
    estados $G/x86_$v.txt > $G/estados_x86_$v.txt
    echo "x86 rodada $v: MainMenu_Init no vblank $(grep -a '^\$1' $G/x86_$v.txt | cut -d' ' -f3); estados: $(cat $G/estados_x86_$v.txt)"
done
echo "   cartão da x86: $(cd $G/sx/card1 && find . -type f -printf '%P:%s ')"
cat $G/x86_1.txt $G/x86_2.txt | grep -a '^LEU ' | cut -d' ' -f2 | grep -v '^disc/' | sort -u > $G/abertos.txt
gcc -std=c89 -O2 -w -o $G/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
for d in gamedata/*; do ln -s "$(realpath "$d")" $G/dados/; done
$G/conv_tool --saida $G/dados/mods gamedata $(cat $G/abertos.txt) | grep -v "sem conversor$" |
    grep -v ': convertido; cobertura [0-9]* de [0-9]* bytes (100.0%)'
touch $G/dados/mods/convertidos.txt
echo "convertidos: $(wc -l < $G/dados/mods/convertidos.txt) arquivos lidos pela x86"

echo "== 3. PPC do jogo"
ppc() {  # nome porta [VAR=valor...]: a PPC com o cartão $G/c_nome, parada pelo gdb do qemu no MainMenu_Init
    local n=$1 p=$2; shift 2
    env "$@" BT3_PAD_PLAY=$G/pad.bin BT3_SAVES=$G/c_$n BT3_DATA=$G/dados BT3_CONVERTIDOS=$G/dados/mods/convertidos.txt \
        BT3_HASH=$G/h_$n.txt timeout ${TP:-2400} qemu-ppc -g $p $B/ppc/bt3_ppc > $G/ppc_$n.txt 2>&1 &
    sleep 2
    timeout $((${TP:-2400} + 60)) gdb-multiarch -batch -nx -ex "file $B/ppc/bt3_ppc" -ex "target remote :$p" \
        "${GDBCMD[@]}" -ex continue "${FIM[@]}" > $G/gdb_$n.txt 2>&1
    wait
}
resumo() {  # nome rodada-da-x86
    local n=$1
    estados $G/gdb_$n.txt > $G/estados_$n.txt
    echo "-- $n: MainMenu_Init no vblank $(grep -a '^\$1' $G/gdb_$n.txt | cut -d' ' -f3); estados: $(cat $G/estados_$n.txt)"
    if [ -n "${2:-}" ]; then
        cmp -s $G/estados_$n.txt $G/estados_x86_$2.txt && echo "   estados iguais aos da x86 (rodada $2)" ||
            echo "   estados DIFERENTES dos da x86 (rodada $2)"
    fi
    grep -a '^mc: ' $G/ppc_$n.txt | sed 's/^/   /'
    echo "   cartão: $(cd $G/c_$n/card1 2>/dev/null && find . -type f -printf '%P:%s ')"
    grep -a -m3 -i "abort\|Sprite_Unpack sem\|não implementado\|queda do port" $G/ppc_$n.txt | sed 's/^/   /'
}
mkdir -p $G/c_A $G/c_F
ppc A 12301 & ppc F 12302 BT3_MC_FALHA=escrita:$DADOS & wait
resumo A 1; resumo F
D=card1/$DADOS
[ -s $G/c_A/$D/$DADOS ] || { echo "A não criou o save"; exit 1; }
for n in B1 B2; do cp -r $G/c_A $G/c_$n; done
head -c 100 $G/c_A/$D/$DADOS > $G/c_B1/$D/$DADOS.tmp
mv $G/c_B2/$D/$DADOS $G/c_B2/$D/$DADOS.new
head -c 100 $G/c_A/$D/$DADOS > $G/c_B2/$D/$DADOS.tmp
ppc B1 12303 & ppc B2 12304 & wait
resumo B1 2; resumo B2 2
cmp -s $G/c_A/$D/$DADOS $G/c_B2/$D/$DADOS && echo "B2: o .new recuperado é o save de A, byte a byte"
rm -f bt3_crash.txt
