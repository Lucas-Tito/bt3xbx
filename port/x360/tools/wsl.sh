#!/usr/bin/env bash
# Atalhos para o WSL do Windows (decisão 6). O código se edita aqui; lá só se compila e roda.
#   wsl.sh enviar        põe o clone de lá no commit daqui (já no GitHub) + as alterações sem commit
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
        # O clone de lá vai para o MESMO commit daqui (que já tem de estar no GitHub) e recebe por cima as
        # alterações ainda sem commit, inclusive arquivos apagados. port/build/ENVIADO registra a origem;
        # headless.py carimba cada build com ela.
        cd "$RAIZ"
        sha=$(git rev-parse HEAD)
        git branch -r --contains "$sha" | grep -q . || { echo "o commit $sha não está no GitHub: faça push antes"; exit 1; }
        mapfile -t arquivos < <(git ls-files -m -o --exclude-standard)
        mapfile -t apagados < <(git ls-files -d)
        dif=$(git diff HEAD | sha1sum | cut -c1-10)
        carimbo="$(git rev-parse --short HEAD) diff:$dif (${#arquivos[@]} alterados, ${#apagados[@]} apagados)"
        { echo "cd ~/bt3xbx || exit 1"
          echo "git fetch -q origin && git checkout -q --detach $sha && git reset -q --hard $sha || exit 1"
          echo "git clean -fdq -- port/x360 src include port/src port/tools docs"
          for a in "${apagados[@]}"; do printf 'rm -f -- %q\n' "$a"; done
          if [ ${#arquivos[@]} -gt 0 ]; then
              echo "base64 -d <<'FIM' | tar -xz"
              tar -cz "${arquivos[@]}" | base64 -w 76
              echo "FIM"
          fi
          printf 'mkdir -p port/build && echo %q > port/build/ENVIADO\n' "$carimbo"
          echo 'echo "WSL em $(cat port/build/ENVIADO)"'
        } | ssh -o LogLevel=ERROR "$HOST" "$WSL bash -s" | tr -d '\0'
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
