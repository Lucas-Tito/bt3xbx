#!/usr/bin/env bash
# F4, etapa 8: o save entre o WSL (a PPC do qemu), esta máquina e o console. Roda aqui, na máquina de trabalho; os
# saves (dado do usuário) ficam em build/saves (fora do git). Os passos do console estão no plano (docs/x360/plano/F4/08.md).
#   save_console.sh traz-wsl NOME [DIR]    o cartão NOME do save_entre.sh no WSL (c_ppc, c_x86, k_CASO/cartao...) para
#                                          DIR (build/saves/NOME): os três arquivos, por base64 (o wsl.sh tira os bytes 0)
#   save_console.sh leva-wsl DIR           os três arquivos de DIR para o WSL, num cartão em port/build/save_entre/console,
#                                          para o CARTAO= do save_entre.sh
#   save_console.sh confere DIR [DIR2]     tamanho e md5 de cada arquivo, a soma e a ordem do arquivo de dados
#                                          (save_entre.py analisa); com DIR2, os dois byte a byte e campo a campo
#   save_console.sh lista [PASTA]          o que há em game:\PASTA\card1\BASLUS-21678DBZT3 no console (PASTA = saves)
#   save_console.sh baixa PASTA DIR        os arquivos de game:\PASTA\card1\BASLUS-21678DBZT3 do console para DIR
#   save_console.sh envia DIR [PASTA]      os três arquivos de DIR para game:\PASTA\card1\BASLUS-21678DBZT3 (PASTA = saves),
#                                          pelo subir.py (nome temporário, DELE, RNFR/RNTO, md5 conferido baixando de
#                                          volta; sem QUIT, sem REST). Lista antes o que vai ser substituído e pede "s"
# Console: parado na FreeStyle, uma conexão por vez; IP pelo subir.py (MAC 4c:0b:be) ou --ip em XBOX_IP.
set -euo pipefail
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd); cd "$RAIZ"
T=port/x360/tools; DADOS=BASLUS-21678DBZT3; ARQS=(icon.sys dbzsm.ico $DADOS)
CONSOLE=/Hdd1/HOMEBREW/bt3xbx

case "${1:-}" in
    traz-wsl)
        nome=$2; dir=${3:-build/saves/$nome}
        mkdir -p "$dir"
        printf 'd=port/build/save_entre/%s/card1/%s\n[ -d "$d" ] || { echo "sem $d" >&2; exit 1; }\ntar -C "$d" -cz %s | base64 -w 76\n' \
            "$nome" "$DADOS" "${ARQS[*]}" | $T/wsl.sh script | base64 -d | tar -C "$dir" -xz
        (cd "$dir" && md5sum "${ARQS[@]}")
        ;;
    leva-wsl)
        dir=$2
        for a in "${ARQS[@]}"; do [ -f "$dir/$a" ] || { echo "falta $dir/$a"; exit 1; }; done
        { echo "d=port/build/save_entre/console/card1/$DADOS; rm -rf port/build/save_entre/console; mkdir -p \$d"
          echo "base64 -d <<'FIM' | tar -C \$d -xz"
          tar -C "$dir" -cz "${ARQS[@]}" | base64 -w 76
          echo "FIM"
          echo "(cd \$d && md5sum ${ARQS[*]})"
        } | $T/wsl.sh script
        echo "no WSL: CARTAO=port/build/save_entre/console port/x360/tools/save_entre.sh"
        ;;
    confere)
        for d in "${@:2}"; do
            echo "== $d"; (cd "$d" && for a in "${ARQS[@]}"; do printf '%-18s %6s bytes  %s\n' $a $(stat -c %s $a) $(md5sum < $a | cut -c1-32); done)
            python3 $T/save_entre.py analisa "$d/$DADOS" | tail -n +2
        done
        if [ $# -ge 3 ]; then
            echo "== $2 contra $3"
            python3 $T/save_entre.py campos "$2" "$3" --mesma-ordem
        fi
        ;;
    lista)
        python3 $T/subir.py --destino "$CONSOLE/${2:-saves}/card1/$DADOS" --listar
        ;;
    baixa)
        pasta=$2; dir=$3
        python3 $T/subir.py --destino "$CONSOLE/$pasta/card1" --baixar-pasta $DADOS --para "$dir"
        (cd "$dir" && md5sum "${ARQS[@]}")
        ;;
    envia)
        dir=$2; pasta=${3:-saves}
        for a in "${ARQS[@]}"; do [ -f "$dir/$a" ] || { echo "falta $dir/$a"; exit 1; }; done
        echo "vai substituir em game:\\$pasta\\card1\\$DADOS (copie antes com: save_console.sh baixa $pasta DIR):"
        python3 $T/subir.py --destino "$CONSOLE/$pasta/card1/$DADOS" --listar || true
        read -r -p "enviar ${ARQS[*]} de $dir? [s/N] " ok
        [ "$ok" = s ] || { echo "nada enviado"; exit 1; }
        # o arquivo de dados por último: sem os outros dois o jogo o daria como danificado
        python3 $T/subir.py --destino "$CONSOLE/$pasta/card1/$DADOS" "${ARQS[@]/#/$dir/}"
        ;;
    *)
        sed -n 2,17p "$0"; exit 1 ;;
esac
