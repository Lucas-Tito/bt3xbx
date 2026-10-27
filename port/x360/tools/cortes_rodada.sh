#!/usr/bin/env bash
# Rodada final em árvores próprias. Uso: cortes_rodada.sh FINAL BASE.
# O serviço de usuário recebe MemoryMax=5G. Toda build e todo emulador passam pela trava comum.
set -euo pipefail
export DEBUGINFOD_URLS=
ORIGEM=$(cd "$(dirname "$0")/../../.." && pwd)
FINAL=$(git -C "$ORIGEM" rev-parse "$1")
BASE=$(git -C "$ORIGEM" rev-parse "$2")
I=$HOME/f5-cortes/final
mkdir -p "$I"
exec > >(tee "$I/rodada.log") 2>&1
P=$I/provas.resumo
: > "$P"
pa() { echo "$*" | tee -a "$P"; }
fim() {
    local rc=$?
    trap - EXIT
    if [ -d "$I/final" ]; then
        git -C "$I/final" restore -- port/x360/include/vu_iguais.h
        git -C "$I/final" status --porcelain > "$I/estado-final.log"
    fi
    rm -rf "$I/save"
    if [ "$rc" = 0 ]; then pa 'tudo certo'; else pa "FALHA RC=$rc; conferir rodada.log"; fi
    pa FIM
    exit "$rc"
}
trap fim EXIT
t() {
    (
        flock 9
        pgrep -af 'qemu|roteiros' >> "$I/processos.log" || true
        if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu'; exit 1; fi
        "$@"
    ) 9>>"$HOME/wsl.lock"
}
build() {
    local w=$1 nome=$2; shift 2
    (cd "$w"; python3 port/x360/tools/headless.py "$@") > "$I/build-$nome.log" 2>&1
    pa "build $nome: tudo certo"
}
lista() {
    local w=$1 alvo=$2 r=$3 nome=$4 thread=$5 constantes=$6 fecho=$7 diag=${8:-0}
    (
        cd "$w"
        export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
        export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_NORMAL=2 BT3_TEX_ORIGEM=1 BT3_TEMPOS=0
        export BT3_GS_THREAD=$thread BT3_GS_DESENHO=$thread BT3_VU_CONSTANTES=$constantes BT3_VU_FECHO=$fecho BT3_CORTES=$diag BT3_VU_CONST_CONFERE=1
        export BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$I/save
        unset BT3_WIDE BT3_MAPA BT3_GUARDA
        rm -rf "$BT3_SAVES"
        pgrep -af 'qemu|roteiros' >> "$I/processos.log" || true
        if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu'; exit 1; fi
        if [ "$alvo" = ppcg ]; then
            export BT3_DATA=$I/dados BT3_CONVERTIDOS=$I/dados/mods/convertidos.txt BT3_LEU=1 SEMMAPA=1
            timeout 3600 qemu-ppc port/build/ppcg/bt3_ppcg
        else
            timeout 900 port/build/x86gv/bt3_x86gv
        fi
    ) > "$I/$nome.log" 2>&1
    grep -q '^roteiro: .* fim ' "$I/$nome.log"
    grep -q '^gpu_falso: [0-9]* quadros, hash do jogo' "$I/$nome.log"
    if grep -q 'FALHA\|CRASH\|race:' "$I/$nome.log"; then return 1; fi
    pa "lista $nome: tudo certo"
}
div() {
    python3 "$I/final/port/x360/tools/divergencias.py" "$I/$1.log" "$I/$2.log" --normal > "$I/div-$2.txt"
    pa "divergencias $1 / $2: tudo certo"
}
pa "final $FINAL; base $BASE; início $(date -Is)"
for par in "base:$BASE" "final:$FINAL"; do
    nome=${par%%:*}; sha=${par#*:}; w=$I/$nome
    [ ! -e "$w" ]
    git -C "$ORIGEM" worktree add --detach "$w" "$sha"
    for d in asm assets disc gamedata; do ln -s /srv/bt3/dados/$d "$w/$d"; done
    mkdir -p "$w/port/build/gen"
    cp -r "$ORIGEM/port/build/gen/data" "$ORIGEM/port/build/gen/gs" "$w/port/build/gen/"
    t build "$w" "$nome-x86" --alvo x86
    objcopy -O binary --only-section=.text "$w/port/build/x86/bt3_x86" "$I/text-$nome.bin"
    [ "$(sha1sum "$I/text-$nome.bin" | cut -d' ' -f1)" = 43444ea4d03a6d448aa1ae74a631e35b356a7caa ]
    pa ".text $nome: 43444ea4d03a6d448aa1ae74a631e35b356a7caa"
    t build "$w" "$nome-x86gv" --alvo x86 --gpu --vu1
    t build "$w" "$nome-ppcg" --alvo ppc --gpu
    test -z "$(git -C "$w" status --porcelain)"
done
for alvo in x86 x86gv ppcg; do
    cmp "$I/base/port/build/$alvo/stubs.txt" "$I/final/port/build/$alvo/stubs.txt"
done
pa 'stubs iguais nas três builds'
python3 - "$I" <<'PY'
import pathlib, subprocess, sys
p = pathlib.Path(sys.argv[1])
a, b = [p / n / 'port/build/x86/obj' for n in ('base', 'final')]
assert {x.name for x in a.glob('*.o')} == {x.name for x in b.glob('*.o')}
dif = []
for x in a.glob('*.o'):
    for obj, nome in ((x, 'obj-a'), (b / x.name, 'obj-b')):
        subprocess.run(['objcopy', '--strip-debug', str(obj), str(p / nome)], check=True)
    if subprocess.run(['cmp', '-s', str(p / 'obj-a'), str(p / 'obj-b')]).returncode: dif.append(x.name)
print('objetos diferentes sem depuração:', dif)
assert set(dif) <= {'x360_passos.o'}
f = p / 'obj-b'; s = bytearray((p / 'obj-a').read_bytes()); s[-1] ^= 1; f.write_bytes(s)
assert subprocess.run(['cmp', '-s', str(p / 'obj-a'), str(f)]).returncode == 1
PY
pa 'objetos sem depuração iguais, salvo carimbo de teste; cópia estragada acusada'
cd "$I/final"
t python3 port/x360/tools/teste_cortes.py > "$I/diagnostico-unidade.log" 2>&1
t bash port/x360/tools/teste_vu_cortes.sh > "$I/cortes-unidade.log" 2>&1
pa 'unidade e controles negativos: tudo certo'
mkdir -p "$I/dados/mods"
for d in gamedata/*; do ln -s "$(realpath "$d")" "$I/dados/$(basename "$d")"; done
t gcc -std=c89 -O2 -w -o "$I/conv_tool" $(find port/x360/conv -maxdepth 1 -name '*.c' ! -name conv_jogo.c | sort)
t "$I/conv_tool" --disco --saida "$I/dados/mods" gamedata > "$I/dados.log" 2>&1
pa 'dados sem mapa: tudo certo'
for alvo in x86gv ppcg; do
    for r in f5_duelo f5_treino; do
        for thread in 0 1; do
            ref=base-$alvo-$r-t$thread
            t lista "$I/base" "$alvo" "$r" "$ref" "$thread" 0 0
            for modo in 00 10 20 01 11 21; do
                nome=final-$alvo-$r-t$thread-c$modo
                t lista "$I/final" "$alvo" "$r" "$nome" "$thread" "${modo:0:1}" "${modo:1:1}"
                div "$ref" "$nome"
            done
        done
    done
done
# Perfil pontual conferido também contra a lista limpa, nas duas configurações de threads.
for thread in 0 1; do
    nome=diagnostico-t$thread
    t lista "$I/final" x86gv f5_duelo "$nome" "$thread" 0 0 6900,7025
    python3 port/x360/tools/cortes.py "$I/$nome.log" > "$I/$nome-tabela.md"
    div "final-x86gv-f5_duelo-t$thread-c00" "$nome"
done
pa 'diagnóstico e listas sem efeito da instrumentação: tudo certo'
# Sabotagem em cópia de trabalho isolada: aceita uniformes distintos, forçando junções erradas.
python3 - <<'PY'
from pathlib import Path
p = Path('port/x360/include/vu_iguais.h'); s = p.read_text(); a = '    unsigned i = 0;'; assert a in s
p.write_text(s.replace(a, a + '\n    return 1; /* sabotagem da igualdade */', 1))
PY
t build "$I/final" sabotagem --alvo x86 --gpu --vu1
for r in f5_duelo f5_treino; do
    nome=sabotagem-$r
    t lista "$I/final" x86gv "$r" "$nome" 0 0 1
    if python3 port/x360/tools/divergencias.py "$I/final-x86gv-$r-t0-c01.log" "$I/$nome.log" --normal > "$I/div-$nome.txt"; then
        pa "FALHA: sabotagem aceita em $r"; exit 1
    fi
    pa "sabotagem acusada em $r"
done
git restore -- port/x360/include/vu_iguais.h
t build "$I/final" restaurada --alvo x86 --gpu --vu1
bash port/x360/tools/cortes_medir.sh "$I/sem-listas" 3 > "$I/sem-listas.log" 2>&1
pa 'medição sem marcas e sem listas: tudo certo'
# Critérios oficiais com os binários do commit final, em série e sem mapa na PPC.
t build "$I/final" x86r --alvo x86 --rastreio
t build "$I/final" ppc --alvo ppc
# O lock cobre cada roteiro completo, incluindo builds auxiliares e conversores.
t env REF=1 SEMMAPA=1 IMG=1 bash port/x360/tools/roteiros.sh f5_duelo f5_treino > "$I/roteiros.log" 2>&1
pa 'roteiros f5_duelo e f5_treino: tudo certo'
t env SEMMAPA=1 IMG=1 bash port/x360/tools/roteiros.sh duelo > "$I/duelo.log" 2>&1
pa 'roteiro duelo: tudo certo'
t bash port/x360/tools/save_mc.sh > "$I/save_mc.log" 2>&1
python3 - "$I/final/port/build/save_mc" <<'PYMC'
from pathlib import Path
import sys
p = Path(sys.argv[1])
for nome, ref in [('A', '1'), ('B1', '2'), ('B2', '2')]:
    estado = (p / ('estados_' + nome + '.txt')).read_bytes()
    assert estado.strip() and estado == (p / ('estados_x86_' + ref + '.txt')).read_bytes(), nome
n = 'BASLUS-21678DBZT3'
f = Path('card1') / n / n
save = (p / 'c_A' / f).read_bytes()
assert save and save == (p / 'c_B2' / f).read_bytes()
print('save_mc: estados e recuperação iguais')
PYMC
pa 'save_mc: tudo certo' 
test -z "$(git status --porcelain)"
pa "fim das provas $(date -Is)"
