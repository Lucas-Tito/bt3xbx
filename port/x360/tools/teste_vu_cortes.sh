#!/usr/bin/env bash
# Quem chama segura flock ~/wsl.lock, inclusive durante a compilação e o qemu.
set -euo pipefail
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$(mktemp -d "$HOME/teste-vu-cortes.XXXXXX")
trap 'rm -rf "$P"' EXIT
mkdir -p "$P/include" "$P/tools"
cp "$R/port/x360/include/vu_iguais.h" "$R/port/x360/include/vu_constantes.h" "$R/port/x360/include/vu4_constantes.h" "$R/port/src/gs/gs_draw.h" "$P/include/"
cp "$R/port/x360/tools/teste_vu_cortes.c" "$P/tools/"
pgrep -af "qemu|roteiros" || true
if pgrep -x qemu-ppc; then echo "FALHA: outro qemu"; exit 1; fi
for alvo in x86 ppc; do
    if [ "$alvo" = x86 ]; then cc=(gcc -m32); executa=(); else cc=(powerpc-linux-gnu-gcc -static -fsigned-char); executa=(qemu-ppc); fi
    "${cc[@]}" -O2 -DBT3_TESTE_VU_CORTES -I"$P/include" "$P/tools/teste_vu_cortes.c" -o "$P/$alvo"
    "${executa[@]}" "$P/$alvo"
    for mutante in fecho constantes cache-xy cache-matriz cache-quadro cache-versao; do
        case $mutante in
            fecho) h=vu_iguais.h;; constantes) h=vu_constantes.h;; *) h=vu4_constantes.h;;
        esac
        cp "$P/include/$h" "$P/antes"
        python3 - "$P/include/$h" "$mutante" <<'PYM'
from pathlib import Path
import sys
p = Path(sys.argv[1]); s = p.read_text()
pares = {
    'fecho': ('    unsigned i = 0;', '    unsigned i = 0;\n    return 1;'),
    'constantes': ('    memset(u->light2, 0, sizeof(u->light2));', '    /* campo esquecido */'),
    'cache-xy': ('sVu4Const.xy == xy && ', ''),
    'cache-matriz': ('memcmp(sVu4Const.u.screen, consts, 64) == 0', '1'),
    'cache-quadro': ('quadro == sVu4Const.quadro && ', ''),
    'cache-versao': ('sVu4Const.versao == sVu4Const.gravadaVersao', '1'),
}
a, b = pares[sys.argv[2]]; assert a in s
p.write_text(s.replace(a, b, 1))
PYM
        "${cc[@]}" -O2 -DBT3_TESTE_VU_CORTES -I"$P/include" "$P/tools/teste_vu_cortes.c" -o "$P/ruim"
        if "${executa[@]}" "$P/ruim"; then echo "FALHA: $alvo aceitou $mutante"; exit 1; fi
        echo "$alvo: sabotagem $mutante acusada"
        cp "$P/antes" "$P/include/$h"
    done
done
echo 'tudo certo'
