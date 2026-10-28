#!/usr/bin/env bash
# F7, etapa 2 (docs/x360/plano/F7/02.md; no WSL, na raiz): os cartões avançados nativos das receitas
# (port/x360/receitas) gerados, reabertos e conferidos numa build, fora do git.
#   F7_SAIDA=~/f7e2/rodada port/x360/tools/f7_receitas.sh x86 MARCO...
#   F7_SAIDA=~/f7e2/rodada port/x360/tools/f7_receitas.sh ppc MARCO...   (qemu-ppc sem mapa, dentro de flock ~/wsl.lock)
#   F7_SAIDA=... port/x360/tools/f7_receitas.sh antes MARCO...           (só o cartão de antes, x86: confere os "exige"
#                                                                         da receita contra o New Game real)
#   F7_SAIDA=... port/x360/tools/f7_receitas.sh verificadores            (x86: tabela e receitas estragadas em cópias,
#                                                                         recusadas pelo valida e pelo receitas.c no jogo)
# Para cada marco, numa pasta nova $F7_SAIDA/MARCO/BUILD:
#   antes      New Game, a receita e a gravação por Opções (f7r_antes.txt); reaberto pela carga normal
#              (f7r_reabre_antes.txt);
#   a, b       duas rodadas do roteiro do marco (f7r_MARCO.txt) do cartão vazio: os dois cartões têm de sair iguais
#              byte a byte (repetível; a semente é a do BT3_SEED, padrão 0x12345678);
#   r          a cópia de a reaberta pela carga normal (f7r_reabre.txt): o cartão não muda;
#   f7_receitas.py confere (+ negativos), e o registro MARCO-BUILD.json (commit, semente, fonte da receita, hashes).
# Na PPC, os arquivos que a x86 lê nas mesmas rodadas vão convertidos (conv_tool --saida), como no save_entre.sh, e o
# cartão da PPC se compara campo a campo com o da x86 da mesma rodada (save_entre.py campos), que tem de existir.
# Pede as builds do commit de agora sem diferença (CARIMBO); DESENVOLVIMENTO=1 roda assim mesmo e diz que não vale.
set -uo pipefail
cd "$(dirname "$0")/../../.."
: "${F7_SAIDA:?defina F7_SAIDA fora do repositório}"
S=$(realpath -m "$F7_SAIDA")
case "$S/" in "$(pwd)/"*) echo 'saída dentro do repositório recusada'; exit 1;; esac
modo=${1:?x86, ppc, antes ou verificadores}; shift
[ $# -gt 0 ] || [ "$modo" = verificadores ] || { echo 'falta o MARCO'; exit 1; }
B=port/build; R=port/x360/roteiros; P=port/x360/tools/f7_receitas.py
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 DEBUGINFOD_URLS=
FALHAS=()
falhou() { FALHAS+=("$*"); echo "   >> FALHA: $*"; }

python3 $P valida > /dev/null || { python3 $P valida; exit 1; }
AGORA=$(git rev-parse --short HEAD); ORIGEM=()
for b in x86 $([ "$modo" = ppc ] && echo ppc); do
    c=$(head -1 $B/$b/CARIMBO 2>/dev/null | cut -d' ' -f1,2)
    [ "$c" = "$AGORA diff:da39a3ee5e" ] || ORIGEM+=("$b é de \"${c:-sem carimbo}\", não do commit de agora ($AGORA) sem diferença")
done
[ -z "$(git status --porcelain --untracked-files=normal)" ] || ORIGEM+=("a árvore tem alterações fora de commit")
if [ ${#ORIGEM[@]} -gt 0 ]; then
    printf '   >> origem: %s\n' "${ORIGEM[@]}"
    [ -n "${DESENVOLVIMENTO:-}" ] || { echo "== builds de outra origem: refaça do HEAD (ou DESENVOLVIMENTO=1)"; exit 1; }
fi
livre=$(df -B1 --output=avail "$HOME" | tail -1 | tr -d ' ')
[ "$livre" -ge 4294967296 ] || { echo "menos de 4 GiB livres em $HOME: pare e preserve o estado"; exit 1; }

roda() {  # build roteiro cartão saída: uma rodada; o código de saída vai para saída.codigo
    local b=$1 rot=$2 c=$3 s=$4
    if [ $b = x86 ]; then
        BT3_DESBLOQUEIO=1 BT3_ROTEIRO=$R/$rot.txt BT3_SAVES=$c timeout 900 $B/x86/bt3_x86 > $s 2>&1 < /dev/null
    else
        BT3_DESBLOQUEIO=1 BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_ROTEIRO=$R/$rot.txt \
            BT3_SAVES=$c timeout ${TP:-7200} qemu-ppc $B/ppc/bt3_ppc > $s 2>&1 < /dev/null
    fi
    echo $? > $s.codigo
    rm -f bt3_crash.txt
}
leu() {  # roteiro cartão: os arquivos que a x86 lê nessa rodada (gdb), para a conversão da PPC
    BT3_DESBLOQUEIO=1 BT3_ROTEIRO=$R/$1.txt BT3_SAVES=$2 timeout 900 gdb -batch -nx -ex 'set pagination off' \
        -ex 'dprintf Port_FileReadAt,"LEU %s\n", rel' -ex run --args $B/x86/bt3_x86 2>&1 < /dev/null |
        grep -a '^LEU ' | cut -d' ' -f2 | grep -v '^disc/' >> $D/lidos_todos.txt
}
dados() { echo "$1/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3"; }

if [ "$modo" = verificadores ]; then
    # os verificadores vistos falhando de propósito: cópias estragadas da tabela e das receitas (valida, e o
    # receitas.c dentro do jogo na x86), uma coisa por vez; cada uma tem de ser recusada
    V=$S/verificadores; [ ! -e $V ] || { echo "$V já existe"; exit 1; }
    mkdir -p $V
    copia() { rm -rf $V/c; mkdir -p $V/c; cp -r port/x360/roteiros port/x360/receitas $V/c/; }
    recusa_valida() {  # nome: a cópia em $V/c/receitas tem de ser recusada pelo valida
        local saida; saida=$(python3 $P valida --pasta $V/c/receitas 2>&1)
        if [ $? = 0 ]; then falhou "valida aceitou: $1"; else echo "   valida recusou $1: $(tail -1 <<< "$saida")"; fi
    }
    copia; echo 'escreve campo.inexistente = 1' >> $V/c/receitas/missao_trinta.txt; recusa_valida campo-desconhecido
    copia; echo 'escreve dinheiro = 5' >> $V/c/receitas/missao_trinta.txt; recusa_valida escrita-sem-exige
    copia; sed -i '/^depois/d' $V/c/receitas/missao_trinta.txt; echo 'depois missao.cumprida[1] = 1' >> $V/c/receitas/missao_trinta.txt
    recusa_valida depois-so-eco
    copia; echo 'sobra u8 0x028C 1 1 teste' >> $V/c/receitas/campos.txt; recusa_valida campo-sobreposto
    copia; echo 'fora s32 0x4000 1 4 teste' >> $V/c/receitas/campos.txt; recusa_valida campo-fora-do-save
    copia; sed -i 's/^exige missao.cumprida\[0..99\] = 0$/exige missao.cumprida[0..98] = 0/' $V/c/receitas/missao_trinta.txt
    echo 'exige item[400] = 0' >> $V/c/receitas/missao_trinta.txt; recusa_valida indice-fora
    recusa_jogo() {  # nome texto-esperado roteiro cartão [VAR=valor...]: a x86 com a cópia tem de parar com o texto
        local n=$1 txt=$2 rot=$3 c=$4; shift 4
        env BT3_DESBLOQUEIO=1 BT3_RECEITA=missao_trinta "$@" BT3_ROTEIRO=$V/c/roteiros/$rot.txt BT3_SAVES=$c \
            timeout 900 $B/x86/bt3_x86 > $V/$n.txt 2>&1 < /dev/null; local r=$?
        rm -f bt3_crash.txt
        if [ $r != 0 ] && grep -aq "roteiro: FALHA: .*$txt" $V/$n.txt; then
            echo "   jogo recusou $n (saiu com $r): $(grep -a 'roteiro: FALHA' $V/$n.txt | head -1 | cut -c1-150)"
        else
            falhou "jogo não recusou $n (saiu com $r)"
        fi
    }
    copia; sed -i 's/^exige ultimate.flags = 0$/exige ultimate.flags = 1/' $V/c/receitas/missao_trinta.txt
    recusa_jogo exige-falso "não é o estado do New Game" f7r_antes $V/k1
    copia; echo 'escreve campo.inexistente = 1' >> $V/c/receitas/missao_trinta.txt
    recusa_jogo campo-desconhecido "receita inválida" f7r_antes $V/k2
    copia; recusa_jogo sem-desbloqueio "sem BT3_DESBLOQUEIO=1" f7r_antes $V/k3 BT3_DESBLOQUEIO=0
    BT3_RECEITA=missao_trinta roda x86 f7r_antes $V/k4 $V/k4.txt  # um cartão de antes do marco, válido
    [ -s "$(dados $V/k4)" ] || falhou "sem o cartão de antes para os dois casos seguintes"
    printf 'inclui save_continua.txt\nreceita aplica\nfim\n' > $V/c/roteiros/aplica_no_continue.txt
    recusa_jogo cartao-ja-gravado "o cartão já tinha um save" aplica_no_continue $V/k4
    recusa_jogo depois-no-antes "fora do esperado" f7r_reabre $V/k4
    rm -rf $V/c $V/k1 $V/k2 $V/k3 $V/k4
    if [ ${#FALHAS[@]} -gt 0 ]; then printf '== falha: %s\n' "${FALHAS[@]}"; exit 1; fi
    echo "== verificadores: tudo recusado"
    exit 0
fi
if [ "$modo" = ppc ]; then
    exec 9> "$HOME/wsl.lock"
    flock 9
    if pgrep -x qemu-ppc > /dev/null || pgrep -f '[r]oteiros.sh' > /dev/null; then echo 'emulador ocupado sem a trava'; exit 1; fi
fi
for marco in "$@"; do
    export BT3_RECEITA=$marco
    b=$([ "$modo" = ppc ] && echo ppc || echo x86)
    D=$S/$marco/$b
    echo "== $marco ($modo)"
    [ ! -e "$D" ] || { falhou "$D já existe"; continue; }
    mkdir -p "$D"
    if [ $b = ppc ]; then
        X=$S/$marco/x86
        [ -s "$(dados $X/a)" ] || { falhou "$marco: falta o cartão da x86 em $X/a (rode antes o modo x86)"; continue; }
        : > $D/lidos_todos.txt
        leu f7r_antes $D/l1; leu f7r_$marco $D/l2; cp -r $X/a $D/l3; leu f7r_reabre $D/l3; cp -r $X/antes $D/l4
        leu f7r_reabre_antes $D/l4; rm -rf $D/l1 $D/l2 $D/l3 $D/l4
        sort -u $D/lidos_todos.txt > $D/lidos.txt
        gcc -std=c89 -O2 -w -o $D/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
        mkdir -p $D/dados/mods; for g in gamedata/*; do ln -s "$(realpath "$g")" $D/dados/; done
        $D/conv_tool --saida $D/dados/mods gamedata $(cat $D/lidos.txt) > $D/conv.txt || falhou "$marco: conv_tool"
        touch $D/dados/mods/convertidos.txt
    fi
    roda $b f7r_antes $D/antes $D/antes.txt
    cp -r $D/antes $D/antes_r
    roda $b f7r_reabre_antes $D/antes_r $D/reabre_antes.txt
    cmp -s "$(dados $D/antes)" "$(dados $D/antes_r)" || falhou "$marco: a reabertura do cartão de antes mudou o cartão"
    if [ "$modo" = antes ]; then
        python3 - "$D" "$marco" <<'PY' || falhou "$marco: cartão de antes"
import sys, pathlib, argparse
sys.path.insert(0, "port/x360/tools")
import f7_receitas as f
d, m = pathlib.Path(sys.argv[1]), sys.argv[2]
r = f.ler_receita(m)
try:
    n = f.confere_cartao(r, d / "antes", "x86", True)
    f.confere_log_evento(r, d / "antes.txt", antes=True)
    f.confere_log_reabre(r, d / "reabre_antes.txt", antes=True)
except f.Falha as e:
    print(f"   FALHA: {e}"); sys.exit(1)
print(f"   antes: {n} elementos da receita no cartão e em memória depois da carga normal; exige do New Game cumpridos")
PY
        continue
    fi
    [ -e $R/f7r_$marco.txt ] || { falhou "$marco: sem o roteiro f7r_$marco.txt"; continue; }
    roda $b f7r_$marco $D/a $D/a.txt
    roda $b f7r_$marco $D/b $D/b.txt
    cmp -s "$(dados $D/a)" "$(dados $D/b)" && echo "   repetível: os dois cartões iguais byte a byte" ||
        falhou "$marco: duas rodadas, cartões diferentes"
    cp -r $D/a $D/r
    roda $b f7r_reabre $D/r $D/reabre.txt
    cmp -s "$(dados $D/a)" "$(dados $D/r)" || falhou "$marco: a reabertura mudou o cartão"
    args=(--receita $marco --build $b --cartao $D/a --evento $D/a.txt --reabre $D/reabre.txt
          --antes $D/antes --reabre-antes $D/reabre_antes.txt)
    python3 $P confere "${args[@]}" --json $S/$marco-$b.json | sed 's/^/   /'
    [ ${PIPESTATUS[0]} = 0 ] || { falhou "$marco: confere"; continue; }
    python3 $P negativos "${args[@]}" --saida $D/negativos > $D/negativos.txt; r=$?
    tail -1 $D/negativos.txt | sed 's/^/   /'
    [ $r = 0 ] || falhou "$marco: negativos ($(grep -m1 'FALHA\|ACEITO' $D/negativos.txt))"
    if [ $b = ppc ]; then
        python3 port/x360/tools/save_entre.py campos $X/a $D/a > $D/campos.txt; r=$?
        tail -1 $D/campos.txt | sed 's/^/   x86 contra ppc: /'
        [ $r = 0 ] || falhou "$marco: o cartão da PPC difere do da x86 campo a campo"
        rm -rf $D/dados $D/conv_tool
    fi
    sha256sum "$(dados $D/a)" "$(dados $D/antes)" | sed "s|$S/||" > $S/$marco-$b.sha256
    rm -rf $D/b $D/r $D/antes_r
done
if [ ${#FALHAS[@]} -gt 0 ]; then
    echo "== ${#FALHAS[@]} falha(s):"; printf '   %s\n' "${FALHAS[@]}"
    exit 1
fi
[ ${#ORIGEM[@]} -eq 0 ] || { echo "== tudo certo, mas NÃO VALE COMO CRITÉRIO: $(printf '%s; ' "${ORIGEM[@]}")"; exit 0; }
echo "== tudo certo"
