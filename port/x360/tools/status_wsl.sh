#!/usr/bin/env bash
# Estado das filas do Codex no WSL, num relance (passagem de chat, guia seção 4; "status e eta" do Lucas).
#   port/x360/tools/status_wsl.sh            (BT3_WSL_HOST=<host do WSL> obrigatório; BT3_WSL_IP=<IP do PC> se o IP do Windows mudou; achar pela porta 22)
# Mostra: quando o WSL subiu, as unidades codex-fila-*, os serviços bt3-* do usuário codex, as esperas armadas, as
# sessões do Codex vivas, as vagas do emulador (~/vez.sh), o fim do log das filas, os últimos commits de cada
# worktree, as entregas recentes e o disco/memória. Só lê; não mexe em nada.
set -euo pipefail
HOST=${BT3_WSL_HOST:?defina BT3_WSL_HOST com o host SSH do PC Windows com o WSL (entrada do ~/.ssh/config)}
OPC=(-o LogLevel=ERROR -o ConnectTimeout=15)
[ -n "${BT3_WSL_IP:-}" ] && OPC+=(-o HostName="$BT3_WSL_IP")
ssh "${OPC[@]}" "$HOST" "wsl -d Ubuntu-24.04 -u root bash -s" <<'FIM' | tr -d '\0\r'
U="sudo -u codex XDG_RUNTIME_DIR=/run/user/1001"
echo "agora $(date "+%d/%m %T"); WSL ligado há $(ps -o etime= -p 1 | tr -d " ") (o uptime -s escorrega com o acerto do relógio)"
echo "== filas"; systemctl list-units --plain --no-pager --no-legend 'codex-fila*' | awk '{print "  " $1, $3, $4}'
echo "== serviços do Codex"; $U systemctl --user list-units --all --plain --no-pager --no-legend 'bt3-*' | awk '$3 != "inactive" {print "  " $1, $3, $4}'
echo "== esperas armadas (de hoje)"
for f in /home/codex/espera/*; do
  [ -s "$f" ] && [ -n "$(find "$f" -mtime -1)" ] && echo "  $(basename "$f"): $(grep -o 'bt3-[A-Za-z0-9_.-]*\|test [^;]*' "$f" | head -1)"
done
echo "== sessões do Codex vivas"; pgrep -fa 'codex exec' | grep -o 'tarefas/[a-z0-9-]*' | sort | uniq -c | sed 's/^/  /' || true
echo "== vagas do emulador"
for l in /home/codex/wsl.lock /home/codex/wsl.lock.2 /home/codex/wsl.lock.3 /home/codex/wsl.lock.4; do
  [ -e "$l" ] && echo "  $(basename "$l"): $(fuser "$l" 2>/dev/null | wc -w) processos"
done
echo "== fim do log das filas"; tail -8 /home/codex/fila-2710.log | sed 's/^/  /'
echo "== worktrees (commits sobre troca/main, último)"
for d in /home/codex/bt3xbx-*/; do
  [ -d "$d/.git" ] || [ -f "$d/.git" ] || continue
  [ -n "$(find "$d" -maxdepth 0 -mtime -3)" ] || continue
  b=$($U git -C "$d" branch --show-current 2>/dev/null || true)
  echo "  $(basename "$d") [$b] +$($U git -C "$d" rev-list --count troca/main..HEAD 2>/dev/null || echo ?): $($U git -C "$d" log -1 --format='%ad %s' --date=format:'%d %H:%M' 2>/dev/null | cut -c1-90)"
done
echo "== entregas dos últimos 2 dias"; find /home/codex -maxdepth 1 -name 'entrega-*.md' -mtime -2 -printf '  %TY-%Tm-%Td %TH:%TM %f\n' | sort
echo "== disco e memória"; df -h /mnt/c | tail -1 | awk '{print "  C: livre " $4 " de " $2}'; free -m | awk 'NR==2 {print "  WSL: usados " $3 " MB, cache " $6 " MB, total " $2 " MB"}'
FIM
