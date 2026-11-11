# 07. O que o upstream de PC tem e o 360 não (triagem de 11/11/2026)

Registro de possibilidades, **sem nada a fazer agora** (decisão do mantenedor, 11/11/2026). Base comum com o
Tenkaichi3Decomp: `7043a3e4` (10/10/2026); `upstream/main` olhado em `48d66c1f` (03/11/2026), 493 commits.
Só leitura: nenhum merge nem cherry-pick.

## Licença

Em `9cdcdcbd` (14/10/2026) o upstream passou de MIT para AGPL-3.0 (o MIT fica em `LICENSE-MIT`; vale para todo commit
depois de `06e4671`). A main do port é MIT. Trazer qualquer commit posterior, ou fazer o merge inteiro, traz a AGPL
para o conjunto. A frente do BT4 já aceitou a AGPL ([decisão 40](../decisoes.md)), mas na sua branch, não na main.
Abaixo, cada item leva a marca MIT ou AGPL pela ancestralidade.

## Possibilidades, da mais barata à mais cara

| Item | Commits | Licença | O que traz | Custo e risco |
|---|---|---|---|---|
| Anel de água no 16:9 | `781b52d7` | MIT | O anel que cresce até o limite do float vira um triângulo azul na tela em 16:9 ao sair da água (Kame House). 12 linhas em `src/battle/eft_e.c`. O 360 usa 16:9 por padrão: conferir no console se o defeito aparece | Muito baixo; sem conflito |
| Reiniciar a luta na pausa da História | `083a6f2c` | MIT | "Fight Again" no menu de pausa do modo 1 (`hud_0.c`) | Baixo |
| Pular logos e copyright | `4c200231`, `2c4a57ae`, `75f80aa9` | MIT | Ajuste para o jogador; hoje só os roteiros pulam o filme (`BT3_NOMOVIE`) | Baixo; conflito trivial em `menu_i_c.c` |
| Aba Tech: Z Counter fix e Bombardment fix | `45fd5e8c` | AGPL | Correções de mecânica da comunidade, desligadas por padrão, só luta local. Arquivos novos (`bt3_zcounter2.c`, `bt3_bombardment.c`, `bt3_gameplay_rules.c`) e ganchos limpos em `src/battle`; a escolha é um `Port_Setting`, sem depender da janela F1. Não tem relação com o quadrado branco do Z counter no BT4 | 1 a 2 dias, com comparação x86 x PPC |
| Salvamento rápido | `3274502b`, `0d7c4662`, `04339c17` | AGPL | Salva e carrega sem as páginas da janela do cartão (`mcflow_a.c`) | Baixo a médio; testar com o nosso `plat_mc.c`, que mudou bastante |
| Para testar no console | `9af92af5` (AGPL), `021fc2fc` e seguintes (MIT), `5a2a000f` (MIT) | misto | O branco do Solar Flare (alfa acima de 0x80), o cross-fade da introdução da luta, as costuras de modelo | Incerto se acontece no 360; passa pelo `gs_draw.c` e pelo `gs_core.c` |
| Ideias de custo de GPU | `cd8a3372`, `0ee1fc74`, `f40dc865` e outros | MIT | Cópias de alfa só onde são lidas, limpeza como passe, contorno com a mesma espessura em qualquer aspecto | Só como ideia para o `gs_d3d.cpp`; o código é GL/Vulkan |
| Menus em widescreen | ~75 commits (`gs_marker.c`, ganchos em ~55 telas) | misto | Menus montados na largura toda em vez de esticados (decisão 26). Referência direta para a frente do 16:9 (branch `estudo-16x9`) | Alto: 1 a 2 semanas, quase tudo no `gs_draw.c` |
| HUD, câmera e visual "modernos" | ~13 commits | AGPL | São mods em Lua sobre ~33 mil linhas novas | Não recomendado: Lua em C89, API de desenho nova e custo de CPU |

Já temos, sem precisar trazer: a tabela de 256 cores comparada inteira (`52b675c3`; nosso `clut_texture` já faz) e as
opções de clima e desfoque (`Port_ClimaQuantidade`, `Port_DesfoqueOff`). Provavelmente não vale: `322b51eb` (`pow` na
lista de replays), que corrige um caminho de FPU do x86.

## Só PC

Online e matchmaking (~140 commits), atualizador e pacote (~37), macOS e Windows (~20), acessibilidade (~12), mods em
Lua (~14), janela F1 e controles (~15), músicas extras (~6), traduções e fontes (~40), backends GL/Vulkan (~55).

## Por que não o merge inteiro

- 14 arquivos em conflito, nenhum em `port/x360/`. Os pesados são `port/src/gs/gs_draw.c` (28 trechos) e `gs_core.c`
  (11), exatamente onde está o trabalho de desempenho do console.
- Custo escondido: o `c89.py` compila todo `port/src/*.c` e `port/src/gs/*.c`. Os 61 arquivos novos de PC (rede,
  atualizador, Lua, miniaudio, acessibilidade, `port_tbo.c`) entrariam sozinhos no build e precisariam de exclusão ou
  stub. O item novo do menu principal (TBO) mexe nos roteiros gravados.
- Estimativa: 2 a 3 semanas, com risco de regressão de desempenho, e o port passa para AGPL.

Se um dia vier: escolher commits em lotes pequenos, começando pelos MIT isolados, e a licença decidida antes de
qualquer AGPL.
