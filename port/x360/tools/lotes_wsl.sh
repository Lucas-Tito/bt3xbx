#!/usr/bin/env bash
# Conferência no WSL da seção 37 do 08-gravacao.md (o relógio da gravação e a primeira parte do BT3_GS_LOTES), sem
# emulador: as duas mudanças só existem no console (o relógio com PORT_X360 e BT3_PERFIL_CADEIA, os lotes no
# gs_d3d.cpp), então as builds x86 e PPC têm de dar os mesmos objetos da base, e com objetos iguais as provas de
# execução da seção 28 (roteiros, listas, save) não mudam. Prova também as unidades e os mutantes das ferramentas.
# Uso: lotes_wsl.sh PASTA_BASE PASTA_SAIDA
#   PASTA_BASE: worktree no commit de base (d3b96c07, o alfa só na compilação), com asm/assets/disc/gamedata ligados.
# Builds pesadas: rodar com nice e sem outro build ao lado; não usa o emulador (não pega o ~/wsl.lock).
set -euo pipefail
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
[ -z "$(git status --porcelain)" ]
[ -z "$(git -C "$BASE" status --porcelain)" ]
pa "commit $(git rev-parse HEAD); base $(git -C "$BASE" rev-parse HEAD); início $(date -Is)"
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
    build "$w" "$n-ppcgl" --alvo ppc --gpu --alfa
    objcopy -O binary --only-section=.text "$w/port/build/x86/bt3_x86" "$I/text-$n.bin"
    sha1sum "$I/text-$n.bin" | tee -a "$P"
done
cmp "$I/text-base.bin" "$I/text-final.bin"
pa '.text da x86 igual à da base'
python3 - "$BASE" "$R" "$I" <<'PY'
import pathlib,subprocess,sys
base,final,out=map(pathlib.Path,sys.argv[1:])
def compara(a,b):
    a,b=a/'obj',b/'obj'
    assert {x.name for x in a.glob('*.o')}=={x.name for x in b.glob('*.o')},(a,b)
    dif=set()
    oc='powerpc-linux-gnu-objcopy' if a.parent.name.startswith('ppc') else 'objcopy'
    for x in sorted(a.glob('*.o')):
        for obj,n in ((x,'obj-a'),(b/x.name,'obj-b')):
            subprocess.run([oc,'--strip-debug',str(obj),str(out/n)],check=True)
        if subprocess.run(['cmp','-s',str(out/'obj-a'),str(out/'obj-b')]).returncode: dif.add(x.name)
    return dif
B,F=base/'port/build',final/'port/build'
for alvo in ('x86','x86gv','ppcg','ppcgl'):
    d=compara(B/alvo,F/alvo); print(alvo,'base x final, diferentes sem depuração:',sorted(d)); assert d<={'x360_passos.o'},(alvo,d)
s=bytearray((out/'obj-a').read_bytes());s[-1]^=1;(out/'obj-b').write_bytes(s)
assert subprocess.run(['cmp','-s',str(out/'obj-a'),str(out/'obj-b')]).returncode==1
PY
pa 'objetos sem depuração (x86, x86gv, PPCg, PPCg com o alfa) iguais aos da base salvo o carimbo; cmp visto falhar'
for a in x86 x86gv ppcg ppcgl; do cmp "$BASE/port/build/$a/stubs.txt" "$R/port/build/$a/stubs.txt"; done
pa 'stubs iguais'
python3 port/x360/tools/teste_relogio.py --saida "$I/relogio" > "$I/unidade-relogio.log" 2>&1
python3 port/x360/tools/teste_lotes_fonte.py > "$I/fonte-lotes.log" 2>&1
python3 port/x360/tools/prefixos_teto.py --mutantes > "$I/teto-mutantes.log" 2>&1
python3 port/x360/tools/lotes_console.py --mutantes > "$I/console-mutantes.log" 2>&1
pa 'unidades e mutantes (relógio, fonte dos lotes, teto com o relógio do console, ambientes): tudo certo'
pa "fim $(date -Is)"
