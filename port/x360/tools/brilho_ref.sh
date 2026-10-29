#!/usr/bin/env bash
# F8, pendência 5: a referência do brilho somado "do PC a 60%" (decisão 34) pelo rasterizador x86 com o diagnóstico
# port/x360/tools/diag_bilinear.patch, e a comparação com as capturas do console da H9 (docs/x360/plano/F8/05-brilho.md).
# Só no WSL, sob a trava (flock ~/wsl.lock): um ciclo de emulador por vez. Nada sai daqui além de números.
#
#   brilho_ref.sh REF SAIDA
#     REF    worktree do commit das parciais da H9 (6dde9c6, f5-shenlong), com port/build/x86 dele já compilado
#            (cópia da do ~/bt3xbx-sh6) e os links asm, assets, disc e gamedata
#     SAIDA  pasta das parciais novas e do resultado.txt
#
# Passos: aplica o patch no REF (o gs_core.c volta ao original no fim), recompila a x86, e roda o Shenlong até o quadro
# 3500 com BT3_GS_FROM=3500 e BT3_GS_STOP=N (as N primeiras primitivas; 15054 é o quadro antes da soma do brilho, 15062
# depois dela, os índices da H9):
#   v1  sem chave nenhuma: tem de dar as parciais da H9 byte a byte (o patch desligado não muda nada)
#   v2  BT3_GS_BILINEAR=1 BT3_GS_ALFA_PC=1 (o bilinear do GS e o alfa preso do PC), o brilho a 100%
#   v3  BT3_GS_BILINEAR=1 (só o bilinear)
#   v4  BT3_GS_BILINEAR=1 BT3_GS_ALFA_PC=1 BT3_GLOW=60, o quadro inteiro (a referência "PC a 60%")
# e o brilho_ajuste.py do console (glow100 menos clut, dividido por 0,4: a soma inteira do console) contra cada um.
set -uo pipefail
export DEBUGINFOD_URLS=
REF=$(cd "$1" && pwd); S=$(mkdir -p "$2" && cd "$2" && pwd); R=$S/resultado.txt
FERR=$(cd "$(dirname "$0")" && pwd)
CONSOLE_COM=${CONSOLE_COM:-$HOME/capturas_2310e/shots/gpu_03500.ppm}
CONSOLE_SEM=${CONSOLE_SEM:-$HOME/capturas_2310c/shots/gpu_03500.ppm}
H9=${H9:-$HOME/shen6/parcial}
ra() { echo "$*" | tee -a "$R"; }
amb="export BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1 BT3_UNCAPPED=1 BT3_NOSOUND=1 BT3_WIDE=1 BT3_GS=1; unset BT3_SHOT BT3_GS_THREAD BT3_GS_DESENHO BT3_GPU_DESENHOS BT3_GPU_LISTA BT3_GPU_NORMAL BT3_GS_ZDUMP BT3_GS_FROM BT3_GS_STOP BT3_GS_DUMP BT3_GS_BILINEAR BT3_GS_ALFA_PC BT3_GLOW"
# o rasterizador até a captura de 3501 aparecer (ou 50 minutos), com as variáveis extras; a de 3500 vai para $S/$nome
roda() { local nome=$1; shift
  rm -rf "$REF/port/build/shots"; mkdir -p "$REF/port/build/shots"
  bash -c "cd '$REF'; $amb; export $*; rm -rf '$S/sv'; BT3_ROTEIRO=port/x360/roteiros/shenlong.txt BT3_SAVES='$S/sv' timeout 3000 port/build/x86/bt3_x86 > '$S/$nome.log' 2>&1 & pid=\$!; \
    while kill -0 \$pid 2>/dev/null; do \
      find port/build/shots -name 'frame_*.ppm' | while read f; do q=\${f##*frame_}; q=\$((10#\${q%.ppm})); [ \$q -lt 3500 ] && rm -f \$f; done; \
      [ -f port/build/shots/frame_03501.ppm ] && { sleep 1; kill \$pid; }; sleep 0.5; done; wait \$pid; rm -rf '$S/sv'"
  if mv "$REF/port/build/shots/frame_03500.ppm" "$S/$nome.ppm" 2>/dev/null; then
    ra "$nome: $(md5sum < "$S/$nome.ppm" | cut -c1-32) ($*)"
  else
    ra "$nome: FALHA, sem a captura de 3500 ($*); fim do log: $(tail -2 "$S/$nome.log" | tr '\n' ' ')"
  fi
  rm -rf "$REF/port/build/shots"
}
: > "$R"
ra "início $(date '+%d/%m %H:%M'); REF $(git -C "$REF" rev-parse --short HEAD); disco livre $(df -h "$S" | awk 'NR==2 {print $4}')"
cd "$REF" || exit 1
if ! git apply "$FERR/diag_bilinear.patch"; then ra "FALHA: o patch não aplica no REF"; exit 1; fi
python3 port/x360/tools/headless.py --alvo x86 > "$S/build_x86.log" 2>&1; st=$?
git checkout -- port/src/gs/gs_core.c
ra "build x86 com o patch: saída $st; $(tail -2 "$S/build_x86.log" | tr '\n' ' ')"
[ $st -eq 0 ] || exit 1
roda v1_15054 BT3_GS_FROM=3500 BT3_GS_STOP=15054 BT3_SHOT=1
roda v1_15062 BT3_GS_FROM=3500 BT3_GS_STOP=15062 BT3_SHOT=1
for n in 15054 15062; do
  if cmp -s "$S/v1_$n.ppm" "$H9/frame_03500_$n.ppm"; then ra "v1_$n igual à parcial da H9 (o patch desligado não muda nada)";
  else ra "v1_$n DIFERENTE da parcial da H9: a referência não vale até explicar"; fi
done
roda v2_15054 BT3_GS_FROM=3500 BT3_GS_STOP=15054 BT3_SHOT=1 BT3_GS_BILINEAR=1 BT3_GS_ALFA_PC=1
roda v2_15062 BT3_GS_FROM=3500 BT3_GS_STOP=15062 BT3_SHOT=1 BT3_GS_BILINEAR=1 BT3_GS_ALFA_PC=1
roda v3_15054 BT3_GS_FROM=3500 BT3_GS_STOP=15054 BT3_SHOT=1 BT3_GS_BILINEAR=1
roda v3_15062 BT3_GS_FROM=3500 BT3_GS_STOP=15062 BT3_SHOT=1 BT3_GS_BILINEAR=1
roda v4_quadro BT3_GS_FROM=3500 BT3_SHOT=1 BT3_GS_BILINEAR=1 BT3_GS_ALFA_PC=1 BT3_GLOW=60
# (as chaves ligadas têm de mudar a imagem: o diagnóstico visto agindo)
cmp -s "$S/v2_15062.ppm" "$S/v1_15062.ppm" && ra "FALHA: v2 igual a v1, as chaves não agiram"
grep -a -m1 "diagnóstico do brilho" "$S/v2_15062.log" | sed 's/^/v2: /' | tee -a "$R"
ra "== o console (glow100 - clut) / 0,4 contra:"
for v in h9 v3 v2; do
  if [ $v = h9 ]; then d="$H9/frame_03500_15062.ppm"; a="$H9/frame_03500_15054.ppm"; else d="$S/${v}_15062.ppm"; a="$S/${v}_15054.ppm"; fi
  ra "-- $v ($( [ $v = h9 ] && echo 'o rasterizador no ponto, a referência da H9' || ([ $v = v3 ] && echo 'bilinear' || echo 'bilinear e alfa do PC') ))"
  python3 -I "$FERR/brilho_ajuste.py" "$CONSOLE_COM" "$CONSOLE_SEM" "$d" "$a" --fator 0.4 --borrao 3 --passo 4 --testa 0,0 --testa 40,36 2>&1 | tee -a "$R"
done
ra "fim $(date '+%d/%m %H:%M')"
echo FIM >> "$R"
