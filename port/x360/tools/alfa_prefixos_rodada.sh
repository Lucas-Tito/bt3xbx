#!/usr/bin/env bash
# Conferência reproduzível do alfa só na compilação e do segundo desenho dos prefixos (F5, seção 36), na worktree da
# tarefa. Quem chama segura flock ~/wsl.lock (um ciclo de emulador por vez).
# Uso: alfa_prefixos_rodada.sh PASTA_BASE PASTA_SAIDA
#   PASTA_BASE: worktree da main sem o alfa (a base do merge), com asm/assets/disc/gamedata ligados como nesta.
# Prova, nesta ordem: os fontes que o console compila são o texto da base na build normal (alfa_igual.py); a build
# normal dá os mesmos objetos da base (x86 plana, x86gv e PPCg, salvo o carimbo de teste e o falso, que tem os
# prefixos); a build com o alfa só muda a gravação e o falso; a .text; os stubs; as unidades com mutantes; a matriz da
# seção 28 com o alfa (Duelo/Treino, sem threads/gravação/desenho, cortes 0/1/2 x 0/1) contra a base em listas
# normalizadas; os prefixos 1 (barreira integral) e 2 (regras reais) com atrasos 1/5/20 ms; os contadores de alfa
# iguais x86/PPC; o teto com as curvas medidas no falso; e os critérios oficiais (roteiros com REF=1, duelo, save_mc).
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
[ -z "$(git -C "$BASE" status --porcelain)" ]
pa "commit $(git rev-parse HEAD); base $(git -C "$BASE" rev-parse HEAD); início $(date -Is)"
python3 port/x360/tools/alfa_igual.py "$BASE" --mutantes > "$I/alfa-igual.log" 2>&1
pa 'fontes do console na build normal iguais à base, texto a texto; quatro mutantes recusados'
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
build "$R" final-x86gvl --alvo x86 --gpu --vu1 --alfa
build "$R" final-ppcgl --alvo ppc --gpu --alfa
cmp "$I/text-base.bin" "$I/text-final.bin"
python3 - "$BASE" "$R" "$I" <<'PY'
import pathlib,subprocess,sys
base,final,out=map(pathlib.Path,sys.argv[1:])
def compara(a,b):
    a,b=a/'obj',b/'obj'
    assert {x.name for x in a.glob('*.o')}=={x.name for x in b.glob('*.o')},(a,b)
    dif=set()
    oc='powerpc-linux-gnu-objcopy' if a.parent.name.startswith('ppc') else 'objcopy'  # (o objcopy do x86 não lê PowerPC)
    for x in sorted(a.glob('*.o')):
        for obj,n in ((x,'obj-a'),(b/x.name,'obj-b')):
            subprocess.run([oc,'--strip-debug',str(obj),str(out/n)],check=True)
        if subprocess.run(['cmp','-s',str(out/'obj-a'),str(out/'obj-b')]).returncode: dif.add(x.name)
    return dif
B,F=base/'port/build',final/'port/build'
for alvo,permitido in (('x86',{'x360_passos.o'}),('x86gv',{'x360_passos.o','x360_gpu_falso.o'}),('ppcg',{'x360_passos.o','x360_gpu_falso.o'})):
    d=compara(B/alvo,F/alvo); print(alvo,'base x final, diferentes sem depuração:',sorted(d)); assert d<=permitido,(alvo,d)
for a,b in (('x86gv','x86gvl'),('ppcg','ppcgl')):
    d=compara(F/a,F/b); print(a,'normal x alfa:',sorted(d)); assert d=={'gs_gs_draw.o','x360_gpu_falso.o'},(a,d)
s=bytearray((out/'obj-a').read_bytes());s[-1]^=1;(out/'obj-b').write_bytes(s)
assert subprocess.run(['cmp','-s',str(out/'obj-a'),str(out/'obj-b')]).returncode==1
PY
pa 'objetos sem depuração: normal igual à base salvo carimbo e falso; o alfa só muda a gravação e o falso; cmp visto falhar'
for a in x86 x86gv ppcg; do cmp "$BASE/port/build/$a/stubs.txt" "$R/port/build/$a/stubs.txt"; done
cmp port/build/x86gv/stubs.txt port/build/x86gvl/stubs.txt; cmp port/build/ppcg/stubs.txt port/build/ppcgl/stubs.txt
pa 'stubs iguais'
bash port/x360/tools/teste_alfa.sh > "$I/unidade-alfa.log" 2>&1
bash port/x360/tools/teste_prefixos.sh > "$I/unidade-prefixos.log" 2>&1
python3 port/x360/tools/prefixos.py --mutantes > "$I/leitor-mutantes.log" 2>&1
python3 port/x360/tools/prefixos_teto.py --mutantes > "$I/teto-mutantes.log" 2>&1
pa 'unidades x86/PPC e mutantes (alfa, prefixos 1 e 2, leitor, teto): tudo certo'
mkdir -p "$I/dados/mods"
for d in gamedata/*; do ln -s "$(realpath "$d")" "$I/dados/$(basename "$d")"; done
gcc -std=c89 -O2 -w -o "$I/conv_tool" $(find port/x360/conv -maxdepth 1 -name '*.c' ! -name conv_jogo.c | sort)
"$I/conv_tool" --disco --saida "$I/dados/mods" gamedata > "$I/dados.log" 2>&1
pa 'dados sem mapa convertidos'
NALFA=0
lista() {
    local w=$1 exe=$2 r=$3 nome=$4 grav=$5 des=$6 c=$7 f=$8 px=$9 atraso=${10:-0}
    (
        cd "$w"
        export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
        export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_NORMAL=2 BT3_TEX_ORIGEM=1 BT3_TEMPOS=0
        export BT3_GS_THREAD=$grav BT3_GS_DESENHO=$des BT3_VU_CONSTANTES=$c BT3_VU_FECHO=$f
        export BT3_GS_PREFIXOS=$px BT3_PREFIXOS_ATRASO=$atraso BT3_VU_CONST_CONFERE=1
        export BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$I/save
        unset BT3_WIDE BT3_MAPA BT3_GUARDA BT3_PREFIXOS_MUTANTE BT3_PREFIXOS_QUADROS BT3_ALFA_FORA
        rm -rf "$BT3_SAVES"
        pgrep -af 'qemu|roteiros' >> "$I/processos.log" || true
        if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu'; exit 1; fi
        if [ "${exe#ppcg}" != "$exe" ]; then
            export BT3_DATA=$I/dados BT3_CONVERTIDOS=$I/dados/mods/convertidos.txt BT3_LEU=1 SEMMAPA=1
            timeout 3600 qemu-ppc port/build/$exe/bt3_$exe
        else timeout 900 port/build/$exe/bt3_$exe; fi
    ) > "$I/$nome.log" 2>&1
    grep -q '^roteiro: .* fim ' "$I/$nome.log"
    grep -q '^gpu_falso: [0-9]* quadros, hash do jogo' "$I/$nome.log"
    if grep -q 'FALHA\|CRASH\|race:\|textura solta usada' "$I/$nome.log"; then return 1; fi
    if [ "${exe%l}" != "$exe" ]; then grep -q '^alfa: quadro ' "$I/$nome.log"; NALFA=$((NALFA + 1))
    elif grep -q '^alfa: quadro ' "$I/$nome.log"; then echo "FALHA: alfa na build normal ($nome)"; return 1; fi
    if [ "$px" != 0 ]; then python3 "$R/port/x360/tools/prefixos.py" "$I/$nome.log" > "$I/$nome-prefixos.txt"; fi
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
            lista "$BASE" "$a" "$r" "$ref" "${t:0:1}" "${t:1:1}" 0 0 0
            # A matriz dos cortes da seção 28 na build com o alfa (a normal tem os objetos da base, conferidos acima).
            for modo in 00 10 20 01 11 21; do
                nome=alfa-${a}l-$r-t$t-c$modo
                lista "$R" "${a}l" "$r" "$nome" "${t:0:1}" "${t:1:1}" "${modo:0:1}" "${modo:1:1}" 0
                div "$ref" "$nome"
            done
            # Prefixos no falso: a barreira integral (1) e as regras reais (2), na build normal e na com o alfa.
            for caso in "px1-$a:$a:21:1" "px2-$a:$a:00:2" "px2-$a:$a:21:2" "px2-${a}l:${a}l:21:2"; do
                IFS=: read -r pre exe modo px <<< "$caso"
                nome=${pre}-$r-t$t-c$modo
                lista "$R" "$exe" "$r" "$nome" "${t:0:1}" "${t:1:1}" "${modo:0:1}" "${modo:1:1}" "$px"
                div "$ref" "$nome"
            done
        done
        for atraso in 1 5 20; do
            nome=atraso-${a}l-$r-$atraso
            lista "$R" "${a}l" "$r" "$nome" 1 1 2 1 2 "$atraso"
            div "base-$a-$r-t11" "$nome"
        done
    done
done
# Os dois contadores de alfa por quadro também têm de concordar entre arquiteturas, em todas as listas com o alfa.
python3 - "$I" "$NALFA" <<'PYALFA'
from pathlib import Path
import re,sys
p=Path(sys.argv[1]); esperado=int(sys.argv[2])//2; padrao=re.compile(r'^alfa: quadro (\d+) desenhos (\d+) altos (\d+), 0 diferentes$',re.M)
n=0
for x in sorted(p.glob('*x86gvl*.log')):
    if x.name.startswith('build-'): continue
    v=padrao.findall(x.read_text(errors='replace'))
    assert v,x.name
    outro=p/x.name.replace('x86gvl','ppcgl')
    assert outro.exists(),outro
    w=padrao.findall(outro.read_text(errors='replace'))
    assert len(v)==len({q for q,_,_ in v}) and v==w,x.name
    n+=1
assert n==esperado and n==48,(n,esperado)
print(f'alfa: {n} pares x86/PPC, dois contadores iguais em cada quadro')
PYALFA
pa 'estatística de alfa por quadro igual entre arquiteturas, 48 pares'
# O verificador por quadro precisa recusar também um resultado real adulterado.
python3 - "$I/alfa-x86gvl-f5_duelo-t00-c00.log" "$I/lista-estragada.log" <<'PY'
from pathlib import Path
import re,sys
s=Path(sys.argv[1]).read_text()
s,n=re.subn(r'(gpu_falso: normal quadro \d+ )[0-9a-f]{16}',r'\g<1>0000000000000000',s,count=1)
assert n==1;Path(sys.argv[2]).write_text(s)
PY
if python3 port/x360/tools/divergencias.py "$I/base-x86gv-f5_duelo-t00.log" "$I/lista-estragada.log" --normal > "$I/mutante-divergencias.txt"; then exit 1; fi
pa 'divergencias recusou lista adulterada'
# O teto dos prefixos com o relógio da gravação medido no falso (sem threads), além dos modelos U e W.
for a in x86gv ppcg; do
    python3 port/x360/tools/prefixos_teto.py /home/codex/f5-gpu-comum/listas-baed-x86gv /home/codex/f5-gpu-fina/rodada/dados.json \
        --aberturas g6930_1,g6960_1,g6930_2,g6960_2 --curva "$I/px2-$a-f5_duelo-t00-c00.log" > "$I/teto-$a.txt"
done
pa 'teto dos prefixos com as curvas x86 e PPC: teto-x86gv.txt, teto-ppcg.txt'
# Critérios oficiais com as builds normais do commit final e referências em pasta própria.
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
rm -rf "$I/save" "$I/obj-a" "$I/obj-b"
[ -z "$(git status --porcelain)" ]
pa "fim $(date -Is)"
