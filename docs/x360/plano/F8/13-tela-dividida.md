# F8: a lentidão da luta de tela dividida

[Escopo](00-escopo.md) | [Pendências](pendencias.md) | [CPU e VU0](11-cpu-vu0.md) | [Índice](../../plano.md)

Legenda: **[medido]** no console, **[lido]** no código, **[estimado]** conta.

## 1. O que o Lucas viu (31/10/2026)

Na luta dividida (dois jogadores), a build da release funciona mas fica lenta, e a imagem parece mais nítida que a do
jogo de um jogador. A vibração e o resto da etapa 3 da F7 passaram na mesma sessão.

## 2. As medidas

[Lido] A luta roda a 30 quadros por segundo no PS2 e no 360: o `Battle_Loop` chama `Gfx_EndFrame(2)`
(`src/battle/battle.c:358`), dois vblanks por passo. A meta da luta dividida é 30, não 60.

[Medido] Luta dividida à mão (`build/sessao_v4/coleta/td1`, linhas `passos: janela`): 4.988 passos em 196,35 s contra
166,43 s esperados, ou seja **25,4 quadros por segundo e o jogo a 85% da velocidade**; trabalho por passo p50 38,6,
p95 45,0, p99 49,0 ms; 3.252 imagens ficaram 2 vblanks na tela, 1.591 ficaram 3 e 126 ficaram 4 ou mais. A GPU termina
uns 10 ms depois da CPU e nunca atrasa: o limite é a CPU.

[Medido] As três threads pesadas nos quadros lentos (média):

| Thread | Núcleo (hw) | ms por quadro |
|---|---|---:|
| Gravação do GS (`render_thread`) | 0 | 39,6 (a mais lenta em 92% dos quadros) |
| Jogo (trabalho, sem as esperas) | 2 | 33,8 |
| Desenho (`frame_end`, dono do D3D) | 3 | 30,1 |

[Medido] Volume por quadro, dividida contra duelo normal: lotes do estágio (programa 4 da VU1) 5.228 contra 1.810 (2,9x);
lotes dos lutadores (programa 0) 3.274 contra 1.512 (2,2x); desenhos 4.170 contra 1.334 (3,1x). Cada metade custa 44%
mais estágio que a vista cheia.

[Lido] A causa provável desse excesso: o `StgFrustum_Build` escolhe a largura por `view->x1 - view->x0 > 256`
(`src/battle/stg_a.c:380` e `:395`), mas esses campos são `scissorX0` e `scissorY0` da `View`
(`include/battle/btl_cam.h:74-77`); a diferença dá 0 ou -257 e o ramo da meia largura nunca roda. Toda vista, inclusive
cada metade, recorta o estágio com a largura da tela cheia. É comportamento do original.

[Medido] Perfis do amostrador (`BT3_AMOSTRA`, ramo `amostra-tela-dividida`):
- thread do jogo: 48,7 ms por quadro lento, 17,9 esperando a gravação; pelo menos 19 ms em funções da VU0 emulada;
  `StgModel_Cull`/`StgFrustum_TestPart` 9,5 ms (0,9 fora da luta), `BtlObjPose_CalcMatrices` 4,5;
- thread de gravação (`BT3_AMOSTRA_ALVO=gravacao`): 44,6 ms por quadro lento, 3,4 ociosa; perfil plano: `GsVu1_Call`
  21,7 inclusivo, `Gs_Gif` 9,3, `memcpy` 5,7 próprio (1,2 de `vu1_unpack`, 1,2 de `GsGpu_DrawVu0`), nenhuma função
  acima de 13%.

Os núcleos do 360 têm duas threads de hardware cada: (0,1) gravação e tarefas leves, (2,3) jogo e desenho, (4,5) som.
Duas threads pesadas no mesmo núcleo rendem 60 a 70% cada (`plano/F5/08-threads.md:226`).

## 3. Os pontos de melhoria

Para 30 firmes as três threads precisam ficar abaixo de uns 30 ms no p95. Os ganhos de uma mesma thread se somam até
ela deixar de ser a mais lenta.

### Gravação

| Ponto | Ganho [estimado] | Custo | Quando |
|---|---|---|---|
| **C1. Recorte justo do estágio na dividida**: manter a tabela larga decidindo a animação de textura (`stgm_a.c:404`) e pular só o `Vu1Pkt_CallProg4` (`stgm_a.c:327` e `:445`) das malhas fora da meia vista; imagem igual | 3 a 6 ms | 2 a 4 dias | 1.0, feito (seção 6) |
| **D2. Cópia especializada** no unpack da VU1 e na cópia de vértices do `GsGpu_DrawVu0` (mesmos bytes) | 1,5 a 3 ms | 1 dia | piorou, fora (seção 6) |
| D3. Otimização entre arquivos (LTCG) só nos objetos do GS | 1 a 4 ms, incerto | Makefile e uma medida | 1.0 ou 1.1 |
| D1. Atalhos por "nada mudou" entre lotes seguidos (`draw_state_vu`, `est_acerta`, `vu_uniforme_igual`, `same_state_vu`, `date_snapshot`, `vu_fecho`) | 3 a 6 ms | 3 a 5 dias | 1.1 |
| D4. Não regravar na VRAM um envio idêntico ao anterior (`envio_faixa32`, hash de página) | 1 a 3 ms | médio | 1.1 |
| D5. Reaproveitar entre as duas vistas os vértices da mesma origem | 2 a 5 ms | mais de 300 linhas | 1.1 ou 1.2 |
| E. Gravação em duas etapas em linha de montagem (corte H; a etapa nova no núcleo do som) | 6 a 9 ms | 1.000 a 1.600 linhas, 2 a 3 semanas, risco de travamento entre as etapas | último recurso |

### Jogo

| Ponto | Ganho [estimado] | Custo | Quando |
|---|---|---|---|
| B-mini. `Port_FastMtxApply` nativo (hoje devolve 0 no 360, `port/x360/src/f2/float360.c:41`) | 4 a 5 ms | 1 a 2 dias, prova por autoteste e `BT3_HASH` | 1.0 se a gravação cair abaixo de uns 34 ms |
| B-b. `f_add`, `f_sub`, `f_mul` nativos no `vu0_b.c` | 3 a 5 ms | 1 a 2 dias | 1.1 |
| B-a. O interpretador do `vu0_a.c` (`op_vmadd*`, `setFlags`, `Mtx_Mul`); faz parte da pendência 17 | 3 a 6 ms | 150 a 300 linhas | 1.1 |

### Desenho

| Ponto | Ganho [estimado] | Custo | Quando |
|---|---|---|---|
| A. Desenho no núcleo do som (hw4 ou hw5) | jogo −7 a 9 ms, desenho −8 a 10 | 5 linhas, medir o som (estalos) | 1.0, feito (seção 6) |
| H. Encurtar o custo fixo por desenho (4.170 por quadro) | 5 a 10 ms | médio | 1.1 |

### Recusados

Travar em 20 (o jogo a 67% da velocidade, contra 85% hoje); pular desenho (a animação de textura avança no desenho);
cortar resolução ou efeitos (a GPU não é o limite).

## 4. A nitidez a mais [lido, provável, não provado]

A dividida carrega um arquivo de estágio próprio e mais leve (`BTL_FILE_STAGE_SPLIT`, `include/battle/battle_work.h:118`,
escolhido em `src/battle/battle_load.c:210-214` e `:584`). O brilho e as funções de estágio de névoa, desfoque e bruma
vêm desse arquivo (`StgGlare_LoadParams`; `BtlStage_HasFeature` em `stg_b.c:65`, usado no `StgFx_DrawPre`,
`stg_c.c:652-671`). O port não trata a metade de outro jeito. Para conferir: capturas da mesma arena nos roteiros
`01_simples` e `02_dividido` da F7.

## 5. Medidas que faltam

Sempre no mesmo roteiro, o `02_dividido` (`port/x360/tools/f7_console.py`, `port/x360/ambientes/f7-etapa3`), e não em
luta à mão: a base com `BT3_LOG=1` e `BT3_PICOS=34` (linhas `passos: janela`, a ocupada da gravação, os lotes `vu4` e
`vu0` por quadro), depois cada ponto ligado pela sua chave.

## 6. O que entrou na 1.0 [medido, 31/10/2026]

Roteiro `f7_perfil_dividido.txt` (`port/x360/tools/f7_console.py`): a luta dividida só com defesa, três janelas de 600
passos, `BT3_PICOS=34`; coletas em `build/sessao_dividida/coleta/` (fora do git). Trabalho por passo em ms:

| Rodada | Variante | p50 | p95 | 3 vblanks ou mais | Velocidade |
|---|---|---:|---:|---:|---:|
| `dvb` | base | 34,5 | 39,2 | 16,9% | 92,5% |
| `dvc` | C1 (`BT3_DIVIDIDA_RECORTE=1`; 38,9% das malhas do estágio não foram à VU1) | 33,3 | 36,4 | 7,9% | 96,5% |
| `dvd` | C1 e D2 (`BT3_COPIA_XMEM=1`; gravação de 33,2 para 35,9) | 35,8 | | 21,3% | pior |
| `dvh4` | C1 e o desenho na hw4 | 30,2 | 30,4 | 0,4% (p99 da janela fria 48,1) | 100% |
| `dvh5` | C1 e o desenho na hw5 | 30,2 | 30,4 | 0,8% na fria, 0 nas outras (p99 frio 34,6) | 100% |

A velocidade no roteiro (92,5% na base) é maior que na luta à mão (85%): só defesa tem menos efeito na tela. O som nas
duas do núcleo do áudio: faltas 0, BuffersQueued mínimo 3, pior bloco 0. O duelo de um jogador com o desenho na hw5
(`dh5duelo`) ficou igual ao da build anterior (`i1`). O C1 e a hw5 viraram o padrão (decisão 39); o D2 ficou de fora.

## 7. Quatro vistas: cortes a retomar depois do Tag Team [estimado, 08/11/2026]

Conversa com o Lucas para reabrir nas otimizações, depois de medir no console os custos do BT4 e do Tag Team (as
otimizações D3, H e D5 ficaram adiadas até essa medida).

**Onde cortar.** Com 2 vistas a gravação (cerca de 40 ms) é a thread mais lenta, e cresce com cada lote de estágio e
de efeito desenhado em cada vista (estágio 2,9 vezes e lutadores 2,2 vezes os lotes do duelo). Tirar elemento visual
corta direto o gargalo; com 4 vistas o peso deve crescer de novo. Alavancas, na ordem da decisão 40:

1. Reaproveitar ossos e matrizes entre as vistas.
2. Limitar efeitos pelo número de vistas.
3. Simplificar o estágio: com 4 vistas, esconder por vista os objetos decorativos, a névoa, o brilho e as camadas
   distantes, além do `BTL_FILE_STAGE_SPLIT` que o jogo já usa na dividida (seção 4). O recorte justo por vista (C1)
   deve render mais com 4, porque cada vista é um quarto da tela e o jogo recorta com a largura da tela cheia.
4. Simplificar os lutadores distantes por vista.
5. Alternar a atualização das vistas (por último).

**O limite.** Nenhum corte pode mudar a luta (decisão 40). Pode sair o que é só visual; não podem sair colisão nem
objeto destrutível do estágio, e efeito que consome `rand()` continua sorteando os mesmos números mesmo sem desenhar.
Baixar a resolução não ajuda: a GPU não é o limite (seção 3, recusados). Estágio mais leve e recorte por vista são as
apostas de melhor relação entre ganho e risco, mas só a medida de 4 vistas no console diz quanto cada corte rende.

**O 1 contra 1 original.** A etapa 2 do Tag Team só foi aceita com os roteiros do BT3 dando as mesmas saídas de antes,
o duelo `f5_duelo` e a Referência inclusive: a luta, o save e o comportamento do duelo são os do jogo original e o modo
de equipe é acréscimo. Falta provar a escolha pelo menu entre o duelo normal e as equipes: hoje as equipes de 2 e 4
jogadores entram por roteiros direto na luta (`f9_equipe1`, `f9_equipe2`, `f9_equipe4`); a seleção pelo menu e o HUD
das equipes são da parte B do BT4.
