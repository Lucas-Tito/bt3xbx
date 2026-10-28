#!/usr/bin/env bash
# Pilotos F7: uma fila, saídas próprias e compactação depois da conferência.
# Uso: F7_SAIDA=~/f7-pilotos-RODADA port/x360/tools/f7_pilotos.sh [ensaio|lote] [CASO...]
# WSL por padrão; F7_MAQUINA=Deck conserva a fila e a prova do contêiner (guia 1.2b).
set -euo pipefail
cd "$(dirname "$0")/../../.."
: "${F7_SAIDA:?defina F7_SAIDA fora do repositório}"
S=$(realpath -m "$F7_SAIDA")
case "$S/" in "$(pwd)/"*) echo 'saída dentro do repositório recusada'; exit 1;; esac
mkdir -p "$S"
maquina=${F7_MAQUINA:-WSL}
case "$maquina" in
    WSL) trava="$HOME/wsl.lock";;
    Deck) trava="$HOME/deck.lock";;
    *) echo 'F7_MAQUINA deve ser WSL ou Deck'; exit 1;;
esac
exec 9> "$trava"
flock 9
if [ "$maquina" = Deck ]; then
    grep -q '^FIM$' "$HOME/prova2810.resumo"
    grep -q 'roteiro duelo .* RC=0' "$HOME/prova2810.resumo"
    grep -q '43444ea4d03a6d448aa1ae74a631e35b356a7caa (igual' "$HOME/prova2810.resumo"
fi
if pgrep -ax qemu-ppc || pgrep -af '[r]oteiros.sh'; then echo 'fila ocupada sem a trava'; exit 1; fi
export DEBUGINFOD_URLS= BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1 BT3_F7=1
livre() {
    local bytes
    bytes=$(df -B1 --output=avail "$HOME" | tail -1 | tr -d ' ')
    echo "disco antes de $1: $bytes bytes livres" | tee -a "$S/disco.txt"
    if [ "$maquina" = WSL ]; then
        bytes=$(df -B1 --output=avail /mnt/c | tail -1 | tr -d ' ')
        echo "Windows antes de $1: $bytes bytes livres" | tee -a "$S/disco.txt"
        [ "$bytes" -ge 17179869184 ] || { echo 'menos de 16 GiB livres no Windows'; exit 1; }
    fi
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
base_obj=${F7_BASE_OBJ:-port/build/base-reference/port/build/x86/obj}
[ -d "$base_obj" ] || { echo 'defina F7_BASE_OBJ com os objetos da base aprovada'; exit 1; }
for base in "$base_obj/"*.o; do
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
    reabre_inicio=$EPOCHREALTIME
    if [ -n "$reabre" ]; then
        for a in x86 ppc; do
            if [ "$a" = x86 ]; then
                cartao=sa; exe=(env -u BT3_DATA -u BT3_CONVERTIDOS port/build/x86/bt3_x86)
            else
                cartao=sq; exe=(env BT3_DATA="$D/dados" BT3_CONVERTIDOS="$D/dados/mods/convertidos.txt"
                               qemu-ppc port/build/ppc/bt3_ppc)
            fi
            BT3_ROTEIRO="port/x360/roteiros/f7_reabre_$reabre.txt" BT3_SAVES="$D/$cartao" \
                timeout 900 "${exe[@]}" > "$D/reabre_$a.txt" 2>&1
        done
        python3 port/x360/tools/save_entre.py campos "$D/sa" "$D/sq" > "$S/$n-campos.txt"
    fi
    printf '%s\t%s\t%s\n' "$n" "$reabre_inicio" "$EPOCHREALTIME" >> "$S/reabertura.tsv"
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
    [ -e "$S/conv_tool" ] || cp "$S/atual/conv_tool" "$S/conv_tool"
    rm -rf "$S/atual"
    tail -1 "$S/$n.log"
done
# A auditoria completa deve continuar reprovando as lacunas: os pilotos são uma amostra.
"$S/conv_tool" --disco gamedata > "$S/coleta/disco.log"
cp port/build/x86/bt3_x86 "$S/bt3_x86"
python3 - "$S" "$maquina" <<'PY2'
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sys
s = Path(sys.argv[1]); coleta = s / 'coleta'
logs = sorted(coleta.glob('roteiros/*/gdb.txt'))
arquivos = [dict(nome=str(p.relative_to(coleta)), tamanho=p.stat().st_size,
                 sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in logs]
agora = datetime.now(timezone.utc).isoformat()
meta = dict(inicio_utc=agora, fim_utc=agora, rodada=str(s), prog=str(s / 'bt3_x86'),
            sha256_prog=hashlib.sha256((s / 'bt3_x86').read_bytes()).hexdigest(), arquivos=arquivos,
            limite='coleta própria dos pilotos concluídos, sem crédito para casos ausentes')
(coleta / 'coleta.json').write_text(json.dumps(meta, indent=2) + '\n')
(coleta / 'origem.json').write_text(json.dumps(dict(commit=(s / 'commit.txt').read_text().strip(), maquina=sys.argv[2])) + '\n')
(coleta / 'casos.json').write_text(json.dumps({p.parent.name: 0 for p in logs}) + '\n')
PY2
set +e
python3 port/x360/tools/f7_cobertura.py conferir --coleta "$S/coleta" \
    --justificativas port/x360/tools/f7_justificativas.json --saida "$S/cobertura.json" > "$S/cobertura.log"
cobertura_rc=$?
set -e
[ "$cobertura_rc" = 1 ] && [ -s "$S/cobertura.json" ]
python3 port/x360/tools/teste_f7.py --coleta "$S/coleta" --dados "$(realpath gamedata)" \
    --saida "$S/negativos-cobertura" | tee "$S/negativos-cobertura.log"
python3 port/x360/tools/f7_manifesto.py --dados gamedata --saida "$S/manifesto.jsonl" > "$S/manifesto.log"
echo 'pilotos: tudo certo; cobertura completa continua pendente'
