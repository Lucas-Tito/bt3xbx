#!/usr/bin/env bash
# F6, etapa 2 (no WSL, na raiz): as redes do som que não precisam do roteiro inteiro, e os controles negativos de todas
# (plano/F6/02.md). Um ciclo de emulador por vez: rodar dentro da trava do WSL.
#   som.sh amostras        rede 3: constrói o teste_som da x86 e da PPC (headless.py --teste-som), faz o censo dos
#                          cabeçalhos e compara o PCM do port com a referência independente (som.py amostras); depois
#                          os cinco controles negativos (o c1 mais 1, um byte da amostra do banco, um byte da saída da
#                          PPC, um byte da saída da x86 logo depois da volta do laço, e a saída da x86 depois da volta
#                          trocada pela variante "continua", o port de antes da decisão 25.10), cada um tem de sair com 1
#                          e dizer o porquê
#   som.sh controles NOME  redes 2 e 4: sobre as saídas do roteiros.sh SOM=1 em $O/NOME (o som_a.txt; a x86a refeita
#                          aqui), o rastro estragado numa cópia (um volume trocado, uma linha a menos) tem de falhar no
#                          som.py rastro, e a x86a com cada estrago de propósito (BT3_SABOTA=pitch, pan, banco) e uma
#                          cópia da gravação com um byte trocado têm de falhar no som.py mixer
# Sai com 1 se qualquer passo não fizer o que devia (inclusive um controle que passou).
set -uo pipefail
B=port/build; O=${O:-$B/roteiros}; T=${T:-$B/som}
mkdir -p $T
FALHAS=()
falhou() { FALHAS+=("$*"); echo "   >> FALHA: $*"; }
# deve_falhar DESCRIÇÃO TEXTO-ESPERADO comando...: o comando tem de sair com 1 e o texto esperado tem de aparecer
deve_falhar() {
    local d=$1 t=$2; shift 2
    local s; s=$("$@" 2>&1); local r=$?
    if [ $r = 1 ] && grep -q -- "$t" <<< "$s"; then
        echo "   controle '$d': falhou como devia ($(grep -m1 -- "$t" <<< "$s" | sed 's/^ *//' | cut -c1-150))"
    else
        falhou "controle '$d' não acusou (saiu com $r): $(tail -2 <<< "$s" | tr '\n' ' ' | cut -c1-200)"
    fi
}
case "${1:-}" in
amostras)
    for a in x86 ppc; do
        python3 port/x360/tools/headless.py --alvo $a --teste-som | tail -1
        [ -x $B/${a}t/teste_som ] || { echo "sem $B/${a}t/teste_som"; exit 1; }
    done
    echo "== censo"
    python3 port/x360/tools/som.py censo gamedata | sed 's/^/   /'
    echo "== rede 3: o port contra a referência e a x86 contra a PPC"
    python3 port/x360/tools/som.py amostras --dados gamedata --x86 $B/x86t/teste_som --ppc $B/ppct/teste_som --tmp $T/amostras \
        > $T/amostras.txt
    r=$?; sed 's/^/   /' $T/amostras.txt
    [ $r = 0 ] || falhou "rede 3: amostras diferentes (som.py amostras)"
    echo "== rede 3: controles negativos"
    for e in adx vag ppc laco continua; do
        case $e in adx) t="x86 diferente da referência";; vag) t="amostras do banco diferentes";; ppc) t="PPC diferente da x86";;
            laco|continua) t="depois da volta do laço, x86 diferente";; esac
        deve_falhar "estraga $e" "$t" python3 port/x360/tools/som.py amostras --dados gamedata --x86 $B/x86t/teste_som \
            --ppc $B/ppct/teste_som --tmp $T/controle --estraga $e
    done
    ;;
controles)
    n=${2:?nome do roteiro}; D=$O/$n; R=port/x360/roteiros/$n.txt
    [ -s $D/som_a.txt ] || { echo "sem $D/som_a.txt: rode antes o roteiros.sh com SOM=1"; exit 1; }
    echo "== rede 2: o rastro estragado numa cópia"
    sed '0,/ vol p[0-9]* -\?[0-9]*/s// vol p0 12345/' $D/som_a.txt > $T/som_vol.txt
    python3 - "$D/som_a.txt" "$T/som_vol.txt" <<'P' || falhou "o estrago do volume não casou"
import sys; a, b = (open(x).read() for x in sys.argv[1:]); assert a != b and " vol p0 12345" in b
P
    deve_falhar "rastro com um volume trocado" "diferente na linha" python3 port/x360/tools/som.py rastro $D/som_a.txt $T/som_vol.txt
    awk 'NR == 1 || !/ se tipo 0 / || f++' $D/som_a.txt > $T/som_menos.txt
    [ "$(wc -l < $T/som_menos.txt)" -lt "$(wc -l < $D/som_a.txt)" ] || falhou "o estrago da linha a menos não casou"
    deve_falhar "rastro sem um efeito tocado" "diferente na linha" python3 port/x360/tools/som.py rastro $D/som_a.txt $T/som_menos.txt
    : > $T/som_vazio.txt
    deve_falhar "rastro vazio" "não prova nada" python3 port/x360/tools/som.py rastro $T/som_vazio.txt $T/som_vazio.txt
    echo "== rede 4: o gravador com estragos de propósito (x86a)"
    rm -rf $T/s0  # (o cartão começa vazio: o boot.txt faz o New Game)
    BT3_SOUND=1 BT3_GRAVADOR=$T/grav_ok.bin BT3_ROTEIRO=$R BT3_SAVES=$T/s0 timeout 900 $B/x86a/bt3_x86a > $T/x86a_ok.txt 2>&1
    rm -rf $T/s0
    for e in pitch pan banco; do
        rm -rf $T/s_$e
        BT3_SABOTA=$e BT3_SOUND=1 BT3_GRAVADOR=$T/grav_$e.bin BT3_ROTEIRO=$R BT3_SAVES=$T/s_$e timeout 900 $B/x86a/bt3_x86a \
            > $T/x86a_$e.txt 2>&1
        grep -q "ESTRAGO DE PROPÓSITO: $e" $T/x86a_$e.txt || falhou "o estrago $e não ligou"
        deve_falhar "gravador com $e estragado" "FALHA: bloco" python3 port/x360/tools/som.py mixer $T/grav_ok.bin $T/grav_$e.bin
        rm -rf $T/s_$e $T/grav_$e.bin
    done
    python3 - "$T/grav_ok.bin" "$T/grav_hash.bin" <<'P' || falhou "o estrago do hash não casou"
import sys, struct
b = bytearray(open(sys.argv[1], "rb").read()); o = 0
while o + 32 <= len(b):  # o hash do primeiro bloco de ADX
    tipo, q, band = b[o + 1], struct.unpack_from("<H", b, o + 12)[0], struct.unpack_from("<H", b, o + 16 + 2)[0]
    if tipo == 0:
        b[o + 24] ^= 1; break
    o += 32 + (q * 4 if band & 1 else 0)
else:
    sys.exit(1)
open(sys.argv[2], "wb").write(b)
P
    deve_falhar "gravação com um hash de ADX trocado" "PCM do ADX diferente" python3 port/x360/tools/som.py mixer $T/grav_ok.bin $T/grav_hash.bin
    rm -f $T/grav_ok.bin $T/grav_hash.bin
    ;;
*)
    sed -n '2,13p' "$0"; exit 2;;
esac
if [ ${#FALHAS[@]} -gt 0 ]; then
    echo "== ${#FALHAS[@]} falha(s):"; printf '   %s\n' "${FALHAS[@]}"; exit 1
fi
echo "== tudo certo"
