#!/usr/bin/env bash
# F7, etapa 4: o lote retomável por caso (plano/F7/04.md). Roda cada caso pelo roteiros.sh com as redes do critério
# (x86 duas vezes, gdb, x86r e mapa, PPC sem mapa, memória e imagens: SEMMAPA=1 IMG=1; nunca PPC=1), julga o caso
# (f7_lote.py caso), preserva as provas textuais e apaga os intermediários, e refaz o resumo por família.
#
#   F7_SAIDA=~/f7e4-lote F7_MAQUINA=WSL|Deck port/x360/tools/f7_lote.sh [CASO...]
#
# Sem CASO, todos os do manifesto (f7_elenco.json); F7_PARTE=K/N fica com os de índice K mod N (dividir entre o WSL
# e o Deck). Um caso com veredito em $F7_SAIDA/casos/NOME é pulado (retomável); F7_REFAZ=1 refaz os reprovados.
# A trava (~/wsl.lock no WSL, ~/deck.lock no Deck) se toma caso a caso, e o caso só começa sem qemu-ppc nem
# roteiros.sh vivos. As builds são as do commit de agora, sem nada fora de commit (o roteiros.sh recusa outra coisa).
# O fim de cada caso e do lote vai a $F7_SAIDA/resumo.txt (a última linha "FIM RC=N" no fim do lote).
set -uo pipefail
cd "$(dirname "$0")/../../.."
: "${F7_SAIDA:?defina F7_SAIDA fora do repositório}"
S=$(realpath -m "$F7_SAIDA")
case "$S/" in "$(pwd)/"*) echo 'saída dentro do repositório recusada'; exit 1;; esac
mkdir -p "$S/casos"
maquina=${F7_MAQUINA:-WSL}
case "$maquina" in
    WSL) trava="$HOME/wsl.lock";;
    Deck) trava="$HOME/deck.lock";;
    *) echo 'F7_MAQUINA deve ser WSL ou Deck'; exit 1;;
esac
export DEBUGINFOD_URLS=
diz() { echo "$(date '+%d/%m %H:%M:%S') $*" | tee -a "$S/resumo.txt"; }
livre() {
    local casa win
    casa=$(df -B1 --output=avail "$HOME" | tail -1 | tr -d ' ')
    echo "disco antes de $1: $casa bytes livres" >> "$S/disco.txt"
    [ "$casa" -ge 6442450944 ] || { diz "PARADO: menos de 6 GiB livres na casa antes de $1"; return 1; }
    if [ "$maquina" = WSL ]; then
        win=$(df -B1 --output=avail /mnt/c | tail -1 | tr -d ' ')
        echo "Windows antes de $1: $win bytes livres" >> "$S/disco.txt"
        [ "$win" -ge 17179869184 ] || { diz "PARADO: menos de 16 GiB livres no C: antes de $1"; return 1; }
    fi
}

# --- um caso, já dentro da trava (o próprio script se chama sob o flock) ---
if [ "${1:-}" = --um ]; then
    n=$2
    if pgrep -ax qemu-ppc >/dev/null || pgrep -af '[r]oteiros.sh' >/dev/null; then
        diz "$n: fila ocupada sem a trava (outro emulador vivo); fica para a próxima volta"
        exit 3
    fi
    livre "$n" || exit 4
    C="$S/casos/$n"; rm -rf "$C" "$S/atual"; mkdir -p "$C"
    cp "port/x360/roteiros/$n.txt" "$C/roteiro.txt"
    python3 -c "import json,sys; print(json.dumps(next(c for c in json.load(open('port/x360/roteiros/f7_elenco.json'))['casos'] if c['nome']==sys.argv[1])))" "$n" > "$C/caso.json"
    inicio=$EPOCHREALTIME
    MEDIDAS="$C/tempos.tsv" O="$S/atual" REFS="$S/referencias" SEMMAPA=1 IMG=1 TP=7200 \
        port/x360/tools/roteiros.sh "$n" > "$C/roteiros.log" 2>&1
    echo $? > "$C/rc.txt"
    D="$S/atual/$n"
    du -sb "$S/atual" > "$C/bytes.txt" 2>/dev/null
    # só as provas textuais; imagens e dumps saem depois do sha256
    if [ -d "$D" ]; then
        (cd "$D" && find . -type f ! -name '*.dump' ! -name '*.ppm' ! -path './dados/*' ! -path './marcas/*' \
            ! -path './partes/*' ! -name 'rastreio.tsv' ! -name '*.mapa' ! -name 'mapa.tsv*' -printf '%P\n') |
            while IFS= read -r a; do mkdir -p "$C/$(dirname "$a")"; cp "$D/$a" "$C/$a"; done
        find "$D/img" -name '*.ppm' -exec sha256sum {} + 2>/dev/null | sed "s#$D/##" > "$C/imagens.sha256"
    fi
    rm -rf "$S/atual"
    printf '%s\t%s\t%s\n' "$n" "$inicio" "$EPOCHREALTIME" >> "$S/parede.tsv"
    python3 port/x360/tools/f7_lote.py caso --pasta "$C" --saida "$C/veredito.json" > "$C/veredito.txt" 2>&1
    vr=$?
    diz "$n: roteiros.sh RC=$(cat "$C/rc.txt"), veredito $(head -1 "$C/veredito.txt") ($(awk -v a="$inicio" -v b="$EPOCHREALTIME" 'BEGIN{printf "%.0f s", b-a}'))"
    # o primeiro caso aprovado também vê o verificador falhando de propósito (cópias, fora do caso)
    if [ "$vr" = 0 ] && [ ! -e "$S/negativos.log" ]; then
        manifesto=${F7_MANIFESTO:-$HOME/f7-etapa1/manifesto.jsonl}
        m=(); [ -s "$manifesto" ] && m=(--manifesto "$manifesto")
        python3 port/x360/tools/f7_lote.py negativos --pasta "$C" --saida "$S/negativos-$n" "${m[@]}" > "$S/negativos.log" 2>&1
        diz "negativos sobre $n: $(tail -1 "$S/negativos.log")"
        rm -rf "$S/negativos-$n"
    fi
    exit 0
fi

# --- as builds, também dentro da trava (não disputar a CPU com a rodada de outra tarefa) ---
if [ "${1:-}" = --builds ]; then
    livre builds || exit 1
    for a in x86 'x86 --rastreio' ppc; do
        python3 port/x360/tools/headless.py --alvo $a > "$S/build-${a// /_}.log" 2>&1 || { diz "PARADO: build $a"; exit 1; }
    done
    grep -q '^104 stubs; link OK' "$S/build-x86.log" ||
        { diz "PARADO: a x86 não ligou com 104 stubs ($(tail -1 "$S/build-x86.log")): o port/build/gen?"; exit 1; }
    objcopy -O binary -j .text port/build/x86/bt3_x86 "$S/text.bin" && sha1sum "$S/text.bin" | cut -d' ' -f1 > "$S/text.sha1"
    rm -f "$S/text.bin"
    exit 0
fi

# --- o lote ---
[ -z "$(git status --porcelain --untracked-files=normal)" ] || { diz 'PARADO: árvore com alterações fora de commit'; exit 1; }
git rev-parse HEAD > "$S/commit.txt"
diz "lote em $maquina, commit $(git rev-parse --short HEAD), trava $trava; esperando a trava para as builds"
flock "$trava" "$0" --builds || { echo "FIM RC=1" >> "$S/resumo.txt"; exit 1; }
diz "builds prontas; .text da x86 $(cat "$S/text.sha1")"
python3 port/x360/tools/f7_elenco.py confere > "$S/elenco-confere.txt" 2>&1 || { diz "PARADO: $(tail -1 "$S/elenco-confere.txt")"; exit 1; }
casos=("$@")
if [ ${#casos[@]} -eq 0 ]; then
    mapfile -t casos < <(python3 -c "
import json, os
c = [x['nome'] for x in json.load(open('port/x360/roteiros/f7_elenco.json'))['casos']]
k, n = map(int, os.environ.get('F7_PARTE', '0/1').split('/'))
print('\n'.join(x for i, x in enumerate(c) if i % n == k))")
fi
diz "casos: ${casos[*]}"
for n in "${casos[@]}"; do
    v="$S/casos/$n/veredito.json"
    if [ -e "$v" ] && { [ -z "${F7_REFAZ:-}" ] || grep -q '"resultado": "ok"' "$v"; }; then
        continue
    fi
    for tentativa in 1 2 3 4 5 6; do
        flock "$trava" "$0" --um "$n"
        r=$?
        [ "$r" = 3 ] || break
        sleep 600
    done
    [ "$r" = 4 ] && { echo "FIM RC=4" >> "$S/resumo.txt"; exit 4; }
done
manifesto=${F7_MANIFESTO:-$HOME/f7-etapa1/manifesto.jsonl}
if [ -s "$manifesto" ]; then
    python3 port/x360/tools/f7_lote.py resumo --casos "$S/casos" --manifesto "$manifesto" --saida "$S/resumo.json" \
        >> "$S/resumo.txt" 2>&1
    rc=$?
else
    diz "sem o manifesto da etapa 1 em $manifesto: só os vereditos"
    rc=0; grep -L '"resultado": "ok"' "$S"/casos/*/veredito.json | grep -q . && rc=1
fi
echo "FIM RC=$rc" >> "$S/resumo.txt"
exit $rc
