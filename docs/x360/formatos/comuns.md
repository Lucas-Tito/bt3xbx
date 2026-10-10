# Arquivos comuns da luta: especificação para a conversão de ordem de bytes

Especificação, por formato e por tipo, dos cinco arquivos comuns que a luta usa: `pzs3us1/00001.bin` a
`00005.bin` (e a variante `00006.bin`). Para cada arquivo: quem o carrega, a tabela de entradas, a struct da
decomp que lê cada entrada, o layout com a largura de cada número, contadores e deslocamentos internos, o que
vira ponteiro na carga, e como delimitar as partes opacas. Regra da conversão (decisões 8 e 12): in-place, cada
número de 2, 4 ou 8 bytes trocado de ordem pela largura com que o código o lê, bytes e strings intactos,
enchimento zero, nada muda de tamanho nem de lugar; pacotes da GPU (DMA/VIF/GIF) como palavras u32, salvo campo
de struct u64 lido como u64.

Este documento segue a forma de `personagem.md` e referencia as peças que já estão especificadas lá em vez de
repeti-las: a tabela de deslocamentos (seção 2), `TexFile`/`TexEntry` e os pacotes de imagem (4.4 e 7.7),
`ChrCamCutDef` (5.11), `DemoCamAnim` (5.12 e 7.12), faixas de lábio `BObjLipData` (6.6), e o pacote de efeitos
F2.1 a F2.9 (7.2 a 7.10).

Fontes: o código da decomp (header e linha citados em cada seção) e, como conferência, o rastreio da luta de
demonstração no WSL (`port/build/tipos.tsv`, `port/build/mapa.tsv`, `port/build/mapa.tsv.conflitos`) sobre os
arquivos de `gamedata/pzs3us1/`. Nenhum byte do jogo está neste documento: só contagens, deslocamentos e
conclusões. "Não determinado" quer dizer que o código não deixa claro e que não foi inventado um tipo.

## 1. Os cinco arquivos

### 1.1 Id e arquivo

`File_OpenById` (src/sys/file.c:438) abre o id `n` da partição 1 (ids 1..0xD47, include/sys/file.h:11) como o
arquivo `n - FILE_PT1_FIRST_ID` dela, e o port nomeia esse arquivo `%05d.bin` (`ADXF_OpenAfs`,
port/src/plat_file.c:163). Portanto **o arquivo NNNNN da partição 1 é o id NNNNN + 1**: 00001 é o id 2, 00005 é o
id 6. Conferido também pelo conteúdo: cada arquivo tem a estrutura que o leitor do id correspondente espera
(seções 2 a 6).

| Arquivo | Id | Onde fica | Quem carrega | Quando | Tamanho | N | Conteúdo | Seção |
|---|---|---|---|---|---|---|---|---|
| 00001 | 2 | `gCommonRes->data[0]` | `Common_LoadBoot` aloca (src/sys/common.c:15), `Common_Reload` lê (common.c:29) | uma vez, na partida do jogo (src/main.c:142) | 88.832 | 213 | luz, flags por animação, IA da CPU, cortes de câmera, vozes, golpes por ação, parâmetros por personagem, avisos de técnica, câmera e pose comuns, faixas de lábio | 2 |
| 00002 | 3 | `gCommonRes->data[1]` | idem | idem | 1.308.992 | 637 | efeitos comuns: 24 conjuntos de emissores, texturas, parâmetros da aura e do brilho, malhas | 3 |
| 00003 | 4 | `gCommonRes->data[2]` | idem | idem | 23.744 | 2 | tabela de personagens e de itens | 4 |
| 00004 | 5 (`COMMON_FILE_BOOT`) | `gCommonRes->boot` | `Common_LoadBoot` (`File_LoadSync`) | uma vez, na partida (main.c:141) | 140.416 | 11 | fontes, ícones do controle, avisos do controle | 5 |
| 00005 | 6 + `gProgress->unk0[0]` | `BattleRes.unk8` (`gCommonRes + 0x28`) | `BtlLoad_StepInitial` (src/battle/battle_load.c:596) | a cada luta, buffer novo do heap, liberado no fim (battle_load.c:841) | 701.184 | 197 | HUD e menu de pausa: folhas de sprites e textos | 6 |

`gProgress->unk0[0]` é o idioma do texto (0 ou 1): o 00005 (id 6) é o idioma 0 e o **00006 (id 7) é o idioma 1**,
com a mesma estrutura (seção 6). Do 00007 (id 8) em diante começam os blocos por membro de
`BTL_FILE_CHARA_DATA` (include/battle/battle_work.h:114), fora deste documento.

`CommonRes` (include/sys/common.h:14) tem `boot` em +0x00 e `data[0..2]` em +0x04..+0x0C. Visões locais da
mesma struct dão outros nomes a esses campos; em particular `BObjCommon` (src/battle/bobj_a.c:1908) chama de
`file3` o campo em `gCommonRes + 4`, que é `data[0]`. **O "arquivo comum 3" de `personagem.md` (nota da 5.1 e
6.6, `BtlObj_BindCommonTables`) é o 00001**, não o 00002 nem o 00003. `personagem.md` não foi alterado aqui.

### 1.2 Formato comum: tabela de deslocamentos

Os cinco arquivos começam com a mesma tabela de `personagem.md` seção 2 (que no 00002 é o pacote F2.1 de
`personagem.md` 7.2): `buf[0]` = N, `buf[1..N]` = deslocamento em bytes da entrada a partir do início do arquivo,
`buf[N+1]` = tamanho do arquivo, zeros até a entrada 1. Os leitores fazem `buf[n] >> 2 << 2` ou `buf[n] / 4`
palavras; nenhum lê `buf[0]`. Nos cinco (e no 00006): N como na tabela acima, `buf[N+1]` igual ao tamanho,
sequência não decrescente, todo deslocamento múltiplo de 0x40, enchimento zero. Conversão: a tabela inteira,
de `buf[0]` até o início da entrada 1, como u32. Validação no conversor: as mesmas três condições.

### 1.3 Quando converter

Os quatro arquivos de `Common_Reload`/`Common_LoadBoot` (00001 a 00004) são lidos uma vez só e ficam residentes
a vida inteira do programa. O jogo grava neles na ligação, sempre sobre campos que antes lê como deslocamento ou
que vêm zerados (relocação de texturas, ponteiros da IA, câmeras, fontes; ver as seções "escritas" de cada
arquivo). A conversão tem de rodar uma vez, depois da leitura e antes de `Progress_Init`, `Font_Init`,
`FontIcon_Init` e da primeira luta. O 00005 é relido a cada luta para um buffer novo; a conversão em tempo de jogo
seria uma por leitura, antes de `Hud_Init`/`BtlMenu_Init`. Com o instalador da decisão 12, todos são convertidos
uma vez no disco e a questão desaparece: o que importa é que nenhum campo é gravado pelo jogo antes de ser lido
na ordem do PS2.

As relocações que o jogo repete (a textura da luz do 00001 a cada `BtlObj_Init`, os `TexFile` do 00002 a cada
carga de efeitos) são idempotentes: calculam o ponteiro de `entOfs`, `pixOfs`, `clutOfs` ou `keysOfs`, que não
mudam.

### 1.4 Convenções

Larguras: "1" é byte (s8/u8, não inverte), "2" é u16/s16, "4" é u32/s32/f32, "8" é u64. Um array `T x[n]` é `n`
números de largura `sizeof(T)` seguidos. "T" nas tabelas de cobertura são posições distintas de `tipos.tsv`; "M"
são posições de número de `mapa.tsv`, expandidas pelas repetições. Pacotes de imagem e paleta das texturas: o
formato de `personagem.md` 4.4 e a regra da decisão 12 (cabeçalho de 0x60 e fim de 0x20 como palavras u32,
imagem e paleta intactas, `TexEntry.tex0` como u64).

## 2. `pzs3us1/00001.bin`: tabelas comuns da luta (id 2, `gCommonRes->data[0]`)

### 2.1 Quem carrega

- Id 2 = arquivo 00001 da partição 1 (`File_OpenById`: `flid = id - FILE_PT1_FIRST_ID`, src/sys/file.c:438;
  `ADXF_OpenAfs` do port monta `%05d.bin` com o `flid`, port/src/plat_file.c:163).
- `Common_LoadBoot` (src/sys/common.c:15) aloca o buffer com `File_GetSize(2)` (alinhamento 0x40) e
  `Common_Reload` (src/sys/common.c:29) lê o arquivo inteiro para ele. `Common_Reload` só é chamada uma vez, na
  partida do jogo (src/main.c:142): o arquivo fica residente o tempo todo, inclusive entre lutas. Não há cópia
  nem descompressão: tudo é lido no próprio buffer.
- Leitores (todos pegam o ponteiro `gCommonRes->data[0]` e pedem índices fixos da tabela):

| Quem liga | Onde | Entradas | Guarda em |
|---|---|---|---|
| `BtlObjLight_Init` | src/battle/btl_obj.c:201 (chamada por `BtlObj_Init`, btl_obj.c:549) | 1 | `BtlObjLight.res` (include/battle/btl_obj.h:286), relocado por `Res_RelocateOffsets` |
| `BtlChar_AllocAll` | src/battle/btl_char_mgr.c:1282 (chamada por `Battle_Init`, src/battle/battle.c:154) | 3, 5, 6, 7, 8, 9 | `gBtlChars->tbl[0..5]` (roster + 0x20..0x34, include/battle/btl_char_mgr.h:190), pela macro `FILE_TABLE` (btl_char_mgr.c:240) |
| `AiThink_BindData` | src/battle/btl_ai_cond.c:2234 (chamada por src/battle/btl_ai_mgr.c:118) | 4 | `gBtlAi->data` |
| `BtlObj_BindCommonTables` | src/battle/bobj_a.c:2451 (só na troca de membro com `BtlChar_TestMemberUnk70`, src/battle/btl_char_mgr.c:427 e :492) | 12, 13, 14..113 ou 114..213 | `mdl.common9C` (objeto + 0xB4), `mdl.common900` (objeto + 0x918), `mdl.lip[100]` (objeto + 0x758) |

Nota de nome: a visão `BObjCommon` (src/battle/bobj_a.c:1908) chama o campo em `gCommonRes + 4` de `file3`, e
por isso `personagem.md` (5.1 e 6.6) fala em "arquivo comum 3". `gCommonRes + 4` é `CommonRes.data[0]`
(include/sys/common.h:16), ou seja, o id 2, este arquivo 00001.

Nenhum outro leitor de `data[0]` foi achado nas visões de `gCommonRes` (`PadWatchCommonRes`, `FontCommonRes`,
`HudCommonRes`, `EftCommonRes` e as dos menus só usam `boot`, `data[2]` e `gCommonRes + 0x20..0x28`). Funções
ainda em assembly (`INCLUDE_ASM`) não foram varridas.

### 2.2 Tabela de deslocamentos

Mesmo formato da tabela do file[0] de personagem (`personagem.md` seção 2): `buf[0]` = N, `buf[n]` = início
em bytes da entrada n a partir do início do arquivo, `buf[N+1]` = tamanho do arquivo, zeros de enchimento até
a entrada 1. Os leitores fazem `buf[n] >> 2 << 2` (`BtlObjLight_Init`, `AiThink_BindData`,
`BtlObj_BindCommonTables`) ou `buf[n] / 4` palavras (`FILE_TABLE`); só `BtlObj_BindCommonTables` testa entrada
vazia (`buf[n+1] != buf[n]`).

Conferido no WSL: N = 213 (0xD5), tabela de 0xD7 palavras, enchimento de 36 bytes todo zero até a entrada 1 em
0x380; `buf[214]` = 88.832 = tamanho do arquivo; sequência não decrescente; todo deslocamento múltiplo de 0x40.
N = 13 + 200: as faixas de lábio vão de 14 a 213, e `BtlObj_BindCommonTables` lê `buf[0xE + 0x64 + 99 + 1]` =
`buf[214]` como fim da última. O jogo não lê `buf[0]`.

Conversão: a tabela inteira (0x000..0x37F) é u32.

### 2.3 Tabela de entradas

Tamanhos da tabela de deslocamentos. "Demo" diz se a luta de demonstração leu a entrada (`tipos.tsv`).

| Índice | Uso | Formato | Struct (header) | Quem lê | Tamanho | Demo | Seção |
|---|---|---|---|---|---|---|---|
| 0 | contagem N | u32 | | ninguém | | | 2.2 |
| 1 | texturas da luz (2 texturas) | `TexFile` | `TexFile`/`TexEntry` (include/sys/gfxm_b_b.h:47, :29) | `Res_RelocateOffsets`, `TexFile_UploadOne` por `BtlObjLight_FindRes0`/`FindRes1` (btl_obj.c:251, :256) | 0x9C0 | sim (relocação e envio) | 2.4 |
| 2 | não ligada | | | ninguém | 0 (vazia) | | 2.16 |
| 3 | palavra de flags por animação | u32[0x1C0] | `BtlStatRoster.animFlags` (include/battle/btl_char_status.h:147) | `BtlAnim_GetFlags` (src/battle/btl_char_status_anim.c:319) | 0x700 | sim | 2.5 |
| 4 | dados da IA da CPU | contêiner próprio | `AiThData` (include/battle/btl_ai_think.h:187), `BtlAiData` (include/battle/btl_ai.h:172), `BtlAiSeqActTable` (include/battle/btl_ai_seq_a.h:42) e outras | `AiThink_*`, `BtlAiSeq_*`, `BtlAiSense_*`, `BtlAiStep_*`, `BtlAiCond_*`, `BtlAi_*` | 0xEFC0 | sim | 2.6 |
| 5 | cortes de câmera comuns | 40 registros de 0x24 | `ChrCamCutDef` (include/battle/btl_char_cam.h:129) | `ChrCam_StartCut` (src/battle/btl_char_cam_modes.c:733, `gBtlChars->cutDefs`) | 0x5C0 | sim | 2.7 |
| 6 | vozes por tipo de fala | 64 registros de 8 | `BtlVoiceEntry` (include/battle/btl_char_get.h:32) | `BtlChar_GetVoiceEntry` e chamadores (src/battle/btl_char_get.c:358..430) | 0x200 | sim | 2.8 |
| 7 | golpes por ação de ataque | 64 registros de 0x18 | `BtlActAttackRec` (include/battle/btl_char_action.h:68), `BtlActIAttackRec` (include/battle/btl_act_i.h:236), `BtlTechAtkAction` (include/battle/btl_tech_a.h:213) | `BtlAct_PrepareAttack`, `BtlAct_TestAttackSkill` (src/battle/btl_char_action.c), `BtlDecide_QueueAttack` (src/battle/btl_act_h_b.c:2771), `BtlAtk_GetId` (src/battle/btl_tech_a.c) | 0x600 | sim (só bytes) | 2.9 |
| 8 | vida, ki inicial e barras por personagem | 164 registros de 0x10 | `BtlMemberParam` (include/battle/btl_char_member.h:124) | `BtlMember_LoadParams` (src/battle/btl_char_member.c), `BtlChar_OnModelLoaded` (btl_char_mgr.c:450) | 0xA40 | sim | 2.10 |
| 9 | linhas de aviso de técnica | 19 registros de 0x14 | `BtlTechPromptRow` (include/battle/btl_tech_a.h:220), `BtlCapiBPrompt` (include/battle/btl_capi_b.h:227), `BtlInputSwitchEntry` (include/battle/btl_input.h:206) | `BtlCtrl_GetTechPromptPad1`, `BtlCtrl_GetSwitchPrompt` (btl_tech_a.c:343, :436), `BtlCharApi_GetPromptButtons` (src/battle/btl_capi_b.c:971), `BtlInput_*` | 0x180 | sim (só bytes) | 2.11 |
| 10, 11 | não ligadas | | | ninguém | 0 (vazias) | | 2.16 |
| 12 | animação de câmera comum (substitui a câmera 1 do objeto) | `DemoCamAnim` | `DemoCamAnim` (include/battle/btl_demo_cam.h:52) | `DemoCam_PlayObjAnim(obj, 1)` (src/battle/btl_demo_cam.c:452), depois de `BtlObj_BindCommonTables` | 0x300 | não | 2.12 |
| 13 | pose de referência por nó | f32 x3 por nó | `BobjVec3` (include/battle/bobj_a.h:295), `BobjMdl.refPose` (bobj_a.h:325) | `BtlObjAnim_Retarget` (src/battle/bobj_a.c:107) | 0x380 | não | 2.13 |
| 14..113 | faixas de lábio, primeira língua | `BObjLipData` | `BObjLipData` (include/battle/bobj_b.h:85) | `BtlObj_SetSubState`, `BObjFace_StepLipData` | 0x40 ou 0x80 cada | não | 2.14 |
| 114..213 | faixas de lábio, segunda língua (`gSaveData->flags & 1`) | `BObjLipData` | idem | idem | idem | não | 2.14 |

### 2.4 Entrada 1: texturas da luz (`TexFile`)

Formato `TexFile`/`TexEntry` de `personagem.md` 4.4 (cabeçalho, registros de 0x40 com `tex0` u64, pacotes de
imagem com cabeçalho VIF/GIF de 0x60 bytes e fim de 0x20 bytes). `BtlObjLight_Init` faz
`Res_RelocateOffsets(&light->res, entrada, entrada)` em toda inicialização de luta: grava `TexFile.ent` (+0x10)
e `TexEntry.pix`/`clut` (+0x38, +0x3C). A relocação é repetida a cada luta sobre o buffer já relocado, mas só
lê `entOfs`, `count`, `pixOfs` e `clutOfs`, que ela não altera: é idempotente.

Na demo: `count` = 2, `entOfs` = 8 (registros em +0x20), 0x08..0x0F não zero (o mesmo campo não determinado de
4.4). Textura 0: PSMCT32 de 128 x 2, enviada ao bloco 0x3D40 (`BtlObjLight_FindRes0`); textura 1: PSMCT32 de
16 x 16, enviada ao bloco que o chamador passa (`BtlObjLight_FindRes1`). Só pacotes de imagem (sem paleta), de
0x480 bytes cada, em +0xA0 e +0x520: cabeçalho de 0x60, 0x400 bytes de cores RGBA (bytes) e fim de 0x20. O
`DIRECT` e o NLOOP da etiqueta IMAGE batem com o tamanho nos dois. O resto da entrada é zero.

Conversão: igual a 4.4 com a regra da decisão 12 (cabeçalho e fim do pacote como palavras u32, imagem intacta,
`tex0` como u64).

### 2.5 Entrada 3: flags por animação (`gBtlChars->tbl[0]`)

`BtlAnim_GetFlags(anim)` devolve `tbl[anim]` (u32) para `anim < BTL_ANIM_COUNT` (0x19E,
include/battle/btl_char_status.h:60); os bits estão descritos em btl_char_status.h:54. Sem contador. A entrada
tem 0x700 bytes = 0x1C0 palavras; a última palavra não zero é a 0x1B0, ou seja, há dados até o id 0x1B0,
além do limite do código (as animações 0x19F..0x1B8 do file[1] também existem sem leitor, `personagem.md` 3.2).
Conversão: a entrada inteira como u32.

### 2.6 Entrada 4: dados da IA da CPU

`AiThink_BindData(0)` toma `p = arquivo + (buf[4] >> 2 << 2)` e grava ponteiros **dentro da entrada**. Visões:
`AiThData`/`AiThActTable`/`AiThRuleList`/`AiThRule`/`AiThRates`/`AiThProfile`
(include/battle/btl_ai_think.h:126..196), `BtlAiData`/`BtlAiActTable` (include/battle/btl_ai.h:165..178),
`BtlAiSeqActTable`/`BtlAiSeqBody`/`BtlAiSeqStep` (include/battle/btl_ai_seq_a.h:13..46), `AiRateRow`
(src/battle/btl_ai_cond.c:455), `DetAiRule` (src/battle/eft_det_b_c.c). Deslocamentos abaixo a partir do
início da entrada. As seções seguem umas às outras sem tabela: o tamanho de cada uma está no cabeçalho.

| Parte | Início (demo) | Tamanho (demo) | Conteúdo |
|---|---|---|---|
| cabeçalho | 0x0000 | 0x148 | 2.6.1 |
| tabela de ações | 0x0148 | `size[0]` = 0x1958 | 2.6.2 |
| 8 listas de regras | 0x1AA0 | `size[1..8]` = 1.996, 2.220, 3.564, 2.276, 624, 1.772, 204, 2.444 | 2.6.3 |
| 32 perfis | depois das listas | `size[9..40]`: 0x580 nos tipos 0..27, 0 nos tipos 28..31 | 2.6.4 |
| enchimento | 0xEF9C | 0x24 | zeros até o fim da entrada (0xEFC0) |

#### 2.6.1 Cabeçalho (`AiThData`, 0x148 bytes)

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | s32[41] | 4 | `size`: tamanho em bytes da tabela de ações, das 8 listas de regras e dos 32 perfis, nesta ordem |
| 0xA4 | ponteiro | 4 | `act`: gravado na ligação (= entrada + 0x148) |
| 0xA8 | ponteiro[8] | 4 | `rules`: gravados na ligação |
| 0xC8 | ponteiro[32] | 4 | `profile`: gravados na ligação; NULL quando o tamanho é 0 |

Os 41 ponteiros são zero no arquivo.

#### 2.6.2 Tabela de ações (`BtlAiSeqActTable`, `size[0]` bytes)

Deslocamentos a partir do início da tabela (entrada + 0x148).

| Desl. | Tipo | Larg. | Campo (leitor) |
|---|---|---|---|
| 0x000 | s32 | 4 | `unk0`: não lido; não zero no arquivo, sem cara de texto: não determinado, tratado como s32 (o tipo das duas visões que o declaram) |
| 0x004 | ponteiro | 4 | `steps`: gravado na ligação (= tabela + 0x788); zero no arquivo |
| 0x008 | u16[0x80] | 2 | `first`: primeiro passo de cada ação (`BtlAiSeq_PhaseInit`, `BtlAiSeq_PhaseStart`) |
| 0x108 | u8[0x80] | 1 | `count`: número de passos |
| 0x188 | s8[0x80] | 1 | `pick`: função de escolha (`gBtlAiPickFuncs`) |
| 0x208 | s8[0x80] | 1 | `timeout` em segundos |
| 0x288 | u16[0x80] | 2 | `flags` / `seqFlags` (`BTLAI_SEQF_*`; bit 2 testado pela condição 18; `BtlAiSeq_IsInterrupted`) |
| 0x388 | s8[0x200] | 1 | `stateClass` por estado do lutador (`BtlAiSense_*`, `BtlAiStep_ReachClass`, `AiThCond_ActRateByFlags`) |
| 0x588 | u8[0x200] | 1 | `stateFlags` por estado (bit 1) |
| 0x788 | `BtlAiSeqStep`[] | | passos, até `size[0]` (228 na demo, sem sobra) |

`AiThActTable` (btl_ai_think.h:166) declara 0x388..0x787 como um só `s8 actClass[0x400]`; tudo byte, a
conversão não muda.

`BtlAiSeqStep` (0x14 bytes, include/battle/btl_ai_seq_a.h:13):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | s8 | 1 | `handler` (índice em `gBtlAiStepFuncs`) |
| 0x01 | s8 | 1 | `group` |
| 0x02 | u8[2] | 1 | não lido: não determinado, tratado como bytes |
| 0x04 | s16 | 2 | `special` |
| 0x06 | s16 | 2 | `cls` |
| 0x08 | s32 | 4 | `hold` (bits de botão) |
| 0x0C | s32 | 4 | `press` |
| 0x10 | s32 | 4 | `once` |

Leitores: `BtlAiSeq_PhaseInit`, `BtlAiSeq_PhaseStart`, `BtlAiSeq_PhaseRun` (src/battle/btl_ai_seq.c).

#### 2.6.3 Listas de regras (`AiThRuleList` + `AiThRule`)

Cada uma das 8 listas ocupa `size[1 + i]` bytes:

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | s16 | 2 | `count` (`AiThink_EvalRules`) |
| 0x02 | u8[2] | 1 | não lido (zero na demo) |
| 0x04 | ponteiro | 4 | `rules`: gravado na ligação (= lista + 8); zero no arquivo |
| 0x08 | `AiThRule`[count] | | regras de 0x1C |

Na demo `size[1 + i]` = 8 + `count` x 0x1C exatamente nas 8 listas (71, 79, 127, 81, 22, 63, 7 e 87 regras).

`AiThRule` (0x1C bytes, btl_ai_think.h:137):

| Desl. | Tipo | Larg. | Campo (leitor) |
|---|---|---|---|
| 0x00 | s16 | 2 | `group`: bit de situação (`AiThink_EvalRules`) |
| 0x02 | u8[2] | 1 | não lido (zero nas 537 regras da demo) |
| 0x04 | u8[8] | 1 | `cond`: ids de condição (0xFF fim, 0xFE repete) |
| 0x0C | u8[8] | 1 | `arg` |
| 0x14 | u8[4] | 1 | `kind` e até 4 ações (`BtlAiSeq_PushRule`, src/battle/eft_det_b_c.c:32, lê `rule + 4 + i + 16`) |
| 0x18 | u8[4] | 1 | `param` e os argumentos das ações (`rule + 8 + i + 16`) |

#### 2.6.4 Perfis (um por tipo de IA, `size[9 + i]` bytes)

Só bytes (s8/u8). Pelas contas do código, cada perfil tem uma metade "nível 0" em +0x004 e uma "nível 29" em
+0x2C4 (`BtlAi_GetPairRate(..., prof + 4, prof + 0x2C4, ...)`, btl_ai_cond.c:951 e src/battle/btl_ai_mgr.c:172;
`prof + 0x2A8`/`+ 0x568`, `+ 0x2AC`/`+ 0x56C`, `+ 0x2B6`/`+ 0x576`, `+ 0x15C`/`+ 0x41C` em btl_ai_cond.c e
btl_ai_seq.c), interpoladas por `BtlAi_ScaleByLevel`; o layout de cada metade está em `AiThRates`
(btl_ai_think.h:182), todo em bytes. Leitores: `AiThink_SumPairRates`, `AiThink_SumQuadRates`,
`AiThink_SumSubRates`, `AiThink_GetSubRate`, `AiThink_RollGroupGate`, `AiThink_GetRollRange`, `BtlAi_GetPairRate`
(com a visão `AiRateRow`), `BtlAi_GetQuadRate`, `BtlAi_GetLowGaugeFactor`, `AiThCond_*`, `BtlAiStep_Unk14`,
`BtlAiSense_CheckBit58`.

Divergência de visão: `AiThProfile` (btl_ai_think.h:194) diz 0x584 bytes (s32 + 2 x 0x2C0), mas os perfis do
arquivo têm 0x580: a segunda metade só tem 0x2BC bytes (os 4 últimos de `misc` não existem; o maior
deslocamento lido pelo código é 0x577). Os 4 bytes em +0x000 (`unk0` s32 na visão) não são lidos e têm valores
de byte (quatro valores pequenos negativos, os mesmos nos 28 perfis): tratar como bytes; largura não
determinada pelo código.

Conversão da entrada 4: inverter o cabeçalho (82 palavras u32), `unk0`/`steps`/`first`/`flags` da tabela de
ações, os campos 0x04..0x13 de cada passo, `count` e `rules` de cada lista, `group` de cada regra; todo o resto
são bytes. Validação: `0x148 + soma(size)` <= tamanho da entrada, `(size[0] - 0x788) % 0x14 == 0`,
`size[1 + i] >= 8 + count x 0x1C`, perfis de 0 ou 0x580 bytes, e enchimento zero.

### 2.7 Entrada 5: cortes de câmera comuns (`ChrCamCutDef`)

Mesmo registro de `personagem.md` 5.11 (0x24 bytes: u8 x4, f32 x7, u16 `cutFlags`, u16 `flags`).
`ChrCam_StartCut` usa esta tabela quando `chr->cut.unk0 == 1` (`gBtlChars->cutDefs[chr->cut.unk4]`,
btl_char_cam_modes.c:733) e a do personagem quando é 0. Sem contador: 0x5C0 / 0x24 = 40 registros + 0x20 de
enchimento zero; o último byte não zero está no registro 39.

### 2.8 Entrada 6: vozes (`BtlVoiceEntry`)

Indexada pelo tipo de fala (`BTL_VOICE_KINDS` = 57, include/battle/btl_char_get.h:13). Sem contador: 0x200 / 8
= 64 registros; o último não zero é o 56 (os 7 seguintes são zero).

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x0 | s16 | 2 | `first`: primeira fala |
| 0x2 | s16 | 2 | `count` |
| 0x4 | f32 | 4 | `interval`: espera em segundos |

### 2.9 Entrada 7: golpes por ação de ataque (`BtlActAttackRec`)

Indexada por `ação - 0x70` (`BtlAct_GetAttackRec`, `BtlAtk_GetActionRow`). O código fala em ações 0x70..0xAD
(62), mas a entrada tem 0x600 bytes = 64 registros de 0x18, todos com dados até o último: tratar como
`tamanho / 0x18`. União das três visões:

| Desl. | Tipo | Larg. | Campo (leitor) |
|---|---|---|---|
| 0x00 | u8 | 1 | `unk0` (`BtlAct_PrepareAttack`: x 10 / 1080) |
| 0x01 | s8 | 1 | `leadIn` / `unk1` |
| 0x02 | s8 | 1 | `inputKind` (`BtlDecide_QueueAttack`) |
| 0x03 | u8 | 1 | `atkId` (`BtlAtk_GetId`) |
| 0x04 | s8 | 1 | `parts` |
| 0x05 | s8 | 1 | `kind` |
| 0x06 | u16 | 2 | `motion` |
| 0x08 | f32 | 4 | `unk8` |
| 0x0C | s8 | 1 | `unkC` (x 0.1) |
| 0x0D | s8[2] | 1 | `follow` |
| 0x0F | s8[2] | 1 | `followAlt` |
| 0x11 | s8 | 1 | `cut` |
| 0x12 | s8 | 1 | `cutAlt` |
| 0x13 | s8 | 1 | `needSkill` |
| 0x14 | u16 | 2 | `flags` |
| 0x16 | u8[2] | 1 | não lido; zero nos 64 registros |

### 2.10 Entrada 8: parâmetros por personagem (`BtlMemberParam`)

Indexada pelo id do personagem (`gBtlChars->paramTbl[chara]`; `tbl[4][chara * 4 + 3]` em
btl_char_mgr.c:450). Quatro s32: `healthMax` (0x0), `kiStart` (0x4), `unk8` (0x8), `blastStocks` (0xC).
0xA40 / 0x10 = 164 registros; o último não zero é o 160. Conversão: a entrada inteira como u32.

### 2.11 Entrada 9: linhas de aviso de técnica (`BtlTechPromptRow`)

Indexada por `BtlSuper_GetPromptRowIndex` (byte do bloco de técnicas do personagem, objeto + 0x92C + 0x227).
Registros de 0x14: 0x00 s8[2] `kind`, 0x02 s8 `buttons`/`button`/`dir`, 0x03..0x13 sem leitor. 0x180 / 0x14
= 19 registros + 4 bytes; só os 17 primeiros têm dados. Nos dados, 0x03 é sempre zero e 0x04..0x13 tem valores
de byte (nenhum grupo de 4 bytes tem cara de f32; muitos 0xFF): bytes. Nada a inverter, mas 0x03..0x13 é não
determinado pelo código.

### 2.12 Entrada 12: animação de câmera comum (`DemoCamAnim`)

Formato de `personagem.md` 5.12. `BtlObj_BindCommonTables` põe esta entrada em `mdl.common9C`, que é
`(obj + 0xB0)[1]`: depois da troca de membro, `DemoCam_PlayObjAnim(obj, 1)` toca esta câmera no lugar da entrada
0x2A do personagem. `DemoCam_FixupAnim` (src/battle/btl_demo_cam.c:31) troca os deslocamentos por ponteiros na
primeira vez que ela toca.

Na demo (não tocada): a mesma assinatura de 4 caracteres das câmeras do personagem, `needsFixup` não zero,
pose em 0x30, 6 canais em 0x50, 3 chaves por canal a partir de 0xB0, `unk1C` = 0x2F0 (bloco de 0x10 zerado),
`version` 0; o que não é cabeçalho, pose, canal ou chave é zero. Mesmo arranjo das entradas 0x2A/0x2B de
`personagem.md`.

### 2.13 Entrada 13: pose de referência (`BobjVec3[]`)

`mdl.common900` = `BobjMdl.refPose` (objeto + 0x918, include/battle/bobj_a.h:325). `BtlObjAnim_Retarget` com
`useOwn` subtrai `refPose[id]` (f32 x, y, z) das chaves de translação, `id < BOBJ_NODE_MAX` (71,
include/battle/bobj_a.h:40). 71 x 12 = 0x354 cabe nos 0x380 da entrada; na demo só os 48 primeiros vetores têm
dados (todas as palavras não zero têm cara de f32). Conversão: a entrada inteira como u32 (f32).

### 2.14 Entradas 14..213: faixas de lábio comuns (`BObjLipData`)

Formato de `personagem.md` 6.6 (8 bytes de assinatura, u16 `count`, 6 bytes zero, `count` chaves u16 frame +
u16 code, zeros até o fim). As 200 entradas existem; a assinatura é a mesma nas 200 e a mesma das faixas do
personagem. `BtlObj_BindCommonTables` liga 14..113 (`gSaveData->flags & 1` = 0) ou 114..213 (bit ligado) em
`mdl.lip[0..99]`. Contagens de chaves de 1 a 24 (78 faixas com 1 chave).

### 2.15 Escritas do jogo no buffer e momento da conversão

| Entrada | Quem escreve | O quê |
|---|---|---|
| 1 | `Res_RelocateOffsets` (src/sys/gfxm_b_b.c:244), a cada `BtlObj_Init` | `TexFile.ent` (+0x10), `TexEntry.pix`/`clut` (+0x38, +0x3C) |
| 4 | `AiThink_BindData`, a cada reinício da IA | cabeçalho 0xA4..0x147, tabela de ações + 0x4, + 0x4 de cada lista de regras |
| 12 | `DemoCam_FixupAnim`, se a câmera tocar | +0x04, +0x08, +0x18, +0x1C e o + 0x04 de cada canal |

Todos esses campos são zero no arquivo (ponteiros) ou lidos como deslocamento antes de virar ponteiro (12).
Como `Common_Reload` lê o arquivo uma vez só na partida, a conversão roda uma vez, logo depois dessa leitura (ou
no instalador), antes de qualquer luta.

### 2.16 Não determinado

- Entradas 2, 10 e 11: vazias e sem leitor (nenhum `FILE_TABLE`/`buf[n]` com esses índices).
- Entrada 1: `TexFile` 0x08..0x0F (o mesmo de 4.4).
- Entrada 4: `unk0` da tabela de ações (s32 pelas visões, não lido); `BtlAiSeqStep` 0x02..0x03; `AiThRule`
  0x02..0x03 e `AiThRuleList` 0x02..0x03 (zero); os 4 primeiros bytes de cada perfil (valores de byte, não
  lidos); o tamanho real do perfil (0x580) diverge de `AiThProfile` (0x584).
- Entrada 7: 0x16..0x17 (zero); e por que o arquivo tem 64 registros e o código só usa 62.
- Entrada 9: 0x03..0x13 de cada linha (bytes pelos dados).
- Entrada 3: os ids 0x19E..0x1B0 têm dados mas o código não os lê.
- Entrada 12: o bloco `unk1C` e as palavras 0x0C..0x1F das chaves (como em 5.12).
- Leitores em funções ainda em assembly não foram procurados.

### 2.17 Cobertura conferida contra o rastreio

Método de `personagem.md` 10: no WSL o arquivo foi percorrido como esta especificação diz (tabela, `TexFile`
com pacotes, cabeçalho e seções da IA pelos tamanhos, registros de cada tabela, câmera pelos deslocamentos,
lábios pelo `count`), cada byte ganhou largura (ou "enchimento"), e cada posição de `tipos.tsv` e de
`mapa.tsv` (expandida pelas repetições) foi comparada. Resultado: **100% dos bytes cobertos, nenhum byte
marcado duas vezes, todo enchimento zero, nenhuma posição de `tipos.tsv` nem de `mapa.tsv` com largura
diferente da especificação, e `mapa.tsv.conflitos` sem nenhuma linha deste arquivo.**

Posições distintas em `tipos.tsv` (T, leituras entre parênteses) e números em `mapa.tsv` (M):

| Parte | T | M (largura: quantidade) |
|---|---|---|
| tabela | 8 (8) | 4: 8 |
| 1 (luz) | 8 (10) | 4: 8 |
| 3 (flags de animação) | 109 (388) | 4: 109 |
| 4, cabeçalho | 37 (37) | 4: 41 |
| 4, tabela de ações (sem passos) | 227 (537) | 2: 39 |
| 4, passos | 202 (239) | 2: 53; 4: 78 |
| 4, listas (`count`) | 8 (16) | 2: 8 |
| 4, regras | 1.368 (2.028) | 2: 537 |
| 4, perfis | 665 (763) | nenhuma (bytes) |
| 5 (cortes) | 23 (31) | 2: 4; 4: 12 |
| 6 (vozes) | 36 (41) | 2: 24; 4: 12 |
| 7 (golpes) | 3 (3) | nenhuma (só bytes lidos) |
| 8 (parâmetros) | 6 (18) | 4: 6 |
| 9 (avisos) | 2 (6) | nenhuma (bytes) |
| 12, 13, 14..213 | 0 | 0 |

Os "?" do catálogo (2.122 leituras sem expressão na coluna do DWARF) são todos leituras por ponteiro calculado,
e todos caem em campos desta especificação com a mesma largura: 922 bytes de regra (`AiThink_EvalRules` 590,
`BtlAiSeq_PushRule` 332, que anda por `rule + 4 + i + 16`), 613 bytes de perfil (`AiThink_SumPairRates` 374,
`AiThink_SumQuadRates` 106, `BtlAi_GetPairRate` 43, `AiThCond_ActRateByFlags` 39 e outros, sobre `prof + k`),
443 bytes e 94 u16 da tabela de ações (`BtlAiSense_*`, `BtlAiSeq_*`, `BtlAiStep_*`), 37 palavras `size` do
cabeçalho (`AiThink_BindData`), 6 s32 da entrada 8 (`BtlMember_LoadParams`), 3 f32 da entrada 5 (operações de
vetor do port sobre a cópia), 3 f32 da entrada 6 (`BtlChar_TickVoiceTimers`, sobre o tempo copiado) e 1 byte
da entrada 9 (`BtlInput_TickCounter`). Os `u32 *[]` (396) são 388 de `BtlAnim_GetFlags` na entrada 3 e 8
palavras da tabela; os `s8 *[]` (69) e `u8 *[]` (19) são bytes de perfil (`AiThink_RollGroupGate`,
`AiThink_SumSubRates`, `AiThink_GetRollRange`, `BtlAiStep_Unk14`, `AiThCond_*`, `BtlAiSense_CheckBit58`).
Algumas leituras aparecem com o nome de outra struct (`ChrCamCut.timer`, `BtlCapiBGauge.ki`, `FxGauge.ki`,
`BtlAiSeqEntry.arg`): são leituras de cópias que o rastreio liga à origem no arquivo, com a mesma largura.

O que a demo não exercita: as entradas 12, 13 e 14..213 (só ligadas na troca de membro), o conteúdo dos
pacotes de imagem da entrada 1 (vão por DMA), os campos de 2 e 4 bytes da entrada 7 (`motion`, `unk8`,
`flags`), e os perfis dos tipos de IA que a luta não usou.

## 3. `pzs3us1/00002.bin`: efeitos comuns da luta (id 3, `gCommonRes->data[1]`)

Arquivo de 0x13F940 bytes (1.308.992). Formato: um pacote F2.1 (personagem.md 7.2) de 637 entradas, plano (nenhuma
entrada é subpacote), com 24 conjuntos de emissores no formato do pacote de técnica (personagem.md 7.6) e 46
entradas lidas diretamente por gerenciadores de efeito (texturas, tabelas de parâmetros, duas malhas). Nada é
comprimido.

### 3.1 Quem carrega e quem lê

- Carga: `Common_LoadBoot` (`src/sys/common.c:15`) aloca o buffer com `File_GetSize(3)`; `Common_Reload`
  (`common.c:30`, chamado uma vez por `Game_Main`, `src/main.c:142`) lê o arquivo inteiro em
  `gCommonRes->data[1]` (`include/sys/common.h:16`). O id 3 é o arquivo `pzs3us1/00002.bin`: ids 1..0xD47 são a
  partição 1 e `File_Open` abre `id - FILE_PT1_FIRST_ID` (`src/sys/file.c:439`), que o port nomeia `%05d.bin`
  (`port/src/plat_file.c:163`). O buffer fica a vida inteira do programa: a conversão roda **uma vez, logo depois de
  `Common_Reload`**, antes de qualquer luta.
- Único caminho de leitura: `BtlScene_GetCommonEntry(idx)` (`src/battle/btl_scene.c:224`) =
  `base + (base[idx] >> 2 << 2)`. Nenhum outro módulo toca `data[1]` (busca por `data[1]`, pelas visões locais de
  `gCommonRes` e pelo deslocamento 0x08 delas). O tamanho da entrada (`base[idx+1]`) nunca é lido: o código não testa
  entrada vazia. Os comentários "common file 2/3" de `btl_obj.c:201` e `bobj_a.c:2450` falam de `data[0]`
  (`00001.bin`), não deste arquivo.
- Leitores (todos `*Mgr_Init` de efeito, chamados na criação das camadas de efeito de cada luta):

| Entradas | Gerenciador (arquivo:linha) | Uso |
|---|---|---|
| 1, 2, 3 | `EftAuraMgr_Init` (`src/battle/eft_n.c:695`) | 1 = parâmetros da aura; 3, 2 = texturas (`EftTexSet_Load32`) |
| 4, 5, 6 | `EftBoltMgr_Init` (`src/battle/eft_o.c:237`) | 5, 6 = texturas (Load32); 4 = pares de nós dos raios |
| 7, 8, 9 | `EftGlowMgr_Init` (`src/battle/eft_q.c:326`) | 9, 8 = texturas (Load32); 7 = parâmetros do brilho |
| 0xA | `EftRayMgr_Init` (`src/battle/eft_s.c:2086`) | texturas (Load32) |
| 0xB | `EftGndDustMgr_Init` (`src/battle/eft_z.c:1260`) | texturas (Load8) |
| 0x0C..0x87 (6 conjuntos) | `EftImpactMgr_Init` (`src/battle/eft_v_b.c:227`), `hit[]` | `EftEmit_LoadSet(..., common = 1, idx)` |
| 0x88..0x191 (8 conjuntos) | idem, `blast[]` (`eft_v_b.c:241`) | idem |
| 0x192..0x236 (8 conjuntos) | `EftTransformMgr_Init` (`src/battle/eft_ab_b.c:377`) | idem |
| 0x237 | `EftSpdLine_Init` (`src/battle/eft_m.c:129`) e `EftShotFxMgr_Init` (`src/battle/eft_t_c.c:110`) | texturas (Load4), o mesmo pacote para os dois |
| 0x238..0x23B | `EftDiscMgr_Init` (`src/battle/eft_o_c.c:307`) | 0x238, 0x23A texturas (Load32); 0x239, 0x23B malhas (`EftMesh_Init`) |
| 0x23C..0x23E | ninguém | não referenciadas (3.7.7) |
| 0x23F..0x24C (1 conjunto) | `EftBodyFxMgr_Init` (`src/battle/eft_o_c.c:251`) | conjunto de emissores |
| 0x24D..0x258 | `EftStreakMgr_Init` (`src/battle/eft_t_b.c:94`) | 0x24D texturas (Load8); 0x24E cores; 0x24F..0x258 dez definições |
| 0x259..0x267 | `EftOrbTailMgr_Init` (`src/battle/eft_aa.c:1679`) | texturas (Load16), parâmetros, cores, partículas |
| 0x268..0x27D (1 conjunto) | `EftClashSparkMgr_Init` (`src/battle/eft_r.c:719`) | conjunto de emissores |

As outras chamadas de `EftEmit_LoadSet` (`eft_k.c`, `eft_j.c`, `eft_l_b.c`, `eft_i.c`, `eft_h.c:878` com `common = 0`;
`eft_r.c`, `eft_q.c`, `eft_l_c.c` com `common = 2`) leem pacotes do personagem (file[2]), não este arquivo.

### 3.2 Tabela de deslocamentos

Formato F2.1 (personagem.md 7.2): `buf[0]` = N = 637 (0x27D), `buf[1..N]` deslocamentos em bytes a partir do início do
arquivo, `buf[N+1]` = 0x13F940 = tamanho do arquivo, depois 4 bytes de enchimento zero até a entrada 1 em 0xA00.
Conferido: sequência não decrescente, nenhum deslocamento com os bits baixos ligados, todos múltiplos de 0x40, nenhuma
entrada vazia, enchimento zero. Conversão: as 640 palavras de 0x000 a 0x9FF como u32. O jogo lê 634 delas (todas as
pedidas por índice; não lê `buf[0]`, `buf[0x23C..0x23E]` nem `buf[N+1]`).

### 3.3 Mapa das entradas

As entradas são contíguas por gerenciador, na ordem da tabela de 3.1. Cada conjunto de emissores ocupa o cabeçalho e,
logo depois, exatamente as entradas que o percurso de `EftEmit_LoadSet` consome (personagem.md 7.6, "Ordem dos
recursos"); conferido nos 24 conjuntos: cada um termina onde começa o seguinte (ou a próxima entrada direta), e as 637
entradas ficam cobertas sem sobra e sem sobreposição:

| Papel | Entradas | Formato | Seção |
|---|---|---|---|
| cabeçalho de conjunto de emissores | 24 | `EftSetHead` + `EftSetGroupDef` + `EftEmitDef` | personagem.md 7.6; 3.4 |
| pacote de texturas de grupo | 100 | `TexFile` | personagem.md 4.4 e 7.7; 3.6 |
| parte, entrada a | 259 | por tipo de grupo | personagem.md 7.8, 7.9; 3.5 |
| parte, entrada b | 208 | por tipo de grupo | personagem.md 7.8; 3.5 |
| pacote de texturas direto | 16 | `TexFile` | 3.6 |
| tabelas diretas, malhas e não referenciadas | 30 | vários | 3.7 |

### 3.4 Os 24 conjuntos de emissores

Formato do cabeçalho, dos registros de grupo e de parte, e ordem dos recursos: personagem.md 7.6, sem diferença. Grupos
como `tipo(partes, texturas)`; tipo 0 (luz) tem partes mas não consome entradas.

| Cabeçalho | Até | Gerenciador | Grupos | Demo |
|---|---|---|---|---|
| 0x00C | 0x01F | impacto `hit[0]` | 5(7,1) 14(3,1) | lido |
| 0x020 | 0x024 | `hit[1]` | 14(3,1) | lido |
| 0x025 | 0x04B | `hit[2]` | 5(12,4) 9(2,1) 16(2,1); `nParts` = 17 | lido |
| 0x04C | 0x065 | `hit[3]` | 5(7,3) 9(3,2) | lido |
| 0x066 | 0x072 | `hit[4]` | 5(4,2) 14(1,1); `nParts` = 6 | lido |
| 0x073 | 0x087 | `hit[5]` | 5(3,1) 14(6,2) 16(2,1); `nParts` = 12 | lido |
| 0x088 | 0x090 | `blast[0]` | 5(3,2) | lido |
| 0x091 | 0x0A3 | `blast[1]` | 5(4,3) 16(3,1) | lido |
| 0x0A4 | 0x0B8 | `blast[2]` | 5(6,3) 16(2,1) | lido |
| 0x0B9 | 0x0F1 | `blast[3]` | 5(7,6) 9(2,1) 16(14,3) | lido |
| 0x0F2 | 0x119 | `blast[4]` | 2(1,1) 5(1,1) 9(3,2) 14(2,2) 16(10,2) | lido |
| 0x11A | 0x136 | `blast[5]` | 5(3,2) 9(2,1) 14(9,1) 16(2,1) | lido |
| 0x137 | 0x157 | `blast[6]` | 5(7,2) 14(2,1) 16(6,1) | lido |
| 0x158 | 0x191 | `blast[7]` | 9(10,3) 16(16,2) | lido |
| 0x192 | 0x1A9 | transformação 0 | 0(1) 5(5,1) 9(1,1) 14(3,1) 18(2,1) | lido |
| 0x1AA | 0x1BF | transformação 1 | 0(1) 5(5,1) 14(4,1) 18(2,1) | lido |
| 0x1C0 | 0x1C0 | transformação 2 | nenhum grupo (cabeçalho de 0x40, `mask` 0) | sem leitura registrada |
| 0x1C1 | 0x1CA | transformação 3 | 0(3) 5(2,1) 14(3,1) | lido |
| 0x1CB | 0x1E5 | transformação 4 | 0(1) 5(7,1) 9(1,1) 14(4,1) 18(1,1) | lido |
| 0x1E6 | 0x1FB | transformação 5 | 0(1) 5(3,2) 9(1,1) 14(4,1) 18(2,1) | lido |
| 0x1FC | 0x216 | transformação 6 | 0(1) 5(4,1) 9(3,1) 14(4,1) 18(2,1) | lido |
| 0x217 | 0x236 | transformação 7 | 0(1) 5(8,1) 9(1,1) 14(2,1) 16(1,1) 18(2,1) | lido |
| 0x23F | 0x24C | `EftBodyFx` | 5(2,1) 16(1,1) 18(2,1) | lido |
| 0x268 | 0x27D | `EftClashSpark` | 0(1) 5(3,3) 9(1,1) 10(1,1) 16(1,1) 18(1,1) | lido (só cabeçalho e texturas) |

Totais: 77 grupos (tipo 5: 21, 14: 14, 9: 12, 16: 12, 0: 8, 18: 8, 2: 1, 10: 1; nenhum 12, 15 ou 17), 272 registros de
parte, 259 partes com entradas. Em todos, `mask` (+0x00) é exatamente o OR de `1 << tipo` dos grupos.

Novidade em relação a personagem.md: em três conjuntos (0x025, 0x066, 0x073) o `nParts` do cabeçalho (+0x05) é um a
mais que a soma dos `nParts` dos grupos. `EftEmit_LoadSet` não usa o `nParts` do cabeçalho (percorre pelos grupos), mas o
tamanho do cabeçalho na tabela é `0x40 + 0x20 * nGroups + 0x40 * nParts(cabeçalho)` arredondado a 0x40: o registro de
parte a mais existe, sem leitor, e é todo zero nos três. O conversor deve delimitar os registros de parte pelo `nParts`
do cabeçalho (como o de personagem já faz) e tratar o excedente como `EftEmitDef` (zeros, largura indiferente).

### 3.5 Entradas das partes

Formatos por tipo: personagem.md 7.8 (tipos 2, 5, 9, 16, 18) e 7.9 ("V000", tipo 14). Tamanhos encontrados aqui
(entrada a / b, arredondados a 0x40): tipo 2: 0x4C0 / não tem; 5: 0x300 / 0x140; 9: 0x280 / 0x140; 10: 0x240 / 0x140;
14: 0x100..0xA00 / não tem; 16: 0xC0 / 0xC0; 18: 0x200 / 0x140. O que é novo:

**Tipo 10** (um grupo, conjunto 0x268, entradas 0x27A e 0x27B), `EftEmit_SpawnType10` (`src/battle/eft_i.c:393`) →
`EftPart10_Create` (`src/battle/eft_x.c:1035`): a = `EftPart10Def` (`include/battle/eft_x.h:104`, 0x20E bytes),
b = `EftPart10Def2` (`eft_x.h:161`, 0x12C bytes), com `EFT_PART10_KEYS` = 3.

| Desl. (a) | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x000..0x1F3 | f32 | 4 | `size`, `sizeRange`, ângulos, giros, velocidades, escalas, vidas, `spinA..scaleRange`, `count`, tempos (125 f32) |
| 0x1F4 | s32 | 4 | `flags` |
| 0x1F8..0x1FB | u8 x4 | 1 | `texCols`, `texRows`, `unk1FA`, `layer` |
| 0x1FC..0x20B | f32 x4 | 4 | `unk1FC`..`unk208` |
| 0x20C..0x20D | u8 x2 | 1 | `drawMode`, `unk20D` |

b: 0x000..0x12B, só f32 (cores Vec4 x12 em 0..255, `stretchX/Y/Z`, `stretchTime`, `unk248`, `unk250`). O resto até
0x240 / 0x140 é enchimento zero (conferido). Na demo só a textura do grupo foi lida; a e b não.

**Tipo 9, entrada a, 0x243..0x27F.** `EftQuadDef` (`include/battle/eft_y.h:100`) e `EftPart9Def` (`eft_x.h:374`) terminam
em 0x242, mas a entrada tem 0x280 bytes, e em 4 das 30 (0x124, 0x126, 0x209, 0x273) há palavras não nulas em
0x24C..0x25B, com valores de f32 plausíveis (de 1 a 3 palavras). Nenhuma visão nem leitura do rastreio chega lá. Não
determinado; proposta: 0x243 enchimento, 0x244..0x25F como u32 (f32 provável), resto enchimento zero (nos dados, zero de
0x25C em diante). É a única diferença de layout de parte em relação a personagem.md, onde esse trecho é zero.

**Tipo 16, a + 0x08..0x0B** (dúvida de personagem.md 7.8): aqui as 60 entradas têm valor não nulo e com cara de f32
nessa palavra; nenhuma leitura no rastreio. Reforça f32 (`EftBillDef.frameTime`).

**Tipo 2** (raios, entrada 0xF4): `count` (+0x4B0) = 10, o layout de personagem.md 7.8 cobre a entrada. Não lido na demo.

**"V000"** (50 pacotes): assinatura conferida nos 50, `keysOfs` = 0x20 + 0x20 x `layerCount` em todos (chaves logo
depois das camadas), 0x06..0x07 e 0x0C..0x1F zero em todos.

### 3.6 Pacotes de texturas

Formato `TexFile`/`TexEntry` e pacotes de imagem: personagem.md 4.4 e 7.7, sem diferença. Leitura:
`EftTexSet_Load32/16/8/4` (`src/battle/eft_ae.c:1719`, `:1736`; `src/battle/eft_det_a.c:174`, `:191`), que chamam
`Res_RelocateOffsets` (`src/sys/gfxm_b_b.c:244`) e **copiam** `tex0` (u64) para o conjunto (`EftAeTex.tex0`,
`include/battle/eft_ae.h:93`); o desenho trabalha na cópia (`EftVram_AddTex`/`AddImage`/`AddTexAt`, `eft_ae.c:1591`..`:1649`).

116 pacotes (100 de grupo, 16 diretos incluindo o não referenciado 0x23C): `entOfs` = 8 em todos (registros em +0x20),
0x08..0x0F não nulo em todos (não determinado, como em personagem.md), contagens 1 (89 pacotes), 2 (6), 3 (12), 4 (3),
5, 7 (2), 8, 14 (2). Nenhum excede a capacidade do conjunto que o carrega (`EftTexSet_CheckCount` é vazio no
executável, então isso não é conferido pelo jogo). 204 registros: 159 PSMT8 e 3 PSMT4 com imagem e paleta, e 42 só
de paleta (`pixOfs` = 0, `tex0` com PSM 0: são as paletas alternativas que `EftVram_AddClut` sobe para a imagem de
outro registro, por exemplo `EftBolt_UpdateTex`, `eft_o.c:100`). Paletas todas PSMCT32 (CPSM 0). 366 pacotes de
imagem/paleta, nenhum compartilhado entre registros; em todos: três NOP + DIRECT cobrindo o pacote, A+D TRXPOS/TRXREG/
TRXDIR, etiqueta IMAGE com NLOOP = (tamanho - 0x80) / 16, e TEXFLUSH no fim. Conversão como decidido (decisão 12):
cabeçalho de 0x60 e fim de 0x20 como palavras u32, imagem e paleta intactas, `TexEntry.tex0` como u64.

### 3.7 Entradas diretas

Nenhuma delas tem deslocamento interno que vire ponteiro, exceto as malhas, cujos deslocamentos são somados a cada
`EftMesh_Init` sem gravar no arquivo. Todas são lidas no próprio buffer.

#### 3.7.1 Entrada 1: parâmetros da aura (0x1080 bytes)

Três visões do mesmo bloco: `EftAuraPrm` (`include/battle/eft_m.h:286`, 0x000..0x193), `EftAuraCfg` (`eft_m.h:248`,
o bloco inteiro como `gEftAuraCfg`) e `EftAuraData` (`include/battle/eft_n.h:143`). Leitores: `EftAura_*`
(`src/battle/eft_m.c`, `src/battle/eft_n.c`).

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x000..0x193 | f32 | 4 | `EftAuraPrm`: razões, distâncias, vidas, velocidades, `stretch`, `size`, `kind[3]` (`EftAuraPrmKind`, 0x20), alfas, `speedRef` |
| 0x194..0x20F | f32 | 4 | `EftAuraCfg.fadeInTime`..`sparkMul` (`EftAuraData.unk194` diz bytes; os leitores leem f32) |
| 0x210..0x40F | f32 (Vec4) | 4 | `corner[2][4]`, `cornerB[2][4]`, `uv[4][4]` |
| 0x410..0x487 | s32 x3 x10 | 4 | `part[10]` = {`node`, `nodeEnd`, `nodeRef`} |
| 0x488..0x49F | s32 x6 | 4 | `fadeNode` |
| 0x4A0..0x4FF | s32 x2 x12 | 4 | `spark[12]` = {`unk0`, `node`} (`EftAuraCfg` vê o mesmo trecho deslocado 4 bytes: `unk4A0` e `spark[i]` = {`kind`, `node`}; as duas visões são palavras de 4 bytes) |
| 0x500..0x50F | ? | | `unk500`, sem leitor; só o byte 0x500 é não nulo (valor de inteiro pequeno): não determinado, s32 provável |
| 0x510..0x106F | f32 (Vec4) | 4 | `color[9][13]` e `colorB[5][13]` (`EftAuraColors` por linha, 0..255) |
| 0x1070..0x107F | | | enchimento zero |

Tudo é palavra de 4 bytes (626 palavras com valor de f32, 61 de inteiro pequeno, o resto zero). **Visão de outra
largura:** `EftAuraCfgFlame` (`eft_m.h:236`) declara em 0x410 + 12 x k um `u8 unk0` sobre o `s32 node` de `part[k]`
(ou de `fadeNode` para k = 10, 11), e `EftAura_SpawnFlame` o copia: `f->unk8 = gEftAuraCfg->flame[part].unk0`
(`src/battle/eft_m.c:1140`). No PS2 é o byte baixo do número; no 360, depois da conversão, seria o alto. O rastreio viu
essa leitura como **cópia** de 1 byte (coluna de cópias de `rastreio.tsv` = 1 nas posições 0x410 + 12 x k, k = 0..9,
que também são lidas como 4), e por isso o mapa não a marcou como conflito (só leituras entram na regra de conflito).
A conversão é u32 (a largura dos leitores de `node`); o código precisa de `PS2_LOW(u8, ...)` em `eft_m.c:1140` (classe
4 do guia), ainda não feito e sem entrada em `visoes_resolvidas.tsv`.

#### 3.7.2 Entrada 4: pares de nós dos raios (0xC0 bytes)

`EftBoltPair` (`include/battle/eft_n.h:280`, 0x14) x 8 (`EftBoltPairTbl.p[8]`, `src/battle/eft_n.c:1835`): s32 `nodeA`,
s32 `nodeB`, f32 `length`, f32 `follow`, f32 `followRange`. 0xA0..0xBF enchimento zero. Todas as 40 palavras lidas
como 4 na demo.

#### 3.7.3 Entrada 7: parâmetros do brilho (0xA80 bytes)

Visões: `EftPGlowCfg2` (`include/battle/eft_p.h:385`, 0x000..0x1A3: f32 e `kind[4]` de `EftPGlowKind`, 0x3C),
`EftPGlowCfg` (`eft_p.h:354`: f32 em 0x1A4..0x1BF, `pts[13]`, `vtx`, `uvA*`, `uvB` em Vec4, `node[10]` = `EftPGlowNode`
{s32 `node`, s32 `shape`, f32 x4} em 0x490, `nodeB[10]` f32 em 0x580) e `EftGlowCfg` (`include/battle/eft_q.h:139`:
`fadeOutTime` em 0x1AC, `color[9 x 5]` em 0x620, `colorTex` em 0x8F0). Tudo palavra de 4 bytes até o fim da entrada.

Correção de visão: `EftGlowCfg.colorTex` é declarado com 4 linhas (`4 * EFT_GLOW_ROW`, terminando em 0xA30), mas
`EftGlow_GetColors` aceita `type` até 4 (`if (type >= 5) return NULL`, `src/battle/eft_q.c:483`) e o arquivo tem uma
quinta linha de cores em 0xA30..0xA7F (16 palavras não nulas, f32). São 5 linhas: a entrada termina exatamente no fim
delas, sem enchimento.

#### 3.7.4 Entradas 0x239 e 0x23B: malhas do disco

`EftMesh_Init` (`src/battle/eft_ad_b.c:160`) com `EftMeshData` / `EftMeshGroup` / `EftMeshTri` / `EftMeshVtx`
(`include/battle/eft_ad.h:230`..`:254`): cabeçalho s32 `unk0`, s32 `numGroups`, s32 `trisOfs`, s32 `vtxOfs` (em bytes a
partir do início da entrada; somados a cada uso, nunca gravados); grupos de 0x10 (4 x s32); triângulos de 0x10 (s32
`v[3]`, s32 `tex`); vértices de 0x30 (Vec4 `pos`, `color`, `uv`). Tudo palavra de 4 bytes. Na demo: 1 grupo cada;
0x239 com 24 triângulos em 0x20 e 18 vértices em 0x1A0 (fim em 0x500 = tamanho); 0x23B com 48 triângulos em 0x20 e
59 vértices em 0x320 (fim em 0xE30, 0x10 de enchimento zero). Número de vértices = maior índice + 1 (o arquivo não o
guarda). Só `trisOfs` e `vtxOfs` foram lidos na demo (o disco não foi desenhado).

#### 3.7.5 Entradas 0x24E..0x258: rastros (`EftStreak`)

- 0x24E: `EftTVec` x 8 (`include/battle/eft_t.h:25`): cores Vec4, f32.
- 0x24F..0x258: `EftStreakDef` (`eft_t.h:259`), um por tipo (`EFT_STREAK_KINDS` = 10), 0x4A bytes em entradas de 0x80:
  f32 x7 (0x00..0x1B), `unk1C` declarado `u8[4]` (0x1C), f32 x8 (0x20..0x3F), s32 `endMode` e `space` (0x40, 0x44),
  u8 `blend`, u8 `count` (0x48, 0x49), enchimento zero. `unk1C` não tem leitor; nos 10 registros só o byte 0x1F é não
  nulo, como o expoente de um f32: não determinado, f32 provável (inverter como u32). O gerenciador não rodou na demo.

#### 3.7.6 Entradas 0x259..0x267: cauda do orbe (`EftOrbTail`)

- 0x259, 0x25C, 0x25E: texturas (Load16); 0x25E é carregada duas vezes (`idsA` = {0x25C, 0x25E, 0x25E}).
- 0x25B: `EftOrbTailParam` (`include/battle/eft_ab.h:53`): f32 x29 (0x00..0x73), u8 x8 (0x74..0x7B, um deles `unk7A`
  sem leitor), enchimento até 0x80 (sem sobra: a entrada tem 0x80).
- 0x25A: `EftOrbTailColor` (`eft_ab.h:82`): Vec4 x7, f32 x3 (0x00..0x7B), enchimento zero até 0xC0.
- 0x25D, 0x25F, 0x260 (`unk428[3]`): definição de partícula, mesmo formato da entrada a do tipo 5
  (`EftUPtclDef`/`EftPtclDef`, personagem.md 7.8), usada por `EftOrbTail_StartBurst` (`src/battle/eft_ab.c:680`) como
  `burstImage` de `EftPtcl_Create`. O rastreio confirma: s16 em 0x1C..0x27, u8 em 0x06.
- 0x261..0x267 (`unk434[7]`): entrada b do tipo 5 (`EftUPtclDef2`, 0x114 de f32), as 7 paletas de cor (`burstPalette`).

#### 3.7.7 Entradas 0x23C..0x23E: sem leitor

Nenhuma referência por constante nem por índice calculado (busca por 0x23C..0x23E e por somas a partir de 0x238). Pela
forma: 0x23C (0x1580) é um `TexFile` válido de 1 textura (imagem de 0x1080 e paleta de 0x480, DIRECT conferido);
0x23D e 0x23E (0x40 cada) são palavras de 4 bytes com valores de f32 e alguns inteiros pequenos, com cara das
entradas do disco (provavelmente um terceiro disco não usado). Não determinado; proposta: 0x23C como `TexFile`, 0x23D
e 0x23E como u32.

### 3.8 O que o jogo grava no arquivo, e quando converter

| Quem | O quê |
|---|---|
| `Res_RelocateOffsets` (em cada `EftTexSet_Load*`, a cada luta) | `TexFile.ent` (+0x10), `TexEntry.pix`/`clut` (+0x38, +0x3C) |
| `EftSprPack_Bind` (`src/battle/eft_ae.c:1087`) | `EftSprLayerDef.keys` (+0x14) das partes de tipo 14 |

As duas gravações são idempotentes (calculadas de `entOfs`, `pixOfs`, `clutOfs` e `keysOfs`, que não mudam), e por
isso repetir a carga de efeitos a cada luta funciona. Os `tex0` gravados pelo desenho (`EftVram_AddTex` e as funções
de `eft_o.c:112`, `eft_q.c:160`) são os das cópias nos conjuntos, não os do arquivo. Converter uma vez, depois de
`Common_Reload` e antes da primeira luta; o buffer nunca é relido.

### 3.9 Os conflitos do `mapa.tsv.conflitos`

As três posições são o mesmo campo, `TexEntry.tex0` (+0x30) do primeiro registro de três pacotes de textura diretos:

| Posição | Entrada | Lugar |
|---|---|---|
| 40592 (0x9E90) | 6 (texturas dos raios, `EftBolt`) | `TexFile` + 0x50 = registro 0 + 0x30 |
| 48784 (0xBE90) | 8 (texturas do brilho, `EftGlow`) | idem |
| 1123280 (0x1123D0) | 0x237 (texturas das linhas de velocidade) | idem |

Larguras: a posição tem leituras de 8 bytes e de 4 bytes (máscara 12) e é copiada como 8. Ligando os pontos do
rastreio ao código (disassembly da build x86r, que é i386):

- Leituras de 8 (o campo de fato): `EftVram_AddTex` (`eft_ae.c:1600`), `EftVram_AddImage` (`eft_ae.c:1618`),
  `EftSpdLine_DrawQuad` (`eft_m.c:491`, `tex->tex`), todas com `__asan_load8` sobre a cópia u64 que `EftTexSet_Load*`
  fez do `tex0` do arquivo (o rastreio leva a origem junto na cópia). `EftBolt_UpdateTex` (`eft_o.c:112`) e
  `EftGlow_UpdateTextures` (`eft_q.c:160`) aparecem na lista por gravarem essa cópia (`__asan_store8`).
- Leituras de 4: `EftPrim_DrawTriangle` (`src/battle/eft_b.c:586`, `p->xyz1.x = v1->x`) e `EftSprAnim_QueueTri`
  (`eft_ae.c:948`, `p->v[2].xyz.x = v2->x`, inline em `EftSprAnim_DrawTriClip`). São leituras de coordenadas de vértice
  (s32) de vetores locais, não do `tex0`: a memória lida (pilha e pacote de GPU) guardava antes uma cópia do `tex0` de
  64 bits, e a sombra do rastreio herdou essa origem. É o mesmo caso já catalogado em
  `port/x360/tools/visoes_resolvidas.tsv` (linhas `EftPrim_DrawTriangle` e `EftSprAnim_DrawTriClip`: "pacote da GPU
  montado byte a byte e em u64, relido em u32", adiado para a F3) e o mesmo tipo de artefato de personagem.md 6.3.1.

Conclusão: **u64**. `TexEntry.tex0` é campo de struct declarado u64 em todas as visões (`include/sys/gfxm_b_b.h:41`,
`EftAeImage` em `include/battle/eft_ae.h:87`) e lido como u64 pelo código C (decisão 12); não há leitura do campo por
metades, e não precisa de `PS2_LD64` nem `PS2_LOW`. O mapa corretamente não inverteu as três; o conversor as inverte
como u64. Outras posições com o mesmo padrão: as outras 200 posições de `tex0` que o rastreio viu têm só largura 8 (no
mapa como 8; 203 dos 204 `tex0` do arquivo foram vistos, falta só o de 0x23C); o padrão depende de qual cópia de `tex0` foi parar na pilha antes do desenho de triângulos, não do campo,
então pode aparecer em qualquer `tex0` noutra luta, sempre com a mesma resposta.

A busca por cópias de largura diferente da leitura (coluna de cópias do `rastreio.tsv`) achou um caso real que o mapa
não marca: o byte de `EftAuraCfgFlame.unk0` (3.7.1). As outras divergências de cópia (vetor de 16 lido e copiado como
4 nos parâmetros da aura, do brilho e das partes; bytes do cabeçalho de emissores copiados como byte) são compatíveis
com a especificação.

### 3.10 Não determinado

- `TexFile` 0x08..0x0F (como em personagem.md).
- Tipo 9, entrada a, 0x243..0x27F: palavras com valor de f32 em 4 entradas, sem visão nem leitor (proposta: u32 em
  0x244..0x25F).
- `EftAuraData.unk500` (0x500..0x50F) e `EftStreakDef.unk1C` (u8[4] na decomp, f32 provável pelos dados): sem leitor.
- Entradas 0x23C..0x23E: sem leitor; formato inferido pela forma (`TexFile` e palavras de 4 bytes).
- O registro de parte excedente nos conjuntos 0x025, 0x066 e 0x073 (zero; o significado do `nParts` maior não é claro).
- Os mesmos bytes sem leitura de personagem.md 7.6 e 7.9 (`EftSetGroupDef` 0x03..0x1F, `EftEmitDef` 0x38..0x3F,
  cabeçalho 0x07, "V000" 0x06..0x07 e 0x0C..0x1F): todos zero neste arquivo.

Depende de decisão: o que fazer com o trecho do tipo 9 e com as três entradas sem leitor (o conversor de personagem
exige zero depois de 0x243 no tipo 9 e não aceita entrada não especificada); a correção de código em `eft_m.c:1140`;
e as duas correções de visão de header (`EftGlowCfg.colorTex` com 5 linhas; `EftAuraData.unk194` são f32), que não
mudam a conversão.

### 3.11 Cobertura conferida contra o rastreio

Método: no WSL, um modelo da especificação acima percorreu o arquivo (tabela, os 24 conjuntos pela ordem de
`EftEmit_LoadSet`, partes por tipo, `TexFile` e pacotes, entradas diretas) e deu a cada byte uma largura ou "enchimento";
cada posição de `tipos.tsv` e de `mapa.tsv` (expandida pelas repetições) foi comparada com ela. Resultado do modelo:
todos os 1.308.992 bytes cobertos, nenhum marcado duas vezes, nenhum byte não nulo em enchimento (com as propostas de
3.10): 73.233 números de 4 bytes, 1.434 de 2, 204 de 8, 981.760 bytes (quase todos imagem e paleta) e 29.800 de
enchimento.

`tipos.tsv`: 10.607 linhas, 6.899 posições distintas; todas com a largura da especificação, exceto as 3 posições de
3.9. `mapa.tsv`: 2.265 linhas, 6.930 posições depois de expandir (4: 6.644; 2: 86; 8: 200), **todas iguais à
especificação**. Por papel (posições de `mapa.tsv`):

| Papel | 4 | 2 | 8 |
|---|---|---|---|
| tabela | 634 | | |
| cabeçalhos de emissores | 351 | | |
| texturas de grupo (`TexFile`, registros) | 608 | | 138 |
| texturas diretas | 244 | | 62 |
| parte tipo 5 a / b | 1.761 / 644 | 56 / | |
| parte tipo 9 a / b | 51 / 23 | | |
| parte tipo 14 ("V000") | 855 | 20 | |
| parte tipo 16 a / b | 198 / 187 | | |
| aura (1) | 308 | | |
| pares dos raios (4) | 40 | | |
| brilho (7) | 354 | | |
| malhas | 4 | | |
| cauda do orbe (0x25A, 0x25B, partículas) | 382 | 10 | |

Entradas sem nenhuma leitura na demo: 416 de 637 (partes que não foram disparadas: 79 + 79 do tipo 5, 53 + 53 do tipo
16, 29 + 29 do tipo 9, 14 + 14 do tipo 18, 42 do tipo 14, o tipo 2, o tipo 10; o cabeçalho 0x1C0; 0x23C..0x23E; os
rastros 0x24E..0x258, cujo gerenciador não rodou; 6 das 7 paletas da cauda do orbe). Tipos 2, 10 e 18 só tiveram as
texturas lidas.

Os "?" do catálogo (3.055 linhas "sem expressão nessa coluna"): a coluna do DWARF não cai numa expressão que
`tipos.py` reconhece. Vêm de `src/battle/eft_u_b.c` (1.428, `EftPtcl_SetKey`/`BlendKeys`/`Spawn`/`StepPtcls`, que
indexam as chaves das partículas por aritmética de ponteiro), `eft_z.c` (373, `EftLine_Animate`, `EftBill_Update`),
`eft_ae.c` (315, `EftSprAnim_DrawLayerAt`), das rotinas de vetor do port (`src/port/vu0_b.c` 293, `vu0_a.c` 128,
`port/src/plat_fastvec.c` 87: `op_lqc2`, `wide4`, `mtx_apply`, `Vec3_Add`...) e de `eft_i.c` (156, `EftEmit_Spawn`).
Todas essas posições estão no `mapa.tsv` com a largura da especificação. As 2.097 linhas "largura" são leituras de 4
bytes de componentes de vetor (`RefVec4f`, `EftUVec`, `RefClipVtx`, `EftPVec`), leituras de 1 byte em `EftSprKey`
(a cor) e as 2 do conflito. Os `s32 *[]` (634) são as palavras da tabela lidas por `BtlScene_GetCommonEntry`
(`base[idx]`); os `EftSprKey *[]` (168) são os bytes de cor das chaves de sprite; os `f32 *[]` são as tabelas da aura e
do brilho lidas por índice.

## 4. `pzs3us1/00003.bin`: tabela de personagens e de itens (id 4, `gCommonRes->data[2]`)

### 4.1 Quem carrega e quem lê

- Id 4 (`pzs3us1/00003.bin`, pois o arquivo NNNNN da partição 1 é `flid` NNNNN e `File_OpenById` usa
  `flid = id - FILE_PT1_FIRST_ID`, src/sys/file.c:439 e port/src/plat_file.c:163). `Common_LoadBoot`
  (src/sys/common.c:15) aloca o buffer pelo tamanho do arquivo; `Common_Reload` (common.c:30) o lê inteiro em
  `gCommonRes->data[2]` com `File_Request`. `Common_Reload` só roda uma vez (src/main.c:142): o arquivo fica residente; não há cópia nem
  descompressão.
- Leitores na luta: `ItemSet_GetStats` (src/battle/view_b_b.c:159, chamado por `BtlMember_ApplyItems`,
  src/battle/battle_load.c:1322), `ChrTbl_IsRelated` (src/battle/view_b_c.c:81), `ChrView_GetCharaInfo`
  (src/battle/view_a_d.c:96), `Save_SetDefaults` (src/sys/save.c:62). Os outros acessores de view_b_b.c
  (`ChrTbl_WrapCostume`, `ChrTbl_GetCost`, `ChrTbl_GetLevel`, `ChrTbl_GetExp`, `ChrTbl_GetMaxExp`,
  `ItemSet_GetStatsList`, `ItemTbl_CanEquip`, `ItemTbl_GetClass`) e o overlay de menu
  (`MPACK_AT(gCommonRes->data[2], 1 ou 2)` em src/menu/menu_c_e.c, menu_e_b.c, menu_g.c, menu_h_b.c, menu_i_d.c,
  menu_l_b.c, menu_m_b.c, menu_u_h.c, menu_v_b.c, menu_v_c.c, menu_v_d.c, menu_y_b.c, menu_z_d.c, menu_za_c.c).
- Nenhuma escrita do jogo no buffer foi encontrada: o arquivo é só leitura. Nada vira ponteiro.

Nota: a "entrada 12 do arquivo comum 3" que `BtlObj_BindCommonTables` (src/battle/bobj_a.c:2451) lê NÃO é este
arquivo. A visão `BObjCommon` (bobj_a.c:1908) põe `file3` em `gCommonRes + 4`, e o único campo real de
`BObjFile` nessa posição é `buf` (include/battle/bobj_b.h:21), isto é, `gCommonRes->data[0]`: id 2,
`pzs3us1/00001.bin`. A função lê ali as entradas 12 e 13 (`buf[0xC]..buf[0xE]`, para `common9C` e `common900`) e
as faixas de lábio `0xE + first + i` (i = 0..99, `first` = 0 ou 0x64 pelo bit 0 de `gSaveData->flags`), ou seja,
as entradas 14..113 ou 114..213 do 00001. O "3" do nome é só a numeração da visão.

### 4.2 Tabela de deslocamentos

O mesmo formato de personagem.md seção 2, lido por `ChrTblFile`/`ItemFile` (include/battle/view_b.h:132,
include/sys/save.h:117) e por `MPACK_AT`/`PACK_AT` (include/menu/menu_a.h:55, include/battle/view_a.h:60), que
fazem `buf[n] >> 2 << 2`:

| Palavra | Tipo | Valor na demo | Significado |
|---|---|---|---|
| `buf[0]` | u32 | 2 | N (`ItemFile.unk0`, `ChrTblFile.unk0`; ninguém lê) |
| `buf[1]` | u32 | 0x40 | `charaOffset`: seção 1, personagens |
| `buf[2]` | u32 | 0x2600 | `itemOffset`: seção 2, itens |
| `buf[3]` | u32 | 0x5CC0 | fim (= tamanho do arquivo) |
| 0x10..0x3F | u32 | zeros | enchimento |

Conferido no WSL: fim igual ao tamanho, sequência crescente, bits baixos zero, enchimento zero. Conversão: u32
até 0x40.

### 4.3 Seção 1: personagens (`ChrTblEntry`), 161 registros de 0x3C

`ChrTblEntry` (include/battle/view_b.h:91); visões do mesmo registro: `ChrViewInfo` (include/battle/view_a.h:226),
`MChrTblEntry` (include/menu/menu_h.h:58), `ZChrEntry` (include/menu/menu_z.h:322), `VChrEntry`
(include/menu/menu_v.h:72), `ZaChrEntry` (include/menu/menu_za.h:91). Indexado pelo id do personagem, sem
contador no arquivo; o código limita a `CHRVIEW_CHARA_COUNT` = 0xA1 (include/battle/view_a.h:181). A seção tem
0x25C0 bytes = 161 x 0x3C + 4 de enchimento zero.

| Desl. | Tipo | Larg. | Campo (leitor) |
|---|---|---|---|
| 0x00 | s32 | 4 | `aiType`: tipo de IA sem item de IA (`ItemSet_GetStats`) |
| 0x04 | s32 | 4 | `unk4`: sem leitor; declarado s32 em todas as visões; inteiro pequeno nos 161 registros |
| 0x08 | u16 | 2 | `flags`: bit 0 `CHRTBL_FLAG_0` (`ItemTbl_CanEquip`; menus h, v, z, za) |
| 0x0A | u16 | 2 | `costumes` (`ChrTbl_WrapCostume`) |
| 0x0C | u16 | 2 | `cost` (`ChrTbl_GetCost`) |
| 0x0E | u16 | 2 | `baseLevel` (`ChrTbl_GetLevel`); `ZaChrEntry` chama o mesmo u16 de `slots` |
| 0x10 | s32[7] | 4 | `exp`: crescente, zero termina (`ChrTbl_GetExp`, `ChrTbl_GetMaxExp`) |
| 0x2C | u8[4] | 1 | `link`: ids das outras formas, 0xFF = nenhuma (`ChrTbl_IsRelated`) |
| 0x30 | f32 | 4 | `targetY` da câmera do visualizador (`ChrView_SetupCamera` via `ChrViewInfo`) |
| 0x34 | f32 | 4 | `dist` |
| 0x38 | 4 bytes | 4 | `unk38`: declarado `u8[4]` e sem leitor, mas nos 161 registros é um float plausível (e em 53 deles só os dois bytes altos são não zero, o padrão de um f32 redondo): tratar como número de 4 bytes (f32 provável). Não determinado pelo código |

### 4.4 Seção 2: itens (`ItemTblEntry`), 350 registros de 0x28

`ItemTblEntry` (include/battle/view_b.h:121); visões: `ItemInfo` (include/sys/save.h:123), `VItemEntry`
(include/menu/menu_v.h:31), `WItemEntry` (include/menu/menu_w.h:44), `UItemEntry` (include/menu/menu_u.h:119),
`MItemEntry` (include/menu/menu_g.h:33), `ZaItemEntry` (include/menu/menu_za.h:100). Indexado por id do item menos
1; `SAVE_ITEM_COUNT` = 350 (include/sys/save.h:21). A seção tem 0x36C0 bytes = 350 x 0x28 + 0x10 de enchimento
zero.

| Desl. | Tipo | Larg. | Campo (leitor) |
|---|---|---|---|
| 0x00 | u8 | 1 | `type` (`ItemSet_*`, menus) |
| 0x01 | u8 | 1 | `group`/`kind` (menus v, za) |
| 0x02 | u8 | 1 | `picture` (menus v, w) |
| 0x03 | u8 | 1 | `slots` (`ItemSet_Fit`, `ItemSet_GetBonus`, menus) |
| 0x04 | s32 | 4 | `price` (`WItemEntry`, src/menu/menu_v_d.c:634, :1105); as outras visões declaram bytes sem ler |
| 0x08 | u32 | 4 | `stockLevel` (`VItemEntry`, `WItemEntry`) |
| 0x0C | s16[4] | 2 | `stat` (`ItemSet_GetBonus`, `ItemSet_GetStats`) |
| 0x14 | s32 | 4 | `flags` (`ItemTbl_*`, `Save_SetDefaults` via `ItemInfo.flags`, menus) |
| 0x18 | s32[4] | 4 | `ability`: máscaras de bits (`ItemSet_GetStats`) |

Nos dados, os bytes 0x06..0x07, 0x09..0x0B e 0x16..0x17 são zero em todos os registros, o que é coerente com
s32/u32 pequenos em ordem little-endian.

### 4.5 Cobertura conferida contra o rastreio

Percurso: tabela, 161 + 350 registros e enchimentos; 100% dos 23.744 bytes cobertos, enchimento todo zero.

| Região | T (posições de tipos.tsv) | M (posições de mapa.tsv) | Largura igual |
|---|---|---|---|
| tabela | 2 (`ChrTblFile`/`ItemFile` +4, +8) | 4: 2 | todas |
| personagens | 1 (registro 0 +0x00) | 4: 1 | todas |
| itens | 348 (`ItemInfo.flags`) | 4: 350 | todas |

Os "?" do catálogo (9 leituras): uma em 0x40 (`aiType` do personagem 0) atribuída a
`BtlMember_ClampBonus` (battle_load.c:1274) e oito em 0x5C4C e 0x5C9C (`flags` dos itens de índice 347 e 349)
atribuídas a `BtlInput_GetKeyMask` (src/battle/btl_input.c:496). A primeira é a leitura de
`charas[chara].aiType` de `ItemSet_GetStats`, chamada logo antes por `BtlMember_ApplyItems`, com o local de
DWARF perdido na otimização. As outras oito estão numa linha que lê `gSaveData->key[chr->player][i]`, e não o
arquivo: quem leu de fato não foi determinado. Nos três casos a largura (4) é a do campo. Não há conflito do
00003 em `mapa.tsv.conflitos`.

Não exercitado pela demo: quase todo o arquivo (os leitores são do menu); da seção 1 só `aiType` do
personagem 0, da seção 2 só `flags`.

## 5. `pzs3us1/00004.bin`: arquivo de partida, fontes, ícones e avisos do controle (id 5, `gCommonRes->boot`)

### 5.1 Quem carrega e quem lê

- Id 5 = `COMMON_FILE_BOOT` (include/sys/common.h:10). `Common_LoadBoot` (src/sys/common.c:15) o lê com
  `File_LoadSync` em `gCommonRes->boot`, uma vez, na partida (src/main.c:141); nunca é relido.
- `Font_InitEx` (src/battle/col_c.c:242) liga as entradas 1 e 2 a dois espaços de fonte com
  `Font_SetFont(0..1, boot + (boot[n] >> 2))`.
- `FontIcon_Init` (src/battle/col_c_b.c:468, chamado de src/main.c:152) liga a entrada 8 (`boot[8]`) e roda
  `Res_RelocateOffsets` nela.
- `PadWatch_DrawMessage` (src/sys/pad_watch.c:174) imprime o texto `boot[9 + message]`, message = 0..2
  (`PAD_WATCH_MSG_PAD1`, `_PAD2`, `_BOTH`, include/sys/pad_watch.h:33).
- Leitores de conteúdo: `Font_SearchGlyph` (col_c.c:1012), `Font_MakeTex0` (col_c.c:184),
  `Font_UploadTexture` → `Tex_Upload` (src/sys/gfxm_b_b.c:306), o desenho dos ícones (col_c_b.c:95..103,
  `tex0`, `tbp`, `cbp`), `TexFile_UploadAll`, `Font_GetHeight`/`Font_Print*` sobre os textos.

### 5.2 Tabela de deslocamentos

Formato de personagem.md seção 2. N = 11 (`buf[0]`), fim 0x22480 = tamanho, entradas a partir de 0x40, palavras
0x34..0x3F zero, bits baixos zero. Conversão: u32 até 0x40.

| Entrada | Faixa | Conteúdo | Formato | Quem lê |
|---|---|---|---|---|
| 1 | 0x40..0x8AC0 | fonte 0 | `FontData` (5.3) | `Font_InitEx`, `Font_SetFont` |
| 2 | 0x8AC0..0x19A40 | fonte 1 | `FontData` | idem |
| 3..7 | vazias | | | ninguém (nenhum leitor de `boot[3..7]`) |
| 8 | 0x19A40..0x22080 | ícones dos botões do controle | `TexFile` (personagem.md 4.4) | `FontIcon_Init`, desenho dos ícones |
| 9, 10, 11 | 0x22080..0x22480 | avisos do controle desconectado | texto u16 (5.5) | `PadWatch_DrawMessage` |

### 5.3 Fonte (`FontData`, entradas 1 e 2)

`FontData`, `FontTex`, `FontGlyph` (include/battle/col_c.h:54, :41, :34). `Font_SetFont` (col_c.c:1078)
transforma no próprio buffer, uma vez (o teste é `(u32)data->tex < (u32)data`), os deslocamentos em ponteiros.
Todos os deslocamentos da fonte são **em bytes a partir do início da `FontData`** (não em palavras, ao contrário
de `TexFile`).

| Desl. | Tipo | Larg. | Campo (leitor) |
|---|---|---|---|
| 0x00 | 8 caracteres | 1 | `unk00[0..7]`: assinatura ASCII (as duas fontes iguais); ninguém lê: bytes |
| 0x08 | u32 | 4 | `unk00[8..11]`: vale 1 nas duas; sem leitor (provavelmente o número de texturas) |
| 0x0C | u32 → ponteiro | 4 | `tex`: deslocamento da `FontTex` (0x40 nas duas); vira ponteiro |
| 0x10 | s32 | 4 | `unk10`: vale 1; sem leitor |
| 0x14 | u32 → ponteiro | 4 | `glyphs`: deslocamento da tabela de glifos; vira ponteiro |
| 0x18 | s32 | 4 | `glyphCount` (`Font_SearchGlyph`): 208 e 425 |
| 0x1C | s32 | 4 | `unk1C`: copiado para `FontSlot.unk0C` |
| 0x20 | s32 | 4 | `lineHeight` |
| 0x24..0x3F | | | zeros |
| `tex` (0x40) | `FontTex` 0x40 bytes | | o layout de `TexEntry` (personagem.md 4.4), com uma diferença: `pixels` (+0x00) e `clut` (+0x04) são deslocamentos em BYTES a partir da `FontData` (0x80 nas duas), e `Font_SetFont` grava `pixelsPtr`/`clutPtr` (+0x38, +0x3C) = `data + deslocamento`. Lidos: +0x00, +0x04, +0x08, +0x0C, +0x18, +0x1C (`Tex_Upload`), +0x30 `tex0` u64 (`Font_MakeTex0`) |
| `pixels` | pacote de imagem | | formato de personagem.md 4.4: cabeçalho VIF/GIF de 0x60, imagem, fim de 0x20; PSMT4 nas duas |
| `clut` | pacote de paleta | | idem, 0x480 bytes |
| `glyphs` | `FontGlyph` x `glyphCount` | 2 | 6 bytes cada: u16 `code`, u16 `u` (bits de x, largura e CLUT), u16 `v` |
| fim | | | zeros até o fim da entrada |

Delimitação conferida no WSL: cabeçalho, zeros, `FontTex` em 0x40, imagem logo em 0x80, paleta logo depois,
glifos logo depois da paleta, zeros até 0x40. A tabela de glifos está em ordem não decrescente de `code` (busca
binária), com 1 código repetido na fonte 0 e 12 na fonte 1 (a busca acha um deles). Os bytes 0x28..0x2F e os
ponteiros +0x38/+0x3C da `FontTex` são zero no arquivo.

### 5.4 Ícones (entrada 8)

`TexFile` com 28 `TexEntry` (`entOfs` = 8 palavras), deslocamentos em palavras relativos ao início da entrada,
como em personagem.md 4.4. Visões: `FontIconRes` (col_c.h:180) e `FontTex`. Leitores além da relocação: o
desenho de ícone lê `tbpOfs` (+0x20), `cbpOfs` (+0x24) e `tex0` (+0x30, u64); `TexFile_UploadAll` sobe todos. Os
28 pacotes de imagem e paleta seguem o formato de 4.4.

### 5.5 Avisos do controle (entradas 9..11)

Texto u16 terminado em 0 (`Font_GetHeight`, `Font_Print*` com `FONT_CMD_STR16`, que expande marcas
`<PAD=...>`, `<COL=...>`, col_c_b.c). Conferido: as três entradas (0x140, 0x140, 0x180 bytes) têm a palavra 0
final e zeros até o fim. Converter cada u16 até o 0; o resto é enchimento zero.

### 5.6 Escritas na carga

`Font_SetFont`: `FontData.tex` (+0x0C), `glyphs` (+0x14), `FontTex.pixelsPtr`/`clutPtr` (+0x78 e +0x7C da
entrada). `FontIcon_Init` → `Res_RelocateOffsets`: `TexFile.ent` (+0x10) e `pix`/`clut` de cada `TexEntry`. Como o
arquivo é lido uma vez, basta converter uma vez antes de `Font_InitEx`/`FontIcon_Init`.

### 5.7 Cobertura conferida contra o rastreio

100% dos 140.416 bytes cobertos, enchimento zero, os 30 pacotes de textura no formato esperado.

| Região | T | M | Largura igual |
|---|---|---|---|
| tabela | 3 (`u32 *[]`: `boot[1]`, `boot[2]`, `boot[8]`) | 4: 3 | todas |
| cabeçalhos das fontes | 4 (`tex`, `glyphs` de cada) | 4: 8 | todas |
| `FontTex` das fontes | 4 (`pixels`, `clut`) | 4: 4 | todas |
| ícones: `TexFile` | 2 (`count`, `entOfs`) | 4: 2 | todas |
| ícones: `TexEntry` | 56 (`pixOfs`, `clutOfs` x 28) | 4: 56 | todas |

Os 2 "?" do catálogo são a releitura de `FontData.tex` em `Font_SetFont` (col_c.c:1084, posições 0x4C e 0x8ACC),
largura 4 como o campo. `u32 *[]` são as palavras da tabela lidas como `boot[n]`. Sem conflito.

Não exercitado: os glifos, `glyphCount`, `lineHeight`, `unk1C`, os textos 9..11, `tbpOfs`/`cbpOfs`/`tex0` dos
ícones e todo o conteúdo dos pacotes (DMA).

## 6. `pzs3us1/00005.bin`: HUD e menu de pausa da luta (id 6, `BattleRes.unk8`)

### 6.1 Quem carrega e quem lê

- Id `6 + gProgress->unk0[0]` (src/battle/battle_load.c:596, passo 4 de `BtlLoad_StepInitial`), lido para o heap
  em `BattleRes.unk8` (`gCommonRes + 0x28`, include/battle/battle_work.h:100) a cada luta e liberado no fim
  (battle_load.c:841). `gProgress->unk0[0]` é o idioma do texto (o mesmo campo que `BtlScriptCmd_LoadText`,
  src/battle/btl_script_cmd.c:463, testa com os valores 0 e 1). Assim 00005 (id 6) é o idioma 0 e
  **00006 (id 7) o idioma 1**; 00007 em diante já são os blocos de 0xD60 bytes de `BTL_FILE_CHARA_DATA` (id 8 +
  personagem x 2 + lado). No WSL, 00006 tem a mesma estrutura (N = 197, mesmas entradas vazias e mesmos
  formatos), com a folha 1 de 24 texturas em vez de 25 e textos de outro tamanho. A demo leu 00005.
- `Hud_Init` (src/battle/hud_a.c:268) liga as entradas 1..5 às cinco folhas de sprites (`res[0]` = entrada 1,
  `res[3]` = 2, `res[4]` = 3, `res[1]` = 4, `res[2]` = 5) e roda `Res_RelocateOffsets` em cada uma.
- `BtlMenu_Init` (src/battle/hud_0_b.c:563) liga a entrada 7 (folha do menu de pausa) e a reloca.
- `BtlMenu_GetText(n)` (hud_0_b.c:226) devolve `file + file[n] / 4` como `u16 *`: `BtlMenu_GetItemText(n)` =
  entrada `n + 8`, `BtlMenu_GetControlsText(padType, config)` = entrada `0x26`, `0x4E`, `0x76` ou `0x9E` mais
  `config` (hud_0_b.c:238). Leitores do conteúdo: `HudRes_UploadTex`, `HudSprite_InitTex`, `HudSprite_DrawAt`,
  `Hud_ClearTexMarks` (src/battle/hud_c_c.c, hud_a.c:145), `BtlMenu_UploadTex`, `BtlText_GetTex0`
  (src/battle/hud_0_c.c:108), `Font_Print*`.

### 6.2 Tabela de deslocamentos

Formato de personagem.md seção 2; N = 197 (palavras 0..198 e zeros até 0x340), fim 0xAB300 = tamanho, todo
deslocamento múltiplo de 0x40. Conversão: u32 até 0x340. `BtlMenu_GetText` não limpa os dois bits baixos, mas
todos são zero.

| Entrada | Conteúdo | Formato | Quem lê | Na demo (00005) |
|---|---|---|---|---|
| 1 | folha do medidor, relógio e equipe (`res[0]`) | `TexFile` | `Hud_Init`, `HudGauge_Init`, `HudTimer_Init`, `HudTeam_Init` | 25 texturas, 0x26040 bytes |
| 2 | folha dos comandos na tela (`res[3]`) | `TexFile` | `HudPrompt_Init` | 9, 0x5B80 |
| 3 | folha das legendas (`res[4]`) | `TexFile` | `HudCaption_Init` | 2, 0xCAC0 |
| 4 | folha dos avisos (`res[1]`) | `TexFile` | `HudNotice_Init` | 9, 0x42280 |
| 5 | folha do combo (`res[2]`) | `TexFile` | `HudCombo_Init` | 8, 0x122C0 |
| 6 | vazia | | ninguém | vazia nos dois idiomas |
| 7 | folha do menu de pausa | `TexFile` | `BtlMenu_Init`, `BtlMenu_UploadTex`, `BtlText_GetTex0` | 47, 0x18E80 |
| 8..37 (0x08..0x25) | textos dos itens do menu de pausa | texto u16 | `BtlMenu_GetItemText` (o índice vem das tabelas de menu do executável; a faixa é deduzida pela lacuna até 0x26) | 30 entradas |
| 38..197 (0x26..0xC5) | página de controles: 4 tipos de controle x 40 configurações | texto u16 | `BtlMenu_GetControlsText` | 160 entradas |

### 6.3 Folhas de sprites (entradas 1..5 e 7)

`TexFile`/`TexEntry` de personagem.md 4.4 (`entOfs` = 8 palavras nas 12 folhas dos dois idiomas, deslocamentos
em palavras relativos ao início da entrada), com as visões `HudRes`/`HudTex` (include/battle/hud_a.h:47, :40),
`HudCRes`/`HudCTex` (include/battle/hud_c.h:60, :44) e `BtlMenuTexFile`/`BtlMenuTex`
(include/battle/hud_0.h:102, :92). Uma diferença em relação ao modelo:

| Desl. em `TexEntry` | Modelo (4.4) | Aqui |
|---|---|---|
| 0x28 | u8[8] (`[4]` e `[5]` lidos como byte) | s32 `mark`: lido e gravado como s32 por `HudRes_UploadTex` (hud_c_c.c:169, :189) e zerado por `Hud_ClearTexMarks` (só folhas 1..5). A primeira leitura vê o valor do arquivo: zero nas 199 entradas dos dois idiomas |
| 0x2C | (idem) | s32 `unk2C`, sem leitor; zero em todas |

Tratar 0x28..0x2F destas folhas como 2 x u32 (como são zeros, a escolha não muda bytes). Os outros campos lidos:
`imageSize`/`clutSize` (+0x08, +0x0C), `imageWidth`/`clutWidth` (+0x18, +0x1C), `imageBlock`/`clutBlock`
(+0x20, +0x24), `tex0` (+0x30, u64, `HudSprite_InitTex` extrai largura e altura dos bits 26..33), `image`/`clut`
(+0x38, +0x3C, ponteiros da relocação). Pacotes de imagem e paleta: formato de personagem.md 4.4, conferidos
todos (nos dois idiomas). Formatos nos 227 `TexEntry` de `TexFile` dos três arquivos 00004..00006, mais as 2
fontes: 144 PSMT4, 53 PSMT8, 32 PSMCT32.

### 6.4 Textos (entradas 8..197)

Texto u16 terminado em 0, desenhado por `Font_Print` (`FONT_CMD_STR16`, com as marcas de ícone de col_c_b.c).
Conferido: as 190 entradas, nos dois idiomas, têm um 0 e zeros dali até o fim da entrada (tamanhos de 0x40 a
0x240). Converter cada u16 até o 0.

### 6.5 Escritas na carga

`Res_RelocateOffsets` nas folhas 1..5 (`Hud_Init`) e 7 (`BtlMenu_Init`): `TexFile.ent` (+0x10) e
`pix`/`clut` (+0x38, +0x3C); `mark` (+0x28) a cada quadro. O arquivo é lido de novo para um buffer novo a cada
luta, então a conversão em tempo de jogo seria uma por leitura; no instalador da decisão 12, uma vez no disco (1.3).

### 6.6 Cobertura conferida contra o rastreio

100% dos 701.184 bytes cobertos (701.632 no 00006), enchimento zero.

| Região | T | M | Largura igual |
|---|---|---|---|
| tabela | 6 (`u32 *[]`: `file[1..5]` e `file[7]`) | 4: 6 | todas |
| folha 1 | `TexFile` 2, `TexEntry` 50, `tex0` 18 | 4: 52; 8: 18 | todas |
| folha 2 | 2, 18, 4 | 4: 20; 8: 4 | todas |
| folha 3 | 2, 13 + `mark` 2, 2 | 4: 20; 8: 2 | todas |
| folha 4 | 2, 18, 3 | 4: 20; 8: 3 | todas |
| folha 5 | 2, 16, 2 | 4: 18; 8: 2 | todas |
| folha 7 | 2, 94 (só `pixOfs`/`clutOfs` da relocação) | 4: 96 | todas |
| textos 8..197 | 0 | 0 | sem leitura |

Os 2 "?" do catálogo são leituras de `mark` (+0x28 das entradas 0 e 1 da folha 3) atribuídas à linha
hud_c_c.c:170 (`t->mark = 1`), sem expressão no DWARF; largura 4 como o campo. `u32 *[]` são as palavras da
tabela. `HudRes.texCount` é o `count` de `TexFile`. Sem conflito.

Não exercitado: os textos, o conteúdo dos pacotes e quase todos os campos de upload fora da folha 3.

## 7. O que fica para decisão ou para o código

Correção de código (classe 4 do guia):
- `eft_m.c:1140`: `f->unk8 = gEftAuraCfg->flame[part].unk0` lê um u8 sobre o s32 `node` da entrada 1 do 00002
  (3.7.1). Precisa de `PS2_LOW(u8, ...)`; ainda não está feito nem em `visoes_resolvidas.tsv`. O mapa não o marca
  porque a leitura só aparece como cópia.

Decisões de conversão (com proposta):
- 00002, tipo 9, entrada a, 0x244..0x25F: palavras não nulas em 4 entradas, sem visão nem leitor. Proposta: u32.
  O conversor de personagem exige zero ali e falharia.
- 00002, entradas 0x23C..0x23E, sem leitor: proposta `TexFile` (0x23C) e u32 (0x23D, 0x23E).
- 00002, registro de parte excedente nos conjuntos 0x025, 0x066 e 0x073: delimitar pelo `nParts` do cabeçalho e
  tratar o excedente como `EftEmitDef` (zeros).
- 00003, `ChrTblEntry.unk38` (declarado `u8[4]`, sem leitor, com cara de f32 nos 161 registros): proposta u32.
- 00002, `EftStreakDef.unk1C` (declarado `u8[4]`, sem leitor, com cara de f32): proposta u32.
- 00001, `unk0` da tabela de ações da IA (s32 nas visões, sem leitor): u32.

Diferenças de formato que o conversor tem de respeitar ao reaproveitar peças de `personagem.md`:
- `TexEntry` + 0x28..0x2F: `u8[8]` no modelo de personagem, mas s32 `mark` + s32 nas folhas do HUD (00005). São
  zero em todas as folhas, então a largura não muda bytes; a validação não pode exigir o padrão de personagem.
- `FontTex` (00004): o layout de `TexEntry`, mas `pixels`/`clut` são deslocamentos em BYTES a partir da `FontData`,
  não em palavras a partir do `TexFile`. O percurso de `TexFile` não serve como está.
- `AiThProfile` diz 0x584 bytes; os perfis do arquivo têm 0x580 (00001, 2.6.4). Delimitar pelos `size` do
  cabeçalho, não pelo `sizeof`.

Correções de visão nos headers da decomp, que não mudam a conversão (o código não foi alterado):
`EftGlowCfg.colorTex` tem 5 linhas, não 4; `EftAuraData.unk194` são f32; `AiThProfile` tem 0x580 bytes.

## 8. Cobertura conferida contra o rastreio (resumo)

Método de `personagem.md` 10: no WSL, cada arquivo foi percorrido como esta especificação diz, cada byte ganhou
uma largura ou "enchimento", e cada posição de `tipos.tsv` e de `mapa.tsv` foi comparada com ela. O detalhe por
entrada está na última subseção de cada arquivo.

| Arquivo | Bytes cobertos | Enchimento | T (iguais / total) | M (iguais / total) | Conflitos |
|---|---|---|---|---|---|
| 00001 | 100% | todo zero | 2.702 / 2.702 | 939 / 939 | nenhum |
| 00002 | 100% (com as propostas da seção 7) | todo zero | 6.896 / 6.899 | 6.930 / 6.930 | 3, todos `TexEntry.tex0`, explicados em 3.9 (u64) |
| 00003 | 100% | todo zero | 351 / 351 | 353 / 353 | nenhum |
| 00004 | 100% | todo zero | 69 / 69 | 73 / 73 | nenhum |
| 00005 | 100% (e 00006) | todo zero | 258 / 258 | 261 / 261 | nenhum |

Os "?" do catálogo (`docs/x360/medicoes/catalogo-luta-demo.txt`) são leituras cuja coluna no DWARF não cai numa
expressão que `tipos.py` reconhece: quase sempre ponteiro calculado (bytes das regras e perfis da IA, chaves de
partícula, rotinas de vetor do port) ou local perdido na otimização. Todas essas posições foram atribuídas a um
campo desta especificação com a mesma largura (2.17, 3.11, 4.5, 5.7, 6.6). Os `u32 *[]` e `s32 *[]` são, em todos
os arquivos, as palavras da tabela de deslocamentos (e, no 00001, as flags por animação).

O que a demo não exercita: o 00003 quase todo (os leitores são do menu); os textos e glifos do 00004 e do 00005;
no 00001, as entradas 12, 13 e 14..213 (ligadas só na troca de membro) e os campos de 2 e 4 bytes da entrada 7;
no 00002, 416 das 637 entradas (partes não disparadas, rastros, tipos 2 e 10, 0x23C..0x23E); e, em todos, o
conteúdo dos pacotes de imagem e paleta (vão por DMA).
