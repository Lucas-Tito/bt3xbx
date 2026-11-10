# Lê as máquinas e a rede de quem desenvolve (port/x360/maquinas.env.example). Uso: . "$(dirname "$0")/maquinas.sh"
# Ordem: BT3_MAQUINAS, port/x360/maquinas.env da worktree, ~/.config/bt3xbx/maquinas.env. Variável já definida
# no ambiente vence o arquivo.
_bt3_maq_raiz=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
for _bt3_maq in "${BT3_MAQUINAS:-}" "$_bt3_maq_raiz/port/x360/maquinas.env" "$HOME/.config/bt3xbx/maquinas.env"; do
    [ -n "$_bt3_maq" ] && [ -f "$_bt3_maq" ] || continue
    while IFS='=' read -r _k _v; do
        case "$_k" in BT3_*) [ -z "${!_k:-}" ] && export "$_k=$_v" ;; esac
    done < <(grep -E '^BT3_[A-Z0-9_]+=' "$_bt3_maq" | sed 's/[[:space:]]*#.*$//')
    break
done
unset _bt3_maq _bt3_maq_raiz _k _v
