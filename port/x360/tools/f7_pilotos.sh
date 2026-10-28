#!/usr/bin/env bash
# Pilotos F7 no Deck: uma fila, saídas próprias e compactação depois da conferência.
# Uso: F7_SAIDA=~/f7-pilotos-RODADA port/x360/tools/f7_pilotos.sh [ensaio|lote] [CASO...]
# Executar no contêiner bt3; o lançador persistente e a espera da prova ficam no guia 1.2b.
set -euo pipefail
cd "$(dirname "$0")/../../.."
: "${F7_SAIDA:?defina F7_SAIDA fora do repositório}"
S=$(realpath -m "$F7_SAIDA")
case "$S/" in "$(pwd)/"*) echo 'saída dentro do repositório recusada'; exit 1;; esac
mkdir -p "$S"
exec 9> "$HOME/deck.lock"
flock 9
# A prova anterior tem de estar encerrada e aprovada antes de usar a fila.
grep -q '^FIM$' "$HOME/prova2810.resumo"
grep -q 'roteiro duelo .* RC=0' "$HOME/prova2810.resumo"
grep -q '43444ea4d03a6d448aa1ae74a631e35b356a7caa (igual' "$HOME/prova2810.resumo"
if pgrep -ax qemu-ppc || pgrep -af '[r]oteiros.sh'; then echo 'fila ocupada sem a trava'; exit 1; fi
export DEBUGINFOD_URLS= BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1 BT3_F7=1
livre() {
    local bytes
    bytes=$(df -B1 --output=avail "$HOME" | tail -1 | tr -d ' ')
    echo "disco antes de $1: $bytes bytes livres" | tee -a "$S/disco.txt"
    [ "$bytes" -ge 4294967296 ] || { echo 'menos de 4 GiB livres, pare e preserve o estado'; exit 1; }
}
livre builds
[ -z "$(git status --porcelain)" ]
git rev-parse HEAD > "$S/commit.txt"
for a in x86 'x86 --rastreio' ppc; do
    python3 port/x360/tools/headless.py --alvo $a 2>&1 | tee "$S/build-${a// /_}.log" >/dev/null
    tail -1 "$S/build-${a// /_}.log"
done
# Objetos do jogo da base aprovada, sem depuração; só roteiro/passos são testes.
mkdir -p "$S/objetos"
: > "$S/objetos.txt"
for base in "$HOME/bt3xbx/port/build/x86/obj/"*.o; do
    nome=$(basename "$base")
    objcopy --strip-debug "$base" "$S/objetos/base.o"
    objcopy --strip-debug "port/build/x86/obj/$nome" "$S/objetos/novo.o"
    if ! cmp -s "$S/objetos/base.o" "$S/objetos/novo.o"; then
        echo "diferente: $nome" | tee -a "$S/objetos.txt"
        case "$nome" in x360_roteiro.o|x360_passos.o) ;; *) exit 1;; esac
    else
        echo "igual: $nome" >> "$S/objetos.txt"
    fi
done
rm -rf "$S/objetos"
objcopy -O binary -j .text port/build/x86/bt3_x86 "$S/text.bin"
sha1sum "$S/text.bin" | tee "$S/text.sha1"
rm "$S/text.bin"
modo=${1:-ensaio}; [ $# -eq 0 ] || shift
casos=("$@"); [ ${#casos[@]} -gt 0 ] || casos=(f7_missao f7_missao29 historia_novo f7_transformacao)
if [ "$modo" = ensaio ]; then
    for n in "${casos[@]}"; do
        livre "$n"
        rm -rf "$S/ensaio-$n"
        mkdir -p "$S/ensaio-$n"
        BT3_ROTEIRO="port/x360/roteiros/$n.txt" BT3_SAVES="$S/ensaio-$n/cartao" \
            timeout 900 port/build/x86/bt3_x86 2>&1 | tee "$S/ensaio-$n/saida.txt" >/dev/null
        tail -8 "$S/ensaio-$n/saida.txt"
    done
    echo 'ensaio: tudo certo'
    exit 0
fi
[ "$modo" = lote ]
mkdir -p "$S/compactos" "$S/coleta/roteiros"
for n in "${casos[@]}"; do
    livre "$n"
    [ ! -e "$S/atual" ] || { echo 'saída anterior ainda presente'; exit 1; }
    inicio=$EPOCHREALTIME
    MEDIDAS="$S/$n-tempos.tsv" O="$S/atual" REFS="$S/referencias" SEMMAPA=1 IMG=1 TP=7200 \
        port/x360/tools/roteiros.sh "$n" 2>&1 | tee "$S/$n.log" >/dev/null
    D="$S/atual/$n"
    case "$n" in
        f7_missao) reabre=missao;; f7_missao29) reabre=missao29;; historia_novo) reabre=historia;; *) reabre=;;
    esac
    if [ -n "$reabre" ]; then
        for a in x86 ppc; do
            if [ "$a" = x86 ]; then cartao=sa; exe=(port/build/x86/bt3_x86); else cartao=sq; exe=(qemu-ppc port/build/ppc/bt3_ppc); fi
            BT3_DATA="$D/dados" BT3_CONVERTIDOS="$D/dados/mods/convertidos.txt" \
                BT3_ROTEIRO="port/x360/roteiros/f7_reabre_$reabre.txt" BT3_SAVES="$D/$cartao" \
                timeout 900 "${exe[@]}" > "$D/reabre_$a.txt" 2>&1
        done
        python3 port/x360/tools/save_entre.py campos "$D/sa" "$D/sq" > "$S/$n-campos.txt"
    fi
    python3 port/x360/tools/f7_pilotos.py --caso "$n" --pasta "$D" --tempos "$S/$n-tempos.tsv" \
        --saida "$S/$n.json" --negativos "$S/negativos-$n" | tee "$S/$n-verificador.log"
    du -sb "$S/atual" | tee "$S/$n-bytes.txt"
    echo "$n $inicio $EPOCHREALTIME" >> "$S/parede.tsv"
    # Guardar somente provas textuais, cartões e metadados, conferidos antes de apagar os intermediários.
    C="$S/compactos/$n"; mkdir -p "$C"
    find "$D" -type f ! -name '*.dump' ! -name '*.ppm' ! -path '*/dados/*' ! -path '*/marcas/*' \
        ! -path '*/partes/*' ! -name 'rastreio.tsv' -printf '%P\n' > "$S/$n-preservados.txt"
    while IFS= read -r arq; do mkdir -p "$C/$(dirname "$arq")"; cp "$D/$arq" "$C/$arq"; done < "$S/$n-preservados.txt"
    mkdir -p "$S/coleta/roteiros/$n"
    cp "$D/gdb.txt" "$S/coleta/roteiros/$n/gdb.txt"
    sha256sum "$D"/img/{x86,ppc}/*.ppm > "$S/$n-imagens.sha256"
    rm -rf "$S/atual"
    tail -1 "$S/$n.log"
done
echo 'pilotos: tudo certo'
