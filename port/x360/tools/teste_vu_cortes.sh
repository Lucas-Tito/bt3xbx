#!/usr/bin/env bash
# Quem chama segura flock ~/wsl.lock, inclusive durante a compilação e o qemu.
set -euo pipefail
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$(mktemp -d "$HOME/teste-vu-cortes.XXXXXX")
trap 'rm -rf "$P"' EXIT
mkdir -p "$P/include" "$P/tools"
cp "$R/port/x360/include/vu_iguais.h" "$R/port/x360/include/vu_constantes.h" "$R/port/src/gs/gs_draw.h" "$P/include/"
cp "$R/port/x360/tools/teste_vu_cortes.c" "$P/tools/"
pgrep -af "qemu|roteiros" || true
if pgrep -x qemu-ppc; then echo "FALHA: outro qemu"; exit 1; fi
for alvo in x86 ppc; do
    if [ "$alvo" = x86 ]; then cc=(gcc -m32); executa=(); else cc=(powerpc-linux-gnu-gcc -static -fsigned-char); executa=(qemu-ppc); fi
    "${cc[@]}" -O2 "$P/tools/teste_vu_cortes.c" -o "$P/$alvo"
    "${executa[@]}" "$P/$alvo"
    for mutante in fecho constantes; do
        if [ "$mutante" = fecho ]; then
            cp "$P/include/vu_iguais.h" "$P/antes"
            python3 - "$P/include/vu_iguais.h" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1]); s = p.read_text(); a = '    unsigned i = 0;'; assert a in s
p.write_text(s.replace(a, a + '\n    return 1;', 1))
PY
        else
            cp "$P/include/vu_constantes.h" "$P/antes"
            python3 - "$P/include/vu_constantes.h" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1]); s = p.read_text(); a = '    memset(u->light2, 0, sizeof(u->light2));'; assert a in s
p.write_text(s.replace(a, '    /* sabotagem: campo esquecido */', 1))
PY
        fi
        "${cc[@]}" -O2 "$P/tools/teste_vu_cortes.c" -o "$P/ruim"
        if "${executa[@]}" "$P/ruim"; then echo "FALHA: $alvo aceitou $mutante"; exit 1; fi
        echo "$alvo: sabotagem $mutante acusada"
        if [ "$mutante" = fecho ]; then cp "$P/antes" "$P/include/vu_iguais.h"; else cp "$P/antes" "$P/include/vu_constantes.h"; fi
    done
done
echo 'tudo certo'
