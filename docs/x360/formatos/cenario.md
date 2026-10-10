# Arquivos de cenário da luta: especificação para a conversão de ordem de bytes

Especificação, por formato e por tipo, do arquivo de cenário que a luta carrega em `BattleRes.stage`
(decisões 8 e 12). Para cada entrada: o formato, a struct da decomp que o lê, o layout com a largura de cada
número, contadores e deslocamentos internos, o que vira ponteiro na carga e quando, e como delimitar as partes
opacas. Regra da conversão: in-place, cada número de 2, 4 ou 8 bytes trocado de ordem pela largura com que o
código o lê, bytes e strings intactos, enchimento zero, nada muda de tamanho nem de lugar. Pacotes da GPU
(DMA/VIF/GIF) como palavras u32; valor de 64 bits como duas palavras, salvo campo de struct declarado u64 que o
código lê como u64.

Fontes: o código da decomp (arquivo e linha citados em cada seção) e, como conferência, o rastreio da luta de
demonstração no WSL (`port/build/tipos.tsv`, `port/build/mapa.tsv`, `port/build/mapa.tsv.conflitos`), cujo
cenário é `pzs3us1/00369.bin`, e os 78 arquivos de cenário extraídos do disco (`gamedata/pzs3us1`), percorridos
por um modelo desta especificação em Python (seção 12). Nenhum byte do jogo está neste documento: só contagens,
deslocamentos e conclusões.

"Não determinado" quer dizer que o código não deixa claro e que não foi inventado um tipo.

Convenção das larguras: "1" é byte (não inverte), "2" é u16/s16, "4" é u32/s32/f32, "8" é u64. "Palavras" são
grupos de 4 bytes invertidos como u32.

## 1. Quem carrega e como o id sai do número do cenário

O arquivo é lido inteiro com `File_Request3(id, res->stage, res->stageSize)` para um buffer fixo de
`BTL_STAGE_BUF_SIZE` = 0x6CB800 bytes (`include/battle/battle_work.h:127`), alocado uma vez por luta em
`BtlLoad_StepInitial` (`src/battle/battle_load.c:545`). Não há cópia nem descompressão: o jogo usa o arquivo no
próprio buffer. Os pontos de carga:

| Quem | Linha | Id |
|---|---|---|
| `BtlLoad_StepInitial` (primeira carga) | `src/battle/battle_load.c:589` | `0x171 + cenário`, ou `0x198 + cenário` em tela dividida |
| `BtlLoad_StepStageReload` (recarga sem transição) | `battle_load.c:215` | idem |
| `BtlLoad_StepStageChange` (troca de cenário na luta) | `battle_load.c:272` | `0x171 + cenário` (macro `BTL_STAGE_MODEL`) |
| `ShenScene` (fundo da tela do dragão) | `src/battle/view_b_d.c:368` | `0x194 + dragão`, dragão 0..2 |
| `ChrView` (fundo do visualizador de personagens) | `src/battle/view_a_d.c:230` | `0x197` |
| `StgDbg_RequestFile` (visualizador de cenário, sem chamador) | `src/battle/stg_a.c:630` | qualquer |

As constantes estão em `include/battle/battle_work.h:116..118` (`BTL_FILE_STAGE` 0x171, `BTL_FILE_STAGE_SPLIT`
0x198, e `BTL_FILE_SND_STAGE` 0x14E para o banco de som do mesmo cenário), `include/battle/view_b.h:236` e
`include/battle/view_a.h:180`.

Do id ao arquivo extraído: `File_OpenById` (`src/sys/file.c:438`) manda os ids 1..0xD47 para a partição 1 com
índice `id - 1`; o extrator grava `pzs3us1/NNNNN.bin` com esse índice (`port/src/plat_file.c:110`, e a mesma
regra já está em `port/x360/conv/conv.c:184`). Portanto **o arquivo de cenário de número N está em
`pzs3us1/(0x170 + N).bin`** (índice 368 + N) e a versão de tela dividida em `pzs3us1/(0x197 + N).bin` (índice
407 + N). Na demo, `00369.bin` é o cenário 1 (o banco de som carregado, `00334.bin` = 0x14E + 1 - 1, confirma).

Para o conversor reconhecer todos os arquivos de cenário: **índices 368..445 de `pzs3us1`** (ids 0x171..0x1BE),
78 arquivos, todos com a mesma tabela de 25 entradas (seção 2):

| Índices | Ids | Conteúdo |
|---|---|---|
| 368..402 | 0x171..0x193 | cenários de luta 0..0x22, versão normal |
| 403..405 | 0x194..0x196 | fundo da tela do dragão (dragão 0..2): mesmo formato, só entradas 1, 2, 6, 7, 24 (e 11 no 405) |
| 406 | 0x197 | fundo do visualizador de personagens: entradas 1, 2, 6, 7, 13, 24 |
| 407..441 | 0x198..0x1BA | cenários 0..0x22, versão de tela dividida (mais leve, mesmo formato) |
| 442..445 | 0x1BB..0x1BE | vagas de tela dividida de 0x23..0x26: arquivos de 2.496 bytes só com a entrada 24 |

O índice 367 (id 0x170) e o 446 (id 0x1BF, `BTL_FILE_TRANSITION_3`) são de outros formatos (tabelas de 3
entradas; o 367 é o banco de som do cenário 0x22, `som-e-sprites.md` 1.2). Reconhecer pelo índice e conferir a tabela (N = 25, seção 2) antes de converter.

Depois da leitura, `StgModel_InitStage` (`src/battle/stg_d_b.c:82`) é quem liga o arquivo. Ela é chamada por
`Battle_Init` (`src/battle/battle.c:153`), por `BtlLoad_EndStageSwap` (`battle_load.c:183`, depois de cada
recarga ou troca), por `ChrView` (`view_a_d.c:369`) e por `ShenScene` (`view_b_d.c:561`). Ela chama
`BtlStage_Init(entrada 1, entrada 1)` (`src/battle/stg_a.c:185`), que faz `BtlStage_Relocate` (seção 4.2),
`ColMesh_SetBase` (base da colisão = início da entrada 1), `BtlStage_BindFile` (entradas 2, 19 e 24),
`StgModel_BindAnims` (entrada 9) e `BtlStage_Reset`. Os efeitos de cenário ligam as outras entradas quando suas
tarefas nascem (seção 7). Consequência: a conversão tem de acontecer uma vez sobre o arquivo como veio do disco,
antes de qualquer uma dessas funções (o instalador, decisão 12, faz isso naturalmente).

## 2. A tabela de deslocamentos

Lida por `BtlScene_GetPackEntry` (`src/battle/btl_scene.c:231`) e por índices fixos em vários lugares
(`file[1]`, `file[2]`, `file[9]`, `file[0x13]`, `file[0x18]`, `file[0x10]`...). É o mesmo formato da tabela dos
arquivos de personagem (personagem.md, seção 2):

| Palavra | Tipo | Significado |
|---|---|---|
| `buf[0]` | u32 | N = número de entradas: 25 nos 78 arquivos |
| `buf[n]`, n = 1..25 | u32 | deslocamento em bytes da entrada n a partir do início do arquivo |
| `buf[26]` | u32 | tamanho do arquivo |
| 0x6C..0x7F | u32 | zeros de enchimento; a entrada 1 começa em 0x80 em todos os arquivos |

A entrada n ocupa `[buf[n], buf[n+1])` e é vazia quando os dois são iguais. O código faz `>> 2 << 2` ou
`/ 4 * 4`; nenhum deslocamento tem os bits baixos ligados (as entradas são alinhadas em 0x40).

Armadilha de numeração: os comentários de `include/battle/stg_a.h` e `BtlStage_GetFileMember`
(`src/battle/stg_b.c:163`) falam em "membro n", que é a palavra n + 3 (membro 0..2 = entradas 3..5, membro 16 =
entrada 19, membro 21 = entrada 24). Aqui sempre se usa o índice da entrada (a palavra da tabela).

Validação para o conversor: `buf[0]` = 25, sequência não decrescente, `buf[26]` = tamanho do arquivo, entrada 1
em 0x80. Conversão: as 32 palavras de 0x00 a 0x7F como u32. O rastreio confirma: 20 posições da tabela lidas
como u32 no 00369.

## 3. Tabela de entradas

"Em quantos" conta os 78 arquivos de cenário em que a entrada não é vazia. "00369" diz o tamanho na demo.

| Entrada | Conteúdo | Formato | Struct (header) | Quem liga / lê | Em quantos | 00369 | Seção |
|---|---|---|---|---|---|---|---|
| 1 | o cenário: modelo, recorte, colisão, zonas, objetos destrutíveis, texturas | bloco com deslocamentos em palavras | `StgRelocHdr` (`include/sys/gfxm_e.h:254`) = `BtlStage` (`include/battle/stg_a.h:204`) = `BtlStageM` (`include/battle/stgm_a.h:135`) | `BtlStage_Relocate`, `StgModel_*`, `StgCol_*`, `ColMesh_*`, `BtlStage_*` | 74 | 0x656EC0 | 4 |
| 2 | parâmetros do cenário (20 listas) | `StgDataA` | `StgDataA` (`stg_a.h:112`) = `StgData` (`include/battle/stg_b.h:23`) | `BtlStage_BindData`, getters `BtlStage_GetList*` | 74 | 0x640 | 5 |
| 3, 4, 5 | animações de câmera de entrada | `DemoCamAnim` | `DemoCamAnim` (`include/battle/btl_demo_cam.h:52`) | `DemoCam_PlayStageAnim(0..2)` (`src/battle/btl_demo_cam.c:504`) | 70 | 0x240 cada | 6 |
| 6 | texturas do cenário rolante | `TexFile` | `TexFile` (`include/sys/gfxm_b_b.h:47`) | `EftStageScroll_Load` (`src/battle/eft_b.c:1359`) | 67 | 0x10580 | 7.1 |
| 7 | malha do cenário rolante | 4 inteiros + `EftMeshData` | `EftMeshData` (`include/battle/eft_ad.h:248`) | `EftStageScroll_Load`, `EftMesh_Init` | 67 | 0xEC0 | 7.2 |
| 8 | nenhum leitor | | | | 0 | vazia | 11 |
| 9 | objetos animados (modelos, nós VU1, trilhas) | `StgOfsTable` + trincas | `StgAnimTable` (`stg_d_b.c:18`), `Vu1Node`, `Vu1Track` (`include/sys/vu1_packet.h:34`, `:58`) | `StgModel_BindAnims`, `StgModel_DrawAnims` | 8 | vazia | 8 |
| 10 | texturas da superfície refletora | `TexFile` | idem | `EftSurf_Init` → `EftTexSet_Load34` (`src/battle/eft_c.c:1007`) | 8 | vazia | 7.1 |
| 11 | texturas do clima | `TexFile` | idem | `EftWeather_Init` (`eft_c.c:345`) | 13 | 0x1580 | 7.1 |
| 12 | nenhum leitor | | | | 0 | vazia | 11 |
| 13 | superfície refletora (água, chão espelhado) | `EftSurfParam` + `EftSurfFile` | `include/battle/eft_c.h:251`, `:264` | `EftSurf_Init` (`eft_c.c:983`), `EftSurf_Relocate` | 30 | vazia | 7.3 |
| 14 | texturas da tempestade | `TexFile` | idem | `EftStorm_Init` (`src/battle/eft_g.c:462`) | 2 | vazia | 7.1 |
| 15 | nenhum leitor | | | | 0 | vazia | 11 |
| 16 | grafo de navegação da IA | `StgNavData` | `StgNavData`, `StgNavNode` (`include/battle/eft_det_b.h:286`, `:273`) | `StgNav_Bind` (`src/battle/eft_det_b_b.c:43`), `StgNav_FindPath` | 35 | 0x1280 | 9.1 |
| 17 | nenhum leitor | | | | 0 | vazia | 11 |
| 18 | texturas de partículas (vapor, fumaça, bolhas, água, gêiser) | `TexFile` | idem | `EftSteamMgr_Init`, `EftSmokeMgr_Init`, `EftBubble_Init`, `EftWater_Init`, `EftGeyser_MgrInit` | 26 | vazia | 7.1 |
| 19 | pontos de efeito "MEF0" (gêiser, fumaça, vapor) | `StgMef` | `StgMef` (`stg_a.h:55`) = `StgFxRes` (`stg_b.h:81`) | `BtlStage_BindFile`, `BtlStage_BindMef`, `BtlStage_GetFxRes*` | 45 | 0x40 | 9.2 |
| 20 | sprite do cenário | `TexFile` | idem | `EftStage_LoadSprite` (`eft_c.c:956`) | 70 | 0x4580 | 7.1 |
| 21 | texturas da parede do limite | `TexFile` | idem | `EftBoundMgr_Init` (`eft_g.c:1435`) | 66 | 0x1580 | 7.1 |
| 22 | cor da parede do limite | f32[4] | `f32 *` | `EftBound_BuildWall` (`eft_g.c:1134`) | 66 | 0x40 | 7.4 |
| 23 | parâmetros da parede do limite | `EftBoundParam` | `include/battle/eft_g.h:215` | `EftBound_*` | 66 | 0x40 | 7.4 |
| 24 | caminhos (posicionamento de choque, IA) | `StgPathTable` | `StgPathTable`, `StgPath` (`stg_a.h:38`, `:31`) | `BtlStage_BindPaths`, `BtlStage_GetPath*` | 78 | 0xA00 | 9.3 |
| 25 | nenhum leitor | | | | 0 | vazia | 11 |

As entradas 8, 12, 15, 17 e 25 são vazias nos 78 arquivos; as palavras delas só servem de fim da entrada
anterior (`EftStage_HasKind`, `eft_c.c:861`, mede o tamanho de 7, 11, 14 e 16).

## 4. Entrada 1: o cenário

### 4.1 Cabeçalho (0x60 bytes)

Visões: `StgRelocHdr` (`include/sys/gfxm_e.h:254`, a da relocação), `BtlStage` (`include/battle/stg_a.h:204`),
`BtlStageMgr` (`include/battle/stg_b.h:71`), `BtlStageM` (`include/battle/stgm_a.h:135`), `StgColStage`
(`include/battle/eft_det_b.h:132`). **Todo deslocamento da entrada 1 é em palavras de 4 bytes a partir do início
da entrada 1** (ponteiro = base + deslocamento x 4, `src/sys/gfxm_e_d.c:14`); a mesma base serve à colisão
(`gColMeshBase`) e às texturas (`Res_RelocateOffsets(&tex, base, ...)`).

| Desl. | Tipo | Larg. | Campo | Leitor |
|---|---|---|---|---|
| 0x00 | s32 | 4 | `unk0`: 18 nos 74 arquivos; sem leitor direto (ver 10, item 1) | |
| 0x04 | ponteiro | 4 | `data`: gravado por `BtlStage_BindData` (aponta a entrada 2); 0 no arquivo | `BtlStage_BindFile` (só liga se 0) |
| 0x08 | s32 | 4 | `flags`: gravado (bit 0 = não pronto); 0 no arquivo | `BtlStage_IsReady` |
| 0x0C | s32 | 4 | `unkC`: -1 nos 74 arquivos; sem leitor | |
| 0x10 | u32 | 4 | `timerCount` | `BtlStage_Relocate`, `BtlStage_UpdateTimers` |
| 0x14 | desl. → ponteiro | 4 | `timers` (4.4) | idem |
| 0x18 | u32 | 4 | `aCount`: número de grupos de desenho, 7 em todos | `BtlStage_Relocate` |
| 0x1C | desl. → ponteiro | 4 | `a` / `BtlStageM.mdl`: grupos (4.5) | `StgModel_Draw` |
| 0x20 | u32 | 4 | `bCount`: listas de partes sempre desenhadas | `BtlStage_Relocate` |
| 0x24 | desl. → ponteiro | 4 | `b` / `BtlStageM.always` (4.6) | `StgModel_Cull` |
| 0x28 | s32 | 4 | `visSize`: bytes da tabela de visibilidade = número de malhas | `StgModel_Cull` |
| 0x2C | desl. → ponteiro | 4 | `vis` (4.6) | `StgModel_Cull`, `StgModel_CullCell`, `StgModel_FadeDebris` |
| 0x30 | u32 | 4 | `cCount`: nós de transformação | `BtlStage_Relocate` |
| 0x34 | desl. → ponteiro | 4 | `c`: nós (4.3) | |
| 0x38 | u32 | 4 | `dCount` / `zoneCount` | `BtlStage_FindZone*`, `BtlStage_ClearZoneMarks` |
| 0x3C | desl. → ponteiro | 4 | `d` / `zones` (4.8) | idem, `StgCol_*` |
| 0x40 | desl. → ponteiro | 4 | `unk40` / `bounds` (4.10) | `BtlStage_GetRadius`, `GetTop`, `GetBottom` |
| 0x44 | u32 | 4 | `eCount` / `objCount` | `BtlStage_*Obj*`, `StgModel_FadeDebris` |
| 0x48 | desl. → ponteiro | 4 | `e` / `objs` (4.10) | idem, `StgCol_QueryZone` |
| 0x4C | f32 | 4 | `size`: meio lado da raiz da árvore de recorte | `StgModel_Cull` |
| 0x50 | desl. → ponteiro | 4 | `tree` / `root` (4.7) | `StgModel_Cull` |
| 0x54 | s32 | 4 | `texOfs`: arquivo de textura (4.11), em palavras | `BtlStage_Relocate` |
| 0x58 | ponteiro | 4 | `tex`: gravado por `Res_RelocateOffsets`; 0 no arquivo | `StgModel_SetTexture` |
| 0x5C | ponteiro | 4 | `base`: gravado por `BtlStage_Relocate`; 0 no arquivo | |

Conversão: as 24 palavras como u32.

### 4.2 Relocação: o que vira ponteiro, quando e com que base

`BtlStage_Relocate(base)` (`src/sys/gfxm_e_d.c:49`), chamada uma vez por carga a partir de `BtlStage_Init`, soma
`base + desl. x 4` a:

- no cabeçalho, sem condição: `timers`, `a`, `b`, `vis` (0x2C), `c`, `d`, `bounds` (0x40), `e`, `tree`;
- em cada temporizador: `+0x04`;
- em cada grupo: `items`; em cada material: `opt` (+0x08) **só se não for 0**, e `items` (+0x04); em cada malha:
  `+0x08` (cadeia) e `+0x0C` (matriz), sem condição;
- em cada lista de partes: `items`; em cada parte: `opt` (+0x08, nó) só se não for 0, `list` (+0x04) e cada
  palavra da lista;
- em cada nó: `+0x44` (chaves), sem condição;
- em cada zona: `+0x04`, `+0x0C`, `+0x10`, `+0x18`; em cada registro de zona: `+0x08`;
- em cada objeto: `+0x28` e cada palavra da lista de pedaços, e `+0x0C..+0x20` (as seis partes), sem condição;
- na árvore, recursiva: `+0x00` só se não for 0, `+0x3C` só se não for 0, cada palavra da lista (0 e -1 viram
  NULL, o resto soma), e cada filho não nulo;
- e chama `Res_RelocateOffsets(&hdr->tex, base, base + texOfs x 4)` (`src/sys/gfxm_b_b.c:244`).

A malha de colisão não é relocada: `ColMesh` guarda `nodeOfs`, `polyOfs` e `vtxOfs` em palavras e o código soma
`gColMeshBase` a cada acesso (`src/battle/col_a.c:154`, `:165`, `:398`, `:433`).

Consequência para a conversão: todos esses deslocamentos são u32 no arquivo. Como "sem condição" soma a base até
a deslocamento 0, um deslocamento 0 vira um ponteiro para o próprio cabeçalho (seção 10, item 1).

### 4.3 Nós de transformação e chaves (`c`)

`StgRelocC` (`gfxm_e.h:220`) = `StgNode` (`stg_a.h:164`) = `StgMNode` (`stgm_a.h:78`), 0x50 bytes, `cCount`
registros (397 no 00369):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | f32[16] | 4 | `mtx` (linha 3 = posição) |
| 0x40 | s32 | 4 | `keyCount`: 1 = pedaço movido por corpo rígido |
| 0x44 | desl. → ponteiro | 4 | `keys` |
| 0x48 | s32 | 4 | `endFrame` |
| 0x4C | s32 | 4 | `body`: alça do corpo rígido, gravada na luta |

Chaves (`StgKey`, `stg_a.h:156`, 0x20 bytes, `keyCount` por nó): f32 x, y, z, s32 frame, f32 rx, ry, rz, s32
unk1C. Tudo palavra de 4. Leitores: `StgPart_Animate`, `BtlStage_UpdateObjs`, `StgModel_FadeDebris`.

### 4.4 Temporizadores = animações de textura (`timers`)

Só existem em 6 arquivos (9 em 00372, 00395, 00411, 00434; 1 em 00376, 00415); nos outros `timerCount` = 0.
Duas visões do mesmo registro de 0x10 bytes: `StgRelocTimer` (`gfxm_e.h:181`) / `StgTimer` (`stg_b.h:62`) e
`StgMTexAnim` (`stgm_a.h:29`). O campo `opt` de um material (4.5) aponta para um desses registros, e o ponteiro
+0x04 do temporizador é a lista de quadros da animação: confirmado nos 6 arquivos (temporizador i = animação do
material i, e quem reloca `frames` é a relocação dos temporizadores; `BtlStage_Relocate` não reloca `frames` por
outro caminho).

| Desl. | Tipo | Larg. | Campo | Leitor |
|---|---|---|---|---|
| 0x00 | u16 | 2 | `flags`: bit 0 laço, bit 1 ida e volta (dados), bit 2 = o período venceu neste quadro (gravado) | `BtlStage_UpdateTimers` (`stg_b.c:497`), `StgModel_Draw` |
| 0x02 | u16 | 2 | `count`: quadros da lista | `StgModel_Draw` |
| 0x04 | desl. → ponteiro | 4 | `frames` | idem |
| 0x08 | f32 | 4 | `time` (gravado) | `BtlStage_UpdateTimers` |
| 0x0C | f32 | 4 | `period` | idem |

Divergência de visões: `StgRelocTimer` declara um s32 em 0x00; os leitores leem dois u16. Vale u16, u16.

Lista de quadros: `count` x s32 (número de textura; `StgModel_Draw` compara `frames[k]` inteiro com `cur`). Fica
logo depois dos temporizadores, no início da entrada.

### 4.5 Modelo: grupos, materiais, malhas e cadeias VIF (`a`)

Leitor: `StgModel_Draw` (`src/battle/stgm_a.c:338`), `StgModel_DrawDebris`, `StgModel_SetTexture`.

Grupo (`StgRelocA` = `StgMGroup`, 8 bytes, `aCount` = 7): u16 `unk0`, u16 `count` (materiais), desl. `items`.
Os 7 grupos são fundo, fundo 2, cenário, três grupos com mistura e destroços (`stgm_a.h:66..73`).

Material (`StgRelocA1` = `StgMMat`, 0x10 bytes; 32 no 00369):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | u16 | 2 | `tex`: textura no arquivo de textura do cenário |
| 0x02 | u16 | 2 | `count`: malhas |
| 0x04 | desl. → ponteiro | 4 | `meshes` |
| 0x08 | desl. → ponteiro | 4 | `anim` (temporizador, 4.4); 0 = sem animação |
| 0x0C | u16 | 2 | `cur`: textura atual da animação, bit 15 = voltando; gravado na luta. O jogo lê o byte baixo com `PS2_LOW(u8, cur)` (já corrigido no código) |
| 0x0E | u16 | 2 | `unkE` (sem leitor) |

Divergência de visões: `StgRelocA1` declara s32 em 0x0C; o único leitor (`StgMMat`) lê u16. Vale u16, u16.

Malha (`StgRelocA2` = `StgMMesh`, 0x10 bytes; 1.968 no 00369, igual a `visSize`):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | u16 | 2 | `visIdx`: byte dela na tabela de visibilidade |
| 0x02 | u8 | 1 | `unk2` |
| 0x03 | u8 | 1 | `prog`: 0 = programa 4 da VU1 |
| 0x04 | u8 | 1 | `alpha`: gravado pelo desvanecimento dos destroços |
| 0x05 | u8[3] | 1 | sem leitor |
| 0x08 | desl. → ponteiro | 4 | `chain`: cadeia VIF (nunca 0) |
| 0x0C | desl. → ponteiro | 4 | `mtx`: matriz do nó do destroço (só no grupo 6); 0 nas outras (837 de 1.968 no 00369), relocado para o cabeçalho e não usado |

Cadeia (o pacote DMA/VIF que `Vu1Pkt_CallProg4`/`CallProg7` chamam com uma etiqueta `call`,
`src/sys/vu1_packet.c:184`, `:46`): uma etiqueta DMA `ret` (id 6, qwc nos bits 0..15) cujas palavras 2 e 3 já
são códigos VIF, o fluxo até `etiqueta + 16 + qwc x 16`, e logo depois uma etiqueta `end` de 16 bytes (palavra
0x70000000 e três zeros). Conferido nas 103.732 cadeias dos 74 arquivos: uma só etiqueta `ret` por cadeia, a
`end` sempre presente, e no fluxo só `NOP`, `MSCNT` e `UNPACK V4-32` (0x6C): todo dado é palavra de 32 bits.
Nenhum código C lê o fluxo (vai à GS por DMA; a textura vem de `StgModel_SetTexture`, não do fluxo). Regra:
**toda palavra da cadeia e da etiqueta `end` é u32** (decisão 12). As cadeias ficam juntas depois das malhas,
separadas por enchimento zero até 0x40.

### 4.6 Partes, listas de partes e tabela de visibilidade

Parte (`StgRelocB1` = `StgMPart` = `StgPart`, 0x20 bytes; 1.202 no 00369), apontada pelas listas `b`, pela
árvore (4.7) e pelos objetos (4.10):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | u16 | 2 | `count`: malhas |
| 0x02 | u16 | 2 | `flags`: bit 0 = desenhada; gravado ao quebrar objetos |
| 0x04 | desl. → ponteiro | 4 | `meshes`: lista de `count` deslocamentos de malha (u32 cada) |
| 0x08 | desl. → ponteiro | 4 | `node`: nó (4.3); 0 = nenhum (não relocado) |
| 0x0C | f32 | 4 | `radius` |
| 0x10 | f32[3] | 4 | x, y, z do centro |
| 0x1C | s32 | 4 | `unk1C` (sem leitor) |

Lista de partes (`StgRelocB` = `StgMPartList`, 8 bytes, `bCount`): u32 `count`, desl. `items` (um vetor de
partes contíguas). Tabela de visibilidade (`vis`): `visSize` bytes, reescrita a cada vista; no arquivo, zeros.

### 4.7 Árvore de recorte (`tree`)

`StgOctNode` (`gfxm_e.h:172`) = `StgMCell` (`stgm_a.h:94`), 0x40 bytes, recursiva (40 células no 00369):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | desl. → ponteiro | 4 | `unk00`: a célula mãe (0 na raiz) |
| 0x04 | desl.[2][2][2] → ponteiros | 4 | filhos; 0 = nenhum |
| 0x24 | s32 | 4 | `vis`: gravado a cada vista |
| 0x28 | f32[3] | 4 | centro |
| 0x34 | s32 | 4 | `depth` |
| 0x38 | s32 | 4 | `partCount` |
| 0x3C | desl. → ponteiro | 4 | `parts`: `partCount` deslocamentos de parte (u32; 0 e -1 = nenhuma) |

### 4.8 Zonas, vizinhas e registros de colisão (`d`)

Zona (`StgRelocD` = `StgZone` = `StgColZone`, `eft_det_b.h:112`, 0x40 bytes; 100 no 00369):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | s32 | 4 | `mark` (gravado) |
| 0x04 | desl. → ponteiro | 4 | `mesh`: malha de colisão estática (4.9); 0 em algumas zonas de 58 dos 74 arquivos (10 zonas no 00369) |
| 0x08 | s32 | 4 | `nearCount` |
| 0x0C | desl. → ponteiro | 4 | `near`: `nearCount` x s32 (índices de zona) |
| 0x10 | desl. → ponteiro | 4 | `unk10`: 0 em todas as zonas dos 74 arquivos; relocado, sem leitor |
| 0x14 | s32 | 4 | `recCount` |
| 0x18 | desl. → ponteiro | 4 | `recs` |
| 0x1C | s32 | 4 | sem leitor |
| 0x20 | f32[4] | 4 | `center` (x e z usados) |
| 0x30 | f32[4] | 4 | `half` (x e z usados) |

Registro (`StgRelocD1` = `StgColRec`, 0x10): s32 `broken`, s32 `obj`, desl. `mesh`, s32 `unkC`.

### 4.9 Malhas de colisão (`ColMesh`)

Apontadas pelas zonas e pelos registros (205 deslocamentos distintos no 00369, um deles 0). Cabeçalho de 0x30
bytes (`ColMesh`, `include/battle/col_a.h:91`, que declara só as 6 primeiras palavras):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | s32 | 4 | `nodeCount` (só se testa `<= 0`) |
| 0x04 | s32 | 4 | número de polígonos |
| 0x08 | s32 | 4 | 0 em todas as malhas dos 74 arquivos |
| 0x0C | s32 | 4 | `nodeOfs` (palavras, base `gColMeshBase`) |
| 0x10 | s32 | 4 | `polyOfs` |
| 0x14 | s32 | 4 | `vtxOfs` |
| 0x18 | f32[6] | 4 | caixa da malha (min, max), sem leitor no código |

Depois do cabeçalho, contíguos e nesta ordem em todas as malhas dos 74 arquivos: vértices, polígonos, nós.

- Vértice (`ColVec`, 0x10): f32 x, y, z, w. Contagem: não há campo; é o maior índice usado pelos polígonos mais
  1, e isso é exatamente `(polyOfs - vtxOfs) x 4 / 0x10` em todas as malhas.
- Polígono (`ColPoly`/`StgColPoly`/`EftDetPoly`, 0x20): s32 `flags` (`STGCOL_POLY_*`), s32 `vtx[3]`, f32 plano
  `nrm[4]`.
- Nó (`ColNode`/`StgColNode`/`EftDetNode`, 0x20): f32 caixa min[3], max[3], s32 `left`, s32 `right` (-1 = folha,
  e então `left` é o polígono). `nodeCount` = 2 x polígonos - 1 em todas as malhas (uma folha por polígono).

Tudo palavra de 4. Leitores: `ColMesh_WalkBox`, `ColMesh_CollectBox`, `ColBox_Overlaps`,
`ColMesh_GetPolyVerts`, `StgCol_*Cb`, `StgShadow_*`, `StgGround_*`, `EftDet_*`.

### 4.10 Objetos destrutíveis, pedaços e limites

Objeto (`StgRelocE` = `StgObj`, `stg_a.h:187`, 0x50 bytes, `eCount`; 16 no 00369): s32 `state`, s32 `unk4`, s32
`hp`, desl.[6] `parts` (inteiro / quebrado, alternados), u32 `pieceCount`, desl. `pieces` (lista de
`pieceCount` deslocamentos de parte), s32 `animEnd`, s32 `frame`, s32 `type`, s32 `parent`, s32 `unk3C`, f32[4]
`hitPos`. Tudo palavra de 4. É o objeto de execução; `hp`, `type` e `parent` são recarregados da definição na
entrada 2 (`StgObj_LoadDef`).

Limites (`bounds`, `StgBounds`, `stg_a.h:149`): f32 `radius`, `top`, `bottom`, seguidos de enchimento.

### 4.11 Texturas do cenário

`TexFile` + `TexEntry` (personagem.md, seção 4.4), com uma diferença: **a base dos deslocamentos é o início da
entrada 1**, não o início do `TexFile` (`Res_RelocateOffsets(&tex, base, base + texOfs x 4)`). `entOfs`,
`pixOfs` e `clutOfs` são palavras a partir da entrada 1. Cabeçalho: u32 `count`, u32 `entOfs`, 2 x u32 não
determinados, ponteiro `ent` (gravado). Registro de 0x40: 10 palavras, 8 bytes em 0x28, **u64 `tex0` em 0x30**
(lido inteiro por `StgModel_SetTexture`, `stgm_a.c:275`: `tex0 | constante`), 2 ponteiros gravados. Pacotes de
pixels e paleta como no personagem: cabeçalho de 0x60 e fim de 0x20 em palavras u32 (como `personagem.c`), a
imagem em bytes. 28 texturas no 00369.

### 4.12 Ordem e enchimento

Nos 74 arquivos a entrada 1 vem nesta ordem: cabeçalho (0x0), temporizadores e listas de quadros (só nos 6 que os
têm), nós, chaves, grupos, materiais, malhas, cadeias VIF (no 00369, de 0x12B80 a cerca de 0x390700: 3,6 MB dos
6,6 MB da entrada), tabela de visibilidade, listas de partes, partes, listas de malhas das partes, arquivo de
textura e seus pacotes, árvore de recorte com as listas das células, zonas, vizinhas, malhas de colisão
intercaladas com os registros de zona, objetos, pedaços, limites. Nos quatro fundos (403..406) não há nós,
registros nem objetos. Os blocos são alinhados em 0x10 ou 0x40; todo byte que nenhuma parte
cobre é zero (seção 12).

## 5. Entrada 2: parâmetros do cenário (`StgDataA`)

Ligada por `BtlStage_BindData` (`src/battle/stg_a_b.c:667`), uma vez por carga (só se `BtlStage.data` = 0).
Visões: `StgDataA` (`stg_a.h:112`) e `StgData` (`stg_b.h:23`). Cabeçalho de 0xA8 bytes: s32 `unk0` (não zero,
sem leitor), s32 `unk4` (0), e 20 pares (s32 contagem, deslocamento). **Os deslocamentos são em bytes a partir
do início da entrada 2** e viram ponteiros na ligação, os 20 sem condição. A primeira lista começa em 0xB0 em
todos os arquivos. Depois de relocar, `BindData` troca o sinal de y e z de várias posições (f32, no lugar). Tudo
o que não é lista é zero.

| Par | Lista | Contagem (74 arquivos) | Registro | Layout (larguras) | Leitores |
|---|---|---|---|---|---|
| 0x08 | `objDefs` | 7..33 (16 no 00369) | `StgObjDef` (`stg_a.h:67`) / `StgMObjDef` (`stgm_a.h:116`), 0x40 | s32 `hp`, s32 `type`, s32 `anim`, s32 `parent`, 0x10..0x1B sem leitor (palavras), f32 `frame` (gravado), f32[4] `pos`, f32[4] `rot` | `StgObj_LoadDef`, `StgModel_*Anims`, `StgObj_GetItemPos` |
| 0x10 | `env` | 1 | `StgEnv`/`StgInfo`, 0x10 | s32 `id`, s32 `flags`, f32 `waterY`, s32 `changeKind` | `BtlStage_GetId`, `GetWaterLevel`, `GetChangeTarget` |
| 0x18 | `light` | 1 | `StgLight`, 0x20 | f32[8] | `BtlStage_GetLightVec*`, `GetLightDir` |
| 0x20 | `points` | 1 | `StgVec`, 0x10 | f32[4] | |
| 0x28 | `starts` | 2 | `StgPlace`, 0x20 | f32[8] | `BtlStage_GetStartPlace`, `GetPlace2C` |
| 0x30 | `altStarts` | 1 | `StgPlace` | f32[8] | `BtlStage_GetStartPlace` |
| 0x38 | `places` | 1 | `StgPlace` | f32[8] | `BtlStage_GetPlace` |
| 0x40 | `list40` | 2 | `StgDepthTintParam` (`include/sys/gfxm_a.h:130`), passo 0x10 | s32 `flags`, u8[4] cor, u8[6] chaves, 2 de enchimento | `StgDepthTint_LoadParams`, `BtlStage_HasFeature` |
| 0x48 | `list48` | 1 | `StgGlareParam` (`gfxm_a.h:106`), 0xC | s32 `flags`, s32 `step`, u8 x4 | `StgGlare_LoadParams` |
| 0x50 | `list50` | 1 | `StgFogParams` (`stg_b.h:159`) | s32 `flags`, u8[6] | `StgFog_SetParams` |
| 0x58 | `list58` | 1 | `StgSurfRec` (`include/battle/eft_c.h:275`), 0x1F | f32[3] `lightDir`, f32 `unkC`, f32 `fadeDist`, u16 `animSpeed`, u8 `unk16`, u8[4] `colorA`, u8[4] `colorB` | `EftSurf_Init`, `BtlStage_HasFeature` (byte 0x16) |
| 0x60 | `list60` | 1 | `StgWeatherRec` (`eft_c.h:167`), 0x18 | f32[3] `wind`, f32 `size`, s32 `flags`, u16 `ptclFlags`, u16 `count` | `EftWeather_Init`, `BtlStage_HasFeature` |
| 0x68 | `list68` | 1 | `EftGndDustStage` (`include/battle/eft_z.h:257`), 0x18 | s32, u8[4], u8[4], f32, f32, u16 `size`, u8, u8 | `EftGndDust_InitTemplate` |
| 0x70 | `list70` | 1 | flags | s32 (`STG_FLAG_*`) | `BtlStage_HasFeature`, `HasFlag8`, `HasMoon`, `GetFxRes*` |
| 0x78 | `list78` | 1 | cores de luz, 12 bytes | u8[12] | `EftGndDust_GetLightColors`, `BtlObj_UpdateAll` |
| 0x80 | `list80` | 1 | cores do escurecimento, 8 bytes | u8[8] | `StgTint_Start`, `EftWeather_*` |
| 0x88 | `list88` | 1 | luz ambiente | u8[4] | `BtlStage_GetAmbient`, `SetAmbient` |
| 0x90 | `list90` | 1 | `GfxWaterStage` (`include/sys/gfxm_b.h:84`), 8 | u32 `flags`, u8[4] | `GfxWater_*`, `EftStage_HasKind` |
| 0x98 | `list98` | 1 | 4 bytes | s32 (bit 0 testado) | `BtlStage_HasFeature` |
| 0xA0 | `listA0` | 1 | `StgHazeParam` (`include/battle/stg_c.h:80`), 0x18 | s32, u8[4] cor, u8[6] chaves, u8 x6, f32 `follow` | `StgHaze_SetParams` |

Divergências de visões resolvidas pelo leitor: `StgSec58` (`stg_b.h:17`) e `StgPlace` vistos sobre `list58` são o
mesmo `StgSurfRec` (a ligação troca o sinal de 0x04 e 0x08 como f32; `HasFeature` lê o byte 0x16); `StgRec18`
sobre `list60` é o começo de `StgWeatherRec`; `StgObjDef.unk10[0x10]` contém o f32 `frame` de `StgMObjDef` em
0x1C. Os tamanhos de registro acima cabem no passo entre as listas (o resto é zero).

## 6. Entradas 3, 4, 5: animações de câmera

`DemoCamAnim`, o mesmo formato de personagem.md, seção 5.12 (cabeçalho de 0x24 com a assinatura de 4 letras em
bytes, pose de 0x20, canais de 0x10, chaves de 0x20, deslocamentos relativos ao início da entrada, corrigidos por
`DemoCam_FixupAnim` na primeira vez que a câmera toca). Lidas por `DemoCam_PlayStageAnim(0..2)`
(`btl_demo_cam.c:504`) via `BtlStage_GetFileMember`, chamadas pela sequência de entrada (`btl_seq.c:1135`,
`:1139`, `:1143`). Presentes em 70 arquivos (0x240 bytes; no 00369, 6 canais e 12 chaves por animação), sempre com
`version` = 0 e o bloco de `unk1C` zerado. A demo não as lê.

## 7. Entradas de efeito de cenário

### 7.1 Arquivos de textura (entradas 6, 10, 11, 14, 18, 20, 21)

`TexFile` com base no início da entrada: os carregadores chamam `Res_RelocateOffsets(&hdr, pack, pack)`
(`EftTexSet_Load4/8/32`, `src/battle/eft_ae.c:1719` e `:1736`, `src/battle/eft_det_a.c:174`; `EftTexSet_Load34`,
`eft_det_a.c:208`). Mesmo layout de 4.11 (u64 `tex0` em 0x30, lido como u64 pelos `EftTexSet_Load*`). Podem ser
ligadas mais de uma vez (a entrada 18 serve a cinco efeitos): a relocação grava o mesmo valor, não importa.

### 7.2 Entrada 7: malha do cenário rolante

Lida por `EftStageScroll_Load` (`eft_b.c:1359`) e `EftMesh_Init` (`src/battle/eft_ad_b.c:160`):

- 0x00..0x0F: quatro s32 lidos por `memcpy` para um `int` (`EFTB_READ_INT`, `eft_b.c:171`): tamanho, `unk2B8`,
  velocidade (x 0,00001), ângulo. Palavras de 4.
- 0x10: `EftMeshData`: s32 `unk0`, s32 `numGroups`, s32 `trisOfs`, s32 `vtxOfs` (bytes a partir do
  `EftMeshData`, somados a cada uso, sem gravar), e `numGroups` x `EftMeshGroup` (0x10: s32 x4).
- Triângulos (`EftMeshTri`, 0x10: s32 `v[3]`, s32 `tex`): a contagem é o maior `firstTri + numTris` dos grupos.
- Vértices (`EftMeshVtx`, 0x30: f32[12]): a contagem é o maior índice usado mais 1.

No 00369: 2 grupos, 84 triângulos, 49 vértices.

### 7.3 Entrada 13: superfície refletora

Lida por `EftSurf_Init` (`eft_c.c:983`) e `EftSurf_Relocate` (`eft_c.c:1076`). Presente em 30 arquivos.

- 0x00..0x3F: `EftSurfParam` (`eft_c.h:264`): f32[12], s32 `animSpeed`, f32 `fadeDist`, f32 `unk3C`. Palavras.
- 0x40: `EftSurfFile` (`eft_c.h:251`), cabeçalho de 0x20: 4 letras de assinatura (`unk0`, sem leitor: bytes), s32
  `version` (= 4, testado), s32 `unk8`, desl. `unkC`, s32 `texCount`, desl. `texs`, s32 `meshCount`, desl.
  `meshes`. **Deslocamentos em bytes a partir do `EftSurfFile`**, relocados por `EftSurf_Relocate`.
- `texs`: `texCount` registros de 0x40 (`EftSurfTex`, `eft_c.h:197`), que são `TexEntry` (`EftVram_AddTex` os lê
  como `EftAeImage`): desl. de pixels e de paleta **em bytes a partir do `EftSurfFile`** (0x00, 0x04), 8
  palavras (tamanhos, passos, BITBLTBUF, deslocamentos de bloco), **u64 em 0x28** (`tex0`, gravado a cada quadro)
  e **u64 em 0x30** (`unk30`, lido inteiro), 2 ponteiros gravados. Pacotes de imagem como em 4.11.
- `meshes`: `meshCount` x `EftSurfMesh` (0x60, palavras: s32 `tex`, s32 `triCount`, desl. `tris`, f32/s32
  restantes, f32[12] de caixas e centro).
- Triângulos: `triCount` x 0x90 por superfície (3 x `EftSurfVtx` de 0x30, f32).
- Bloco `unkC`: quando `unk8` > 0 (só em 4 arquivos: `unk8` = 5 ou 6), `unk8` registros de 0x100 bytes entre o
  cabeçalho e `texs`; quando `unk8` = 0, `unkC` aponta para o próprio início de `texs` e o bloco é vazio. Só é
  relocado, ninguém o lê: largura não determinada pelo código; os dados são palavras com cara de f32 (8% das
  palavras não zero não têm), converter como u32.

`EftSurf_Unrelocate` (na saída) devolve os ponteiros a deslocamentos, e os de textura voltam negados (bug do
original); como o arquivo é recarregado a cada luta, não afeta a conversão.

### 7.4 Entradas 22 e 23: parede do limite

Entrada 22 (`gEftBound->color`, `eft_g.c:1437`): f32[4]; o código lê as três primeiras (`eft_g.c:1219`); a
quarta é não zero em 66 arquivos e sem leitor (palavra de 4). Entrada 23 (`EftBoundParam`, `eft_g.h:215`):
f32 x14 e s32 `otZ`. Ambas de 0x40 bytes com enchimento.

## 8. Entrada 9: objetos animados do cenário

Ligada por `StgModel_BindAnims` (`stg_d_b.c:121`), desenhada por `StgModel_DrawAnims` (`stgm_a.c:61`). Presente
em 8 arquivos (00372, 00373, 00380, 00385, 00386, 00395, 00412, 00419).

Tabela (`StgAnimTable`/`StgOfsTable`, `stg_a.h:22`): 4 letras de assinatura (`unk0`, sem leitor: bytes), s32
`unk4` (1), s32 `count`, s32 `relocated` (0 no arquivo; gravado 1), e `count` deslocamentos em bytes a partir da
tabela (somados por `StgOfsTable_Relocate`, `stg_a.c:121`). `count` = 3 x animações; para a animação n
(1..count/3), as entradas 3n-3, 3n-2, 3n-1 são:

1. `TexFile` com base no próprio `TexFile` (`Res_RelocateOffsets(&res, res, res)`): como 7.1.
2. Contêiner de nós: cabeçalho de 0x10 (u32 deslocamento até o primeiro nó, = 0x10 em todos; u32 não
   determinado, não zero; dois zeros), e a lista de `Vu1Node` (`vu1_packet.h:34`): registros de tamanho `size`,
   encadeados até o que tem `last` != 0. Cabeçalho do nó (0xF0): s32 `size`, `popCount`, `last`, `hasMesh`, e
   f32 até 0xEF (parte gravada por `Vu1Node_Animate`). Com `hasMesh`, em +0xF0 um pacote DMA/VIF como 4.5 (etiqueta
   `ret`, fluxo, etiqueta `end` de 16 bytes; `size` termina exatamente na `end`): conferido nos 64 fluxos, só
   `NOP`, `MSCNT` e `UNPACK V4-32`, e a primeira palavra 0x70000000 é a `end` (é o que `Vu1Node_OrFlags`,
   `vu1_packet.c:252`, procura, gravando as palavras 5 e 6 depois de cada 0x6C038000). Tudo u32. Depois do
   último nó, até a trilha, um bloco de 0x30..0x150 bytes que ninguém lê (palavras com cara de f32: u32).
3. Trilhas (`Vu1Track`, `vu1_packet.h:58`), uma por nó, em sequência: cabeçalho de 0x10 e `keyCount` chaves
   `Vu1Key` de 0x20 (f32 `pos[3]`, u32 `frame`, f32 `rot[4]`). Cabeçalho: 0x00..0x09 sem leitor (valores pequenos
   só nos bytes 0x00 e 0x04: tratar como duas palavras e um u16 zero), u16 `keyCount` em 0x0A
   (`Vu1Node_Animate`), u16 `length` em 0x0C (`StgAnimTrack.length`, `stg_d_b.c:27`: duração em quadros, lida na
   primeira trilha), u16 0 em 0x0E. Divergência: `Vu1Track` declara u32 em 0x0C, sem leitor; vale o u16 de
   `StgAnimTrack`.

## 9. Navegação, pontos de efeito e caminhos

### 9.1 Entrada 16: grafo de navegação (`StgNavData`)

`StgNav_Bind` (`eft_det_b_b.c:43`): cabeçalho de 0x10 (s32 `version` = 1, testado; s32 `count`; s32 `unk8`;
ponteiro `nodes` gravado = início + 0x10), e `count` nós `StgNavNode` (`eft_det_b.h:273`, 0x30): f32 `pos[4]`, s32
`flags` (gravado), s32 `obj`, s32 `unk18[2]`, **s16 `link[8]`** (lidos como u16 por `StgNav_FindPath`). 98 nós no
00369.

### 9.2 Entrada 19: pontos de efeito "MEF0"

`BtlStage_BindFile` liga a entrada se não for vazia; `BtlStage_BindMef` (`stg_a_b.c:639`) só reloca e troca o
sinal de y e z se `magic` = 0x3046454D. `StgFxRes` (`stg_b.h:81`) é a mesma estrutura com outros nomes
(`a` = `count0`, `a2` = `list0`...). Cabeçalho de 0x20: **u32 `magic`** (comparado como número: tem de ser
invertido como u32 para a comparação continuar valendo no 360), s32 `unk4`, e três pares (s32 contagem, desl. em
bytes a partir do início do bloco). Registros de 0x30, um tipo por lista:

| Lista | Registro (header) | Layout |
|---|---|---|
| 0 (gêiser, `BtlStage_GetFxResA2`) | `EftGeyserRec` (`include/battle/eft_b.h:181`) | f32 x6, u16 x3, u8 x4, u8[0xE] |
| 1 (fumaça, `GetFxResB2`) | `EftSmokeSrc` (`include/battle/eft_g.h:128`) | f32 x6, u16 x2, u8[4], u8[4], u8[0xC] |
| 2 (vapor, `GetFxResC2`) | `EftStageMarker` (`include/battle/eft_e.h:169`) | f32 x6, u16, u8[4], u8[0x12] |

`eft_c.c:220` e `:283` leem bytes desses mesmos registros (0x1C..0x23 e 0x1A..0x1D), coerentes com o layout.

### 9.3 Entrada 24: caminhos (`StgPathTable`)

`BtlStage_BindPaths` e `StgPathTable_Relocate` (`stg_a.c:146`): 4 letras de assinatura (`unk0`, sem leitor: bytes),
s32 `unk4` (1), s32 `count`, desl. `paths` (bytes a partir da tabela), s32 `relocated` (0; gravado 1). Caminho
(`StgPath`, 0x10): s32 `end`, desl. `points` (bytes a partir da tabela), s32 `first`, s32 `unkC`. Pontos: `end` x
0x30 por caminho (`BtlCtlPathPoint`, `include/battle/btl_char_ctl.h:207`): f32 x, y, z, e 0x24 bytes que a decomp
não nomeia (palavras de 4; o comentário de `BtlClashPath` diz "x y z ? rx ry ..."). Presente nos 78 arquivos,
inclusive nos quatro de 2.496 bytes.

## 10. O que o jogo grava, quando converter, e o que a conversão in-place não resolve

O jogo usa o arquivo no próprio buffer e grava nele na ligação e durante a luta:

| Entrada | Quem grava | O quê |
|---|---|---|
| 1 | `BtlStage_Relocate` | todos os deslocamentos de 4.2 viram ponteiros; `base`, `tex` |
| 1 | `Res_RelocateOffsets` | `TexFile.ent`, `TexEntry.pix`/`clut` |
| 1 | `BtlStage_BindData`, `IsReady`, `ResetObjs`, `BreakObj`, `UpdateObjs` | `data`, `flags`, estado dos objetos, `flags` das partes |
| 1 | `StgModel_Cull*`, `FadeDebris`, `Draw` | tabela `vis`, `vis` das células, `alpha` das malhas, `cur` dos materiais |
| 1 | `BtlStage_UpdateTimers` | `flags` e `time` dos temporizadores |
| 1 | `StgCol_*`, `BtlStage_ClearZoneMarks` | `mark` das zonas |
| 2 | `BtlStage_BindData` | 20 ponteiros; sinal de y e z das posições; `objDefs.frame` e `pos.w` na luta |
| 3..5 | `DemoCam_FixupAnim` | deslocamentos viram ponteiros, `needsFixup` zera |
| 6, 10, 11, 14, 18, 20, 21 | `Res_RelocateOffsets` | ponteiros de textura |
| 9 | `StgOfsTable_Relocate`, `Res_RelocateOffsets`, `Vu1Node_OrFlags`, `Vu1Node_Animate` | ponteiros, palavras 5 e 6 dos cabeçalhos de nó do fluxo VIF, poses |
| 13 | `EftSurf_Relocate`, `EftSurf_UpdateTextures` | ponteiros, `tex0` |
| 16 | `StgNav_Bind`, `StgNav_*Obj` | `nodes`, `flags` |
| 19 | `BtlStage_BindMef` | listas viram ponteiros, sinal de y e z |
| 24 | `StgPathTable_Relocate` | `paths`, `points`, `relocated` |

Todos esses campos são lidos como número antes de serem sobrescritos: a conversão tem de rodar uma vez sobre o
arquivo como veio do disco (no instalador, decisão 12) ou, se fosse em tempo de jogo, logo depois de
`File_UpdateRequests` e antes de `StgModel_InitStage` nos cinco caminhos da seção 1. Nunca sobre um buffer ligado.

Pontos que a conversão in-place sozinha não resolve:

1. **Deslocamento 0 que vira ponteiro para o cabeçalho.** A relocação sem condição transforma 0 em "início da
   entrada 1". Dois casos reais: (a) partes de objeto com deslocamento 0 (o código as trata como NULL, mas não
   são): `BtlStage_ResetObjs` e `BtlStage_BreakObj` (`stg_a_b.c:340`, `:243`) mexem no bit 0 do u16 em cabeçalho +
   0x02, que é a metade alta da palavra 0 (`unk0` = 18). É a única divergência de largura do rastreio na parte
   numérica do 00369 (4 linhas de 2 bytes, 1 posição de `mapa.tsv`), uma visão de outra largura (classe 4 do
   guia) sobre um ponteiro errado do original. No PS2 a palavra vira 0x10012; no 360, com a palavra convertida
   como u32, o bit cai na metade baixa (19). (b) zonas com `mesh` = 0 (há em 58 dos 74 arquivos; 10 no 00369): `ColMesh_WalkBox` leria
   o cabeçalho como `ColMesh` (`nodeCount` = palavra 0, `nodeOfs` = -1) e andaria em lixo, o que no PS2 também
   quebraria; o rastreio não passa por nenhuma, então são zonas que o jogo não consulta. A palavra 0 não tem
   outro leitor. Conclusão: converter como u32 (é o que o cabeçalho é); a divergência só importa se uma zona de
   `mesh` 0 for consultada. **Depende de decisão** se vale uma guarda no código (`#ifdef PORT_BIG_ENDIAN`,
   tratando deslocamento 0 de parte como NULL), o que muda o comportamento em relação ao PS2 nesse caso
   impossível.
2. **Flags de polígono lidas como byte.** `StgShadow_Cb`, `StgGround_Cb`, `StgCol_SegCb`, `StgCol_PushCb`,
   `StgCol_SweepCb`, `EftDet_StageCb` leem `flags` (s32) com máscaras como `0x0C000060`; o clang da x86 estreita
   essas leituras para bytes, e o rastreio as vê com largura 1 (980 linhas no 00369). É otimização do compilador
   sobre um campo s32: qualquer compilador big-endian faz o mesmo estreitamento no byte certo. As cópias do
   polígono lidas por campo de bits (`BtlCapiBPose.unkD0_7`, `FxPose.unkD0`) são a mesma coisa ou já estão
   resolvidas (`BtlCapiBPose` em `layout.PERMUTADAS`, decisão 11). Nada a fazer no conversor.
3. **Valores de 64 bits.** `TexEntry.tex0` (entrada 1 e arquivos de textura) e `EftSurfTex` 0x28/0x30 são lidos
   como u64 inteiros: convertem-se como u64 (o rastreio confirma: 32 leituras de 8 bytes no 00369, todas em
   `tex0`). As cadeias VIF (entrada 1), os fluxos dos nós (entrada 9) e o cabeçalho e o fim dos pacotes de imagem
   são palavras u32 (decisão 12): quem os ler no 360 (F3) monta 64 bits como `lo | hi << 32`.
4. **Assinaturas e `magic`.** As assinaturas de 4 letras das entradas 3..5, 9, 13 e 24 ninguém lê: bytes.
   O `magic` da entrada 19 é comparado como u32: inverter como u32.
5. **`PS2_LOW(u8, mats[i].cur)`** em `StgModel_Draw` já existe no código; com `cur` convertido como u16 ela lê o
   byte certo.

## 11. Não determinado

Entrada 1:
- Cabeçalho 0x00 (`unk0`, 18 em todos) e 0x0C (-1 em todos): sem leitor próprio.
- `ColMesh` 0x08 (0 em todas) e a caixa 0x18..0x2F (sem leitor).
- Zona 0x10 (`unk10`, 0 em todas, relocada) e 0x1C; registro de zona 0x0C; parte 0x1C; malha 0x02 e 0x05..0x07;
  grupo 0x00; material 0x0E; objeto 0x04 e 0x3C; chave 0x1C; `TexFile` 0x08..0x0F.
- O significado de partes de objeto e de `mesh` de zona com deslocamento 0 (10, item 1).

Entrada 2: `unk0` do cabeçalho; `objDefs` 0x10..0x1B; os bytes sem leitor de `StgSurfRec`, `StgGlareParam`,
`StgDepthTintParam` e `StgHazeParam` (tratados como bytes, como declarados).

Efeitos: entrada 7, se os quatro inteiros iniciais são mesmo inteiros (o código os lê como `int`); entrada 13,
bloco `unkC` (u32 por hipótese); entrada 22, a quarta palavra.

Entrada 9: cabeçalho da trilha 0x00..0x09, palavra 1 do cabeçalho do contêiner de nós e o bloco entre o último nó
e as trilhas (u32 por hipótese).

Entradas 19 e 24: os 0x0C..0x12 bytes finais dos registros de efeito (bytes, como declarados) e os 0x24 bytes
finais de cada ponto de caminho (palavras, sem leitor).

Entradas 8, 12, 15, 17, 25: sem leitor e vazias nos 78 arquivos; formato desconhecido.

## 12. Cobertura conferida contra o rastreio e contra todos os arquivos

Método: no WSL, um modelo desta especificação em Python percorre cada arquivo como o jogo percorre (tabela,
cabeçalho da entrada 1 e todas as suas listas e ponteiros, cadeias VIF pelas etiquetas DMA, malhas de colisão pelos
índices, árvore recursiva, `StgDataA` pelos 20 pares, `DemoCamAnim`, `TexFile` e pacotes de imagem com o
`DIRECT` conferido, `EftMeshData`, `EftSurfFile`, `Vu1Node`, `StgNavData`, MEF0, caminhos), marca a largura de
cada byte, acusa conflito quando duas partes marcam o mesmo byte com larguras diferentes, e exige que todo byte
não marcado seja zero.

### 12.1 Os 78 arquivos (índices 368..445)

- Tabela: N = 25, entrada 1 em 0x80, fim = tamanho, em todos.
- Todos os bytes cobertos: nenhum byte fora das partes é diferente de zero, nenhum erro de limite, nenhum
  conflito de largura entre partes (estruturas compartilhadas, como malhas apontadas por várias partes ou a lista
  de quadros de um temporizador, recebem sempre a mesma largura).
- Invariantes: `unk0` = 18, `unk4` = `unk8` = 0, `unkC` = -1, 7 grupos, `tex` e `base` = 0 nos 74 arquivos com
  entrada 1; malhas de colisão contíguas (cabeçalho, vértices, polígonos, nós), `nodeCount` = 2 x polígonos - 1,
  palavra 0x08 = 0; zonas com `unk10` = 0; cadeias VIF só com `ret`, `NOP`, `MSCNT`, `UNPACK V4-32` (103.732
  cadeias); fluxos dos nós da entrada 9 idem (64); `StgDataA` com a primeira lista em 0xB0 e contagens 1 (exceto
  `starts` e `list40`, 2, e `objDefs`); `DemoCamAnim` com `version` 0; entrada 13 com `version` 4.
- Entradas vazias em todos: 8, 12, 15, 17, 25. Arquivos de 2.496 bytes (442..445): só a entrada 24.

### 12.2 00369 (cenário da demo) contra o rastreio

114.461 linhas de `tipos.tsv` em 74.013 posições distintas; `mapa.tsv` com 75.241 posições de número
(4 bytes: 70.536; 2 bytes: 4.673; 8 bytes: 32); união 75.847. Todas caem em partes da especificação.

| Parte | Números | Vistos (posições) | Larguras do rastreio |
|---|---|---|---|
| tabela | 32 | 20 | 4 |
| E1 cabeçalho | 24 | 19 | 4 (mais a leitura de 2 bytes em +0x02, 10, item 1) |
| E1 nós / chaves | 7.940 / 3.176 | 449 / 156 | 4 |
| E1 grupos / materiais / malhas | 21 / 192 / 5.904 | 14 / 128 / 5.904 | 2 e 4 como especificado; 529 posições lidas como byte em `prog`/`alpha` |
| E1 cadeias VIF | 903.952 | 0 | (DMA, não passa pela instrumentação) |
| E1 listas de partes / partes / listas de malhas | 18 / 10.818 / 1.968 | 18 / 7.592 / 1.968 | 2 e 4 |
| E1 árvore (células / listas) | 640 / 850 | 600 / 850 | 4 |
| E1 zonas / vizinhas / registros | 1.600 / 684 / 456 | 823 / 130 / 254 | 4 |
| E1 colisão: cabeçalhos / vértices / polígonos / nós | 2.448 / 70.228 / 156.456 / 311.280 | 198 / 6.132 / 10.034 / 37.042 | 4 (e as leituras de byte das flags, 10, item 2) |
| E1 objetos / pedaços / limites | 320 / 397 / 3 | 129 / 397 / 3 | 4 |
| E1 texturas (`TexFile`, `TexEntry`, `tex0`) | 5 / 336 / 28 | 2 / 168 / 28 | 4 e 8 |
| E1 pacotes de imagem (cabeçalho e fim) | 1.792 | 0 | (DMA) |
| E2 | 373 | 175 (e 77 posições de byte) | 4 e 2 nos números; bytes onde a especificação diz bytes |
| E6, E11, E20, E21 (texturas) | 18 cada | 5 a 11 | 4 e 8 |
| E7 | 940 | 934 | 4 |
| E16 (cabeçalho / nós / ligações s16) | 4 / 784 / 784 | 2 / 377 / 624 | 4 e 2 |
| E19 | 8 | 7 | 4 |
| E22 / E23 | 4 / 15 | 3 / 13 | 4 |
| E24 (cabeçalho / caminhos / pontos) | 5 / 24 / 600 | 3 / 6 / 0 | 4 |
| E3..E5 | 136 cada | 0 | |

Divergências de largura: só as 980 linhas de byte sobre `flags` de polígono (otimização do compilador, 10,
item 2) e as 4 linhas de 2 bytes no cabeçalho + 0x02 (10, item 1; é a única posição em que `mapa.tsv`, que a
inverteria como u16, discorda do conversor). Leituras de 16 bytes (vetores, máscara 4|16) contam como compatíveis
com f32. `mapa.tsv.conflitos` não tem nenhuma linha do 00369.

Os 52.741 "?" do catálogo (leituras sem expressão na coluna do DWARF) vêm todos de funções que recebem ponteiro
genérico ou indexam vetor, e caem em partes de largura 4 já especificadas:

| Função | Parte | Linhas |
|---|---|---|
| `ColBox_Overlaps` (caixa do nó lida por `ColBox *`) | E1 nós de colisão | 33.293 |
| `ColMesh_GetPolyVerts` (`poly->vtx[k]`) | E1 polígonos | 4.481 |
| `Vec3_Dot` | E1 polígonos (plano), vértices, partes, nós de colisão | 4.649 |
| `BtlStage_Relocate` (palavras das listas) | E1 listas de malhas, partes, pedaços, objetos | 2.858 |
| `StgModel_CullCell`, `StgOctree_Relocate` | E1 árvore e listas de malhas | 3.279 |
| `Vec4_Add`, `Vec4_Scale`, `wide4`, `mtx_apply` (vetores do port, `port/src/plat_fastvec.c`) | E1 partes (centro), E7 vértices | 2.753 |
| `StgNav_FindPath` (`link[k]`) | E16 ligações (s16) | 624 |
| `EftMesh_DrawNowClip` | E7 triângulos | 252 |
| outras (`BtlStage_ResetObjs`, `StgCol_*`, `BtlStage_FindZoneNear`, `StgPart_Animate`, getters de E2, `StgPathTable_Relocate`...) | E1, E2, E16, E24 | o resto |

### 12.3 O que a demo não exercita

As cadeias VIF e os pacotes de imagem (vão por DMA); entradas 3..5 (câmera de entrada); os pontos de caminho; as
entradas que o 00369 não tem: temporizadores e animações de textura (6 arquivos), objetos animados (entrada 9, 8
arquivos), superfície refletora (13, 30 arquivos), texturas 10, 14 e 18, e a versão de tela dividida. Para essas, a
especificação vem só do código e da conferência estrutural da seção 12.1.
