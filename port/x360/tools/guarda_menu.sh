#!/usr/bin/env bash
# F4, etapa 2 (no WSL, na raiz): a guarda do Sprite_Unpack (port/x360/conv/conv_jogo.c) na PPC sem mapa, entrando no
# menu principal com o controle gravado. Pede as builds x86 e ppc (headless.py --alvo x86|ppc).
#   1. um controle sintético (BT3_PAD_PLAY, 18 bytes por leitura, botões ativos em 0): a cada 90 leituras START, depois
#      ESQUERDA (o diálogo do cartão começa no "não") e X; do cartão vazio ele entra no menu principal pelo New Game
#      SEM criar o save: a ESQUERDA chega antes de o "criar o save?" aceitar entrada, e o X responde "não" e "seguir
#      sem salvar" (para criar o save, o ciclo de 120 do save_mc.sh);
#   2. a x86 até o MainMenu_Init (gdb), listando os arquivos lidos, que a conv_tool converte para a PPC;
#   3. a PPC no qemu, sem mapa: com o menu principal ainda sem conversor, ela tem de parar com "Sprite_Unpack sem
#      conversor em MainMenu_Init", e o addr2line da chamada confere o nome.
# Histórico da etapa 2: desde a etapa 5a o menu principal tem conversor (menu_principal) e a PPC passa dele; o controle
# sintético deste teste não sai do menu. A PPC sem mapa pelos roteiros (roteiros.sh com SEMMAPA=1) o substitui, e a
# guarda nomeia lá o próximo bloqueio de cada roteiro.
#   TP=2400  segundos da PPC no qemu
set -uo pipefail
B=port/build; G=$B/guarda
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0
rm -rf $G && mkdir -p $G/dados/mods
python3 - "$G/pad.bin" <<'PY'
import sys
ocioso = bytes([0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80] + [0] * 12)
start, esq, x = (bytes([b0, b1]) + ocioso[2:] for b0, b1 in ((0xF7, 0xFF), (0x7F, 0xFF), (0xFF, 0xBF)))
out = bytearray()
for r in range(200000):
    k = r % 90
    out += start if k < 6 else esq if 30 <= k < 36 else x if 60 <= k < 66 else ocioso
open(sys.argv[1], "wb").write(out)
PY
printf 'set pagination off\nset confirm off\ndprintf Port_FileReadAt,"LEU %%s\\n", rel\nbreak MainMenu_Init\nrun\nprint gPortVBlanks\nkill\nquit\n' > $G/x86.gdb
BT3_PAD_PLAY=$G/pad.bin BT3_SAVES=$G/sx BT3_HASH=$G/hx.txt timeout 600 gdb -batch -nx -x $G/x86.gdb --args $B/x86/bt3_x86 > $G/x86.txt 2>&1
echo "x86: MainMenu_Init no vblank $(grep '^\$1' $G/x86.txt | cut -d' ' -f3)"
grep '^LEU ' $G/x86.txt | cut -d' ' -f2 | grep -v '^disc/' | sort -u > $G/abertos.txt
gcc -std=c89 -O2 -w -o $G/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
for d in gamedata/*; do ln -s "$(realpath "$d")" $G/dados/; done
$G/conv_tool --saida $G/dados/mods gamedata $(cat $G/abertos.txt) | grep -v "sem conversor$" |
    grep -v ': convertido; cobertura [0-9]* de [0-9]* bytes (100.0%)'
touch $G/dados/mods/convertidos.txt
echo "convertidos: $(wc -l < $G/dados/mods/convertidos.txt) arquivos lidos pela x86"
BT3_PAD_PLAY=$G/pad.bin BT3_SAVES=$G/sp BT3_DATA=$G/dados BT3_CONVERTIDOS=$G/dados/mods/convertidos.txt \
    BT3_HASH=$G/hp.txt timeout ${TP:-2400} qemu-ppc $B/ppc/bt3_ppc > $G/ppc.txt 2>&1
echo "ppc saiu com $? (134: abort, o fim esperado)"
rm -f bt3_crash.txt  # (o relatório de queda do port, que a falha da guarda também grava na raiz)
grep -m1 "Sprite_Unpack\|^conv:\|gancho" $G/ppc.txt
a=$(grep -o 'retorno 0x[0-9A-F]*' $G/ppc.txt | head -1 | cut -d' ' -f2)
if [ -n "$a" ]; then
    c=$(printf '0x%X' $((a - 4))); A2L=$(command -v powerpc-linux-gnu-addr2line || command -v addr2line)
    echo "addr2line da chamada ($c): $($A2L -f -e $B/ppc/bt3_ppc $c | tr '\n' ' ')"
fi
