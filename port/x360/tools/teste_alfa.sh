#!/usr/bin/env bash
# Quem chama segura a trava comum, inclusive no teste PPC.
set -euo pipefail
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$(mktemp -d "$HOME/teste-alfa.XXXXXX")
trap 'rm -rf "$P"' EXIT
mkdir -p "$P/port/x360/tools" "$P/port/x360/include" "$P/port/src/gs"
cp "$R/port/x360/tools/teste_alfa.c" "$P/port/x360/tools/"
cp "$R/port/src/gs/gs_draw.h" "$P/port/src/gs/"
ulimit -c 0
pgrep -af 'qemu|roteiros' || true
if pgrep -x qemu-ppc; then exit 1; fi
for alvo in x86 ppc; do
    if [ "$alvo" = x86 ]; then cc=(gcc -m32); exe=(); else cc=(powerpc-linux-gnu-gcc -static -fsigned-char); exe=(qemu-ppc); fi
    "${cc[@]}" -O2 -DPORT_BIG_ENDIAN -DBT3_ALFA_ORIGEM "$R/port/x360/tools/teste_alfa.c" -o "$P/$alvo"
    "${exe[@]}" "$P/$alvo"
    if "${exe[@]}" "$P/$alvo" estragado; then echo 'FALHA: metadado adulterado aceito'; exit 1; fi
    echo "$alvo: metadado adulterado recusado"
    for mutante in mascara misc indice uniforme fusao; do
        cp "$R/port/x360/include/gs_alfa.h" "$P/port/x360/include/gs_alfa.h"
        python3 - "$P/port/x360/include/gs_alfa.h" "$mutante" <<'PYM'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
a=s.index('static int gs_alfa_metadado'); original,s=s[:a],s[a:]
pares={
 'mascara': (' || !(mascara & GS_CC_A)', ''),
 'misc': ('u->misc[3] == 3.0f', '1'),
 'indice': ('v[(i + 2) * 12 + 7] > 128.0f', '0'),
 'uniforme': ('u->color0[3] > 128.0f', '0'),
 'fusao': ('(junta ? saida[n] : 0)', '0'),
}
a,b=pares[sys.argv[2]]; assert a in s
p.write_text(original+s.replace(a,b,1))
PYM
        "${cc[@]}" -O2 -DPORT_BIG_ENDIAN -DBT3_ALFA_ORIGEM "$P/port/x360/tools/teste_alfa.c" -o "$P/ruim"
        if "${exe[@]}" "$P/ruim"; then echo "FALHA: $alvo aceitou $mutante"; exit 1; fi
        echo "$alvo: $mutante recusado"
    done
done
echo 'tudo certo'
