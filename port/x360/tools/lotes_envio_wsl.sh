#!/usr/bin/env bash
# Rodada da segunda parte dos lotes. Quem chama segura flock ~/wsl.lock.
# Uso: lotes_envio_wsl.sh SAIDA_NOVA; base fixa 979cc928, fonte copiado só em port/build.
# Previsto: até 2 h depois da trava; teto 4 h pelo lançador, sem reaproveitar falhas.
set -euo pipefail
ulimit -c 0
R=$(cd "$(dirname "$0")/../../.." && pwd)
I=$(realpath -m "$1")
mkdir -p "$I"
cd "$R"
exec > >(tee "$I/rodada.log") 2>&1
P=$I/provas.resumo
pa() { echo "$*" | tee -a "$P"; }
trap 'rc=$?; pa "fim $(date -Is) RC=$rc"; if [ "$rc" = 0 ]; then pa "tudo certo"; else pa "FALHA RC=$rc"; fi; exit "$rc"' EXIT
[ -z "$(git status --porcelain)" ]
pa "commit $(git rev-parse HEAD); base 979cc928; início $(date -Is)"
# Não herdar mapa, reuso, salvamento antigo ou sabotagem de quem dispara.
while IFS= read -r v; do
    case "$v" in BT3_*|PPC|SEMMAPA|IMG|REF|REFS|SOM|REUSA|DESENVOLVIMENTO|MEM|O|TP|ALVO) unset "$v";; esac
done < <(compgen -e)
export DEBUGINFOD_URLS=
pgrep -af 'qemu|roteiros' > "$I/processos.log" || true
if pgrep -x qemu-ppc || pgrep -f '(^|/|[[:space:]])roteiros[.]sh([[:space:]]|$)'; then exit 1; fi
df -h /mnt/c . | tee "$I/espaco.log"
python3 port/x360/tools/lotes_envio_igual.py > "$I/fonte-normal.log" 2>&1
python3 port/x360/tools/teste_lotes_envio.py > "$I/unidade-x86.log" 2>&1
python3 port/x360/tools/teste_lotes_envio.py --ppc > "$I/unidade-ppc.log" 2>&1
python3 port/x360/tools/teste_lotes_protocolo.py > "$I/protocolo.log" 2>&1
python3 port/x360/tools/teste_lotes_fonte.py > "$I/plano.log" 2>&1
python3 port/x360/tools/teste_relogio.py --saida "$I/relogio" > "$I/relogio.log" 2>&1
python3 port/x360/tools/lotes_envio_console.py --mutantes > "$I/ambientes.log" 2>&1
pa 'unidades x86/PPC, protocolo, plano, relógio e ambientes com sabotagens: tudo certo'
# Base é uma cópia descartável de fonte, dentro desta worktree. Não mudar outra worktree.
BASE=$R/port/build/lotes-envio-base
[ ! -e "$BASE" ]
mkdir -p "$BASE/port/build"
git archive 979cc928 | tar -x -C "$BASE"
printf '979cc928 diff:da39a3ee5e\n' > "$BASE/port/build/ENVIADO"
for d in asm assets disc gamedata; do ln -s "/srv/bt3/dados/$d" "$BASE/$d"; done
python3 port/tools/gen_data.py --asm asm > "$I/gen-final.log" 2>&1
(cd "$BASE"; python3 port/tools/gen_data.py --asm asm) > "$I/gen-base.log" 2>&1
build() {
    local w=$1 nome=$2; shift 2
    (cd "$w"; python3 port/x360/tools/headless.py "$@") > "$I/build-$nome.log" 2>&1
    pa "build $nome: tudo certo"
}
for par in "base:$BASE" "final:$R"; do
    n=${par%%:*}; w=${par#*:}
    build "$w" "$n-x86" --alvo x86
    build "$w" "$n-x86gv" --alvo x86 --gpu --vu1
    build "$w" "$n-ppcg" --alvo ppc --gpu
    objcopy -O binary --only-section=.text "$w/port/build/x86/bt3_x86" "$I/text-$n.bin"
    sha1sum "$I/text-$n.bin" | tee -a "$P"
done
cmp "$I/text-base.bin" "$I/text-final.bin"
python3 - "$BASE" "$R" "$I" <<'PY'
from pathlib import Path
import subprocess,sys
b,f,o=map(Path,sys.argv[1:])
for alvo in ('x86','x86gv','ppcg'):
    a=b/'port/build'/alvo/'obj'; z=f/'port/build'/alvo/'obj'
    aa={x.name for x in a.glob('*.o')}; zz={x.name for x in z.glob('*.o')}
    assert aa and aa==zz,(alvo,'objetos ausentes')
    oc='powerpc-linux-gnu-objcopy' if alvo.startswith('ppc') else 'objcopy'
    dif=[]
    for n in sorted(aa):
        subprocess.run([oc,'--strip-debug',str(a/n),str(o/'base.o')],check=True)
        subprocess.run([oc,'--strip-debug',str(z/n),str(o/'final.o')],check=True)
        if subprocess.run(['cmp','-s',str(o/'base.o'),str(o/'final.o')]).returncode: dif.append(n)
    print(alvo,'objetos diferentes sem depuração:',dif)
    assert set(dif)<={'x360_passos.o'},(alvo,dif)
s=bytearray((o/'base.o').read_bytes());s[-1]^=1;(o/'final.o').write_bytes(s)
assert subprocess.run(['cmp','-s',str(o/'base.o'),str(o/'final.o')]).returncode==1
PY
for a in x86 x86gv ppcg; do cmp "$BASE/port/build/$a/stubs.txt" "port/build/$a/stubs.txt"; done
pa 'objetos normais sem depuração e stubs iguais à base salvo carimbo; cmp visto falhar; .text idêntica'
build "$R" final-x86r --alvo x86 --rastreio
build "$R" final-ppc --alvo ppc
for args in 'x86 --gpu --vu1 --alfa' 'ppc --gpu --alfa' 'x86 --gpu --vu1 --lotes' 'ppc --gpu --lotes' \
            'x86 --gpu --vu1 --alfa --lotes' 'ppc --gpu --alfa --lotes'; do
    read -ra a <<< "$args"
    build "$R" "final-${args// /-}" --alvo "${a[@]}"
done
for flags in '' 'BT3_ALFA_ORIGEM=1' 'BT3_PERFIL_CADEIA=1 BT3_ALFA_ORIGEM=1'; do
    env BT3_GS_LOTES=1 $flags python3 port/x360/tools/c89.py port/build/c89-lotes port/src/gs/gs_draw.c >> "$I/c89.log" 2>&1
done
pa 'três variantes C89 dos lotes preparadas, sem XDK'
mkdir -p "$I/dados/mods"
for d in gamedata/*; do ln -s "$(realpath "$d")" "$I/dados/$(basename "$d")"; done
gcc -std=c89 -O2 -w -o "$I/conv_tool" $(find port/x360/conv -maxdepth 1 -name '*.c' ! -name conv_jogo.c | sort)
"$I/conv_tool" --disco --saida "$I/dados/mods" gamedata > "$I/conversao.log" 2>&1
pa 'dados convertidos sem mapa: tudo certo'
lista() {
    local exe=$1 r=$2 nome=$3 envio=$4 atraso=${5:-0} mutante=${6:-}
    (
        export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
        export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_NORMAL=2 BT3_TEX_ORIGEM=1 BT3_TEMPOS=0
        export BT3_GS_THREAD=1 BT3_GS_DESENHO=1 BT3_VU1_DIRETO=1 BT3_GPU_FALSO_ATRASO=2
        export BT3_VU_CONSTANTES=0 BT3_VU_FECHO=0 BT3_LOTES=1 BT3_LOTES_ENVIO=$envio BT3_LOTES_ATRASO=$atraso
        export BT3_LOTES_MUTANTE=$mutante BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$I/save-$nome
        unset BT3_WIDE BT3_MAPA BT3_GUARDA BT3_GS_PREFIXOS
        if [[ "$exe" = ppc* ]]; then
            export BT3_DATA=$I/dados BT3_CONVERTIDOS=$I/dados/mods/convertidos.txt BT3_LEU=1 SEMMAPA=1
            timeout 1800 qemu-ppc port/build/$exe/bt3_$exe
        else timeout 900 port/build/$exe/bt3_$exe; fi
    ) > "$I/$nome.log" 2>&1
}
valida() {
    local nome=$1
    grep -q '^roteiro: .* fim ' "$I/$nome.log" || return 1
    grep -q '^gpu_falso: [0-9]* quadros, hash do jogo' "$I/$nome.log" || return 1
    if grep -q 'FALHA\|CRASH\|race:\|textura solta usada' "$I/$nome.log"; then return 1; fi
    grep -Eq '^gpu_falso: VU1 direto: [1-9][0-9]* quadros relidos pela GPU falsa \(atraso 2\), 0 diferentes$' "$I/$nome.log" || return 1
    grep -q '^gpu_falso: 0 desenhos com textura já solta$' "$I/$nome.log" || return 1
    grep -q '(0 passaram do limite de 2000 ms)' "$I/$nome.log" || return 1
    ! grep -Eq '\([1-9][0-9]* passaram do limite|, [1-9][0-9]* diferentes$' "$I/$nome.log"
}
div() { python3 port/x360/tools/divergencias.py "$I/$1.log" "$I/$2.log" --normal > "$I/div-$1-$2.log"; }
for a in x86gv ppcg; do
    for r in f5_duelo f5_treino; do
        ref=normal-$a-$r
        lista "$a" "$r" "$ref" 0; valida "$ref"
        for caso in ctl lot alfa atraso1 atraso5 atraso20; do
            exe=${a}e; envio=1; atraso=0
            case "$caso" in ctl) envio=0;; alfa) exe=${a}le;; atraso*) atraso=${caso#atraso};; esac
            nome=$caso-$a-$r
            lista "$exe" "$r" "$nome" "$envio" "$atraso"; valida "$nome"
            if [ "$envio" = 1 ]; then grep -Eq '^lotes: quadro [0-9]+ faixas [1-9]' "$I/$nome.log"; fi
            div "$ref" "$nome"
            pa "listas $nome: comandos/cauda e divergencias --normal iguais, tudo certo"
        done
    done
done
# A normal já difere entre x86gv e ppcg no duelo desde a etapa das threads (08-threads.md): cada
# variante tem de repetir essa diferença (mesmos quadros e hashes), não zerá-la. Sabotagens no script.
python3 port/x360/tools/lotes_envio_arquiteturas.py "$I" "$I" 2>&1 | tee "$I/arquiteturas.log"
pa "entre arquiteturas: $(tail -1 "$I/arquiteturas.log")"
for a in x86gve ppcge; do
    for m in dados ordem; do
        nome=negativo-$a-$m
        if lista "$a" f5_duelo "$nome" 1 20 "$m"; then pa "FALHA: sabotagem $nome aceita"; exit 1; fi
        grep -q 'FALHA: prefixos envio ' "$I/$nome.log"
        if valida "$nome"; then pa "FALHA: validador aceitou $nome"; exit 1; fi
        pa "sabotagem $nome recusada"
    done
done
pa 'matriz: 28 execuções completas, 24 pares contra normal e 14 entre arquiteturas iguais às da normal (três sabotagens recusadas); quatro sabotagens de execução recusadas'
# A imagem aqui é o raster de software dos roteiros. O falso não rasteriza D3D;
# imagem do novo envio no D3D exige as capturas da rodada preparada para o Lucas.
export O=$I/roteiros REFS=$I/referencias REF=1 SEMMAPA=1 IMG=1
cp -a "$HOME/referencias" "$REFS"
bash port/x360/tools/roteiros.sh f5_duelo f5_treino 2>&1 | tee "$I/roteiros-f5.log"
pa 'REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino: tudo certo'
unset REF
bash port/x360/tools/roteiros.sh duelo 2>&1 | tee "$I/roteiros-duelo.log"
pa 'SEMMAPA=1 IMG=1 roteiros.sh duelo: tudo certo'
bash port/x360/tools/save_mc.sh 2>&1 | tee "$I/save-mc.log"
python3 port/x360/tools/f5_final_confere.py save-mc port/build/save_mc | tee "$I/save-conferido.log"
pa 'save_mc.sh, estados e saves não vazios/iguais: tudo certo'
df -h /mnt/c . | tee "$I/espaco-final.log"
# As provas já foram conferidas. Guardar hashes dos dumps e retirar cópias grandes
# que só serviam de intermediário; em falha, o trap preserva tudo para investigar.
find "$I/roteiros" -type f -name '*.dump' -print0 | xargs -0 -r sha256sum > "$I/dumps.sha256"
find "$I/roteiros" -type f -name '*.dump' -delete
rm -rf "$BASE" "$I/dados"
pa "fim $(date -Is)"
