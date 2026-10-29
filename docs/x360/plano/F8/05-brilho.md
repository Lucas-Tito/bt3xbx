# F8, pendência 5: o brilho somado do 360 contra o do PC

[Índice do plano](../../plano.md) · [pendências da F8](pendencias.md) · decisão 34

Branch `claude/f8-brilho` (base `main` `e8d3ff56`). O pedido: o deserto à noite escuro demais no console, com a hipótese H9
de [F5/09](../F5/09.md) (branches `codex/f5-namek` e `f5-shenlong`, "O que de fato falta no céu: o brilho somado"): o brilho
do console soma 39% do que o rasterizador x86 soma, deslocado, e os 60% do port não explicariam tudo. O alvo (decisão 34):
o brilho do 360 igual ao do port de PC com `GLOW_DEFAULT 60`, sem subir para os 100% do PS2.

## 1. Resultado

**A H9 não mostrou um defeito do 360: a referência dela é que não é a do PC.** Nenhuma mudança no código do 360 até aqui; a
prova que fecha a questão no WSL está armada (seção 4) e a rodada curta de console está preparada (seção 5).

- **O rasterizador x86 não tem filtro bilinear** (`port/src/gs/gs_core.c`, cabeçalho: "Not here yet: ... bilinear
  filtering"; `texel(ctx, u >> 4, v >> 4)` em todo sprite). A cadeia do brilho pede bilinear em todos os passos
  (`GfxPost_DrawGlow`, `src/sys/gfxm_a.c`: `TEX1` 0x60) e foi escrita para ele: as cinco idas de 5 para 6 leem um texel
  adiante (`u` de 1 a 65) e as voltas não, o que no GS (o texel em `u - 0,5`) faz a média de dois texels e anda meio texel
  para cada lado. Lida no ponto, cada ida anda um texel inteiro e não borra: o halo do rasterizador sai **5 texels de 64
  acima e à esquerda** e três vezes mais estreito. O caminho de GPU (PC e 360, o mesmo `gs_draw.c`) tira meio pixel das
  coordenadas dos desenhos de alvo em alvo justamente para ler onde o GS lê.
- **O modelo da cadeia** (os oito passos com as coordenadas do `gfxm_a.c` e do `BT3_GS_DUMP`, uma faixa de 16 pixels em
  três posições, nos dois eixos; `brilho_modelo.py`) dá, do ponto para o bilinear: o centro anda
  **+40,5 pixels em x e +35 em y**, a largura vai de 4,6 para 13,8 pixels (x) e de 4,0 para 12,2 (y), a soma é a mesma.
  O halo do bilinear fica a 3 pixels da fonte.
- **As capturas da H9 contra o modelo** (`brilho_ajuste.py`, quadro 3500; A = (abertura `glow100` menos `clut`) / 0,4, o
  brilho inteiro do console; B = a parcial 15062 menos a 15054 do rasterizador):

| Medida | Valor |
|---|---|
| soma média por pixel, A / B (os dois a 100%) | 17,52 / 28,50 = **0,615** |
| encaixe sem mexer (blocos de 4 pixels) | correlação 0,531 |
| melhor deslocamento (até 64 pixels, borrão de 0 a 5 passadas) | **(+40, +16)**, correlação 0,666, ganho 0,53 |
| no deslocamento do modelo, (+40, +36) | correlação 0,623 |
| centro do halo na metade de cima, console menos rasterizador, 3500 e 3390 | (+38,8, +13,6) e (+38,9, +13,0) |

  O x bate com o modelo no pixel. O y fica em 13 a 16 em vez de 35, e a força em 0,615 do rasterizador com os dois a 100%.
  Os "39%" da H9 são 0,6 (a escolha do port) vezes esses 0,615.
- **O que sobra tem candidato comum ao PC, não ao 360:** o PC e o 360 guardam e misturam o alfa preso em 1
  (`gs.frag`: `outColor` com `clamp(a, 0, 1)`; o 360, `min(A / 0x80, 1)`), e o rasterizador, como o GS, usa o alfa até
  0xFF: o desenho 900 (o quadro pelo alfa dele para o alvo 3) soma até o dobro onde o alfa passa de 0x80. Por região, o
  console a 100% tem 0,79 do rasterizador na metade de cima e 0,55 na de baixo (a água, que grava alfa com o bit 7). A
  metade de baixo puxa o centro do halo do rasterizador para baixo, o que explica o y menor que o do modelo. **Não medido
  ainda**: é a parte que a seção 4 mede.
- **Descartados pela leitura** (o mesmo em PC e 360 ou sem efeito na soma): as idas e voltas e as subidas substituem o
  destino (`ALPHA` 0x64 com FIX 0x80; o único desenho que soma é o 915), então a volta do conteúdo na entrada da EDRAM não
  pesa; o 913 lê o alvo 6 (o resultado da quinta ida, o da quinta volta é jogado fora) nos dois caminhos; o 902 desenha
  dois sprites e o segundo substitui o primeiro nos dois; o `rect` e o `esc` do 360 levam o uv ao alvo de 640 x 448 sem
  desvio; o meio pixel no 1,33 tem média zero por pixel do GS; os alvos são de 8 bits nos dois (`GL_RGBA8`,
  `D3DFMT_A8R8G8B8`). O "alvo lido sem Resolve" de [08-720p](../F5/08-720p.md) foi corrigido em `9710096` e tem contador.
- **Uma diferença real entre os caminhos de GPU, que não é do 360:** o brilho depende da escala. Na escala 1 (o 360 da H9)
  cada ida e volta borra [1 2 1] / 4; numa escala inteira 2 (o PC numa janela de 1024 x 896) as leituras caem nos centros
  dos texels de saída e as dez passadas só tremem meio texel, sem borrar; na 1,33 do 360 de release, borram menos que na 1.
  A soma é a mesma em todas; muda a largura do halo.

## 2. A referência escolhida e por quê

O PC não roda no WSL (sem GL), e o Deck está com a F7. A referência "PC a 60%" é o **rasterizador x86 com o diagnóstico
`port/x360/tools/diag_bilinear.patch`** (só no WSL, nunca no produto; o `gs_core.c` volta ao original no fim do script):

- `BT3_GS_BILINEAR=1`: o bilinear do GS (os quatro texels em volta de `u - 0,5`, pesos de 1/16) nos sprites em UV com
  `TEX1.MMAG`, que é a cadeia do brilho e os outros efeitos de alvo em alvo; o 3D segue no ponto. É a fórmula do protótipo
  `investiga-filtro-gs` da F4 (`1df9c3b`) e a que o `gs_draw.c` reproduz na GPU.
- `BT3_GS_ALFA_PC=1`: o alfa que a mistura usa como fator preso em 0x80, como o PC e o 360 o guardam.
- `BT3_GLOW=N`: a cor do desenho que soma um alvo ao quadro (`ALPHA` 0x68) a N%, a regra do `gs_draw.c`.

Os dois candidatos do pedido não servem sozinhos: o rasterizador com `BT3_GLOW=60` não honra a cor 0x4C (soma a 0x80) e
lê no ponto (seção 1, visto falhando: o x dele anda 40 pixels); a lista do x86gv só grava os desenhos, não os desenha.

**O rasterizador visto falhando de propósito:** o deslocamento de 40 pixels em x que o modelo do ponto prevê aparece nas
capturas. O `brilho_ajuste.py` visto falhando: as diferenças do console contra elas mesmas dão (0, 0) com ganho 2,5 (o
fator 0,4); com `--desloca 24,16` o melhor encaixe anda de (+40, +16) para (+64, +32); com `--desloca=-16,8`, (-16, +8).

## 3. Ferramentas (`port/x360/tools/`)

| Arquivo | O que faz |
|---|---|
| `brilho_modelo.py` | a cadeia do brilho em uma dimensão, no ponto e com o bilinear do GS: centro, largura e soma da resposta (sem dados do jogo; roda em qualquer máquina) |
| `brilho_ajuste.py A_COM A_SEM B_COM B_SEM` | o brilho de A (com menos sem) contra o de B, em blocos: soma média, encaixe sem mexer, melhor deslocamento e borrão, ganho e correlação; `--fator`, `--desloca` (o teste dele), `--testa DX,DY`. Só a biblioteca padrão (o WSL não tem numpy) |
| `diag_bilinear.patch` | o diagnóstico do rasterizador da seção 2; aplica no `6dde9c6` (o das parciais da H9) e na main |
| `brilho_ref.sh REF SAIDA` | sob a trava: aplica o patch no REF, recompila a x86, roda as parciais de 3500 (v1 sem chave, que tem de dar as da H9 byte a byte; v2 bilinear e alfa do PC; v3 só bilinear; v4 o quadro inteiro a 60%) e o `brilho_ajuste.py` do console contra a H9, a v3 e a v2 em `SAIDA/resultado.txt` |

## 4. A prova no WSL (armada, esperando a trava)

Serviço `bt3-brilho-ref` (`systemd-run --user`, `flock ~/wsl.lock`), armado em 29/10 às 15:52 atrás de `bt3-alfa-rodada3`
e `bt3-f5-lotes-envio`. Worktrees `/home/codex/bt3xbx-brilho` (este branch) e `/home/codex/bt3xbx-brilho-ref` (`6dde9c6`,
destacado, com `port/build/{gen,x86}` copiados do `~/bt3xbx-sh6`); saída em `~/brilho/ref/` (o `resultado.txt` e as
parciais, que ficam no WSL). Uma recompilação da x86 e sete idas do rasterizador até o quadro 3500 (uns 30 s cada nas
parciais da H9). Se o WSL reiniciar antes, o serviço some: armar de novo com a mesma linha
(`systemd-run --user --unit=bt3-brilho-ref /usr/bin/flock /home/codex/wsl.lock /bin/bash
/home/codex/bt3xbx-brilho/port/x360/tools/brilho_ref.sh /home/codex/bt3xbx-brilho-ref /home/codex/brilho/ref`).

| No `resultado.txt` | Conclusão |
|---|---|
| `v1_15054` ou `v1_15062` diferente da parcial da H9 | a referência não vale até explicar (o patch desligado mudou algo, ou a build do REF não é a da H9) |
| console contra v2: melhor deslocamento até 8 pixels de (0, 0), ganho de 0,85 a 1,15, correlação acima da da H9 | o 360 na escala 1 soma o brilho como o PC: a H9 era a referência; a pendência 5 vai para a rodada de console (seção 5) |
| contra v2 o x volta a uns 40 pixels, ou o ganho fica perto de 0,6 | o 360 difere do PC na cadeia: abrir passada por passada (a captura dos alvos 3 a 6 no console) |
| v3 e v2 iguais no encaixe | o alfa acima de 0x80 não pesa na cena; o resto da força é outra coisa |

## 5. A rodada curta de console (preparada)

O `.xex` é o da main (nenhuma mudança no 360 neste branch). Ambientes em [05-ambientes](05-ambientes/), um por abertura;
cada um grava as capturas numa pasta própria (`game:\shots_<nome>`). Envio, com o console na FreeStyle e o `.xex` e o
`shenlong.txt` da main já lá: `python3 port/x360/tools/subir.py --nome ambiente.txt docs/x360/plano/F8/05-ambientes/ambiente_<nome>.txt`;
abrir o `.xex`; no fim, voltar à FreeStyle, `subir.py --baixar bt3xbx.log` (para o Linux, só o log) e, **no WSL**,
`subir.py --baixar-pasta shots_<nome> --para ~/brilho/console/<nome>`. Antes de baixar, conferir o espaço do C:
(as 241 capturas de uma abertura do Shenlong são uns 200 MB).

| Abertura | O que é | Quem joga | Captura |
|---|---|---|---|
| `brilho_rel60` | Shenlong, a configuração de release (648 x 456, escala 1,33, EDRAM residente, afiado, FXAA), brilho padrão (60) | roteiro `shenlong.txt` | 3360 a 3600 |
| `brilho_rel0` | a mesma com `BT3_GLOW=0` | roteiro | 3360 a 3600 |
| `deserto_60` | luta no deserto à noite, configuração de release, brilho padrão | o Lucas: Duelo, o mesmo par e o mesmo estágio nas três, uns 90 s de luta | a cada 60 quadros do GS |
| `deserto_0` | a mesma com `BT3_GLOW=0` | o Lucas | a cada 60 |
| `deserto_100` | a mesma com `BT3_GLOW=100` (só para ver; não é o alvo) | o Lucas | a cada 60 |
| `brilho_e1_60`, `brilho_e1_0` (opcionais) | o Shenlong na escala 1 sem residência (o caminho da H9), 60 e 0 | roteiro | 3360 a 3600 |

**No log de cada uma:** a origem (`passos: origem`), `BT3_GLOW` quando posto, nenhum `FALHA`, `CRASH`, `vigia:` ou
`ERRO`; o contador `alvo lido sem Resolve` em 0 em todas as linhas (é o defeito de [08-720p](../F5/08-720p.md) que
escurecia o brilho em vez de clarear); nas do Shenlong, `shenlong.txt:52 fim`.

**O que medir (no WSL; só números voltam):**

```
cd ~/bt3xbx-brilho; F=port/x360/tools; C=~/brilho/console
# o brilho de release do console contra a referência do PC a 100% (ganho esperado 0,6) e contra o console da escala 1
# da H9 (ganho esperado 1,5: 60 contra 40), nos quadros 3500, 3530, 3580 e 3600 (a glow100 da H9 só tem 3500 a 3600)
python3 -I $F/brilho_ajuste.py $C/brilho_rel60/gpu_03500.ppm $C/brilho_rel0/gpu_03500.ppm ~/brilho/ref/v2_15062.ppm ~/brilho/ref/v2_15054.ppm --testa 0,0
for q in 03500 03530 03580 03600; do python3 -I $F/brilho_ajuste.py $C/brilho_rel60/gpu_$q.ppm $C/brilho_rel0/gpu_$q.ppm \
    ~/capturas_2310e/shots/gpu_$q.ppm ~/capturas_2310c/shots/gpu_$q.ppm --testa 0,0; done
# o deserto: luminância média e escuros da área 3D por captura da luta (as da luta: depois da linha de luta no log)
for n in deserto_0 deserto_60 deserto_100; do for f in $C/$n/gpu_*.ppm; do echo "$n $(basename $f) $(python3 -I $F/ppm_stats.py stats $f)"; done; done
```

| Resultado | Conclusão |
|---|---|
| `brilho_rel60 - rel0` contra a v2: deslocamento até 8 pixels, ganho de 0,5 a 0,7; contra a H9, ganho de 1,3 a 1,7 | o brilho do 360 de release é o do PC a 60%: nada a corrigir no 360. O deserto escuro é a força de 60% (decisão 34 mantém) ou outra coisa (a média do deserto a 0 contra 60 diz quanto o brilho pesa ali) |
| o ganho de release bem abaixo do da escala 1, ou o halo deslocado só no release | defeito do caminho de release do 360 (residência ou escala 1,33): as aberturas `e1` separam qual |
| `alvo lido sem Resolve` acima de 0 | o defeito de 23/10 de volta; corrigir antes de medir o resto |
| no deserto, `deserto_60` quase igual a `deserto_0` | o brilho quase não age no deserto: o escuro não é o brilho; seguir pelos outros efeitos de tela (`BT3_FX_OFF`) |

**Para o Lucas olhar:** o deserto nas três aberturas, e se o `deserto_60` lhe parece igual ao PC que ele conhece. A saída
de vídeo do console também escurece os escuros se a TV espera faixa limitada e o console manda a cheia (Configurações do
sistema, Tela, "Níveis de referência"); vale conferir antes de julgar.

## 6. O que não foi conferido

O PC de verdade (sem GL no WSL; o Deck está na F7); a referência da seção 4 (esperando a trava); o console de release (a
seção 5); o y que o modelo não fecha, até a v2 rodar.
