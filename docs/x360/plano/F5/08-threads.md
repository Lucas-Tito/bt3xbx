# F5, etapa 8: estudo da divisão em threads da gravação e do desenho

Feito no branch `cursor/f5-threads-estudo`, a partir de `troca/main` (`9a922a9`, a tarefa em
[`tarefas/f5-threads-estudo.md`](../../tarefas/f5-threads-estudo.md)). Só leitura de código e de documento: nenhuma
build, nenhum emulador, nenhuma mudança de código. Serve para o Lucas decidir a arquitetura; **nada aqui está decidido
nem implementado**. A decisão 24, resposta 5, deixou a thread de desenho separada "só se isso não bastar, reestimada
antes": este é o "reestimada".

Convenção: **[código]** é o que se lê nos fontes (com o arquivo), **[medido]** vem de um log ou nota do console,
**[derivado]** é conta feita aqui sobre números medidos, **[estimado]** e **[hipótese]** são palpite meu, dito como tal.

## 1. Resumo

1. **O corte natural já existe e é só um: `Port_GsVif1Chain`** (`port/src/gs/gs_core.c`). O jogo monta a lista do
   quadro na memória dele e a entrega nesse ponto; tudo o que vem depois (andar a cadeia, VIF, GIF, VU1, registradores
   do GS, VRAM, decodificar textura, gravar a lista de desenhos, refazer a lista no D3D, `Present`) está dentro do
   `run_chain`. Quem fez o `BT3_GS_THREAD` do PC cortou exatamente aí.
2. **A conta não fecha com duas threads.** Com os números do console (lógica 15, resto da gravação 20, VU1 26,3, fim do
   quadro 14,5, em ms [medido]; o 20 é [derivado] por subtração) o melhor corte de duas threads dá uns 61 ms por
   quadro (de 75,7), 16 fps na luta. Para 33,37 ms (30 fps) é preciso **três estágios** (lógica, gravação, desenho no
   D3D) **e** cortar trabalho da gravação, ou quatro threads de verdade (seção 5).
3. **O `BT3_GS_THREAD` não falhou por estar errado no corte.** O que o README do port de PC registra como corrida
   (a lista "tinha N blocos na entrega e 1 depois") bate com o `Dma_ResetBuffers` do jogo, que escreve um `END` na
   primeira etiqueta dos dois buffers de lista, e com o `sceGsSyncPath` do port, que é um **stub vazio**: os 23 pontos
   onde o jogo espera o GS terminar não esperam nada no PC [hipótese forte, não provada; o código dos dois lados está
   citado na seção 3]. A correção é barata (esperar a thread nesses pontos). A nota antiga de que a câmera saltava
   estava errada (o próprio README a corrige).
4. **Recomendo, nesta ordem** (cada degrau se prova sozinho): **(c)** a gravação inteira numa thread, com a thread do
   jogo só entregando a lista e esperando nos pontos onde o jogo já espera; depois **(c + a)** separar o desenho do D3D
   numa terceira; o resto só se faltar. O (c) sozinho não cumpre os 30 fps, mas é o degrau que todos os outros
   precisam, já está quase todo escrito, e tira da thread do jogo as esperas da Guide e das cargas.
5. **A memória é o limite duro:** o mínimo livre visto nas rodadas é 30 MB contra um piso de 32 [medido]. Qualquer
   opção que dobre as listas (66,4 MiB cada) não cabe sem encolher as capacidades, que são fixas e usadas em menos de
   um décimo (seção 4.2). Encolher é decisão do Lucas (pergunta 1 da seção 9).

## 2. A thread do jogo, quadro a quadro

### 2.1 A ordem, numa luta

`Battle_Loop` (`src/battle/battle.c`, [código]), e o que cada passo faz pelo lado do port:

| # | Passo | Onde | O que faz | ms (luta, 60 quadros) |
|---|---|---|---|---|
| 1 | Lógica | `Job_Run` a `BtlGame_Update` | estado do jogo, IA, câmera, `Snd_Update`; só memória do jogo | **L = uns 15** (F2, decisão 17) |
| 2 | Montar a lista | `Battle_Draw` | `Dma_Alloc`/`Dma_AddRef`/`Dma_AddData` no buffer de lista (`gDmaBuf[2]`, 1 MiB cada, em dois); matrizes e constantes de cada objeto copiadas para dentro da lista; REF para dados que não se copiam (microprogramas, cadeias de vértices dos modelos, imagens e CLUTs de textura) | dentro do L |
| 3 | Fim do quadro do jogo | `Gfx_EndFrame(2)` | `sceGsSyncPath` (**stub**), `Vsync_Wait(2)` (a thread dorme até a grade de 16,68 ms em `Port_VBlank`), `sceGsSwapDBuff`, que manda o envelope de desenho e o sprite de limpeza pelo `Port_GsGifChannel` (grava na lista do GS, na hora, **fora** da lista do quadro) | espera, ~0 |
| 4 | Entregar | `Dma_Flush` | fecha a lista, `Dma_SendVif1` e `Port_DmaChcr`, que chama `Port_GsVif1Chain(tadr)` | |
| 5 | Andar a cadeia | `run_chain`, `vif`, `Gs_Gif`, `transfer_data` | tags DMA, códigos VIF, UNPACK para a memória da VU1 (copia), DIRECT para o GIF, registradores do GS, vértices, **envio de textura para a VRAM de 4 MiB** (o jogo reenvia as texturas do estágio em todo quadro, comentário em `transfer_data`) | **W = uns 20** [derivado: 61,2 - 15 - 26,3] |
| 6 | VU1 | `GsVu1_Call` (um por `MSCAL`, uns 3,5 mil por quadro de luta) | `hle_program*` lê a memória da VU1, `GsGpu_DrawVu0/4/6` grava vértices, índices e constantes (`Vu0Uniform`, 304 bytes) na lista, `draw_state` e `texture_get` (hash de página, decodificação) | **V = 26,3** (`gGsVu1Ms`) |
| 7 | Fim do quadro | `GsVu1_FrameEnd`, `GsGpu_FrameEnd`, `frame_end` (`gs_d3d.cpp`) | `vu_copia` (as três famílias para o par de buffers da GPU), laço sobre `gsDraws` (EDRAM, `desenha_vu`, nativas, resolves), `Present`, `cova_varre`, medidas | **E = 14,5** na CPU (cópia 6,5, vu0 6,3, o resto pequeno); a GPU leva uns 17 a 19,7 por conta própria |

Os passos 5 a 7 são **uma chamada só** (`Port_GsVif1Chain`), hoje síncrona. Total na luta, [medido]: 75,7 ms por quadro,
dos quais a thread do jogo gasta 61,2 (L + W + V) e o fim do quadro mais 14,5. O W é a única parcela que não tem
medida direta (é a sobra): a primeira coisa a medir é separar L de W (seção 8).

A proporção do V, por amostras do contador de programa na x86 (`08.md`, primeira parte), como **proxy** do que acontece no
Xenon: `Gs_PageHash` 28% (uns 7,4 ms), `draw_state` com a `texture_get` 14,5% (3,8), `memcpy` 13,9% (3,7),
`GsGpu_DrawVu0` 12,3% (3,2), `vram_rw` 5% (1,3), o resto 26,3% (6,9). O que lê a VRAM (hash, `texture_get`, decodificação)
é uns 33%, uns 8,7 ms; o resto, uns 17,6 ms, só monta vértices e constantes a partir da memória da VU1.

### 2.2 Quem lê e escreve o quê

| Dado | Quem escreve | Quem lê | Observação |
|---|---|---|---|
| Memória do jogo (heap, objetos, texturas carregadas, cadeias de vértices dos modelos) | passos 1 e 2, e as cargas | passo 2 (escreve a lista) e passo 5 (**lê pelos REF e CALL**) | o ponto delicado: a cadeia lê dados vivos do jogo durante todo o passo 5 |
| Buffers de lista `gDmaBuf[0/1]` (1 MiB cada) | passo 2 | passo 5 | dois, trocados em `Dma_Flush`; o do quadro N só é reescrito no quadro N+2 |
| Estado do GS (`gGs`, ~400 bytes), registradores, `vif()` (estático) | passo 5 (e o `Port_GsGifChannel`, passo 3) | passos 5 e 6 | uma máquina de estado sequencial: o que o desenho lê vale "no ponto da cadeia em que está" |
| VRAM do GS (`sVram`, 4 MiB), `gGsPageGen[512]`, `gGsPageEpoch` | `transfer_data` (envios), `vram_rw` | `texture_get`, `Gs_PageHash`, `GsGpu_FbUpload` | só o passo 5 escreve; ninguém do jogo lê (o `sceGsExecStoreImage` do port é um stub que devolve 0) |
| VU1: memória de 16 KB, microprograma de 16 KB, registradores (`vu`) | UNPACK e `Program` (passo 5) | `GsVu1_Call` (passo 6) | cópia do que veio da cadeia: **depois do UNPACK não aponta mais para o jogo** |
| Listas gravadas (`gsDraws` 65.536 de 128 B, `gsVerts` 1 Mi de 28 B, `gsVuVerts` 512 Ki de 48 B, `gsVuIdx` 1 Mi de 4 B, `gsVuUni` 8.192 de 304 B; 66,375 MiB fixos, `F5/01.md`) | passos 5 e 6 | passo 7 | globais únicas; o `frame_end` as zera no fim |
| Cache de texturas (`sTex`, `sTexBucket`, `memo`, paletas) e alvos (`gsTargets[24]`) | passos 5 e 6, **e** o passo 7 (`cleared`, `draws`) | os dois | campos com **dois escritores**: `gsTargets[i].cleared` é posto na gravação (`gs_draw.c:616`) e no desenho (`gs_d3d.cpp:2915`), lido nos dois |
| Recursos do D3D (texturas, alvos, pipelines, `cova`, contadores `sMem`) | o backend, chamado **pela gravação** (`texCreate`, `texUpload`, `texDestroy`, `texSlotsLeft`, `targetEnsure`, `targetDrop`, `targetColor`, `pipeGet`, mais os ganchos `GsD3d_Falta`, `GsD3d_TexBytes`, `GsD3d_Descartou`, `GsD3d_Branca`) e pelo `frame_end` | os dois | **oito das quatorze entradas do `GsBackend` são chamadas pela gravação e devolvem objetos do D3D**: o backend não é uma fronteira produtor/consumidor, só o `frameEnd` é |
| Contador de quadros `gGsFrame` (`sFrame`) | `screenshot()`, no fim do `run_chain` | a gravação (idade das texturas e alvos), o `gs_input.c` (**pad por roteiro, por número de quadro**), `snd_se.c`, `snd_adx.c`, o desenho | ver seção 7: precisa de um contador do lado do jogo |

## 3. O que já existe e por que falhou

### 3.1 O `BT3_GS_THREAD` do port de PC

[código] `gs_core.c`, "The render thread": com `BT3_GS_THREAD=1` (e só em `BT3_GS=gpu`), `GsGpu_Init` roda na thread de
desenho (que passa a ser dona da janela e do dispositivo), `Port_GsVif1Chain` faz `render_wait` (a lista anterior tem que
ter acabado) e depois `render_post(2, tadr, tte)`; a thread roda `run_chain`, que **inclui** `GsGpu_FrameEnd`. O
`Port_GsGifChannel` também espera (`render_wait`) antes de tocar no GS. É o corte (c) da tarefa, com um estágio de
profundidade um: a lista N roda enquanto a lógica N+1 corre, e a entrega N+1 espera a N, que é o contrato do PS2
(`Gfx_EndFrame` passo 5: "espera o GS acabar a lista do `Dma_Flush` anterior").

[código] O contrato do PS2 é a chave: no console de verdade o DMA e o GS leem a lista **em paralelo** com a lógica do quadro
seguinte, então o jogo foi escrito para não mexer, na lógica N+1, no que a lista N ainda aponta. Isso vale para os
buffers de lista (dois, trocados), para as constantes por objeto (copiadas **para dentro** da lista pelo `Dma_Alloc`) e
para os microprogramas (dados fixos). O port de PC só deixa de honrar o contrato onde o hardware o honrava de graça.

### 3.2 O que o README do port registra (`docs/port/README.md`)

- Linha 526 (a seção de desempenho, a mais antiga): a thread foi "tentada e desligada": "o jogo muda dados que a lista ainda referencia; o usuário
  viu a câmera saltar". **A linha 1158 desfaz isso**: "a nota acima, que culpava a thread pelo salto da câmera, estava
  errada: era outro defeito, já corrigido". O plano da F5 (`00-escopo.md`, seção 4) e o `01.md` ainda repetem a versão
  velha; **o salto da câmera não é evidência contra a thread**.
- O que sobra de verdade (linhas 1150 a 1180): ligada num i5 em velocidade normal, a thread rodou menus e uma luta
  corretamente, com o lado do jogo caindo para **3,5 a 4,1 ms por quadro**, e os quadros desenhados **idênticos byte a
  byte** aos sem thread, salvo 4 de 121 quadros de uma luta de demonstração, que diferem na coluna de pixels mais à
  direita (uma ou duas colunas; **não explicado**). Sem limite de velocidade (`session5.pad`, tela dividida) a versão com
  thread **para no quadro 2010**, onde a luta começaria, "interpretando lixo como programa de vértices".
- O `BT3_GS_RACE=1` (hash dos blocos da lista na entrega e de novo depois do desenho) achou **12 quadros entre 489 e
  2015** em que o jogo reescreveu a lista enquanto era desenhada ("a lista tinha N blocos na entrega e 1 depois"), todos
  em menus e cargas, nenhum nos quadros de luta que a execução alcançou. Correção proposta lá: dar à thread uma cópia da lista.

### 3.3 Minha leitura do que a corrida é [hipótese forte]

"N blocos na entrega e 1 depois" é **exatamente** o que o `race_walk` conta numa lista que passou a começar por um `END`:
a primeira etiqueta vira `0x70000000` (id 7, o ramo `default` do `race_walk`, um bloco). E o `Dma_ResetBuffers`
(`src/sys/dma.c`) faz isso: escreve `DMA_TAG_END` na etiqueta zero dos **dois** buffers. Ele é chamado em 51 pontos (as
telas de menu na saída, as cargas em `loading.c`, o início e o fim da batalha), e o port faz do `sceGsSyncPath` que costuma vir antes (`menu_a.c:556`, `menu_i_c.c:107`,
`menu_a_c.c:368` e mais) **um stub que devolve 0** (`port/src/plat_stub.c:46`). O `Dma_WaitCond` e o `Dma_WaitVu1`
são vazios sob `PORT` (`dma.c`), e `VIF1_STAT`/`GIF_STAT` são memória comum no port (`plat_mem.c`: lê-se o que foi
gravado por último), então os laços de espera do `Dma_SendVif1` terminam na hora. Ou seja: **todos os
lugares onde o jogo esperava o GS acabar são, no PC, esperas que não esperam**, e o `render_wait` só existe dentro do
`gs_core.c`, no começo da entrega seguinte. O mesmo explica o quadro 2010 sem limite de velocidade: a thread do jogo vai
muito à frente da de desenho e a carga reaproveita memória que a lista anterior ainda lê.

Como confirmar: ligar o `render_wait` nesses pontos e rodar o `BT3_GS_RACE` de novo nas mesmas gravações. Se os 12
quadros somem, é isso.

### 3.4 O que o jogo altera depois de entregar, e como evitar

| O que | Quando muda | Quem lê | Como evitar |
|---|---|---|---|
| Os dois buffers de lista (`Dma_ResetBuffers`, `Dma_LendBufferTails`, `Dma_ReclaimBufferTails`) | trocas de tela, cargas | a cadeia, enquanto roda | esperar a thread dentro dessas três funções (sob `PORT`) |
| Memória de partição, texturas, cadeias de modelo (liberadas e reaproveitadas) | cargas, troca de estágio, fim de luta | os REF e CALL da cadeia | o `sceGsSyncPath` espera de verdade (23 pontos), mais um `render_wait` na entrada da carga de partição |
| Pacotes montados em estruturas persistentes, não no buffer de lista (o pacote do CLUT em tempo de execução, `gfxm_b_b.c`, ~linha 400, `work->ref`, tem a etiqueta REF dentro da própria estrutura) | quando o jogo troca o CLUT | a cadeia (CALL/REF para a estrutura) | candidato à cópia para dentro da lista; o detector de corrida diz se acontece |
| Fluxos VIF dos modelos, `Vu1Node_OrFlags` (`vu1_packet.c`: faz `|=` nas palavras do próprio fluxo) | uma vez, na carga do estágio (`stg_d_b.c:142`) | a cadeia, nos quadros seguintes | só acontece na carga; coberto pela espera da carga |
| `gDbuff` (envelope e sprite de limpeza, que o jogo remenda por quadro) | todo quadro, antes do `sceGsSwapDBuff` | `Port_GsGifChannel`, que **já espera** a thread e lê na hora | nada a fazer: a espera existe |
| Contadores e estado lidos pelos dois lados (`gGsFrame`, `gGsMainFbp`, `gX360Luta`) | | | ver seção 7 |

O que **não** precisa de cópia: a memória da VU1, a VRAM, os registradores e as listas gravadas, porque só quem anda a
cadeia as escreve e lê (seção 2.2). A pergunta "copiar ou travar" se resume aos dados do jogo que a cadeia lê por REF e CALL.

### 3.5 A decisão 24, resposta 5, e o vigia

- **Resposta 5:** "a thread do jogo continua dona do D3D, com as cargas divididas e apresentação entre os blocos; a
  Guide pausa a luta. A thread de desenho separada só se isso não bastar, reestimada antes." O gatilho é a CPU em série
  desta etapa: 75,7 ms contra 33,37, **não é problema de GPU**. Com a thread de desenho dona do D3D, o "uma carga longa
  deixa de apresentar" some de graça (a thread de desenho repete o último quadro enquanto a do jogo carrega), o que
  substitui a metade do custo de dividir as cargas. A pausa da luta pela Guide continua do lado do jogo.
- **A `main360.cpp`** cria o dispositivo, chama `ReleaseThreadOwnership` e só vigia (`Vigiar`, a cada 50 ms olha
  `gPortVBlanks` e `gGsD3dQuadros`); o backend pega a posse no `d3d_init` (`gs_d3d.cpp:1637`), e `GsD3d_Soltar` a devolve
  (só se quem chama é o dono). **Sob o `BT3_GS_THREAD` quem pega a posse é a thread de desenho** (é ela que roda o
  `GsGpu_Init`): o `Port_Sair`, rodando na do jogo, não consegue soltar, e a main, depois de 2 s, toma o dispositivo à
  força (já previsto no código, com a linha "ainda era da thread N: tomado"). Para a saída ao dashboard (lição do
  `CLAUDE.md`: desmontar threads antes do `XLaunchNewImage`) a thread de desenho tem que ser parada e solta antes.
- **`sMem.seguro`** (`gs_d3d.cpp:1475`): com `BT3_GS_THREAD` ou `BT3_INTERP` definidos o backend **recusa descartar**
  texturas por falta de memória (`GsD3d_Falta` devolve 0). Foi posto de propósito na etapa 1: o descarte supõe "a lista em
  gravação só aponta para texturas com `last == gGsFrame`", e as entradas dos dois últimos quadros nunca saem
  (`gs_draw.c`, `GsDraw_X360Abre`: `last + 2 <= gGsFrame`). Esse contrato **serve** à thread de desenho com um quadro de
  profundidade, **desde que** o `gGsFrame` avance na entrega e não no fim do desenho, e que a varredura de "cache
  cheio" (`gsTexCount == MAX_TEX`, segunda passada com `keep = gGsFrame`) não tire as texturas da lista em desenho
  (ela tiraria: precisa do mesmo "dois quadros").
- **O `BT3_GS_THREAD` nunca rodou no console** (nenhuma nota registra isso, e o descarte fica desligado nele). O que o
  `pthread` do 360 faz: `posix360.c` implementa `pthread_create` com `CreateThread` **sem `XSetThreadProcessor`**, mutex
  por `CRITICAL_SECTION` e a condição por **um evento de reinício automático** (`SetEvent`): o `pthread_cond_broadcast`
  acorda **um** esperador só. Serve ao par que o `gs_core.c` usa (um esperador por condição), não serve a mais de um
  trabalhador sem trocar por eventos de verdade. A thread nasce com o modo de float certo (`_controlfp(_RC_CHOP)` no
  `partida`): isso é obrigatório, porque o `hle_program*` faz conta de ponto flutuante e a lista tem que sair igual à da
  thread do jogo.

## 4. Os pontos de corte e o que cada um exige

### 4.1 Três lugares onde se corta

- **C1, depois da lógica (a entrega da lista):** `Port_GsVif1Chain`. A thread do jogo faz L e a entrega; a de gravação
  faz o passo 5 em diante. É o (c). Dados que precisam de cuidado: **só os REF e CALL da cadeia** (seção 3.4); todo o
  resto do GS fica de um lado só.
- **C2, entre a gravação e o desenho:** a chamada `sBackend->frameEnd()`. O (a) corta só aqui (com C1 já cortado, é o
  degrau seguinte). Exige separar o que o `frame_end` toca em comum com a gravação: as listas gravadas (dois jogos,
  uma troca de ponteiros por quadro ou uma cópia da parte usada), a tabela `gsTargets` (campos com dois escritores),
  o cache de texturas (a gravação cria e destrói; o desenho usa), a `cova`, os contadores `sMem` (sem trava hoje) e as
  oito entradas do backend que a gravação chama. Nada disso é difícil sozinho, mas é **a parte mais espalhada** de tudo
  que este estudo olhou (seção 4.3).
- **C3, dentro do V:** entre andar a cadeia e montar vértices. A parte que lê a VRAM (hash, `texture_get`) tem que ficar
  do lado que anda a cadeia; a que só monta vértices e constantes a partir da memória da VU1 pode ir para o outro lado,
  se cada lote da VU1 levar consigo uma **cópia** da memória usada da VU1 (uns 1,5 a 2 KB) e do estado do GS que o
  desenho lê (`GsState`, uns 400 bytes, só quando mudou). Cópia: uns 3,5 mil lotes por quadro, de 3 a 8 MB [estimado];
  na ordem de 2 a 3 ms. É o (d) da tarefa, e o (e) abaixo.

### 4.2 O que se copia, o que se trava, a memória

| Dado | (c) corte C1 | (c + a) C1 e C2 | (e) C1, C2 e C3 |
|---|---|---|---|
| **Cadeia (lista + REF/CALL)** | travar: esperar a thread nos pontos de espera do PS2 (barato). Alternativa: copiar o fecho da cadeia na entrega: lista ≤ 1 MiB + texturas reenviadas (uns **2,8 MB por quadro de luta**: as 351 páginas de 8 KB que o hash refaz, `08.md`) + cadeias de vértices (uns 2 a 3 MB [estimado]): **uns 5 a 7 MB por quadro, uns 2 a 3 ms na thread do jogo, 2 jogos = 10 a 14 MiB a mais** [estimado] | igual | igual |
| **VRAM do GS (4 MiB), estado do GS, VU1** | **nada**: um escritor só, na thread de gravação | nada | VRAM: ou a thread que anda a cadeia resolve as texturas (fica com o hash) ou há duas VRAMs (4 MiB a mais, mais a cópia dos envios por quadro); recomendo a primeira |
| **Listas de desenho** (66,375 MiB, fixas) | nada (uma só) | **dois jogos**: ou encolher as capacidades e fazer *ping-pong* (zero cópia), ou copiar só a parte usada na troca (uns 4,4 MiB, 1,5 a 2 ms na thread de gravação) | igual |
| **Texturas decodificadas** | nada | nada de cópia: o contrato "nunca os dois últimos quadros" cobre (com os dois ajustes da seção 3.5) | igual |
| **Estado do GS por lote da VU1** | nada | nada | por lote: cópia de `GsState` quando muda, mais a memória usada da VU1 |
| **Alvos (`gsTargets`)** | nada | separar `cleared` em "tem imagem para a gravação" e "tem imagem na EDRAM/textura" | igual |

**Capacidade das listas.** As máximas vistas nas duas rodadas da etapa 6 (`/srv/bt3/dados/capturas/etapa6_r1.log` e
`etapa6_r2.log`, linhas "o maior quadro do trecho") são, [medido]: **13.182 vértices de CPU de 1.048.576, 53.618 de VU
de 524.288, 128.772 índices de 1.048.576, 2.224 constantes de 8.192, 2.454 desenhos de 65.536**. A parte usada dá uns
4,4 MiB; a reservada é 66,375 MiB. Duas rodadas com uma luta e menus não são o jogo todo (estágios com mais efeitos, a
tela dividida de 4 do Tag Team da decisão 16): por isso a resposta 2 da decisão 24 manda redimensionar "só com os
máximos medidos". Com o dobro da folga sobre esses máximos (32 Ki vértices de CPU, 128 Ki de VU, 256 Ki índices, 4.096
constantes, 8.192 desenhos) um jogo ocupa uns **10,1 MiB**; **dois ocupam 20,2, ou seja, 46 MiB a menos que hoje**.
Sem encolher, dobrar custa 66,4 MiB a mais, e o log da etapa 6 diz "livres 45 MB, mínimo visto 30 MB (piso 32)".

### 4.3 Por que o C2 custa mais do que parece

O `GsBackend` tem catorze entradas; só o `frameEnd` consome a lista. As outras oito que a gravação chama devolvem recursos
do D3D (texturas, alvos, pipelines) e o eviction olha os contadores de memória do D3D no meio da gravação (`GsD3d_Falta`
lê `mem_livres`). Para a gravação continuar numa thread e o D3D noutra:

- **A criação de recurso fora da thread dona do dispositivo** (`CreateTexture`, `LockRect` na `texCreate`/`texUpload`,
  chamados pela gravação): **precisa ser confirmada no XDK** (a documentação de `AcquireThreadOwnership` e de
  `CreateTexture`; a máquina do XDK é a do Linux, não esta). Se não for livre de thread, a saída é a gravação decodificar
  em memória comum e **enfileirar** a criação e o envio para a thread de desenho (o `GsTex` vira um número de vaga
  resolvido do outro lado): trabalho a mais, e o `texture_get` que hoje devolve um handle pronto passa a devolver uma
  vaga ainda vazia.
- **A `cova`, os contadores `sMem`, `sTexBucket`:** hoje sem trava. Cada um precisa de dono ou de trava.
- **O eviction** só é seguro com o contrato dos dois quadros e com o `gGsFrame` avançando na entrega.

No C1 sozinho **nada disso existe**: gravação e desenho ficam na mesma thread e o backend continua a ser chamado como hoje.
Por isso o (c) é o degrau barato e o (a) o caro, ao contrário do que a lista da tarefa sugere pela ordem.

## 5. As opções, com a conta

**Conta.** Estágios (ms por quadro de luta): L 15 (jogo), W 20 (andar a cadeia e GS), V 26,3 (VU1), E 14,5 (desenho na
CPU). Período do pipeline = o estágio mais lento, **sem custo de entrega, sem SMT, sem disputa de L2 e de banda**
[derivado]. Meta: 33,37 ms (30 fps) com margem; a GPU (17 a 19,7) cabe e corre sozinha. A conta é otimista: o Xenon é em
ordem, os 3 núcleos dividem 1 MB de L2, e duas threads no mesmo núcleo (SMT) rendem cada uma uns 60 a 70% de uma sozinha
[estimado]: **não se põe a thread do jogo e uma pesada no mesmo núcleo.** Acrescente de 10 a 30% aos períodos [estimado].

| Opção | Estágios (ms) | Período | Ganho sobre 75,7 | Threads pesadas | Memória a mais | Dias de agente [estimado] | Risco |
|---|---|---|---|---|---|---|---|
| Hoje | L+W+V+E = 75,7 | 75,7 | | 1 | | | |
| **(a)** só o fim do quadro (C2) | L+W+V = 61,3 / E = 14,5 | **61,3** | -14,4 (19%) | 2 | listas (20 MiB com capacidade encolhida; 66 sem) | 4 a 6 | alto: C2 inteiro (4.3) para ganhar o que o (c) ganha |
| **(b)** VU1 e fim do quadro numa thread, resto na do jogo | L+W+cópia por lote ≈ 38 / V+E+cópia ≈ 44 | **~44** | -32 (42%) | 2 | VRAM duplicada (4 MiB) e a cópia dos envios; listas | 6 a 8 | alto: C3 sem o ganho do C1 |
| **(c)** a gravação inteira numa thread (C1) | L ≈ 15,5 / W+V+E = 60,8 | **60,8** | -15 (20%) | 2 | 0 (travar) ou 10 a 14 MiB (copiar a cadeia) | 3,5 a 5 | médio: corridas nos REF; ganho de 20% só |
| **(c + a)** gravação e desenho em threads separadas (C1 e C2) | L 15,5 / W+V = 46,3 / E = 14,5 | **46,3** | -29 (39%) | 3 | o do (c), mais listas (20 MiB) | +4 a 6 sobre o (c) | alto (C2); ainda 13 ms acima de 33,37 |
| **(e)** três estágios com o corte dentro da VU1 (C1, C2, C3) | L 15,5 / W+hash+texturas+cópia ≈ 31,7 / vértices e constantes+E ≈ 32,1 | **~32** | -44 (58%) | 3 | o do (c + a), mais a cópia por lote | +5 a 8 sobre o (c + a) | alto; **no papel passa 33,37 por 1,3 ms, sem margem** |
| **(d)** dividir a VU1 por lote entre trabalhadores (sobre o (c + a)) | L 15,5 / W+hash+cópia ≈ 31,7 / trabalhadores 17,6 ÷ n / E 14,5 | **~32 com 4 ou 5 threads pesadas** | igual ao (e) | 4 a 5 | igual ao (e) | +3 a 5 sobre o (e) | alto: não há threads de hardware livres (abaixo) |

**O que a tabela diz, sem rodeio:**

1. **Nenhum corte de duas threads cumpre os 30 fps.** (c) e (a) dão uns 61 ms, (b) uns 44. O ganho dos dois primeiros é
   o mesmo (um estágio de uns 15 ms), mas o (c) custa uma fração do (a) (seção 4.3).
2. **O mínimo que passa de 33,37 só no papel é o (e), com ~32 ms.** Para ter margem, o trabalho da gravação também
   tem que cair, em paralelo, ou o (c + a) (46,3) precisa perder uns 13 ms de W+V. O maior candidato conhecido é o
   `Gs_PageHash` (7,4 ms do V, o laço de 8 KB por página, 351 páginas por quadro porque o jogo reenvia as texturas do
   estágio todo quadro): hash só das páginas que a textura usa, `dcbt`, ou a chave pela origem do envio (item 3 do
   plano de `pesquisa/03`). A primeira parte desta etapa tirou uns 5 ms do V (31 para 26).
3. **(b) fica dominado:** por menos de 3 ms de diferença sobre o (c + a) ele exige as mesmas cópias do (e) e a VRAM em
   dois buffers, que no (c) e no (c + a) **não existe** (um escritor só). A "VRAM em dois buffers" que a lista da tarefa
   põe no (c) só aparece quando metade da gravação (o que lê VRAM) e a outra metade (o que a usa) rodam em threads
   diferentes, isto é, no (b) e no (e); nestes recomendo manter o hash e a decodificação do mesmo lado em vez de duplicar.
4. **Threads de hardware:** o jogo está na 2, o diário na 1, o XAudio2 da F6 na 4 e na 5 (`CLAUDE.md`). Pares de SMT: (0,1),
   (2,3), (4,5), um por núcleo. A thread de gravação vai para a **0** (outro núcleo que o jogo; o diário na irmã dela só
   acorda a cada 50 ms), e a de desenho, que trabalha uns 44% do período, para a **3** (irmã do jogo: custa uns pontos
   dele) ou para a 1 (irmã da gravação: custa uns pontos dela): **medir as duas**. Uma terceira pesada (as 4 e 5 do
   (d)) divide o núcleo do áudio e arrisca estalo: o (d) não cabe sem decidir com a F6.
5. **A latência não piora:** o `Gfx_EndFrame` do jogo já mostra "um quadro depois de montado" (comentário do `gfx.c`), e o
   quadro leva hoje 75,7 ms de ponta a ponta; no pipeline a lista N sai mais cedo e a de N+1 já está pronta.

### Recomendação (sem decidir pelo Lucas)

**Ir em degraus, cada um provado antes do seguinte:**

1. **Degrau 1, o (c) com as esperas do PS2** (3,5 a 5 dias, dos quais uns 2 de console). Ganho: 75,7 para uns 61 ms, a
   thread do jogo com uns 16 ms (menus a 60 fps ficam folgados), Guide e cargas sem o dispositivo preso. É o que já existe
   no `gs_core.c` mais: `sceGsSyncPath`, `Dma_ResetBuffers`, `Dma_Lend/ReclaimBufferTails` e o início das cargas
   esperando a thread, um contador de quadros do lado do jogo, o `gGsFrame` e o descarte com "dois quadros", e do lado
   do 360 a thread, a posse do dispositivo e a saída. **Por que primeiro:** é pré-requisito dos outros, o mais barato, o
   único que se prova inteiro no WSL, e diz se os REF são seguros antes de pagar uma cópia.
2. **Degrau 2, só se o 1 provar:** o (c + a) (46,3 ms), com o pedaço do C2 que a seção 4.3 lista, e a decisão da
   capacidade das listas.
3. **Em paralelo aos degraus 1 e 2:** cortar W e V (o hash de página primeiro), porque nenhum arranjo de threads
   chega a 33,37 com a margem que o aceite pede sem isso.
4. **O (e) e o (d) só se faltar**, e com a F6 no mesmo quadro (núcleo do áudio).

Se o Lucas quiser **uma só mudança grande**, o (e) é o único que passa no papel, mas com 1,3 ms de folga, o mais caro e o
mais arriscado; não recomendo começar por ele.

## 6. O que fica no código do 360 e o que é portável

| Parte | Onde | Prova |
|---|---|---|
| Entrega e espera (`render_post`/`render_wait`), ganchos de espera nos pontos do PS2 (`sceGsSyncPath`, `Dma_ResetBuffers`...), o contador de quadros do jogo, o contador `gGsFrame` na entrega, a trava de `gGsFrame` na gravação | `gs_core.c`, `plat_stub.c`, `dma.c` (sob `PORT_X360`/`PORT`, original no `#else`, regra do `AGENTS.md`) | **WSL**, x86 e PPC, com a build `--gpu` e o backend falso |
| A cópia do fecho da cadeia (se a espera não bastar), a separação das listas em dois jogos, os campos de `gsTargets` com dois escritores, o cache de texturas com a regra dos dois quadros, a fila de criação de texturas | `gs_draw.c`, `gs_core.c` | **WSL** (a lista por quadro compara) |
| O detector de corrida (`BT3_GS_RACE`) estendido a todos os pontos, e o atraso proposital da thread | `gs_core.c` e `port/x360/headless/` | WSL |
| A thread de gravação e a de desenho: `CreateThread`, **`XSetThreadProcessor`** (0 e 3, nunca 4 e 5), a prioridade, o modo de float | `port/x360/src/` | **só o console** (compilador do XDK, `cl.exe`; o WSL compila a mesma lógica com `pthread`) |
| A posse do dispositivo: `AcquireThreadOwnership` na thread de desenho no `d3d_init`, `GsD3d_Soltar` e a saída ao dashboard, o vigia olhando as duas | `gs_d3d.cpp`, `main360.cpp` | só o console |
| `CreateTexture`/`LockRect` fora da thread dona, `Present` bloqueando na thread de desenho, o descarte com a GPU ocupada (`cova`, `IsBusy`) | `gs_d3d.cpp` | só o console (e a confirmação no XDK) |
| A **ordem de memória** do PowerPC: tudo que for entrega **sem trava** (um anel, um contador publicado) precisa de `__lwsync()`; com mutex e evento (o `pthread` do shim) a barreira vem junto | | **só o console**: a x86 e o `qemu-ppc` sobre ela **não mostram** um erro de ordem (a x86 tem ordem forte); um defeito de barreira só aparece no Xenon, como quadro raro errado |
| Medição (`Port_GsVif1Chain` por dentro, tempo por estágio, a ocupação de cada thread) | `gs_core.c`, `gs_d3d.cpp` | console |

**Um detalhe já visto:** `Port_GetEnv` (`gs_core.c`) publica a entrada no vetor com `count++` "por último" e lê sem
trava, o que na x86 basta; no PowerPC a leitura de `known[i]` pode passar à frente da de `count`. Com duas threads
chamando `getenv` (e hoje só há uma) precisa de `__lwsync` antes do `count++` sob `PORT_X360`.

## 7. Como provar, no WSL, que nada mudou

**A prova que já existe e serve:** as builds `--gpu` (`x86g`, `ppcg`) rodam o `gs_draw.c` de verdade com o backend falso
(`port/x360/headless/gpu_falso.c`), que faz o hash da lista de cada quadro **por seção** (texturas, contagens, desenhos,
vértices de CPU, vértices, índices, constantes da VU1) e o do jogo todo, e `vu1_perfil.sh lista` e `compara` as comparam
quadro a quadro. Hoje o `BT3_GS_THREAD=1` já roda nessas builds sem mudança de código (o backend falso apenas passa a
rodar o `frameEnd` na thread de desenho): é a primeira rodada da seção 8.

| Quero provar | Como | Critério |
|---|---|---|
| A lista de desenhos é a mesma com a thread | `lista` sem e com `BT3_GS_THREAD=1`, `f5_duelo` (8.380 quadros) e `f5_treino` (7.386), x86g e ppcg, `compara` | iguais em todos os quadros; hash do jogo igual aos já registrados (`efcae9024a500155` e `f971b3ff64229bf3` na x86; `18db6517cd301b15` e `f971b3ff64229bf3` na PPC) |
| Não há corrida | o `BT3_GS_RACE` (já faz o hash dos blocos na entrega e depois do desenho, **inclusive o dado apontado por REF**, `race_walk`) ligado em **todos** os roteiros (`f5_*`, `duelo`, `missao`, `historia`, `boot`...), mais **o atraso proposital**: uma chave só de teste que faça a thread de desenho dormir X ms antes de cada trabalho, para a thread do jogo correr à frente e a janela da corrida abrir, sem depender da sorte do agendador | nenhuma linha `race:` e a lista igual **com o atraso também** |
| Os dois tipos de corrida que o hash de bloco não vê (um dado que muda e **volta** no mesmo quadro; a leitura de um dado enquanto o jogo o escreve pela metade) | só o atraso proposital em vários tamanhos (1, 5, 20 ms) diz; o hash final igual nos três | igual nos três |
| A lógica não mudou | a thread do jogo não muda: o rastro (`roteiros.sh`) x86 e PPC, a memória nos pontos, os saves | `tudo certo`, como na conferência da etapa 8 (`REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino`, `SEMMAPA=1 IMG=1 roteiros.sh duelo`) |
| O roteiro de pad continua determinístico | o `gs_input.c` escolhe o botão pelo `gGsFrame`: **se a thread de desenho avança o contador, o pad do quadro N+1 passa a depender de quanto ela adiantou**. O contador do jogo tem de avançar **na entrega** (`Port_GsVif1Chain`, na thread do jogo, mesma sequência numérica que hoje) e a thread de desenho guarda o número do quadro dela dentro do trabalho | rastro e memória iguais com e sem thread |

**O que o WSL não prova:** a ordem de memória do Xenon (seção 6); o ganho real em ms (o `qemu-ppc` e a x86 não têm o
tempo do console); `CreateTexture` fora do dono; a Guide. **O que não serve:** o `tsan`/`helgrind` (a build é x86 de
**32 bits estática**: `file port/build/x86g/bt3_x86g`, e o ThreadSanitizer só existe para 64 bits).

**Verificadores que têm de ser vistos falhando de propósito** (regra do `AGENTS.md`), antes de valerem: (1) tirar a espera
do `sceGsSyncPath` ou do `Dma_ResetBuffers` numa cópia e conferir que o `BT3_GS_RACE` e o `compara` acusam; (2) uma
versão da entrega que copia a cadeia **depois** de a thread do jogo mexer; (3) o contador de quadros na thread de
desenho, e o rastro tem de divergir; (4) o descarte sem a regra dos dois quadros, e o hash das texturas tem de divergir.

**A prova no console** (imagem): as capturas `BT3_SHOT` (`gpu_NNNNN.ppm`) da build com e sem thread, nos mesmos quadros de
cenas controladas, com os limiares da seção 3 do escopo (98% dos pixels até 8, PSNR de 30 dB): a lógica do console
diverge da x86 já no primeiro quadro de luta (float), mas **o console contra o console** tem a mesma divergência nas duas
builds se a thread não mexe no jogo, o que se confere pelo `hash.txt` ligado com `BT3_HASH` (a thread não muda a lógica,
então os dois têm que ser iguais entre si).

## 8. O que medir primeiro (nada disso é código novo, ou é uma linha)

1. **No console, uma rodada de luta com o `BT3_GS_VERBOSE` ligado no `ambiente.txt`**: o `gs_core.c` já imprime "work per frame:
   game code X ms, display list Y ms (texture decoding, pipeline creation, ending and submitting the frame)" a cada 60
   quadros (`stderr`; **conferir se o `stderr` do 360 cai no `saida.txt`**, o `Port_Printf` cai). Isso separa **L de W+V+E**
   (hoje o W é por subtração). Se não sair, duas marcas `GsD3d_Tique` na entrada e na saída da `Port_GsVif1Chain` fazem o
   mesmo.
2. **No WSL, um contador de bytes por quadro**: bytes de lista (`Dma_GetUsedSize`), bytes de REF e CALL e bytes de
   IMAGE, por quadro de luta, nas builds `--gpu`. É o número que decide entre travar e copiar a cadeia (seção 4.2); hoje só
   há a estimativa das 351 páginas por quadro (2,8 MB).
3. **No WSL, a rodada do `BT3_GS_THREAD=1` sem mudar nada** (as duas builds `--gpu`, `f5_duelo`, `f5_treino`, `lista`, `compara`,
   `BT3_GS_RACE`), para ver quantos quadros divergem **antes** das esperas novas: é a linha de base que o degrau 1
   precisa vencer, e confirma ou derruba a hipótese da seção 3.3 (12 quadros em menus e cargas). Custo: x86g na ordem de
   minutos por roteiro, ppcg na de horas (o `qemu-ppc` leva horas no `f5_duelo`); **um ciclo de emulador por vez** no WSL.
4. **No console**: a ocupação das threads de hardware 0, 1 e 3 com o jogo rodando (um contador de tempo ocupado por
   thread de teste), para escolher entre 0, 1 e 3 sem chute.

## 9. Perguntas para o Lucas (com a opção conservadora)

1. **Encolher as capacidades das listas** (66,375 MiB fixos para um uso máximo de uns 4,4 MiB nas rodadas medidas)? Sem
   isso o (a), o (c + a) e o (e) não cabem. *Conservadora:* **não encolher agora**; o degrau 1 não precisa. Decidir quando
   o degrau 2 for pedido, com os máximos da luta de 4 jogadores e dos estágios pesados.
2. **O gatilho da resposta 5 da decisão 24 foi cumprido?** (A CPU em série da etapa 8, 75,7 contra 33,37, não se resolve
   dividindo as cargas.) *Conservadora:* sim, mas **só para o degrau 1** (o (c)), reversível por uma chave no
   `ambiente.txt`; os demais ficam para depois da medição do degrau 1.
3. **Quem fica com o dispositivo** (a thread de desenho passa a ser dona, e repete o último quadro enquanto o jogo carrega)?
   *Conservadora:* sim, com a main só vigiando como hoje; a Guide pausa a luta pelo estado do jogo, como a resposta 5 já dizia.
4. **Em que thread de hardware** a de gravação (0, a minha escolha) e a de desenho (3 ou 1), com o áudio da F6 nas 4 e 5?
   *Conservadora:* gravação na 0, desenho na 1, **nunca na 3** (irmã do jogo) até medir.
5. **Correção do plano da F5:** o `00-escopo.md` (seção 4) e o `01.md` repetem que a câmera saltava por causa da thread;
   o README do port a corrige. *Conservadora:* **não mexi** nesses arquivos (o entregável é só este); a correção entra
   no commit que fechar essa decisão.

## 10. Limites desta leitura

- Todos os ms da seção 5 são **conta sobre números do console de uma rodada** (a luta da etapa 8), não de execução do que
  se propõe. As divisões do V por trecho vêm de amostras na **x86**, e a premissa é que a proporção vale no Xenon (não
  está provada: a mesma página de 8 KB custa outra coisa numa CPU em ordem com 1 MB de L2 dividido).
- O W (uns 20 ms) é a sobra de uma subtração, não uma medida.
- Não rodei nada: a hipótese da seção 3.3 sobre o `Dma_ResetBuffers` e o `sceGsSyncPath` **bate com o código e com o que o
  README registra, mas não foi confirmada** (seção 8, item 3).
- Não vi o XDK (está na outra máquina): o que digo da posse do dispositivo e de `CreateTexture` fora do dono é o que o
  código e o guia dizem (`guia-de-port.md`, "só a thread dona usa o dispositivo"), e o que falta confirmar está marcado.
- "Dias de agente" seguem a régua do plano da F5 (um dia útil são 6 h; a etapa 6 estimou 2 a 4 dias) e são palpite pela leitura.

## Degrau 1

Feito no branch `cursor/f5-degrau1`, a partir de `troca/main` (`2da745d`, a tarefa em
[`tarefas/f5-degrau1.md`](../../tarefas/f5-degrau1.md)). O Lucas aprovou a recomendação deste estudo com as respostas
conservadoras da seção 9. As partes, na ordem da tarefa, cada uma com commit próprio: as medições baratas (1), o corte do
hash de página (2), a gravação numa thread (3). Convenção: **[medido]** é o que uma rodada deste branch deu.

### Parte 1: as medições baratas, no WSL

#### 1a. Os bytes que a cadeia leva por quadro [medido]

`gs_core.c` ganhou, só nas builds `--gpu` (`-DBT3_GPU_FALSO`, posto por `headless.py`; as outras builds não o têm e a
`.text` da x86 não muda), o `bytes_walk`: antes de andar a cadeia do quadro, anda-a de novo contando etiquetas, bytes da lista
(as etiquetas e os dados dentro dos dois buffers de lista do jogo), bytes de cadeia externa (CNT e CALL fora dos buffers: as
cadeias de vértices dos modelos), bytes de REF/REFS/REFE (os dados do jogo apontados) com e sem contar duas vezes o mesmo
(endereço, tamanho), e bytes de imagem (os IMAGE, o envio de textura; os envios pelo canal GIF fora da lista entram no
quadro seguinte). `BT3_GPU_BYTES=1` imprime uma linha `gpu_falso: bytes quadro N ...` por quadro; `bytes_quadro.py` resume
(confere uma linha de bytes por linha de lista, na mesma ordem, e valores possíveis). A lista de cada quadro saiu igual à de
antes nos dois roteiros (a medida não toca na gravação).

Por quadro de luta (os de mais de 20 mil vértices de VU1), em KiB:

| Roteiro | Quadros | Lista | Externa | REF | REF sem repetir | Imagem (dentro dos REF) | Etiquetas |
|---|---:|---:|---:|---:|---:|---:|---:|
| `f5_duelo` | 3.869 | 168,5 (máx 262) | 1.771 (máx 2.364) | 2.318 (máx 3.216) | 2.234 | 2.266 | 1.668 |
| `f5_treino` | 3.166 | 176,0 (máx 237) | 1.550 (máx 2.067) | 2.101 (máx 2.682) | 2.026 | 2.052 | 1.492 |

O **fecho** que uma cópia da cadeia na entrega levaria (lista + externa + REF sem repetir) é de **4,08 MiB por quadro de luta
no `f5_duelo` (máximo 5,37) e 3,66 no `f5_treino` (máximo 4,65)**; nos menus, 0,86 e 0,81 MiB (máximo 2,5). A estimativa da
seção 4.2 (5 a 7 MB) estava um pouco alta. O que isso diz: copiar a cadeia custaria uns 4 MB de `memcpy` por quadro na thread do
jogo (de 1 a 2 ms no Xenon [estimado]) e 8 a 11 MiB de memória nos dois jogos, contra **zero** da espera. A espera nos pontos
do PS2 é o caminho, e a cópia só entra se a Parte 3 mostrar que a espera não basta. Quase todo REF é textura (a imagem é
97% do REF): o jogo reenvia as texturas do estágio inteiras em todo quadro, e o hash de página (2,8 MB estimados) é sobre o
mesmo volume.

#### 1b. O `BT3_GS_THREAD=1` sem mudar nada: a linha de base [medido]

`vu1_perfil.sh lista` com `BT3_GS_THREAD=1 BT3_GS_RACE=1`, no código de antes (o `gs_core.c` do `9010268` mais a medida acima),
contra a lista sem thread (`divergencias.py`, que casa os quadros pelo número):

| Build e roteiro | Quadros que saem | Quadros com a lista diferente | Quadros com linha `race:` | Fim |
|---|---:|---:|---:|---|
| `x86g` `f5_treino` | 7.235 de 7.386 | 4.183 (o primeiro, 233) | 12 (24 linhas) | `SIGSEGV` no quadro 7.235 |
| `x86g` `f5_duelo` | 8.229 de 8.380 | 5.202 (o primeiro, 233) | 15 (27 linhas) | `SIGSEGV` no quadro 8.229 |
| `ppcg` `f5_treino` | 7.235 de 7.386 | 4.181 (o primeiro, 737) | 11 (22 linhas) | `SIGSEGV` no quadro 7.235 |
| `ppcg` `f5_duelo` | 8.229 de 8.380 | 4.096 (o primeiro, 989) | 12 (23 linhas) | `SIGSEGV` no quadro 8.229 |

**A hipótese da seção 3.3 se confirma.** Todas as corridas dos menus e das cargas (os quadros 234 e 486, 738, 990, 1.242,
1.494, 1.746, de 252 em 252, depois 2.869, 3.036 a 3.262, 3.978 a 4.136) têm a forma "a lista tinha N blocos na entrega e 1
depois" e o bloco 0 mudado: a etiqueta na primeira posição de um dos dois buffers de lista (`006bd740`, `007bd780`), que é
o `END` que o `Dma_ResetBuffers` escreve nos dois. Três corridas de outro tipo, em luta (quadros 4.905, 4.934 e 6.971, só nos
roteiros do duelo e do treino conforme o caso): um REF de 73 palavras de quatro (`01039d00`, 1.168 bytes) que o jogo mexe
enquanto a lista o lê, o pacote em estrutura persistente da seção 3.4 (o CLUT em tempo de execução). A lista que sai errada
num quadro desloca o número das texturas criadas depois dele (o backend falso numera por ordem de criação): por isso a
divergência vira quase todos os quadros a partir da primeira luta, e **o número de quadros divergentes não conta corridas**,
conta o quanto uma corrida se propaga; o número que conta é o de quadros com `race:`. O fim por `SIGSEGV` nos quadros 7.235 e
8.229 é o mesmo nas duas CPUs (o jogo lê memória que a carga já reaproveitou).

O que o degrau 1 tem de vencer: **zero linhas `race:`, zero quadros diferentes e o fim do roteiro**, nas quatro combinações.

### Parte 2: o corte do hash de página

#### O que mudou

`texture_get` (`gs_draw.c`) calculava o `gen` da textura (a chave do cache de texturas) com o hash das páginas
`tbp / 32` a `tbp / 32 + pages`, `pages` sendo o tamanho linear da textura em páginas, e o laço ia até `i <= pages`. Duas
imprecisões: sempre uma página a mais (uma textura alinhada que ocupa N páginas hasheava N + 1, e bastava a vizinha mudar
para a textura ser decodificada de novo sem necessidade), e, nos formatos de 8 e 4 bits dentro de um pixel de 32
(`PSM` 0x1B, 0x24, 0x2C), o `bits` da conta era o do formato e o endereço é o do pixel de 32 (uma página a menos do que a
decodificação lê). A função nova `tex_pages` (em `#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)`, com o laço de
antes no `#else`) dá o número de páginas que a decodificação **pode ler**: a página do canto de baixo e da direita (a página só
cresce com x e com y), pelas mesmas expressões do `vram_rw` escritas em páginas (32 bits: 64 x 32 pixels por página, 16 bits:
64 x 64, 8 bits: 128 x 64, 4 bits: 128 x 128, e a largura `bw >> 1` nos dois últimos), mais a página para a qual um bloco do
fim da grade vaza quando `tbp` não começa numa página. É o que a decodificação lê, nem uma página a mais, nem a menos.

**Por que não foi a forma mais agressiva.** O primeiro ensaio usou o tamanho linear exato em bytes (sem o `+ 1`): cortou mais
(2.129 decodificações no `f5_duelo` contra as 2.437 de agora), mas é a conta que sub-conta os formatos de 8 e 4 bits dentro
de 32 e o vazamento de bloco, e não foi adotada. A forma de agora segue a decodificação.

#### O que ficou provado

O hash do quadro de sempre **não pode** ser a prova aqui: com menos decodificações, o backend falso (que numera as texturas
pela ordem de criação) dá outros números às mesmas texturas e todos os quadros depois da primeira diferença mudam. Por isso o
backend falso ganhou o **hash normal** (`BT3_GPU_NORMAL=1`, `gpu_falso.c`): cada textura do desenho vale pelo hash do
conteúdo que o backend recebeu (largura, altura, formato e todos os pixels), não pelo número, e a seção das texturas criadas
fica de fora; o resto (contagens, alvos, pipelines, desenhos, vértices de CPU, vértices, índices e constantes da VU1) é o de
sempre. **Igual no hash normal quer dizer: em todo quadro, cada desenho amostra uma textura com os mesmos pixels que
amostrava.** `divergencias.py --normal` compara.

| Prova | Resultado |
|---|---|
| `x86g` antes contra `x86gv` depois, hash normal, `f5_treino` (7.386 quadros) e `f5_duelo` (8.380) | iguais nos dois, nenhum quadro diferente |
| `ppcg` antes (o `af356fe`, guardada) contra `ppcg` depois, hash normal, os mesmos dois | iguais nos dois, nenhum quadro diferente |
| `x86gv` contra `ppcg`, depois, hash normal | `f5_treino`: iguais; `f5_duelo`: os mesmos 11 quadros (4.918 a 4.937) da seção dos vértices de CPU que já diferiam antes (1/16 de pixel num sprite), nenhum outro |
| Seções da VU1 (vértices, índices, constantes), `x86gv` contra `ppcg`, depois | iguais nos 7.386 e nos 8.380 quadros |
| Texturas decodificadas (o que o corte economiza) | `f5_treino` 1.607 para 1.252 (de 148 para 115 MB); `f5_duelo` 2.968 para 2.437 (de 197 para 154 MB) |
| Recálculos de página (`Gs_PageHash`) | `f5_treino` 1.000.345 para 873.805 (menos 13%); `f5_duelo` 1.436.476 para 1.207.603 (menos 16%) |
| `GsVu1_Call` em tiques na `x86gv`, o `f5_duelo`, três rodadas alternadas sem e com o corte | 14,76 / 14,63 / 14,70 G contra 13,73 / 13,63 / 13,65 G: **7% menos** |
| Verificador visto falhando: `tex_pages` sem a última página (`return grid;`) | hash normal diferente em 3.282 de 7.386 quadros (`f5_treino`) e 4.578 de 8.380 (`f5_duelo`), na seção dos desenhos: `divergencias.py` sai com 1 |
| A `.text` da x86 plana | `cc46fa0752f092fca305bc8bf2e2e54390763af0`, igual à referência (a mudança é toda em `PORT_BIG_ENDIAN` ou `BT3_TESTE_VU1`) |

O hash de jogo de sempre (que **não** vale como prova) passa a ser `70b6b1d1bb3f1d85` (`f5_treino`, x86 e PPC iguais) e
`29caae14a161459a` na x86, `b65001fe18962dda` na PPC (`f5_duelo`, a mesma diferença dos 11 quadros).

#### A estimativa para o 360

Sete por cento dos tiques do `GsVu1_Call` na x86 (as três rodadas, estáveis dentro de 1%): sobre os 26,3 ms da gravação da VU1
da luta do console, **de 0,5 a 1,8 ms** se o Xenon seguir a proporção da x86 (1,8 é o limite de cima, 7% de 26,3; 0,5 é o piso
se o tempo no Xenon for de espera de memória e as páginas a menos pesarem só pelo volume lido: 13 a 16% de 2,8 MB, uns 0,4
MB por quadro). É pouco contra os 75,7 ms, e é **sem risco de lista** (a prova acima). As decodificações a menos (uns 20% no
total dos roteiros, 49 MB de pixels no `f5_duelo`) são cargas de estágio, não regime; no console poupam também a criação e o
envio da textura no D3D. O maior item do hash, o laço de 8 KB por página (uns 1,2 milhão de recálculos no roteiro), segue de pé:
o que sobra é o conteúdo que muda de fato (79% dos recálculos dão um hash que a página não tinha, a textura nova do ciclo do
estágio).

#### O que não foi feito, e por quê

- **Pular o recálculo quando o envio não mudou nenhum byte** (comparar dentro do `transfer_data`): só pegaria os 21% de
  recálculos de conteúdo igual, custaria uma comparação por palavra em todo envio (o laço que já foi um quarto do tempo) e
  mexeria em cinco caminhos de escrita: ganho e custo se anulam.
- **Contar a página "a mais" do `transfer_data`** (`last` inclusivo, que bumpa uma página que o envio alinhado não tocou): o
  `gGsPageGen` tem outros leitores (a idade dos alvos, o "nunca recebeu envio"), então mexer nele muda mais do que o hash;
  precisaria de um contador à parte só para o hash.
- **Um hash mais barato por byte** (somas com rotação, sem multiplicação de 64 bits): o ganho no Xenon não se mede no WSL
  (a PPC no `qemu` conta instruções do GCC de 32 bits, não os `mulld` do Xenon) e o risco de colisão em texturas de gradiente
  não vale um ganho que não se vê. Fica para depois da medição no console.

### Parte 3: a gravação inteira numa thread (a opção (c) com as esperas do PS2)

#### O que mudou

Tudo na chave `BT3_GS_THREAD` (no `ambiente.txt` do 360, junto com `BT3_GS=gpu`; sem a linha, ou com `0`, o jogo grava na
própria thread como antes). A parte portável vale nas builds com `PORT_BIG_ENDIAN` (a PPC e o 360) ou `BT3_TESTE_VU1` (a
`x86gv`), com o original no `#else`; a `.text` da x86 plana não muda.

| Peça | Onde | O que faz |
|---|---|---|
| `Port_GsEspera` | `gs_core.c`; chamada em `sceGsSyncPath` (`plat_stub.c`), `Dma_ResetBuffers`, `Dma_LendBufferTails`, `Dma_ReclaimBufferTails` (`dma.c`), `ADXF_ReadNw` (`plat_file.c`), `Gs_StateKeep` e `Gs_StateBack` | o jogo espera a thread acabar a lista entregue, onde o PS2 já esperava. Sem a thread, não faz nada |
| O canal da GIF na thread | `Port_GsGifChannel` vira o trabalho 4 da thread (`gif_corpo`), que o jogo espera | o clear do quadro e os envios de textura fora da lista criam textura e alvo no backend: com a gravação e o dispositivo de uma thread só (no 360 o D3D só vê uma thread) |
| Cópia dos REF pequenos | `ref_copia`, na entrega | os REF de até 128 quadwords cuja etiqueta está num dos dois buffers de lista levam uma cópia dos dados (área de 1 MiB, uma só: o jogo espera a lista anterior antes de copiar). Até **235 KB por quadro** no `f5_duelo` (200 KB no treino), nenhum REF sem cópia por falta de área |
| Contador do jogo | `gGsFrameJogo` (avança na entrega); `gs_input.c` o lê no lugar do `gGsFrame` | o `gGsFrame` segue sendo o da gravação (o descarte dos dois quadros e o `gs_d3d` o leem, só na thread de gravação) |
| Parada | `Port_GsParar` (espera, manda o trabalho 3, junta; `atexit` nos testes, `Port_Sair` no 360), `Port_GsPararJa` (a queda: só pede) | a thread solta o dispositivo (`GsD3d_Soltar`, ela é a dona) e sai |
| `Port_GetEnv` | `gs_core.c` | entradas publicadas com barreira e lidas pelo ponteiro (a ordem de memória do PowerPC, seção 6) |
| Chaves só dos testes | `BT3_GS_ATRASO_US` (a gravação demora de propósito, só nas builds `--gpu`), `BT3_GS_SEM_COPIA` | as provas negativas e a prova das esperas com a gravação lenta |

**A cópia dos REF pequenos não estava prevista como certa** (a tarefa a pedia "se as corridas do CLUT persistirem"): o pacote
do CLUT em tempo de execução (`GfxClut_InitPacket`, 73 quadwords, REF no meio da lista) é refeito pela lógica do quadro
seguinte, que roda **antes** do `sceGsSyncPath` do fim do quadro; nenhuma espera do PS2 o cobre, e a thread de gravação lê o
REF no meio da lista, depois de o jogo ter mexido. Sem a cópia o `f5_duelo` dá as 3 corridas de sempre (quadros 4.905, 4.934
e 6.971) e um quadro de lista diferente (4.933). Copiar na entrega dá o que a gravação na thread do jogo sempre deu: os bytes
do instante da entrega. Copiou-se tudo que é pequeno e está na lista, e não só o 73 do CLUT, porque o detector só vê os
pacotes que mudam nos roteiros de hoje; o custo é uma `memcpy` de uns 200 KB por quadro (menos de 0,1 ms).

**A transferência da GIF vai para a thread, e o jogo a espera**, em vez de rodar na thread do jogo como no `BT3_GS_THREAD`
do PC: o clear de cada quadro (`sceGsSwapDBuff`) é um desenho, e o backend cria o alvo e as texturas na hora em que o grava.
O custo são duas trocas de thread por quadro de luta.

**Não se copiou a cadeia inteira** (decisão da parte 1: 4 MB por quadro de luta): a espera nos pontos do PS2 mais a cópia dos
REF pequenos bastaram.

#### O que ficou provado (WSL)

A base de cada prova é a mesma build sem thread (`x86gv`, `ppcg`), comparada quadro a quadro (`divergencias.py`: o hash da
lista, cada seção, as linhas `race:`, o fim do roteiro e os dois contadores de quadros iguais no fim). A linha de base
da parte 1 (4.096 a 5.202 quadros diferentes, 11 a 15 quadros com corrida, `SIGSEGV` no fim) virou:

| Build | Roteiro | Atraso da gravação | Quadros diferentes | Linhas `race:` | Fim |
|---|---|---|---:|---:|---|
| `x86gv` | `f5_treino` (7.386) | 0, 1, 5 e 20 ms | 0 nos quatro | 0 | normal, hash do jogo igual (`70b6b1d1bb3f1d85`) |
| `x86gv` | `f5_duelo` (8.380) | 0, 1, 5 e 20 ms | 0 nos quatro | 0 | normal, hash do jogo igual (`29caae14a161459a`) |
| `ppcg` | `f5_treino` | 0 | 0 | 0 | normal, hash igual (`70b6b1d1bb3f1d85`) |
| `ppcg` | `f5_duelo` | 0, 5 e 20 ms | 0 nos três | 0 | normal, hash igual (`b65001fe18962dda`) |
| `ppcg` sem thread, depois contra antes (a `ppcg` do `5445c3f`) | os dois | | 0 | | hash do jogo igual: a parte 3 não muda a lista sem thread |

O atraso de 5 ms dobra o tempo da rodada do `f5_treino` na x86 (29 para 66 s) e o de 20 ms o multiplica por seis: a gravação
é de fato o lado lento, e o jogo espera nos pontos certos.

**Verificadores vistos falhando de propósito** (`x86gv`, o mesmo roteiro, a gravação atrasada em 5 ms onde se diz):

| Estrago | Resultado |
|---|---|
| `sceGsSyncPath` sem a espera (`f5_treino`, 5 ms) | 3.316 de 7.386 quadros diferentes (o primeiro, 4.070); **nenhuma linha `race:`**: o detector não vê tudo, a comparação das listas vê |
| `Dma_ResetBuffers` sem a espera (`f5_treino`, 5 ms) | 3.319 quadros diferentes (o primeiro, 3.195) e 8 linhas `race:` (a lista que "tinha 406 blocos e 1 depois") |
| Sem a cópia dos REF pequenos (`BT3_GS_SEM_COPIA=1`, `f5_duelo`) | o quadro 4.933 diferente e as 3 linhas `race:` do CLUT (`01039d00`, 73 quadwords) |
| `gGsFrameJogo` sem avançar (`f5_treino`) | `divergencias.py` sai com 1: "contador de quadros: jogo 0, desenho 7386" |

**Não ficou visto falhando:** a espera do `ADXF_ReadNw` (sem ela, `f5_treino` com 5 ms e `f5_duelo` com 20 ms saem iguais: nas
cargas dos roteiros o `sceGsSyncPath` do fim do quadro já cobre o que a carga reaproveita). Fica como no desenho do PS2, sem
prova de que é necessária. O contador do jogo também não tem consumidor nas builds de teste (o `gs_input.c`, o pad por roteiro,
só existe na build com janela): o que se prova é que ele chega ao mesmo número do contador do desenho. O descarte com a regra dos
dois quadros é do `gs_d3d.cpp` (360) e só o console o exercita.

#### A parte do 360, escrita sem compilar

`port/x360/src` (o `cl.exe` e o XDK só existem no Linux da revisão; aqui o C89 foi conferido preparando os fontes pelo
`c89.py` e passando o `clang -std=c89 -Wdeclaration-after-statement`, que só reclama das três declarações antigas de `__sync_*`):

- `main360.cpp`: `Port_GsThreadPartida` (a thread de gravação na thread de hardware 0, o jogo na 2, o áudio na 4 e 5, nunca a
  3); `Port_Sair` para a thread antes de soltar o dispositivo; a queda do jogo pede a parada sem esperar e a da própria gravação
  solta o dispositivo e sai; a espera final da main pelo jogo ganha 3 s de limite (o jogo pode ficar preso esperando uma
  gravação que caiu); o vigia diz o estado da gravação (ociosa, gravando, no canal da GIF).
- `posix360.c`: `Port_X360CondEsperaMs` (a condição com tempo) e `Port_X360Barreira` (`__lwsync`).
- `gs_d3d.cpp`: `GsD3d_Repete`, a **Guide**: a thread de gravação, ociosa por 50 ms (o jogo carrega e não entrega quadro),
  apresenta de novo a última imagem (`tela_copia`, a mesma cópia do fim do quadro, e `Present`), porque o dispositivo é dela e
  sem `Present` a Guide congela o console. Não conta como quadro (o vigia continua vendo o jogo parado). O descarte de texturas
  deixa de ser desligado pelo `BT3_GS_THREAD`: gravação e desenho são da mesma thread, como na divisão (c); só o `BT3_INTERP` o desliga.
- A posse do dispositivo não mudou de lugar no código: o `d3d_init` pega com `AcquireThreadOwnership` quem roda o `GsGpu_Init`,
  que agora é a thread de gravação; a main o soltou antes de criar o jogo.

#### A estimativa de ganho no 360 [estimado]

Pelo estudo (a opção (c), seção 4.3): o quadro de luta de 75,7 ms vira **uns 61 ms** (a thread do jogo com uns 16 ms, a de
gravação com os 61 ms de gravação mais fim do quadro, que passam a ser o período), menos de 0,5 a 1,8 ms do hash de página
(parte 2), mais a cópia dos REF pequenos (menos de 0,1 ms) e as trocas do canal da GIF (dezenas de microssegundos): **uns 59 a
60 ms na luta, ainda o dobro do período de 33,37 ms**. Os **menus** (60 fps, 16,6 ms por quadro) ficam com folga na thread
do jogo, e a **Guide e as cargas** deixam de depender da thread do jogo (o dispositivo é da gravação). Não é a rodada que fecha
os 30 fps: o degrau 2 e o corte de W e V seguem (seção 5).

#### O que falta compilar e conferir no console

1. **Compilar** `main360.cpp`, `posix360.c`, `gs_d3d.cpp` (C++ do backend) e os fontes portáveis preparados (C89 do `cl.exe`).
2. A **posse do dispositivo**: o `d3d_init` na thread de gravação (`AcquireThreadOwnership`) e o `ReleaseThreadOwnership` do `GsD3d_Soltar` nela;
   o `CreateTexture` e o `LockRect` na mesma thread do `Present`.
3. A **Guide** durante uma carga: a linha `repetindo o último quadro` no estado do vigia e a Guide abrindo e fechando; que o `Present` repetido mostra a imagem
   certa (o buffer de trás depois de um `Present` com `SwapEffect` DISCARD: se aparecer lixo, a `tela_copia` é refeita a cada vez, então só uma
   textura de alvo perdida o causaria).
4. A **saída** (a pausa, o dashboard) e a **queda** (uma exceção do jogo e uma da gravação): a tela vermelha, o log descarregado, o `XLaunchNewImage`.
5. O **tempo por quadro** da luta (as linhas `gs_d3d: ... Present` e os 60 quadros) contra os 75,7 ms, e o custo das esperas (o `vigia` diz onde fica a thread do jogo).
6. A **ordem de memória** (nenhuma barreira se vê na x86 nem no `qemu`): um quadro raro errado seria o sintoma; os pontos sem trava são o `Port_GetEnv` e os contadores lidos pelo vigia.
7. A pilha de 4 MB da thread de gravação (a VU1 e as texturas), e a thread de hardware 0 (`XSetThreadProcessor`).

**Roteiro da rodada** (o `.xex` do commit final com o `default.dat` e o `default.swap` da mesma build, como na rodada de 18/10):

- **Abertura 1**, `BT3_GS_THREAD=1` ligado: o `ambiente.txt` de sempre (`BT3_ISO`, `BT3_GS=gpu`, `BT3_MEM_PISO_MB=32`, `BT3_MEM_TEX_MB=128`,
  `BT3_VU1=1`, `BT3_VU_VERTS=98304`, `BT3_SHOT=600`) mais `BT3_GS_THREAD=1`. Menus até o Duelo, Goku contra Kuririn, estágio 5, uns 60 s de
  luta, **abrir a Guide uma vez no menu e uma vez durante a carga da luta**, sair pela pausa.
- **Abertura 2**, desligado: o mesmo, sem a linha `BT3_GS_THREAD` (ou com `BT3_GS_THREAD=0`), uns 30 s de luta: a comparação
  com a de antes (75,7 ms). Baixar o `bt3xbx.log` e o `saida.txt` entre as duas.
- O que ler: na abertura 1, `thread de gravação do GS (BT3_GS_THREAD) na thread de hardware 0` e `descarte ligado`; o quadro inteiro
  da luta; nenhuma linha `vigia` com a gravação "gravando" por mais de 3 s; no fim, `saída do jogo com código 0`.
