#!/usr/bin/env bash
# Quem chama segura flock ~/wsl.lock.
set -euo pipefail
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$(mktemp -d "$HOME/teste-prefixos.XXXXXX")
trap 'rm -rf "$P"' EXIT
ulimit -c 0
pgrep -af 'qemu|roteiros' || true
if pgrep -x qemu-ppc; then exit 1; fi
for alvo in x86 ppc; do
    if [ "$alvo" = x86 ]; then cc=(gcc -m32); exe=(); else cc=(powerpc-linux-gnu-gcc -static -fsigned-char); exe=(qemu-ppc); fi
    "${cc[@]}" -O2 -pthread -DPORT_BIG_ENDIAN "$R/port/x360/tools/teste_prefixos.c" -o "$P/$alvo"
    for atraso in 0 1 5 20; do
        env BT3_GS_PREFIXOS=1 BT3_PREFIXOS_ATRASO=$atraso "${exe[@]}" "$P/$alvo"
    done
    for mutante in ordem versao barreira liberacao; do
        if env BT3_GS_PREFIXOS=1 BT3_PREFIXOS_MUTANTE=$mutante "${exe[@]}" "$P/$alvo"; then echo "FALHA: $mutante aceito"; exit 1; fi
        echo "$alvo: $mutante recusado"
    done
done
echo 'tudo certo'
