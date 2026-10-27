#!/usr/bin/env bash
# F5: rodada longa em worktrees próprios; passar SHA final e SHA base, ambos já commitados.
# systemd-run --user --unit=bt3-clut16 -p MemoryMax=5G bash .../clut16_rodada.sh FINAL BASE
# Um ciclo por vez, sem mapa na PPC. Nada é enviado ao console nem ao git pelo serviço.
set -euo pipefail
export DEBUGINFOD_URLS=
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
FINAL=$(git -C "$RAIZ" rev-parse "$1")
BASE=$(git -C "$RAIZ" rev-parse "$2")
I=$HOME/f5-clut16
mkdir -p "$I"
exec > >(tee "$I/rodada.log") 2>&1
P=$I/provas.resumo
: > "$P"
pa() { echo "$*" | tee -a "$P"; }
termina() {
    local rc=$?
    trap - EXIT
    for w in "$I/rodada-base" "$I/rodada-final"; do
        if [ -d "$w/.git" ] || [ -f "$w/.git" ]; then
            git -C "$w" restore -- port/src/gs/gs_draw.c
            rm -f "$w/port/x360/headless/clut16_diag.h.base"
            if [ "$w" = "$I/rodada-base" ]; then rm -f "$w/port/x360/headless/clut16_diag.h"; fi
            git -C "$w" status --porcelain >> "$I/estado-final.log"
        fi
    done
    rm -rf "$I/save-rodada"
    if [ "$rc" = 0 ]; then pa 'tudo certo'; else pa "FALHA RC=$rc; conferir rodada.log e o último passo do resumo"; fi
    pa 'FIM'
    exit "$rc"
}
trap termina EXIT
# Cada trecho pesado tem o mesmo bloqueio usado nas demais rodadas do WSL.
t() { ( flock 9; "$@" ) 9>>"$HOME/wsl.lock"; }
build() {
    local w=$1 nome=$2; shift 2
    (cd "$w"; python3 port/x360/tools/headless.py "$@") > "$I/build-$nome.log" 2>&1
    pa "build $nome: tudo certo"
}
lista() {
    local w=$1 alvo=$2 r=$3 nome=$4 diag=$5
    (
        cd "$w"
        export BT3_GS=gpu BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
        export BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_GPU_LISTA=1 BT3_GPU_NORMAL=2 BT3_TEX_ORIGEM=0
        unset BT3_GS_THREAD BT3_GS_DESENHO BT3_WIDE BT3_MAPA BT3_GUARDA BT3_CLUT16_DIAG
        [ "$r" != shenlong ] || export BT3_WIDE=1
        [ "$diag" = 0 ] || export BT3_CLUT16_DIAG=1
        export BT3_ROTEIRO=port/x360/roteiros/$r.txt BT3_SAVES=$I/save-rodada
        rm -rf "$BT3_SAVES"
        pgrep -af 'qemu|roteiros' >> "$I/processos.log" || true
        if pgrep -x qemu-ppc; then echo 'FALHA: outro qemu em execução'; exit 1; fi
        if [ "$alvo" = ppcg ]; then
            export BT3_DATA=$I/dados BT3_CONVERTIDOS=$I/dados/mods/convertidos.txt BT3_LEU=1 SEMMAPA=1
            timeout 3600 qemu-ppc port/build/ppcg/bt3_ppcg
        else
            timeout 900 port/build/x86gv/bt3_x86gv
        fi
    ) > "$I/$nome-$r.log" 2>&1
    grep -q '^roteiro: .* fim ' "$I/$nome-$r.log"
    grep -q '^gpu_falso: [0-9]* quadros, hash do jogo' "$I/$nome-$r.log"
    if grep -q 'FALHA\|CRASH\|race:' "$I/$nome-$r.log"; then return 1; fi
    pa "$nome $r: $(grep '^gpu_falso: [0-9]* quadros, hash do jogo' "$I/$nome-$r.log")"
}
div() {
    local nome=$1 a=$2 b=$3
    python3 "$RAIZ/port/x360/tools/divergencias.py" "$a" "$b" --normal > "$I/div-$nome.txt"
    pa "divergencias $nome: tudo certo"
}
pa "final $FINAL; base $BASE; início $(date -Is)"
for par in "base:$BASE" "final:$FINAL"; do
    nome=${par%%:*}; sha=${par#*:}; w=$I/rodada-$nome
    [ ! -e "$w" ] # Não reaproveitar uma árvore que possa guardar sabotagens.
    git -C "$RAIZ" worktree add --detach "$w" "$sha"
    for d in asm assets disc gamedata; do ln -s /srv/bt3/dados/$d "$w/$d"; done
    mkdir -p "$w/port/build/gen"
    cp -r "$RAIZ/port/build/gen/data" "$RAIZ/port/build/gen/gs" "$w/port/build/gen/"
    t build "$w" "$nome-x86" --alvo x86
    objcopy -O binary --only-section=.text "$w/port/build/x86/bt3_x86" "$I/text-$nome.bin"
    pa ".text $nome: $(sha1sum "$I/text-$nome.bin" | cut -d' ' -f1)"
    [ "$(sha1sum "$I/text-$nome.bin" | cut -d' ' -f1)" = 43444ea4d03a6d448aa1ae74a631e35b356a7caa ]
    t build "$w" "$nome-x86gv" --alvo x86 --gpu --vu1
    t build "$w" "$nome-ppcg" --alvo ppc --gpu
    test -z "$(git -C "$w" status --porcelain)"
done
python3 - "$I" <<'PY'
import pathlib, subprocess, sys
p = pathlib.Path(sys.argv[1])
a, b = [p / ('rodada-' + n) / 'port/build/x86/obj' for n in ('base', 'final')]
assert {x.name for x in a.glob('*.o')} == {x.name for x in b.glob('*.o')}
dif = []
for x in a.glob('*.o'):
    for arq, n in ((x, 'a'), (b / x.name, 'b')):
        subprocess.run(['objcopy', '--strip-debug', str(arq), str(p / ('obj-' + n))], check=True)
    if subprocess.run(['cmp', '-s', str(p / 'obj-a'), str(p / 'obj-b')]).returncode:
        dif.append(x.name)
print('objetos diferentes sem depuração:', dif)
assert set(dif) <= {'x360_passos.o'} # Só o carimbo do código de teste pode mudar.
# Ver cmp recusar um byte estragado numa cópia.
f = p / 'obj-b'; s = bytearray((p / 'obj-a').read_bytes()); s[-1] ^= 1; f.write_bytes(s)
assert subprocess.run(['cmp', '-s', str(p / 'obj-a'), str(f)]).returncode == 1
PY
pa 'objetos sem depuração: tudo certo; cmp viu a cópia estragada'
cd "$I/rodada-final"
port/x360/tools/teste_clut16.sh > "$I/final-unidade-x86.log" 2>&1
ALVO=ppc port/x360/tools/teste_clut16.sh > "$I/final-unidade-ppc.log" 2>&1
pa 'unidade x86 e PPC, chave antiga e logs estragados: tudo certo'
# Dados convertidos por formato para todos os três roteiros, sem mapa por posição.
mkdir -p "$I/dados/mods"
for d in gamedata/*; do ln -s "$(realpath "$d")" "$I/dados/$(basename "$d")"; done
gcc -std=c89 -O2 -w -o "$I/conv_tool" $(find port/x360/conv -maxdepth 1 -name '*.c' ! -name conv_jogo.c | sort)
t "$I/conv_tool" --disco --saida "$I/dados/mods" gamedata > "$I/dados.log" 2>&1
pa 'dados convertidos sem mapa: tudo certo'
for nome in base final; do
    w=$I/rodada-$nome
    for alvo in x86gv ppcg; do
        for r in f5_duelo f5_treino shenlong; do
            t lista "$w" "$alvo" "$r" "$nome-$alvo" 0
            if [ "$nome" = final ]; then
                div "$alvo-$r" "$I/base-$alvo-$r.log" "$I/final-$alvo-$r.log"
            fi
        done
    done
    # Só a x86 precisa do diagnóstico. Depois se confere a lista instrumentada contra a limpa.
    if [ "$nome" = base ]; then cp "$RAIZ/port/x360/headless/clut16_diag.h" "$w/port/x360/headless/"; fi
    python3 "$RAIZ/port/x360/tools/clut16.py" diagnostico "$w/port/src/gs/gs_draw.c"
    t build "$w" "$nome-diagnostico" --alvo x86 --gpu --vu1
    for r in f5_duelo f5_treino shenlong; do
        t lista "$w" x86gv "$r" "$nome-diag" 1
        python3 "$RAIZ/port/x360/tools/clut16_confere.py" "$I/$nome-diag-$r.log" > "$I/confere-$nome-$r.txt"
        div "$nome-instrumentacao-$r" "$I/$nome-x86gv-$r.log" "$I/$nome-diag-$r.log"
        pa "diagnóstico $nome $r: $(head -1 "$I/confere-$nome-$r.txt")"
    done
    git -C "$w" restore -- port/src/gs/gs_draw.c
    if [ "$nome" = base ]; then rm "$w/port/x360/headless/clut16_diag.h"; fi
    t build "$w" "$nome-x86gv-restaurada" --alvo x86 --gpu --vu1
done
# O comparador oficial deve recusar uma lista normal com um quadro adulterado.
python3 - "$I/final-x86gv-f5_duelo.log" "$I/lista-estragada.log" <<'PY'
import pathlib, re, sys
s = pathlib.Path(sys.argv[1]).read_text()
s, n = re.subn(r'(gpu_falso: normal quadro \d+ )[0-9a-f]+', r'\g<1>0000000000000000', s, count=1)
assert n == 1
pathlib.Path(sys.argv[2]).write_text(s)
PY
if python3 "$RAIZ/port/x360/tools/divergencias.py" "$I/final-x86gv-f5_duelo.log" "$I/lista-estragada.log" --normal > "$I/div-negativo.txt"; then
    pa 'FALHA: divergencias aceitou a cópia estragada'; exit 1
fi
pa 'divergencias viu a lista estragada: tudo certo'
pa "fim das provas $(date -Is)"
