#!/usr/bin/env bash
# Conferência reproduzível na worktree da tarefa. Quem chama segura flock ~/wsl.lock.
# Uso: alfa_prefixos_rodada.sh PASTA_BASE PASTA_SAIDA
set -euo pipefail
export DEBUGINFOD_URLS=
R=$(cd "$(dirname "$0")/../../.." && pwd)
BASE=$(realpath "$1")
I=$(realpath -m "$2")
mkdir -p "$I"
cd "$R"
exec > >(tee "$I/rodada.log") 2>&1
P=$I/provas.resumo
: > "$P"
pa() { echo "$*" | tee -a "$P"; }
trap 'rc=$?; if [ "$rc" = 0 ]; then pa "tudo certo"; else pa "FALHA RC=$rc"; fi; exit "$rc"' EXIT
pgrep -af 'qemu|roteiros' > "$I/processos.log" || true
if pgrep -x qemu-ppc; then echo 'FALHA: outro emulador'; exit 1; fi
[ -z "$(git status --porcelain)" ]
pa "commit $(git rev-parse HEAD); base 80fd1358; início $(date -Is)"
build() {
    local w=$1 n=$2; shift 2
    (cd "$w"; python3 port/x360/tools/headless.py "$@") > "$I/build-$n.log" 2>&1
    pa "build $n: tudo certo"
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
import pathlib,subprocess,sys
base,final,out=map(pathlib.Path,sys.argv[1:])
a,b=[p/'port/build/x86/obj' for p in (base,final)]
assert {x.name for x in a.glob('*.o')}=={x.name for x in b.glob('*.o')}
dif=[]
for x in sorted(a.glob('*.o')):
    for obj,n in ((x,'obj-a'),(b/x.name,'obj-b')):
        subprocess.run(['objcopy','--strip-debug',str(obj),str(out/n)],check=True)
    if subprocess.run(['cmp','-s',str(out/'obj-a'),str(out/'obj-b')]).returncode: dif.append(x.name)
print('objetos sem depuração diferentes:',dif)
assert set(dif)<={'x360_passos.o'},dif
s=bytearray((out/'obj-a').read_bytes());s[-1]^=1;(out/'obj-b').write_bytes(s)
assert subprocess.run(['cmp','-s',str(out/'obj-a'),str(out/'obj-b')]).returncode==1
PY
pa 'objetos sem depuração iguais salvo carimbo de teste; cmp visto falhar'
for a in x86 x86gv ppcg; do cmp "$BASE/port/build/$a/stubs.txt" "$R/port/build/$a/stubs.txt"; done
pa 'stubs iguais'
bash port/x360/tools/teste_alfa.sh > "$I/unidade-alfa.log" 2>&1
bash port/x360/tools/teste_prefixos.sh > "$I/unidade-prefixos.log" 2>&1
python3 port/x360/tools/prefixos.py --mutantes > "$I/leitor-mutantes.log" 2>&1
pa 'unidades x86/PPC e mutantes: tudo certo'
mkdir -p "$I/dados/mods"
for d in gamedata/*; do ln -s "$(realpath "$d")" "$I/dados/$(basename "$d")"; done
gcc -std=c89 -O2 -w -o "$I/conv_tool" $(find port/x360/conv -maxdepth 1 -name '*.c' ! -name conv_jogo.c | sort)
"$I/conv_tool" --disco --saida "$I/dados/mods" gamedata > "$I/dados.log" 2>&1
pa 'dados sem mapa convertidos'
lista() {
    local w=$1 alvo=$2 r=$3 nome=$4 grav=$5 des=$6 c=$7 f=$8 alfa=$9 px=${10} atraso=${11:-0}
    (
        cd "$w"
        export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
        export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_NORMAL=2 BT3_TEX_ORIGEM=1 BT3_TEMPOS=0
        export BT3_GS_THREAD=$grav BT3_GS_DESENHO=$des BT3_VU_CONSTANTES=$c BT3_VU_FECHO=$f
        export BT3_ALFA_FORA=$alfa BT3_GS_PREFIXOS=$px BT3_PREFIXOS_ATRASO=$atraso BT3_VU_CONST_CONFERE=1
        export BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$I/save
        unset BT3_WIDE BT3_MAPA BT3_GUARDA BT3_PREFIXOS_MUTANTE
        rm -rf "$BT3_SAVES"
        pgrep -af 'qemu|roteiros' >> "$I/processos.log" || true
        if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu'; exit 1; fi
        if [ "$alvo" = ppcg ]; then
            export BT3_DATA=$I/dados BT3_CONVERTIDOS=$I/dados/mods/convertidos.txt BT3_LEU=1 SEMMAPA=1
            timeout 3600 qemu-ppc port/build/ppcg/bt3_ppcg
        else timeout 900 port/build/x86gv/bt3_x86gv; fi
    ) > "$I/$nome.log" 2>&1
    grep -q '^roteiro: .* fim ' "$I/$nome.log"
    grep -q '^gpu_falso: [0-9]* quadros, hash do jogo' "$I/$nome.log"
    if grep -q 'FALHA\|CRASH\|race:\|textura solta usada' "$I/$nome.log"; then return 1; fi
    if [ "$alfa" = 1 ]; then grep -q '^alfa: quadro ' "$I/$nome.log"; fi
    if [ "$px" = 1 ]; then python3 "$R/port/x360/tools/prefixos.py" "$I/$nome.log" > "$I/$nome-prefixos.txt"; fi
    pa "lista $nome: tudo certo"
}
div() {
    python3 port/x360/tools/divergencias.py "$I/$1.log" "$I/$2.log" --normal > "$I/div-$2.txt"
    pa "igualdade $1 / $2: tudo certo"
}
for a in x86gv ppcg; do
    for r in f5_duelo f5_treino; do
        for t in 00 10 11; do
            ref=base-$a-$r-t$t
            lista "$BASE" "$a" "$r" "$ref" "${t:0:1}" "${t:1:1}" 0 0 0 0
            # A matriz dos cortes da seção 28, com alfa desligado/ligado.
            for modo in 00 10 20 01 11 21; do
                for alfa in 0 1; do
                    nome=final-$a-$r-t$t-c$modo-a$alfa
                    lista "$R" "$a" "$r" "$nome" "${t:0:1}" "${t:1:1}" "${modo:0:1}" "${modo:1:1}" "$alfa" 0
                    div "$ref" "$nome"
                done
            done
            # Ensaio no falso, isolado e junto do alfa e dos cortes existentes.
            for modo in 00 21; do
                for alfa in 0 1; do
                    nome=prefixos-$a-$r-t$t-c$modo-a$alfa
                    lista "$R" "$a" "$r" "$nome" "${t:0:1}" "${t:1:1}" "${modo:0:1}" "${modo:1:1}" "$alfa" 1
                    div "$ref" "$nome"
                done
            done
        done
        for atraso in 1 5 20; do
            nome=atraso-$a-$r-$atraso
            lista "$R" "$a" "$r" "$nome" 1 1 2 1 1 1 "$atraso"
            div "base-$a-$r-t11" "$nome"
        done
    done
done
# O verificador por quadro precisa recusar também um resultado real adulterado.
python3 - "$I/final-x86gv-f5_duelo-t00-c00-a1.log" "$I/lista-estragada.log" <<'PY'
from pathlib import Path
import re,sys
s=Path(sys.argv[1]).read_text()
s,n=re.subn(r'(gpu_falso: normal quadro \d+ )[0-9a-f]{16}',r'\g<1>0000000000000000',s,count=1)
assert n==1;Path(sys.argv[2]).write_text(s)
PY
if python3 port/x360/tools/divergencias.py "$I/base-x86gv-f5_duelo-t00.log" "$I/lista-estragada.log" --normal > "$I/mutante-divergencias.txt"; then exit 1; fi
pa 'divergencias recusou lista adulterada'
# Critérios oficiais com as builds do commit final e referências em pasta própria.
build "$R" final-x86r --alvo x86 --rastreio
build "$R" final-ppc --alvo ppc
mkdir -p "$I/referencias"
cp /home/codex/referencias/*.telas "$I/referencias/"
env REF=1 REFS="$I/referencias" O="$I/roteiros" SEMMAPA=1 IMG=1 bash port/x360/tools/roteiros.sh f5_duelo f5_treino > "$I/roteiros.log" 2>&1
pa 'roteiros f5_duelo e f5_treino: tudo certo'
env REFS="$I/referencias" O="$I/roteiros" SEMMAPA=1 IMG=1 bash port/x360/tools/roteiros.sh duelo > "$I/duelo.log" 2>&1
pa 'roteiro duelo: tudo certo'
bash port/x360/tools/save_mc.sh > "$I/save_mc.log" 2>&1
python3 - "$R/port/build/save_mc" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
for n,r in [('A','1'),('B1','2'),('B2','2')]:
    s=(p/f'estados_{n}.txt').read_bytes(); assert s.strip() and s==(p/f'estados_x86_{r}.txt').read_bytes(),n
f=Path('card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3')
s=(p/'c_A'/f).read_bytes();assert s and s==(p/'c_B2'/f).read_bytes()
print('save_mc: estados não vazios iguais e recuperação byte a byte')
PY
pa 'save_mc: tudo certo'
rm -rf "$I/save"
[ -z "$(git status --porcelain)" ]
pa "fim $(date -Is)"
