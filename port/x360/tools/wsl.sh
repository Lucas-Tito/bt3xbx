#!/usr/bin/env bash
# Atalhos para o WSL do Windows (decisão 6). O código se edita aqui; lá só se compila e roda.
#   wsl.sh enviar        manda os arquivos alterados e novos (ainda sem commit) para o clone de lá
#   wsl.sh rodar 'cmd'   roda um comando na raiz do repositório de lá
#   wsl.sh puxar         git pull lá (depois de um push daqui)
#   wsl.sh script < x.sh roda um script bash (da entrada) na raiz do repositório de lá, sem as aspas
#                        passarem pelo cmd do Windows
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
HOST=${BT3_WSL_HOST:-bt3win}
WSL="wsl -d Ubuntu-24.04 -u lucas --"

case "${1:-}" in
    enviar)
        cd "$RAIZ"
        mapfile -t arquivos < <(git ls-files -m -o --exclude-standard)
        [ ${#arquivos[@]} -eq 0 ] && { echo "nada para enviar"; exit 0; }
        printf '%s\n' "${arquivos[@]}"
        tar -cz "${arquivos[@]}" | base64 -w0 | ssh -o LogLevel=ERROR "$HOST" "$WSL bash -c \"base64 -d | tar -xz -C ~/bt3xbx\""
        ;;
    rodar)
        shift
        ssh -o LogLevel=ERROR -o ServerAliveInterval=30 "$HOST" "$WSL bash -lc \"cd ~/bt3xbx && $*\"" | tr -d '\0'
        ;;
    script)
        { echo 'cd ~/bt3xbx || exit 1'; cat; } | ssh -o LogLevel=ERROR -o ServerAliveInterval=30 "$HOST" "$WSL bash -s" | tr -d '\0'
        ;;
    puxar)
        ssh -o LogLevel=ERROR "$HOST" "$WSL bash -lc \"cd ~/bt3xbx && git checkout -- . && git clean -fdq port/x360 && git pull -q && git log --oneline -1\"" | tr -d '\0'
        ;;
    *)
        sed -n 2,6p "$0"; exit 1 ;;
esac
