# As telas do Ultimate Battle (F4, etapa 9b)

Continuação de `menus-telas.md` (o caminho essencial), `menus-pecas.md` (as peças comuns), `menus-historia.md` (a
História) e `menus-modos.md` (o inventário das 82 chamadas de `Sprite_Unpack`). Aqui ficam os pacotes de tela do
Ultimate Battle (modos 13 a 30 do gerenciador de menus, `Ub_Main`, src/menu/menu_o_c.c): o pacote 00451 (`gMenuArc3`,
`baseFile + 3`, seis seções BPE), o tabuleiro do Sim Dragon (00474), o ranking (00475) e os cursos (00476) do Disc
Fusion, e os retratos que essas telas carregam (`menu_texfile`, já especificado em `menus-telas.md` 6). Conversores
em `port/x360/conv/menus_ub.c`, uma parte por grupo de telas, registrados em `sFormatos` e `sFaixas` de `conv.c`.

Escrita pela linha de produção da guia (seção 4): cinco especificações em paralelo, cada uma do código da decomp, com
o disco inteiro como conferência (`conv_tool --disco` no WSL; só números voltaram) e cada conversor visto falhando
sobre cópias estragadas. As partes:

| Parte | Formatos | Conteúdo | Telas (modo) |
| --- | --- | --- | --- |
| A | `menu_ub_menu`, `menu_sim_inicio`, `menu_discfusion` | 451 s1, s5, s6 | UbMenu (13), SimTop (20), DiscFusion (24) |
| B | `menu_ub_selecao`, `menu_ub_resultado` | 451 s2, s3 | MisSel (14), SurvSel (17); MisResult (16), SurvResult (19), SimResult (23), UbResult (27, 30) |
| C | `menu_ubsel` (e os retratos dela) | 451 s4 | SoloSel (15, 18, 21, 25, 29), UbTeamSel (15) |
| D | `menu_sim_dia` (e os retratos dela) | 474 inteiro | SimDay (22) |
| E | `menu_ub_ranking`, `menu_ub_cursos` (e os retratos do ranking) | 475 e 476 inteiros | UbRank (26), UbzSel (28) |

Regra da conversão (decisões 8 e 12) como nas outras: in-place, cada número pela largura com que o código o lê, bytes
e strings intactos, enchimento exigido zero; cada `TexFile` com o mínimo de registros que o leitor usa; o N de cada
tabela exato; seção sem leitor convertida pela forma, sempre dito. Nenhum byte do jogo aqui. **Hipótese** marca o que o
código não prova. Duas mudanças fora dos pacotes vieram daqui (parte D, 6): três códigos de ação do Flash que o jogo
lê como um byte, e o `CONV_JOGO_MAX` de 8 MB para 9 MB (o 00474 descomprimido tem 0x81C580 bytes).

## Parte A: o menu, o topo do Sim Dragon e o Disc Fusion

Parte da especificação do Ultimate Battle (modos 13 a 30 do gerenciador de menus, `Ub_Main`, src/menu/menu_o_c.c:56):
as três telas que o agente ubA cobre, as seções 1, 5 e 6 do pacote 00451. As seções 2, 3 e 4 e os arquivos 474..476
são dos outros agentes. Continuação de `menus-pecas.md` (as peças), `menus-telas.md` (as regras do `TexFile` nos
menus) e `menus-historia.md` (o modelo). Conversores em `port/x360/conv/menus_ub_ubA.c` (`Conv_MenuUbMenu`,
`Conv_MenuSimInicio`, `Conv_MenuDiscFusion`), registrados em `sFormatos` e `sFaixas` de `conv.c`.

Regra da conversão (decisões 8 e 12): in-place, cada número trocado pela largura com que o código o lê, bytes e
strings intactos, enchimento exigido zero, nada muda de tamanho nem de lugar. Cada `TexFile` leva o mínimo de registros
que o leitor usa (1 + o maior índice de `MTEX`; 8 num fundo desenhado por `Sprite_DrawPicture`); o N de cada tabela é
exato (`Conv_Secoes`).

Fontes: o código da decomp (arquivo e linha, as de antes dos ganchos: cada gancho desloca as linhas seguintes do
arquivo em 6); o disco inteiro pelo `conv_tool --disco` e contagens de estrutura no WSL (só números voltaram). Nenhum
byte do jogo está aqui. **Hipótese** marca o que o código não prova.

### 1. O arquivo e quem chama

| Arquivo | Id (como o código calcula) | Índice | Forma | Formato do arquivo | Conteúdo (destas três telas) |
|---|---|---|---|---|---|
| `gMenuArc3` | `UO_PROG->baseFile + 3` = 0x1C4 (`Ub_Main`, menu_o_c.c:63) | 451 | pacote, 6 seções BPE | `menu_pacote` | s1 `menu_ub_menu`, s5 `menu_sim_inicio`, s6 `menu_discfusion` |

O arquivo tem 8.374.016 bytes; a tabela dele, N = 6. `Ub_Main` o carrega uma vez (`File_LoadSync`) e o libera quando
volta ao menu principal ou começa uma luta; cada tela descomprime a sua seção num buffer novo (`Sprite_Unpack(src,
NULL, NULL)`), liberado no `_Term` dela.

| Chamada | Conteúdo | Formato | Tela |
|---|---|---|---|
| `UbMenu_Init` src/menu/menu_p_c.c:70 | 451 s1 (`UbMenu_Run(1)`, menu_o_c.c:68) | `menu_ub_menu` | modo 13 |
| `SimTop_Init` src/menu/menu_r_d.c:53 | 451 s5 (`SimTop_Run(5)`, menu_o_c.c:150) | `menu_sim_inicio` | modo 20 |
| `DiscFusion_Init` src/menu/menu_o_b.c:63 | 451 s6 (`DiscFusion_Run(6)`, menu_o_c.c:186) | `menu_discfusion` | modo 24 |

Cada `_Run` só é chamado de `Ub_Main`, sempre com a mesma seção: um formato por chamada.

### 2. O menu do Ultimate Battle (`menu_ub_menu`: 00451, seção 1)

`UbMenu_Init(1)` (src/menu/menu_p_c.c:63, modo 13): quatro placas (Sim Dragon, Missão 100, Sobrevivência, Disc
Fusion) e as duas guias (C17 e C18 pelos nomes dos clipes). Tabela: N = 9 (o código lê 1..9; o disco confirma).
Conteúdo descomprimido: 1.231.680 bytes.

| Seção | Bytes | Formato | Registros mínimos (disco) | Quem lê |
|---|---|---|---|---|
| 1 | 20.992 | Flash | | `Flash_Create(&flash[0], MPACK_AT(res, 1), tex)` (:92) |
| 2 | 272.960 | `TexFile`, o fundo | 8 (8) | `UM_RES(2)`, `gUbMenu->bg` (:71); `Sprite_DrawPicture` (`UbMenu_Draw`, :137) |
| 3 | 348.352 | `TexFile` | 11 (11) | `UM_RES(3)`, `MTEX(res, 0..10)` (:80..:91) |
| 4 | 39.616 | `TexFile`, os ícones da janela de ícone | 2 (2) | `UM_RES(4)` (:99), `IconWin_Init(pack 7, res)` (:100): registros 0 e 1 (view_a_c.c:35, :36) |
| 5 | 296.128 | `TexFile` | 9 (10) | `UM_RES(5)`, `MTEX(res, 0, 1, 3, 5, 6, 8)` (:73..:79) |
| 6 | 135.808 | JanelaMsg (N = 2) | | `MsgWin_Init(MPACK_AT(res, 6), text, 0, 0)` (:102) |
| 7 | 91.520 | JanelaIcone (N = 2) | | `IconWin_Init(MPACK_AT(res, 7), res)` (:100) |
| 8 | 10.624 | textos | 17 linhas (88) | `gUbMenu->text` (:97), a tabela da janela de mensagem: `MsgWin_Draw(0, 0, voiceLine)` (:172), fala 0..16 |
| 9 | 15.616 | LipPack | 17 falas, 34 entradas (176) | `gUbMenu->subtitles` (:98): `Voice_PlayWithSubtitle` com a fala 0..16 (`UbMenu_UpdateVoice` :230..:288, `UM_SAY_PLATE` 2..5) |

Todas as seções têm leitor.

### 3. O topo do Sim Dragon (`menu_sim_inicio`: 00451, seção 5)

`SimTop_Init(5)` (src/menu/menu_r_d.c:46, modo 20): três placas (começar, ranking, como jogar), o ranking de dez linhas
com retrato e o texto de "como jogar". Tabela: N = 9 (o código lê 1..9; o disco confirma). Conteúdo: 2.527.232 bytes.

| Seção | Bytes | Formato | Registros mínimos (disco) | Quem lê |
|---|---|---|---|---|
| 1 | 39.424 | Flash | | `Flash_Create(&flash[0], MPACK_AT(res, 1), tex)` (:111) |
| 2 | 824.768 | `TexFile` | 30 (30) | `MTEX(res, 0..29)` (:65..:103) |
| 3 | 39.616 | `TexFile`, os ícones da janela de ícone | 2 (2) | relocada em :114, `IconWin_Init(pack 6, res)` (:116): registros 0 e 1 |
| 4 | 272.960 | `TexFile`, o fundo | 8 (8) | `gSimTop->bg` (:55..:57); `Sprite_DrawPicture` (`SimTop_Draw`, :178) |
| 5 | 296.128 | `TexFile` | 9 (10) | `MTEX(res, 5, 6, 8)` (:59..:63) |
| 6 | 91.520 | JanelaIcone (N = 2) | | `IconWin_Init(MPACK_AT(res, 6), res)` (:116) |
| 7 | 908.864 | SubpacoteTex, 165 subseções (`Conv_FichasPersonagem`) | registro 0 de cada | `gSimTop->faces` (:105); o laço reloca `MPACK_AT(faces, i + 1)`, i = 0..164 (`SIMTOP_FACE_NUM`, :106..:109); `SimTop_Draw` lê `face->tex` de `MPACK_AT(faces, chara + 1)` (:248, :252) |
| 8 | 38.272 | textos | 11 linhas (350) | `gSimTop->text` (:119), `TextBox_Init` (:121); `TextBox_AttachLine(..., page, ...)` (:277), página 0..10 (`SIMTOP_HELP_PAGES`, `SimTop_Input` :447) |
| 9 | 15.616 | LipPack | 22 falas, 44 entradas (176) | `gSimTop->subtitles` (:120): `Voice_PlayWithSubtitle` com a fala 0x11..0x15 (`SimTop_UpdateVoice`, :334..:352) |

Todas as seções têm leitor.

### 4. O Disc Fusion (`menu_discfusion`: 00451, seção 6)

`DiscFusion_Init(6)` (src/menu/menu_o_b.c:56, modo 24): duas placas, uma por disco anterior (Budokai Tenkaichi 1 e 2),
a guia (C17), a janela de mensagem e o diálogo da troca de disco. Tabela: N = 12 (o código lê 1..12; o disco
confirma). Conteúdo: 1.568.128 bytes.

| Seção | Bytes | Formato | Registros mínimos (disco) | Quem lê |
|---|---|---|---|---|
| 1 | 21.056 | Flash, a tela | | `Flash_Create(&flash[0], MPACK_AT(res, 1), texA)` (:90) |
| 2 | 7.808 | Flash, "disco reconhecido" | | `Flash_Create(&flash[1], MPACK_AT(res, 2), texB)` (:92); `fl_start` em `DiscFusion_UpdateDisc` (:317) |
| 3 | 557.312 | `TexFile` | 19 (19) | `DF_RES(3)`, `MTEX(res, 0..18)` (:70..:89; texA e texB) |
| 4 | 39.616 | `TexFile`, os ícones da janela de ícone | 2 (2) | `DF_RES(4)` (:99), `IconWin_Init(pack 6, res)` (:100): registros 0 e 1 |
| 5 | 81.344 | arquivo de diálogo (`menus.md` 6, N = 6) | | `Dialog_Init(MPACK_AT(res, 5), text, 0)` (:105) |
| 6 | 91.520 | JanelaIcone (N = 2) | | `IconWin_Init(MPACK_AT(res, 6), res)` (:100) |
| 7 | 135.808 | JanelaMsg (N = 2) | | `MsgWin_Init(MPACK_AT(res, 7), msgText, 0, ...)` (:102) |
| 8 | 272.960 | `TexFile`, o fundo | 8 (8) | `DF_RES(8)`, `gDiscFusion->bg` (:64); `Sprite_DrawPicture` (`DiscFusion_Draw`, :146) |
| 9 | 296.128 | `TexFile` | 4 (10) | `DF_RES(9)`, `MTEX(res, 0, 1, 3)` (:66..:69) |
| 10 | 10.624 | textos | 0x54 = 84 linhas (88) | `msgText` (:97), da janela de mensagem: `MsgWin_Draw(0, 0, voiceLine)` (:192), fala 0x40..0x53 |
| 11 | 15.616 | LipPack | 0x54 falas, 168 entradas (176) | `subtitles` (:98): `Voice_PlayWithSubtitle` com a fala 0x40..0x53 (`DiscFusion_UpdateTalk`, :450..:486) |
| 12 | 38.272 | textos | 0x15E = 350 linhas (350) | `text` (:104): o diálogo (`Dialog_SetMsg` 0x14..0x1F, `DiscFusion_UpdateDisc` :246..:401) e as caixas das placas (`TextBox_AttachLine` com 0x15A..0x15D, `DiscFusion_Draw` :156, :158) |

Todas as seções têm leitor. A seção 12 tem exatamente as 350 linhas que o leitor usa (a mais alta, 0x15D, é a
última da tabela).

### 5. Os limites que o conversor confere

Nenhuma peça nova: as seções são as peças de `menus-pecas.md` e `menus.md`. Além do mínimo de registros de cada
`TexFile` (`Conv_TexFileMin`), o conversor confere, antes de converter a peça, os dois limites que a tela usa e que
as peças sozinhas não sabem:

- **textos** (`ubA_textos`): a contagem da tabela (`menus.md` 6.1, `text[0]`, que ninguém lê) tem de cobrir a linha
  mais alta que a tela pede (`TextBox_AttachLine` lê `text[linha + 1]`, src/battle/view_b.c:243; o diálogo,
  `msgTbl[msgIdx]`): 17 no menu, 11 no topo do Sim Dragon, 84 e 350 no Disc Fusion. Uma linha além da contagem
  apontaria para dentro do texto da primeira linha.
- **legendas** (`ubA_legendas`): a tabela do LipPack tem de ter duas entradas por fala até a mais alta que a tela toca
  (`pack[fala * 2 + 1]` e `pack[fala * 2 + 2]`, view_b_c.c:38, :43): 34, 44 e 168 entradas.

As três tabelas de legendas (15.616 bytes, 176 entradas), as duas de textos de 350 linhas (38.272 bytes) e os
`TexFile` de 296.128 e 272.960 bytes têm o mesmo tamanho nas telas que os usam: **hipótese**, que sejam cópias das
mesmas seções (só os tamanhos foram comparados). Cada tela usa um subconjunto das linhas; o conversor converte a tabela
inteira em todas.

O registro mínimo dos ícones da janela de ícone é 2 (os registros 0 e 1: `IconWin_Init` guarda `icons->tex` e
`icons->tex + FLASH_TEX_SIZE`, view_a_c.c:35..:36, e nenhuma destas três telas chama `IconWin_SetIcon`); o disco tem 2
nas três.

### 6. O que cada tela faz com a entrada (para os roteiros de controle)

Os três `_Input` só leem o controle 0 quando o filme da tela chegou ao estado de entrada (`MFLASH_PAD`), sem fala da
guia pendente e sem saída em curso. X durante uma fala da guia a corta (`gamePressed` 0x200 nos `_UpdateVoice` /
`_UpdateTalk`). CIMA e BAIXO usam `gameRepeat` (repetem segurando); X e TRIÂNGULO, `gamePressed`.

#### 6.1 O menu (modo 13, `UbMenu_Input`, menu_p_c.c:301)

- Na primeira leitura, a guia cumprimenta (`script` 1 ou 11, ao acaso); enquanto um roteiro de falas roda
  (`script != 0`), a entrada fica parada (`UbMenu_Run`, :416), e cada X corta uma fala.
- **CIMA / BAIXO**: o cursor anda pelas quatro placas, dando a volta; a placa 3 (índice 2, Sobrevivência) é pulada
  enquanto `P_SAVE->ubFlags & P_UBFLAG_ITEM3` for zero (30 missões, menu_p_b.c:160). Cada passo toca o comentário da
  guia (fala cursor + 2).
- **X**: escolhe a placa (`result` = cursor + 1), 15 quadros e o escurecimento; o cursor fica em `P_PROG->ubCursor`
  (é onde a próxima visita começa). `Ub_Main` (menu_o_c.c:69..:83): placa 1 (cursor 0) vai ao **modo 20** (Sim Dragon,
  `SimTop`), 2 ao **14** (Missão 100, `MisSel`), 3 ao **17** (Sobrevivência, `SurvSel`), 4 ao **24** (Disc Fusion).
- **TRIÂNGULO**: `result` = 0, a guia diz uma despedida (uma de três, ao acaso), depois o escurecimento; `Ub_Main` volta
  ao **modo 4** (menu principal) e sai.
- Parado 1800 quadros: um diálogo das guias (bloqueia a entrada até acabar; X corta).

#### 6.2 O topo do Sim Dragon (modo 20, `SimTop_Input`, menu_r_d.c:360)

- `SimTop_Init` zera a corrida do Sim Dragon em `gProgress` (nível, ataque, defesa, vida 100, pontos, itens, turno,
  `teamSize` = 1, `dpRule` = 0; :130..:143).
- Nível 0 (as três placas): **CIMA / BAIXO** andam com volta (0 começar, 1 ranking, 2 como jogar), com a fala da placa.
  **X** na placa 0: 15 quadros e o escurecimento, `result` 1: **modo 21** (`SoloSel(4)`, o personagem) e depois o 22
  (o tabuleiro, `SimDay`). **X** na placa 1: o ranking (nível 1). **X** na placa 2: "como jogar" (nível 2, página 0).
  **TRIÂNGULO**: escurecimento na hora, `result` 0, volta ao **modo 13**.
- Nível 1 (ranking): **CIMA / BAIXO** rolam a lista (topo 0..5, as dez linhas mostradas de cinco em cinco);
  **TRIÂNGULO** volta ao nível 0.
- Nível 2 (como jogar): **X** ou **DIREITA** passam a página (0..10); na página 10, mais um fecha e volta ao nível 0;
  **TRIÂNGULO** fecha.
- Parado 0xE10 quadros: uma fala da guia.

#### 6.3 O Disc Fusion (modo 24, `DiscFusion_Input`, menu_o_b.c:498)

- A entrada só é lida sem troca de disco em curso (`dlgStep` == 0) e sem fala pedida (`talkStep` == 0). Na primeira
  leitura, a guia cumprimenta (fala 0x41 se os dois discos já foram reconhecidos, senão 0x40).
- **CIMA / BAIXO**: alternam as duas placas (disco 0 e disco 1), com a fala da placa (0x42 / 0x52, 0x43 / 0x53, pelo
  "reconhecido").
- **TRIÂNGULO**: escurecimento, `result` 0, volta ao **modo 13**.
- **X** numa placa cujo disco **já foi reconhecido** (`have[cursor]`, a cópia de `UO_PROG->discFlags` bit 0 ou 1):
  15 quadros e o escurecimento, `UO_PROG->cursor` = a placa, `result` 1. `Ub_Main` (menu_o_c.c:186..:196): placa 0
  vai ao **modo 25** (`teamSize` = 1, `dpRule` = 0; `SoloSel(4)`, e dele ao 26, `UbRank`, que lê o 00475); placa 1 vai
  ao **modo 28** (`UbzSel`, que lê o 00476).
- **X** numa placa cujo disco **não foi reconhecido**: começa a troca de disco (`dlgStep` = `DISCFUSION_DLG_ASK`,
  `DiscFusion_UpdateDisc`, :220). É o diálogo da seção 5: um arquivo de diálogo com a mesma forma do diálogo do cartão
  (`Dialog_Init`, src/sys/dialog.c:237), por isso o inventário o chamou de "diálogo do cartão"; o conteúdo, porém, é a
  troca de disco, e o cartão de memória não entra. O passo a passo:
  1. ASK/CONFIRM: mensagem 0x14 + placa com duas escolhas, o cursor na segunda (`Dialog_SetCursor(1)`). X na segunda
     (`Dialog_Input` devolve -2) fecha o diálogo (CLOSE) e a guia diz a fala 0x48: de volta às placas. CIMA ou BAIXO
     leva à primeira; X nela (1) segue. TRIÂNGULO não faz nada aqui (`Dialog_Input(0)`, sem cancelar).
  2. OPEN: mensagem 0x16 ("abra a bandeja", pelo fluxo), espera `Disc_GetDriveState()` == `DISC_TRAY_OPEN`.
  3. INSERT: mensagem 0x17 + placa, espera `DISC_STOPPED` (o outro disco fechado na gaveta).
  4. READ: com a unidade em pausa, `Disc_Identify` (src/sys/file.c:176) procura o executável de cada jogo
     (`sceCdSearchFile` sobre `gDiscExeNameTbl[0..2]`); o da placa leva a FOUND; 0x40 tentativas ou 0xF0 quadros sem
     tipo de disco levam a WRONG (mensagens 0x1A / 0x1E, com escolha).
  5. FOUND: **grava `UO_PROG->discFlags |= UB_DISC_0` (placa 0) ou `UB_DISC_1` (placa 1)** (:311..:315), toca o filme
     da seção 2, pede o disco do BT3 de volta (0x1B, 0x1C), lê até achar o próprio (`DISCFUSION_ID_OWN`), fecha.
  Só depois disso o X naquela placa leva aos modos 25 ou 28.

**O que o jogo precisa para seguir aos modos 25 e 28 (`UbRank` e `UbzSel`):** o bit 0 (modo 25, e dele ao 26, o
`UbRank`) ou o bit 1 (modo 28, o `UbzSel`) de `gProgress->discFlags` (`UoProgress` +0x684, include/menu/menu_o.h:92).
Nenhum outro código grava esses bits (só o passo FOUND), eles não estão no save (estão em `gProgress`), e
`Progress_ClearSession` (src/battle/view_a_e.c:58, chamado por `Progress_Init`, pelos `Save_*` e na volta ao título)
os zera: o `unk634[0x58]` de `ViewProgress` (include/battle/view_a.h:283) cobre 0x634..0x68B, onde estão o
`discFlags` (0x684) e o `ubCursor` (0x638).

No port a troca de disco não pode terminar: a unidade de mentira responde sempre "disco de PS2, em pausa"
(`sceCdGetDiskTypeSafe` = 0x14, `sceCdStatus` = 10, port/src/plat_file.c:239..:240), então o passo OPEN espera para
sempre a bandeja abrir, sem saída pelo controle (o OPEN não lê o diálogo, e `DiscFusion_Input` não roda com
`dlgStep` != 0): o jogo fica preso no diálogo. E, mesmo que passasse, `sceCdSearchFile` não é definido por nenhum
arquivo do port (port/src, port/x360): vira stub gerado pelo link, que encerra o programa com o nome
("not implemented yet" no PC, "não implementado no 360" no console; **hipótese**: não conferido num `stubs.txt` de
build). Para os roteiros alcançarem `UbRank_Init`, `UbRank_UpdateFaceLoad` e `UbzSel_Init`, o caminho é pôr os bits
em `discFlags` por fora (uma opção do port, como o atalho de terminar luta, ou um passo do roteiro que grave
`gProgress` +0x684 = 3), depois do último `Progress_ClearSession` (na entrada do modo 24, por exemplo), e não por
emular a unidade. Nos roteiros de hoje, o X no Disc Fusion é para evitar (com o cursor na segunda escolha ele só abre
e fecha o diálogo; na primeira, trava).

### 7. Escritas do jogo nos buffers depois da conversão

- `Res_RelocateOffsets` grava os ponteiros de cada `TexFile` relocado (`menus.md` 9): as seções 2..5 do menu; 2..5 e as
  165 subseções da 7 do topo do Sim Dragon; 3, 4, 8, 9 do Disc Fusion; e as partes 1..4 do diálogo (`Dialog_Init`) e a
  seção 1 das janelas.
- Nada mais. As outras escritas das três telas vão às estruturas delas (`gUbMenu`, `gSimTop`, `gDiscFusion`, como
  `gSimTop->tex[...] = face->tex`), a `gProgress` (`ubCursor`, a corrida do Sim Dragon, `cursor`, `discFlags`) e ao
  save (nenhuma destas três grava no save).

### 8. Cobertura conferida

#### 8.1 Contra o disco (`conv_tool --disco`)

Sem erro e sem aviso C89 (`gcc -std=c89 -pedantic -Wdeclaration-after-statement -Wall`):

| Conteúdo | Formato | Bytes | Cobertura | Em números trocados |
|---|---|---|---|---|
| 00451 (o arquivo) | `menu_pacote` | 8.374.016 | 100% | 112 |
| 451 s1 (`@0x40#bpe`) | `menu_ub_menu` | 1.231.680 | 100% | 50.862 |
| 451 s5 (`@0x5B1C00#bpe`) | `menu_sim_inicio` | 2.527.232 | 100% | 145.700 |
| 451 s6 (`@0x7438C0#bpe`) | `menu_discfusion` | 1.568.128 | 100% | 111.182 |

Os três conteúdos convertem igual pelos dois caminhos (o da `conv_tool` e o do jogo, `Conv_FormatoBits`). Antes e
depois, no `--disco` inteiro:

| | Antes | Depois |
|---|---|---|
| arquivos convertidos sem erro | 3.295 | 3.296 |
| conteúdos descomprimidos sem erro | 675 | 678 |
| censo dos menus (448..477): arquivos com conversor | 15 de 30 | 16 de 30 |
| censo dos menus: contêineres BPE com conversor | 22 de 49 | 25 de 49 |
| 00451 | arquivo sem conversor, 6 de 6 contêineres sem | arquivo convertido, 3 de 6 sem (as seções 2, 3 e 4) |

Os já aprovados continuam aprovados (nenhuma linha de erro). O `unpacks.py`, numa árvore temporária com os ganchos:
`82 chamadas de Sprite_Unpack em 62 funções: bpe-direto 1, com gancho 34, pendente 48; 0 com erro` (eram 31 com
gancho e 51 pendentes); as três chamadas daqui saem com gancho.

#### 8.2 Os conversores vistos falhando

No WSL, sobre cópias do conteúdo antes da conversão (o `.orig` do `conv_tool --marcas`), cada uma estragada num ponto e
convertida com `--formato`. As três cópias intactas saem com 0; cada estragada sai com 1 e o nome da seção:

| Formato | Estrago | Mensagem |
|---|---|---|
| `menu_ub_menu` | o `TexFile` da seção 3 com 10 registros | "10 registros, o leitor usa 11 [seção 3 do menu do Ultimate Battle]" |
| `menu_ub_menu` | os ícones (seção 4) com 1 registro | "1 registros, o leitor usa 2 [seção 4 do menu do Ultimate Battle]" |
| `menu_ub_menu` | os textos da seção 8 com 16 linhas | "16 linhas, o leitor usa 17 [seção 8 do menu do Ultimate Battle]" |
| `menu_ub_menu` | as legendas da seção 9 com 32 entradas | "32 entradas, a tela toca 17 falas [seção 9 do menu do Ultimate Battle]" |
| `menu_sim_inicio` | o `TexFile` da seção 2 com 29 registros | "29 registros, o leitor usa 30 [seção 2 do topo do Sim Dragon]" |
| `menu_sim_inicio` | os textos da seção 8 com 10 linhas | "10 linhas, o leitor usa 11 [seção 8 do topo do Sim Dragon]" |
| `menu_sim_inicio` | as legendas com 42 entradas | "42 entradas, a tela toca 22 falas [seção 9 do topo do Sim Dragon]" |
| `menu_sim_inicio` | um byte não nulo no enchimento (+0xC) do primeiro bloco de boca | "boca: byte ... não reconhecido (não é enchimento) [legenda 1] [seção 9 do topo do Sim Dragon]" |
| `menu_discfusion` | a tabela com 11 seções | "Conv_Tabela: fim ..., arquivo ... [Disc Fusion]" |
| `menu_discfusion` | o `TexFile` da seção 3 com 18 registros | "18 registros, o leitor usa 19 [seção 3 do Disc Fusion]" |
| `menu_discfusion` | o diálogo (seção 5) com 5 partes | "Conv_Tabela: fim ..., arquivo ... [diálogo] [seção 5 do Disc Fusion]" |
| `menu_discfusion` | os textos da seção 10 com 83 linhas | "83 linhas, o leitor usa 84 [seção 10 do Disc Fusion]" |
| `menu_discfusion` | as legendas com 166 entradas | "166 entradas, a tela toca 84 falas [seção 11 do Disc Fusion]" |
| `menu_discfusion` | os textos da seção 12 com 349 linhas | "349 linhas, o leitor usa 350 [seção 12 do Disc Fusion]" |

### 9. Hipóteses e o que fica aberto

- Que as seções de mesmo tamanho nas três telas (legendas, textos de 350 linhas, os `TexFile` de 296.128 e 272.960
  bytes) sejam cópias das mesmas (5); não muda a conversão.
- Que `sceCdSearchFile` vire stub que encerra o programa no port (6.3): o código do port não o define, mas não conferi
  a lista de stubs de uma build.
- O mínimo 2 nos ícones da janela de ícone segue o leitor (`IconWin_Init` aponta o registro 1); a seleção de saga da
  História (`menus-historia.md` 2, seção 11) usou 1 para o mesmo papel. Vale alinhar as duas na integração.

## Parte B: a seleção e os resultados

Seções para a especificação do Ultimate Battle (o modelo é `menus-historia.md`): as seções 2 e 3 do pacote do grupo de
modos 13..30, lidas por seis chamadas de `Sprite_Unpack`. Conversor em `port/x360/conv/menus_ub_ubB.c`
(`Conv_MenuUbSelecao`, `Conv_MenuUbResultado`), registrado em `sFormatos` e `sFaixas` de `conv.c`. Nenhuma peça nova:
só as de `conv_menus.h` e quatro tabelas de registros (missões, adversários, cursos, preços) e uma lista de ids.

Regra da conversão (decisões 8 e 12): in-place, cada número trocado pela largura com que o código o lê, bytes e
strings intactos, enchimento exigido zero, nada muda de tamanho nem de lugar; cada `TexFile` com o mínimo de registros
que o leitor usa (1 + o maior índice de `MTEX`; 8 num fundo do `Sprite_DrawPicture`); N exato (`Conv_Secoes`); seção
sem leitor convertida pela forma. Fontes: o código da decomp (as linhas são as de antes dos ganchos); o disco pelo
`conv_tool --disco` e contagens de estrutura no WSL (só números voltaram). **Hipótese** marca o que o código não prova.

### 1. O arquivo e quem chama

O pacote é `gMenuArc3`, `baseFile + 3` = 0x1C4 (`Ub_Main`, src/menu/menu_o_c.c:63), índice 451 (`pzs3us1/00451.bin`),
uma tabela de 6 seções BPE (o arquivo inteiro é `menu_pacote`, linha do agente ubA). As seções 2 e 3:

| Chamada | Modo (`Ub_Main`) | Conteúdo | Formato |
|---|---|---|---|
| `MisSel_Init` src/menu/menu_o_d.c:146 | 14, seleção de missão da Missão 100 (`MisSel_Run(2)`, menu_o_c.c:93) | 451 s2 | `menu_ub_selecao` |
| `SurvSel_Init` src/menu/menu_s_b.c:93 | 17, seleção de curso da Sobrevivência (`SurvSel_Run(2)`, :124) | 451 s2 | `menu_ub_selecao` |
| `MisResult_Init` src/menu/menu_p_b.c:67 | 16, resultado da missão (`MisResult_Run(3)`, :118) | 451 s3 | `menu_ub_resultado` |
| `SurvResult_Init` src/menu/menu_s_c.c:30 | 19, resultado da Sobrevivência (`SurvResult_Run(3)`, :144) | 451 s3 | `menu_ub_resultado` |
| `SimResult_Init` src/menu/menu_r_c.c:38 | 23, resultado do Sim Dragon (`SimResult_Run(3)`, :176) | 451 s3 | `menu_ub_resultado` |
| `UbResult_Init` src/menu/menu_n_d.c:53 | 27 e 30, resultado do Disc Fusion (`UbResult_Run(3)`, :220 e :246) | 451 s3 | `menu_ub_resultado` |

Cada chamada descomprime num buffer novo (`Sprite_Unpack(pack, NULL, NULL)`), liberado no `_Term` da tela: converter
logo depois de cada uma não converte duas vezes. As duas seleções leem o mesmo conteúdo, cada uma as suas seções; os
quatro resultados também. Cada conversor cobre a união.

### 2. A seleção (`menu_ub_selecao`: 00451, seção 2)

Tabela: N = 18 (o disco confirma; a maior seção lida é a 18, pelas duas telas). Conteúdo descomprimido: 2.700.480 bytes.

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | Flash | | MisSel: `Flash_Create` (menu_o_d.c:196) |
| 2 | Flash | | SurvSel: `Flash_Create` (menu_s_b.c:130) |
| 3 | `TexFile` | 28 | MisSel: `MS_RES(3)`, registros 0..27 (:155..:183); SurvSel: `SS_RES(3)`, 0, 1, 5..8, 10..23, 25..27 (:103..:126). No disco, 29 |
| 4 | `TexFile`, o fundo | 8 | as duas: `bg`, `Sprite_DrawPicture` (`MisSel_Draw`, `SurvSel_Draw`) |
| 5 | `TexFile` | 4 | as duas: 0, 1, 3 (:151, :99). No disco, 10 |
| 6 | sem leitor: a forma de janela de mensagem (N = 2, `TexFile` e Flash) | | ninguém; `Conv_JanelaMsg` pela forma |
| 7 | JanelaIcone | | as duas: `IconWin_Init(MPACK_AT(res, 7), ...)` (:200, :134) |
| 8 | `TexFile`, os ícones da janela de ícone | 2 | MisSel: `MS_RES(8)` (:199); `IconWin_Init` lê os registros 0 e 1 (sem `IconWin_SetIcon`) |
| 9 | `TexFile`, os ícones da janela de ícone | 2 | SurvSel: `SS_RES(9)` (:133); registros 0 e 1 |
| 10 | missões (2.1) | 100 | MisSel: `missions` (:147) |
| 11 | adversários das missões (2.2) | 1 (cada índice das missões dentro) | MisSel: `opps` (:148) |
| 12 | cursos (2.3) | 3 | SurvSel: `courses` (:94) |
| 13 | adversários da Sobrevivência (2.2) | 1 (cada índice dos cursos dentro) | SurvSel: `opps` (:96) |
| 14 | personagens do sorteio (2.4) | 0x66 | SurvSel: `randomChara` (:95) |
| 15 | SubpacoteTex, as 165 fichas | 165 subseções, 1 registro cada | MisSel: o laço de relocação (:192..:195); `MisSel_Draw` lê a subseção `chara + 1` ou a 165 (:288, :290) |
| 16 | sem leitor: a forma de tabela de textos (N = 88) | | ninguém; `Conv_Textos` pela forma |
| 17 | LipPack | | as duas: `subtitles` (:203, :137), falas 0x33..0x35 |
| 18 | textos | | as duas: `text` (:202, :136); linhas 0xD8 + missão (até 0x13B), 0x148 + kind, 0x156 + dpRule, 0x13C..0x13E, 0x150; no disco, 350 textos |

#### 2.1 As missões (`MisSelDef`, seção 10)

`MisSelDef` (include/menu/menu_o.h): 0x34 bytes, treze s32, todos lidos como s32 nativos (`MisSel_SetupBattle`,
menu_o_d.c:50; `MisSel_Draw`, :270..:292):

| Desl. | Campo | Quem lê | Limite conferido |
|---|---|---|---|
| 0x00 | announcer | `SetupBattle` (`MISSEL_RANDOM` = 0x3E6 sorteia) | nenhum |
| 0x04 | unk4 | `SetupBattle` (`!= 0`) | nenhum |
| 0x08 | timeLimit | `SetupBattle` | nenhum |
| 0x0C | stage | `SetupBattle` (0x3E6 sorteia `Rand_Range(0x23)`) | 0x3E6 ou menor que 0x24 |
| 0x10 | bgm | `SetupBattle` (0x3E6 vira 0x18) | nenhum |
| 0x14 | unk14 | `SetupBattle` (`!= 0`) | nenhum |
| 0x18 | kind | `SetupBattle` (tamanho do time: 0..3 = 1..4, 6, 7, 10 = 5, os outros 0); `Draw` (linha 0x148 + kind) | 0..3, 6, 7 ou 10 (com outro valor o time do jogador sairia vazio) |
| 0x1C | dpRule | `SetupBattle` (`teamSize`/`dpRule` do `gProgress`); `Draw` (linha 0x156 + dpRule) | 0..3 |
| 0x20 | opp[5] | `SetupBattle` (até o adversário `MISSEL_NONE`); `Draw` lê os cinco de toda missão | cada um menor que o número de registros da seção 11 |

Registros: a missão é `page * 5 + row` (`MisSel_Input`), com `pageCount` = `gSaveData` +0x20C (5 no New Game, 20 no
desbloqueio): 100 registros. No disco: 100 registros e 48 bytes de enchimento zerado (a seção fecha em múltiplo de 64).

#### 2.2 Os adversários (`MisSelOpp` na seção 11, `SurvOpp` na 13)

A mesma forma (include/menu/menu_o.h e menu_s.h): 0x2C bytes, onze s32: chara, costume, cpuLevel, item7, item[7]
(`SetupBattle`: `BattleSetup_SetMember` / `SetPoolMember`; os itens viram `id + 1` num u16 de `BattleItemSet`).
Conferido: cada item `0x3E7` ou menor que 350 (`SAVE_ITEM_COUNT`); o personagem menor que 0xA1, ou `MISSEL_NONE`
(0x3E7, fim do time e a ficha 165) na seção 11, ou `SURV_RANDOM` (0x3E6, sorteado da seção 14) na 13. Todos os
registros inteiros da seção se convertem (os índices apontam para eles); depois, zeros. No disco: seção 11, 500
registros e 16 bytes zerados; seção 13, 151 registros e 12 bytes zerados (os cursos apontam para 0..149; o 151º não
tem leitor e se converte pela forma).

#### 2.3 Os cursos (`SurvCourse`, seção 12)

0xE0 bytes: a cabeça da `MisSelDef` (seis s32, o estágio conferido como em 2.1) e opp[50] s32, índices na seção 13
(`SurvSel_SetupBattle` lê os 50 do curso escolhido, `cur[0]` 0..2). Mínimo 3 registros; cada índice menor que o número
de registros da 13. No disco: 3 registros e 32 bytes zerados.

#### 2.4 Os personagens do sorteio (seção 14)

s32, `randomChara[Rand_Range(0x66)]` (menu_s_b.c:60): as entradas 0..0x65. No disco a seção tem 112 palavras: 103 ids e
9 zeros. O conversor troca como s32 as entradas até a última palavra não nula (no mínimo 0x66), confere cada uma menor
que 0xA1, e exige zero no resto. **Hipótese:** a 103ª entrada (sem leitor) é um id da mesma lista.

### 3. O resultado (`menu_ub_resultado`: 00451, seção 3)

Tabela: N = 14 (o disco confirma; a maior lida é a 14, pelas quatro telas). Conteúdo descomprimido: 3.366.912 bytes.

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | Flash | | MisResult (menu_p_b.c:119), UbResult (menu_n_d.c:105) |
| 2 | Flash | | SurvResult (menu_s_c.c:78) |
| 3 | Flash | | SimResult (menu_r_c.c:99) |
| 4 | `TexFile` | 48 | MisResult e UbResult: registros até 47; SurvResult: até 47; SimResult: 1..43. No disco, 48 |
| 5 | `TexFile`, o fundo | 8 | MisResult, SurvResult, UbResult: `bg`, `Sprite_DrawPicture` |
| 6 | `TexFile`, o fundo | 8 | SimResult: `bg` (:40) |
| 7 | `TexFile` | 9 | MisResult, SurvResult, UbResult: 0, 1, 3; SimResult: 5, 6, 8. No disco, 10 |
| 8 | `TexFile`, os ícones da janela de ícone | 6 | as quatro: registros 0 e `1 + ícone` (`IconWin_SetIcon`: Sim 0, Mis 1, Surv 2, Ub 3 ou 4). No disco, 6 |
| 9 | JanelaIcone | | as quatro: `IconWin_Init(MPACK_AT(res, 9), ...)` |
| 10 | sem leitor: a forma de tabela de textos (N = 88) | | ninguém; `Conv_Textos` pela forma |
| 11 | LipPack | | as quatro: `subtitles`; falas até 0x51 (UbResult), na tabela de 176 entradas (88 falas) |
| 12 | textos | | as quatro: linhas 0x141, 0x142, 0x140, 0xF..0x13, 0xD8 + missão, bônus + 0x20 (até 0x4F) |
| 13 | preços (3.1) | 101 | as quatro: `UbScore_CalcPoints` (src/menu/menu_p_d.c:93) |
| 14 | textos, os nomes dos itens | | as quatro: `itemText`, linha = id do item de recompensa; no disco, 351 textos |

#### 3.1 Os preços (`UbScorePrice`, seção 13)

`UbScorePrice` (include/menu/menu_p.h): 0x18 bytes, seis s32 (line[4], bonus, unk14). `UbScore_CalcPoints` lê
`price[valor].line[i]` com o valor da linha (a vida e a segunda linha vão a 100; a quarta, os adversários vencidos na
Sobrevivência, a 50), `price[j].line[2]` com o degrau de tempo j de 0 a 99, e `price[bônus].bonus` com o bônus de 0 a
47: 101 registros. O `unk14` não tem leitor e é s32 pela forma. No disco: 101 registros e 8 bytes zerados.

### 4. Escritas do jogo depois da conversão

Só as relocações (`Res_RelocateOffsets`, `menus.md` 9) dos `TexFile` que cada tela usa, das 165 fichas (MisSel) e do
`TexFile` da janela de ícone. Nenhuma tela grava nas tabelas (missões, adversários, cursos, sorteio, preços): o que o
jogo calcula vai para a estrutura da tela (`UbScore`), para o `gProgress` e para o `gSaveData`. Uma comparação de
memória vê as tabelas iguais às convertidas.

### 5. Cobertura conferida

#### 5.1 Contra o disco (`conv_tool --disco`)

Sem nenhuma linha de erro, saída 0. Os dois conteúdos com 100% de cobertura, pelos dois caminhos (o da `conv_tool` e
o do jogo, `Conv_FormatoBits`):

| Conteúdo | Bytes | Em números trocados |
|---|---|---|
| `pzs3us1/00451.bin@0x78000#bpe` (s2, `menu_ub_selecao`) | 2.700.480 | 211.770 |
| `pzs3us1/00451.bin@0x212900#bpe` (s3, `menu_ub_resultado`) | 3.366.912 | 193.392 |

Antes e depois (com a linha do arquivo inteiro, que é do ubA): arquivos do disco 3295 para 3296 sem erro; conteúdos
descomprimidos 675 para 677 sem erro; censo da faixa dos menus 448..477, arquivos convertidos 15 para 16 e conteúdos BPE
com conversor 22 para 24 (de 49); o 00451 fica com 4 dos 6 contêineres sem conversor (as seções 1, 4, 5 e 6, dos
outros agentes).

#### 5.2 Os conversores vistos falhando

No WSL, sobre cópias do conteúdo antes da conversão (o `.orig` do `conv_tool --marcas`), cada uma estragada num ponto e
convertida com `--formato`. A cópia intacta sai com 0; cada estragada, com 1 e a mensagem:

| Formato | Estrago | Mensagem |
|---|---|---|
| `menu_ub_selecao` | o `TexFile` da seção 3 com 27 registros | "27 registros, o leitor usa 28 [seção 3 da seleção do Ultimate Battle]" |
| `menu_ub_selecao` | a missão 0 com kind 4 | "missão 0: kind 4 (o time do jogador sairia com 0 lutadores) [seção 10 ...]" |
| `menu_ub_selecao` | a missão 3 com regra de DP 4 | "missão 3: regra de DP 4 (vai a 3) [seção 10 ...]" |
| `menu_ub_selecao` | um adversário da missão 0 no registro 500 | "missão 0: adversário 2 é o registro 500 (a seção 11 tem 500) [seção 10 ...]" |
| `menu_ub_selecao` | um byte não nulo no enchimento depois das missões | "depois das missões: byte ... não reconhecido (não é enchimento) [seção 10 ...]" |
| `menu_ub_selecao` | um item 350 num adversário de missão | "adversário da missão 0: item 350 (os ids vão a 349) [seção 11 ...]" |
| `menu_ub_selecao` | um adversário da Sobrevivência com 0x3E7 | "adversário da Sobrevivência 0: personagem 999 [seção 13 ...]" |
| `menu_ub_selecao` | um índice de curso no registro 151 | "curso 0: adversário 7 é o registro 151 (a seção 13 tem 151) [seção 12 ...]" |
| `menu_ub_selecao` | uma entrada do sorteio com 0xA1 | "personagens do sorteio: entrada 10 é 161 [seção 14 ...]" |
| `menu_ub_selecao` | o subpacote das fichas com 164 subseções | "Conv_Tabela: fim ..., arquivo ... [seção 15 ...]" |
| `menu_ub_selecao` | a tabela do pacote com 17 seções | "Conv_Tabela: fim ..., arquivo ... [seleção do Ultimate Battle]" |
| `menu_ub_resultado` | o `TexFile` da seção 4 com 47 registros | "47 registros, o leitor usa 48 [seção 4 do resultado do Ultimate Battle]" |
| `menu_ub_resultado` | o `TexFile` da seção 7 com 8 registros | "8 registros, o leitor usa 9 [seção 7 ...]" |
| `menu_ub_resultado` | os ícones (seção 8) com 5 registros | "5 registros, o leitor usa 6 [seção 8 ...]" |
| `menu_ub_resultado` | um byte não nulo no enchimento depois dos preços | "depois dos preços: byte ... não reconhecido (não é enchimento) [seção 13 ...]" |
| `menu_ub_resultado` | a tabela do pacote com 13 seções | "Conv_Tabela: fim ..., arquivo ... [resultado do Ultimate Battle]" |

#### 5.3 Os ganchos (`unpacks.py`)

Numa árvore temporária com o `ganchos.diff`: `82 chamadas de Sprite_Unpack em 62 funções: bpe-direto 1, com gancho 37,
pendente 45; 0 com erro` (eram 31 e 51 depois da 9a; as seis desta entrega saem com gancho). Visto falhando: com o
formato trocado por um nome fora de `sFormatos` num dos ganchos, "1 com erro" e a linha da chamada.

### 6. Hipóteses e o que fica aberto

- As seções sem leitor se convertem pela forma: seleção 6 (janela de mensagem), seleção 16 e resultado 10 (tabelas de
  88 textos, com o perfil de caracteres das seções de texto lidas; o número é o de falas da LipPack ao lado).
- A 103ª entrada da lista do sorteio e o 151º adversário da Sobrevivência, sem leitor, são da mesma forma da tabela.
- Os limites de `kind` (0..3, 6, 7, 10) e do estágio são mais estreitos que o que o código impede (ele não confere
  nada): vêm do que cada valor faz no leitor; um disco com outro valor dá erro com o nome da seção, para virar caso.
- Os campos `announcer`, `unk4`, `timeLimit`, `bgm`, `unk14`, `costume` e `cpuLevel` vão como s32 sem limite: o leitor
  os passa adiante sem índice próprio aqui.

## Parte C: as seleções de lutador e de time (`menu_ubsel`: 00451, seção 4)

Etapa 9b, agente ubC. Continuação de `menus-pecas.md` (as peças) e de `menus-modos.md` 3.4 (o inventário). Conversor em
`port/x360/conv/menus_ub_ubC.c` (`Conv_MenuUbSel`), registrado em `sFormatos` e `sFaixas` de `conv.c`. Os retratos dessas
telas já tinham formato (`menu_texfile`, `menus-telas.md` 6); aqui só ganham os ganchos.

Fontes: o código da decomp (as linhas são as de antes dos ganchos da 9b, que deslocam as seguintes de cada arquivo); o
disco inteiro pelo `conv_tool --disco` e contagens de estrutura no WSL (só números voltaram); o rastreio do roteiro
`ultimate` da etapa 4 pelo `conv_tool --mapa`. Nenhum byte do jogo está aqui. **Hipótese** marca o que o código não
prova.

### 1. Os arquivos e quem chama

`Ub_Main` (src/menu/menu_o_c.c) carrega `gMenuArc3` = `baseFile + 3` = 0x1C4 (índice 451), um pacote de seis seções BPE
(`menu_pacote`; a linha do arquivo inteiro em `sFaixas` é do agente ubA). A seção 4 é aberta por duas telas, sempre com
`section = 4`:

| Modo | Tela | Chamada em `Ub_Main` | Volta com 1 | Volta com 0 |
|---|---|---|---|---|
| 15, `teamSize >= 2` | `UbTeamSel_Run(4)` | menu_o_c.c:103 | `Ub_SetupTeam`, modo 16, luta | modo 14 (MisSel) |
| 15, `teamSize < 2` | `SoloSel_Run(4)` | menu_o_c.c:105 | `Ub_SetupTeam`, modo 16, luta | modo 14 |
| 18 | `SoloSel_Run(4)` | menu_o_c.c:133 | `Ub_SetupSolo`, modo 19, luta | modo 17 (SurvSel) |
| 21 | `SoloSel_Run(4)` | menu_o_c.c:159 | modo 22 (SimDay, sem luta ainda) | modo 20 (SimTop) |
| 25 | `SoloSel_Run(4)` | menu_o_c.c:201 | modo 26 (UbRank) | modo 24 (DiscFusion) |
| 29 | `SoloSel_Run(4)` | menu_o_c.c:235 | `Ub_SetupSolo2`, modo 30, luta | modo 28 (UbzSel) |

Os `Sprite_Unpack` e o formato que cada gancho passa (`Port_ConvertMenuPack(buffer, rawSize, "formato")`):

| Chamada | Conteúdo | Buffer | Formato |
|---|---|---|---|
| `SoloSel_Init` menu_l_b.c:148 | 451 s4 | novo (`Sprite_Unpack(pack, NULL, NULL)`), liberado em `SoloSel_Term` | `menu_ubsel` |
| `SoloSel_UpdateImage` menu_l_b.c:83 | retrato `0x2F9 + image` | fixo `imageRes`, 0x20800 | `menu_texfile` |
| `SoloSel_Init` menu_l_b.c:254 | retrato `0x2F9 + image` | o mesmo `imageRes` | `menu_texfile` |
| `UbTeamSel_Init` menu_m_b.c:266 | 451 s4 | novo, liberado em `UbTeamSel_Term` | `menu_ubsel` |
| `UbTeamSel_UpdateFaceLoad` menu_m_b.c:184 | retrato `UB_FACE_FILE (0x2F9) + chara` | fixo `faceRes`, 0x20800 | `menu_texfile` |
| `UbTeamSel_Init` menu_m_b.c:421 | retrato `0x2F9 + chara` | o mesmo `faceRes` | `menu_texfile` |

### 2. O pacote (`menu_ubsel`)

`SoloSel_Init(4)` (src/menu/menu_l_b.c:141) e `UbTeamSel_Init(4)` (src/menu/menu_m_b.c:258). Tabela: N = 34 (o maior
`MPACK_AT` das duas telas é o 34; o disco tem 34). Conteúdo descomprimido: 4.260.672 bytes (abaixo dos 8 MB de
`CONV_JOGO_MAX`). Nenhuma seção vazia; todas começam em múltiplo de 4.

| Seção | Bytes | Formato | Registros (mínimo; disco) | SoloSel | UbTeamSel |
|---|---|---|---|---|---|
| 1 | 960 | `TexFile` | 1; 1 | `SS_RES(1)` :160, registro 0 | `UT_RES(1)` :295, 0 |
| 2 | 38.208 | `TexFile` | 3; 4 | :162, 0 e 2 | :297, 0 e 2 |
| 3 | 19.136 | `TexFile` | 1; 2 | sem leitor | só com DP: :308, 0 |
| 4 | 51.904 | `TexFile` | 2; 2 | sem leitor | só com DP: :310, 0 e 1 |
| 5 | 19.584 | `TexFile` | 3; 3 | sem leitor | só com DP: :317, 2 |
| 6 | 102.400 | `TexFile` | 2; 3 | :174, 0 e 1 | :321, 0 e 1 |
| 7 | 170.624 | `TexFile` | 5; 5 | :177, 1..4 | :324, 1..4 |
| 8 | 34.176 | `TexFile` | 1; 1 | :182, 0 | :329, 0 |
| 9 | 9.600 | `TexFile` | 1; 1 | :184, 0 | :331, 0 |
| 10 | 10.368 | `TexFile` | 2; 3 | :186, 0 e 1 | :333, 0 e 1 |
| 11 | 47.872 | Flash | | `Flash_Create(flash[1])` :190 | `Flash_Create(flash[2])` :339 |
| 12..16 | 19.136, 12.992, 68.288, 35.520, 101.056 | `TexFile` | 2; 2 cada | :192..:206, 0 e 1 | :341..:355, 0 e 1 |
| 17 | 14.400 | Flash | | `flash[2]` :207 | `flash[3]` :356 |
| 18 | 66.944 | `TexFile` | 1; 1 | sem leitor | :358, 0 |
| 19 | 52.288 | `TexFile` | 3; 3 | sem leitor | :360, 0..2 |
| 20 | 19.136 | `TexFile` | 2; 2 | sem leitor | :364, 0 e 1 |
| 21 | 15.872 | Flash | | sem leitor | `flash[1]` :367 (as placas do time) |
| 22 | 1.267.328 | ItemPanel (`menus-pecas.md` 5.1), N = 16 | | `ItemPanel_Init(pack, 0)` :209 | :369 |
| 23 | 135.808 | JanelaMsg (2), N = 2 | | `MsgWin_Init(pack 23, msgText, 1, 0)` :212 | :372 |
| 24 | 360.960 | ItemHelp (5.2), N = 6 | | `ItemHelp_Init` :214 | :374 |
| 25 | 10.624 | textos (`menus.md` 6.1), 88 linhas; mínimo 0x38 | | `msgText` :210; `MsgWin_Draw(0, 0, voiceLine)` :428 | :370; :673 |
| 26 | 15.616 | LipPack (4), 176 entradas (88 falas); mínimo 112 | | `subtitles` :211; `Voice_PlayWithSubtitle` :712, :766, :770 | :371; :1094, :1147 |
| 27 | 908.864 | SubpacoteTex (5.5), 165 fichas | 1 cada | laço :219..:222; `MPACK_AT(chips, id + 1)` :30, :52 | :379..:382; :96, :118, :139, :146 |
| 28 | 11.456 | textos, 168 linhas; mínimo 0xA4 | | `nameText` :216; `TextBox_AttachLine(.., image, ..)` :330 | :376; `(.., chara, ..)` :498 |
| 29 | 11.456 | textos, 168 linhas; mínimo 0xA4 | | `formText` :217; :332 | :377; :500 |
| 30 | 3.584 | Células (5.4), 99 células | | `LChrGridList` :223, contagem `res[res[30] >> 2]` (u32) :224 | `+ 0x10` :383, :384 |
| 31 | 16.128 | Flash | | `flash[0]` :172 | `flash[0]` :314 |
| 32 | 272.960 | `TexFile`, o fundo | 8; 8 | `gSoloSel->bg` :152, `Sprite_DrawPicture` :303 | :287, :471 |
| 33 | 39.104 | `TexFile` | 5; 5 | :154, 0..4 | :289, 0..4 |
| 34 | 296.128 | `TexFile` | 9; 10 | :165, 0, 1, 3, 5, 6, 8 | :300, as mesmas |

As seções que só uma das telas lê (o "todas as variantes" do inventário, `menus-modos.md` 5.5): 3, 4 e 5 só o
UbTeamSel com a regra de DP (`UB_PROG->dpRule` 1..3, o limite de 10, 15 ou 20 pontos); 18, 19, 20 e 21 só o UbTeamSel
(o filme das placas do time). O conversor é um só e converte as 34 em toda chamada; o mínimo de cada `TexFile` é o do
leitor que mais lê.

#### 2.1 Os limites conferidos

- **Textos da janela de mensagem (25):** as falas são 0x16 e 0x17 (0x18 com o personagem 0x66) no modo 21, 0x36 e
  0x37 nos outros (`SoloSel_Run` :766, :770; `SoloSel_UpdateEnd` :702..:712; `UbTeamSel_Run` :1147;
  `UbTeamSel_UpdateEnd` :1094). `MsgWin_Draw` passa a fala a `TextBox_AttachLine`, que lê a entrada `linha + 1`:
  pelo menos 0x38 linhas.
- **Legendas (26):** `Voice_PlayWithSubtitle` lê `pack[fala * 2 + 1]` e `pack[fala * 2 + 2]`: a fala 0x37 pede
  pelo menos 112 entradas. O conversor confere as duas contagens antes de converter.
- **Nomes e formas (28, 29):** a linha é o personagem mostrado (`sel->image`, `sel->chara`). O cursor da grade só para
  em personagem, no aleatório (0xA1) e no personagem salvo (0xA3) (`ChrGrid_Move*`, src/battle/view_a_e.c:549..:637;
  `ChrGrid_NextForm`/`PrevForm` :641, :656 pulam os marcadores), então a linha vai a 0xA3: pelo menos 0xA4 linhas.
  **Hipótese:** que `image` nunca seja 0xA4 (a célula vazia); o disco tem 168, o que cobre também 0xA4.
- **Células (30):** 99 no disco, ids até 0xA3 (os marcadores 0xA1 e 0xA3 aparecem), formas até 0xA0, no máximo 7 por
  célula: dentro dos limites de `Conv_Celulas` (5.4.4).
- **Fichas (27):** N = 165 (`LCHR_CELL_MAX`, `UB_CELL_MAX`); `SetTeamTex` e `SetFormChips` leem a ficha 0xA4 + 1
  (`UB_ID_EMPTY`, `LCHR_ID_NONE`).
- **N = 34** exato (`Conv_Secoes`).

#### 2.2 Escritas do jogo depois da conversão

- `Res_RelocateOffsets` grava os ponteiros (`TexFile.ent`, `pix`, `clut`) em cada `TexFile` que a tela reloca: no
  SoloSel 1, 2, 6..10, 12..16, 32..34, as 165 fichas e os das peças (ItemPanel, JanelaMsg, ItemHelp); no UbTeamSel o
  mesmo mais 18..20 e, com DP, 3..5. Um `TexFile` sem leitor na tela fica como o conversor o deixou.
- Nada mais: não há ListaIds neste pacote (os desbloqueios vão pelas células, e `ChrGrid_Build`, view_a_e.c:776, só
  lê a lista do pacote e grava a grade em `gridBuf`, na struct da tela); a contagem das células é copiada para
  `gridCount`. A escolha vai para `gProgress` (+0x440, `lastEntry` do SoloSel e `team` do UbTeamSel), fora do pacote.
- Cada `_Init` descomprime o pacote num buffer novo: nunca há conversão dupla.

### 3. Os retratos (`menu_texfile`)

| Leitor | Arquivo:linha | Registros lidos | Buffer do arquivo | Buffer descomprimido |
|---|---|---|---|---|
| `SoloSel_UpdateImage` | menu_l_b.c:83..:86 | `MTEX(res, 0)` -> `texA[17]` | `imageFile`, 0x16800 (`File_Request`, :74) | `imageRes`, 0x20800 (:238) |
| `SoloSel_Init` | menu_l_b.c:254..:257 | 0 | o mesmo (`File_LoadSync`, :253) | o mesmo |
| `UbTeamSel_UpdateFaceLoad` | menu_m_b.c:184..:187 | 0 | `faceFile`, 0x16800 (`File_Request`, :174) | `faceRes`, 0x20800 (:398) |
| `UbTeamSel_Init` | menu_m_b.c:421..:424 | 0 | o mesmo (:420) | o mesmo |

- Os quatro leem só o registro 0: o `menu_texfile` (mínimo 1) serve sem mudança.
- No disco (os 164 retratos 760..923): o maior arquivo tem 0x160C0 bytes (cabe nos 0x16800 lidos) e o maior conteúdo
  descomprimido 0x20560 bytes (cabe nos 0x20800). Nenhum passa.
- O buffer é fixo e reaproveitado: cada `Sprite_Unpack` reescreve o conteúdo inteiro, e o gancho converte o que acabou
  de sair.
- Pelo cursor (2.1), essas telas carregam 0x2F9 + 0..0xA1 e 0x2F9 + 0xA3 (índices 760..921 e 923); o travado (922)
  não. No UbTeamSel, `chara < 0` (nenhum membro) não carrega nada (`UBTEAM_SEL_NO_FACE`, :170..:173).

### 4. A entrada de cada tela (para os roteiros de controle)

Bits de `gPad[0]` (include/sys/pad.h:38..:51): ESQUERDA 1, DIREITA 2, BAIXO 4, CIMA 8, X (`PADG_CROSS`) 0x200,
TRIÂNGULO 0x400. Nenhuma das duas telas lê START, CÍRCULO nem QUADRADO. Setas por `gameRepeat` (repetem segurando), X e
TRIÂNGULO por `gamePressed`. A entrada só vale depois de o filme 0 aceitar (`MFLASH_PAD`) e enquanto o fim não começou;
a fala de boas-vindas (0x36; 0x16 no modo 21) toca uma vez quando o fade de entrada termina, sem travar a entrada.

#### 4.1 SoloSel (um lutador; `SoloSel_Input` menu_l_b.c:447)

| Passo | Setas | X | TRIÂNGULO |
|---|---|---|---|
| personagem (`STEP_CHARA`) | ESQ/DIR andam na linha (pulam travado e vazio), CIMA/BAIXO trocam de linha (com 2 linhas ou mais); cada movimento pede outro retrato | célula com formas: vai às formas; sem formas: vai às placas de itens | sai: fade, volta 0 |
| formas (`STEP_FORM`) | ESQ/DIR trocam a forma (retrato novo) | vai às placas de itens | volta ao personagem |
| placas de itens (`STEP_CUSTOM`) | CIMA/BAIXO trocam a placa (4 placas; no modo 21 só a "sem itens"); DIREITA numa placa 1..3 abre o painel de itens | vai às cores | volta às formas ou ao personagem |
| painel de itens (`STEP_PANEL`, `ItemPanel_Input` menu_g.c:235) | CIMA/BAIXO no item; ESQUERDA fecha | item não vazio: abre a ajuda dele | fecha, volta às placas |
| ajuda do item (`STEP_HELP`) | | fecha | fecha |
| cor (`STEP_COLOR`) | ESQ/DIR trocam a cor (de 0 a `colorCount - 1`, `ChrTbl_WrapCostume`, com volta) | **decide**: personagem, itens da placa (do save) e cor vão para `gProgress` (+0x440); a fala de despedida (0x37; 0x17 ou 0x18 no modo 21), que um X pula; 15 quadros e fade; volta 1 | volta às placas |

Caminho mínimo até a luta: X (personagem sem formas), X (placa sem itens), X (cor), X (pula a fala). Com formas, um X a
mais.

#### 4.2 UbTeamSel (um time; `UbTeamSel_Input` menu_m_b.c:696)

Começa no passo do personagem para o membro 0 (`memberCount` = 0). `teamMax` = `teamSize` do modo; a regra de DP vem
de `dpRule`.

| Passo | Setas | X | TRIÂNGULO |
|---|---|---|---|
| personagem (`STEP_CHARA`) | como no SoloSel | se o DP permite e o personagem não repete outro membro (`UbTeamSel_FitsDp` :58, `UbTeamSel_IsCharaFree` :43): formas ou placas; senão, som de erro | sem membros: sai, fade, volta 0; com membros: volta às placas do time, restaurando o membro que estava sendo trocado |
| formas, placas, painel, ajuda | como no SoloSel (as placas são sempre 4) | X nas formas confere DP e repetição de novo | como no SoloSel |
| cor (`STEP_COLOR`) | ESQ/DIR trocam a cor | fecha o membro (personagem, itens, cor); um membro novo leva o cursor à vaga seguinte, ou à placa "pronto" (`UB_CUR_MENU`) quando o time encheu; vai às placas do time | volta às placas de itens |
| placas do time (`STEP_TEAM`) | ESQ/DIR andam entre os membros escolhidos, a primeira vaga livre e a placa "pronto" (com volta) | num membro ou na vaga livre: escolhe (ou troca) aquele membro, volta ao personagem; na placa "pronto" com 1 membro ou mais: a fala 0x37, que um X pula, e o fim: o time vai para `gProgress->team`, `teamSize` = membros escolhidos, volta 1 | tira um membro (com um só membro, volta ao personagem para o membro 0) |

O time pode sair com menos membros que `teamSize` (a placa "pronto" aceita a partir de 1). Caminho mínimo para um time
de 2: X, X, X (membro 0), X na vaga 1, uma seta (cada membro começa na célula guardada em `gProgress->team`; se
for a do membro 0, o mesmo personagem dá som de erro), X, X, X (membro 1); o cursor já está na
placa "pronto" com o time cheio: X, e X pula a fala.

### 5. Cobertura conferida

#### 5.1 Contra o disco (`conv_tool --disco`)

| | Antes | Depois |
|---|---|---|
| arquivos convertidos sem erro | 3.295 | 3.296 (00451 como `menu_pacote`, a linha do ubA) |
| conteúdos descomprimidos sem erro | 675 | 676 (451 s4) |
| censo dos menus 448..477: arquivos com conversor | 15 de 30 | 16 de 30 |
| censo dos menus: contêineres BPE com conversor | 22 de 49 | 23 de 49 |
| retratos 760..923 | 164 de 164 | 164 de 164 |

`pzs3us1/00451.bin@0x398440#bpe`: convertido, cobertura de 4.260.672 bytes (100%), 303.028 em números trocados, igual
pelo caminho do jogo (`Conv_FormatoBits`). O 00451 inteiro: 8.374.016 bytes, 100%, 112 em números trocados. Nenhuma
linha de erro, nenhum conteúdo aprovado antes mudou, nenhum aviso C89.

#### 5.2 Contra o rastreio (`conv_tool --mapa`, roteiro `ultimate` da etapa 4)

O roteiro passa pelo UbTeamSel (modo 15) e pelo SoloSel (modo 21) e carrega os retratos 760, 773 e 782.

| Conteúdo | Posições | Iguais | Outra largura | Não trocadas |
|---|---|---|---|---|
| 451 s4 | 10.605 | 10.604 (+1 em cadeia de texto lida por inteiro) | 0 | 0 |
| 00451 (o arquivo) | 15 | 15 | 0 | 0 |
| 00760, 00773, 00782 (arquivo e retrato) | 2 e 10 cada | todas | 0 | 0 |

#### 5.3 O conversor visto falhando

No WSL, sobre cópias do conteúdo antes da conversão (o `.orig` do `conv_tool --marcas`), cada uma estragada num ponto e
convertida com `--formato menu_ubsel`: a intacta sai com 0, cada estragada com 1 e o nome da seção.

| Estrago | Mensagem |
|---|---|
| a tabela com N = 33 | "Conv_Tabela: fim ..., arquivo ... [seleção do Ultimate Battle]" |
| seção 34 com 8 registros | "texturas em ...: 8 registros, o leitor usa 9 [seção 34 da seleção do Ultimate Battle]" |
| seção 32 (o fundo) com 7 registros | "7 registros, o leitor usa 8 [seção 32 ...]" |
| seção 7 com 4 registros | "4 registros, o leitor usa 5 [seção 7 ...]" |
| textos 25 com 55 linhas | "textos em ...: 55 linhas, o leitor usa 56 [seção 25 ...]" |
| legendas 26 com 110 entradas | "legendas em ...: 110 entradas, a fala 0x37 usa 112 [seção 26 ...]" |
| nomes 28 com 163 linhas | "163 linhas, o leitor usa 164 [seção 28 ...]" |
| um byte não nulo depois da última célula | "depois das células: byte ... não reconhecido (não é enchimento) [seção 30 ...]" |
| um byte não nulo em +4 das células | "cabeçalho das células: byte ... não reconhecido [seção 30 ...]" |
| a célula 0 com id 0xA5 | "célula 0 em ...: id 0xA5 [seção 30 ...]" |
| as fichas com N = 164 | "Conv_Tabela: fim ..., arquivo ... [seção 27 ...]" |

### 6. Hipóteses e o que fica aberto

- Que o retrato e os nomes nunca peçam a célula vazia (0xA4): o cursor não para nela; o disco cobre de qualquer forma.
- As seções 3..5 e 18..21 não têm leitor no SoloSel; convertidas pela forma que o UbTeamSel lê.
- Nenhuma peça nova: tudo sai de `conv_menus.h`. As duas conferências de contagem (textos e legendas com mínimo) são
  auxiliares internas do conversor (`ububC_textos`, `ububC_legendas`); se outra tela pedir o mesmo, valem como peça.

## Parte D: o tabuleiro do Sim Dragon (`menu_sim_dia`: 00474 inteiro) e os retratos do anúncio da luta

Parte da especificação do Ultimate Battle (as outras seções, do 00451, do 00475 e do 00476, são dos outros agentes).
Conversor em `menus_ub_ubD.c` (`Conv_MenuSimDia`), registrado em `sFormatos` e `sFaixas` de `conv.c`. Fontes: o
código da decomp (as linhas são as de antes dos ganchos desta etapa); o disco inteiro pelo `conv_tool --disco` e
contagens de estrutura no WSL (só números voltaram). Nenhum byte do jogo está aqui. **Hipótese** marca o que o código
não prova.

### 1. Os arquivos e quem chama

| Arquivo | Id (como o código calcula) | Índices | Forma | Formato do arquivo | Conteúdo |
|---|---|---|---|---|---|
| tabuleiro do Sim Dragon | `QPROG->baseFile + 0x1A` = 0x1DB (`SimDay_Init`, src/menu/menu_q_b.c:897) | 474 | BPE inteiro (4.154.480 bytes no disco) | `menu_bpe` | `menu_sim_dia` |
| retratos | `SIMDAY_FACE_FILE` + personagem = 0x2F9 + personagem (`SimDay_UpdateFaceLoad`, menu_q_b.c:1960, :1961) | 760..923 | BPE inteiro | `menu_bpe` (já existia) | `menu_texfile` (já existia) |

| Chamada | Conteúdo | Buffer | Formato |
|---|---|---|---|
| `SimDay_Init` menu_q_b.c:899 | 474 inteiro | novo (`Sprite_Unpack(pack, NULL, NULL)`), liberado em `SimDay_Term` | `menu_sim_dia` |
| `SimDay_UpdateFaceLoad` menu_q_b.c:1970 | retrato do lutador do jogador (`QPROG->member.chara`) | `faceRes[0]`, fixo, 0x20800 | `menu_texfile` |
| `SimDay_UpdateFaceLoad` menu_q_b.c:1971 | retrato do adversário da rodada (`gSimDay->enemyChara`) | `faceRes[1]`, fixo, 0x20800 | `menu_texfile` |

Com os ganchos, as duas chamadas dos retratos passam a :1976 e :1983 (o gancho do `SimDay_Init` desloca 6 linhas).

O `SimDay_Init` libera o `gMenuArc3` (00451) antes de carregar o 00474, para caber: o conteúdo descomprimido tem
**8.504.704 bytes (0x81C580), o maior do disco**, acima dos 8 MB de `CONV_JOGO_MAX` (seção 6.2).

### 2. O pacote (`menu_sim_dia`)

`SimDay_Init(0)` (menu_q_b.c:877, modo 22). Tabela: N = 33 (o disco confirma; o comentário de `SimDay.res`, "a pack of
34 sections", conta errado). **Todas as 33 seções têm leitor**: a 29, que o inventário (`menus-modos.md` 3.4) dava como
sem leitor, é `gSimDay->unkB5C`, que os roteiros de evento leem como `TSimDay.train` (include/menu/menu_t.h:83), a
tabela dos treinos.

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | Flash, o tabuleiro | | `Flash_Create(&flash[SIMDAY_FL_BOARD], pack 1, tex0)` :1069 |
| 2 | Flash, o número da rodada | | `flash[SIMDAY_FL_ROUND]`, `tex6` :1074 |
| 3 | `TexFile` | 60 | `SD_RES(3)` :907; `MTEX(res, 0..58)` :908..:966 e `faceDefault = MTEX(res, 59)` :969 |
| 4 | `TexFile`, o fundo | 8 | `SD_RES(4)` :905, `gSimDay->bg`, `Sprite_DrawPicture` (`SimDay_Draw` :1177) |
| 5 | `TexFile` | 14 | `SD_RES(5)` :1054, 0..13 |
| 6 | `TexFile`, as figuras do monitor | 12 | `faceResA` :974; `SimDay_SetFaceA(n)` :79 com `faceA` 0..11 (os roteiros de evento, inclusive `Rand_Range(12)`; 12 é a figura padrão, da seção 3) |
| 7 | `TexFile`, os personagens do monitor | 12 | `faceResB` :977; `SimDay_SetFaceB(n)` :85 com `faceB` 0..11 |
| 8 | `TexFile`, o lutador do jogador | 161 | `SD_RES(8)` :972, `MTEX(res, QPROG->member.chara)` :973; o personagem vem das células do SoloSel, 0..0xA0 (`LCHR_ID_MAX`, include/menu/menu_l.h:66) |
| 9 | JanelaMsg | | `MsgWin_Init(pack 9, msgText, 1, 0)` :1083 |
| 10, 11, 12 | Flash, o jogo das cartas | | `Flash_Create(..., MPACK_AT(res, gSimTrain0[rank[0]].param + 5), tex2)` :1078: `param` 5, 6, 7 (menu_q_b.c:20), conforme o turno; as três se leem ao longo da campanha |
| 13 | Flash, o treino 1 | | `flash[SIMDAY_FL_POPO]`, `tex3` :1080 |
| 14 | Flash, o treino 2 | | `flash[SIMDAY_FL_SHOT]`, `tex4` :1081 |
| 15 | Flash, a janela de três linhas | | `flash[SIMDAY_FL_MENU]`, `tex5` :1072 |
| 16 | `TexFile` | 28 | `SD_RES(16)` :1003, o maior é 27 (:1015) |
| 17 | `TexFile` | 6 | `SD_RES(17)` :1039, 0..5 |
| 18 | Flash, o evento | | `flash[SIMDAY_FL_EVENT]`, `tex1` :1075 |
| 19 | `TexFile` | 8 | `SD_RES(19)` :987, 0..7 |
| 20 | `TexFile` | 1 | `SD_RES(20)` :996, 0 (2 no disco) |
| 21 | `TexFile` | 1 | `SD_RES(21)` :998, 0 (5 no disco) |
| 22 | `TexFile`, os sinais de mais e menos | 2 | `numRes` :1000; `MTEX(numRes, 0)` :1002, :1299, :1321 e `MTEX(numRes, 1)` :1301, :1323 |
| 23 | `TexFile` | 5 | `SD_RES(23)` :980, 0..4 |
| 24 | textos (`menus.md` 6.1) | 216 linhas | `msgText` :1082: o texto da janela de mensagem (`MsgWin_Draw(0, 0, msgLine)` :1337) e o `msgTbl` do diálogo (:1090, `Dialog_SetMsg(0xE)` :1821). `msgLine` vai até 0xD7 (literal mais alto dos roteiros de evento) |
| 25 | rodadas (`SimRound`, 3.1) | 7 | `gSimDay->round` :1092, `SimDay_SetupBattle` :96 |
| 26 | adversários (`SimEnemy`, 3.2) | 1 + o maior `SimRound.enemy` (7) | `gSimDay->enemy` :1094, `SimDay_SetupBattle` :103 |
| 27 | sorteios de adversário (`SimPool`, 3.3) | 3 | `gSimDay->pool` :1093, `SimDay_SetupBattle` :135, :140 |
| 28 | eventos ao acaso (`SimEventRow`, 3.4) | 32 | `gSimDay->eventTbl` :1091, `SimDay_PickEvent` :816 |
| 29 | treinos (`SimTrainTbl`, 3.5) | 3 | `gSimDay->unkB5C` :1095 = `TSimDay.train`; `SimEv00..02` e `SimEv00_Roll..02_Roll` (src/menu/menu_s_d.c:30, :65..:69, :107, :142..:146, :184, :219..:223) |
| 30 | níveis (`SimLevel`, 3.6) | 8 | `gSimDay->level` :1096; :666, :861, :1209, :1227, :1422, :1533 |
| 31 | `TexFile` | 1 | `SD_RES(31)` :970, 0 (2 no disco) |
| 32 | textos | 113 linhas | `gSimDay->text` :1085, três `TextBox_Init` :1087; `TextBox_AttachLine(..., QPROG->sim.item[slot], ...)` :1259: a linha é o id do item levado, que só vem de `SimDay_GiveItem` (:249, `own[]`, os itens de `gSimEventItems`, até 0x70) |
| 33 | diálogo (`menus.md` 6) | | `Dialog_Init(pack 33, msgText, 0)` :1090, o "sair do Sim Dragon?" |

Os `TexFile` dos retratos do anúncio (`tex6[13]`, `tex6[14]`) não são deste pacote: vêm dos retratos (seção 5).

### 3. As tabelas de registros

Todas são lidas por campo `s32` (structs de include/menu/menu_q.h e menu_t.h), alinhadas a 4, e o que sobra depois dos
registros que o leitor usa é enchimento, exigido zero. Os números marcadores: 998 (`SIMDAY_RANDOM`, sorteio) e 999
(`SIMDAY_NONE`, nenhum).

#### 3.1 Rodadas (`SimRound`, seção 25, 0x1C bytes)

| +0x | Campo | Largura | Leitor | Conferência |
|---|---|---|---|---|
| 00 | `announcer` | s32 | `BattleSetup_SetRule` (998: `Rand_Range(8)`) | |
| 04 | `unk4` | s32 | `!= 0` para o último argumento de `BattleSetup_SetRule` | |
| 08 | `timeLimit` | s32 | `BattleSetup_SetRule` | |
| 0C | `stage` | s32 | `BattleSetup_SetRule` (998: `Rand_Range(0x23)`) | |
| 10 | `bgm` | s32 | `BattleSetup_SetRule` (998: 0x18) | |
| 14 | `unk14` | s32 | `!= 0` para o `unk1FC` do lado 1 (`BattleSetup_SetSide`) | |
| 18 | `enemy` | s32 | índice da tabela de adversários | >= 0; a tabela 26 tem de ter 1 + o maior |

O registro é `round = turn / 10` (`SimDay_SetupBattle` :95), com `turn` até 69 (`SIM_LAST_TURN`, include/menu/menu_r.h:71:
a luta da sétima rodada; depois dela o `SimResult` volta ao modo 20): **7 registros**. No disco a seção tem 0x100
bytes: os 7 registros (0xC4) e 0x3C bytes zerados.

#### 3.2 Adversários (`SimEnemy`, seção 26, 0x2C bytes)

| +0x | Campo | Largura | Leitor | Conferência |
|---|---|---|---|---|
| 00 | `chara` | s32 | `enemyChara` (998: um do sorteio da rodada); o retrato é 0x2F9 + `chara` | 998 ou 0..0xA3 (o retrato existe: 760..923) |
| 04 | `color` | s32 | `BattleSetup_SetMember` | |
| 08 | `cpuLevel` | s32 | `BattleSetup_SetMember` | |
| 0C | `lastItem` | s32 | `enemyItems[7] = lastItem + 1` (u16 do jogo, não do pacote) | 999 ou 0..349 |
| 10 | `item[7]` | s32 x 7 | `enemyItems[n++] = id + 1` | 999 ou 0..349 |

No disco: 7 registros (0x134) e 12 bytes zerados; as rodadas 0..6 usam os adversários 0..6, um cada.

#### 3.3 Sorteios de adversário (`SimPool`, seção 27, 0x40 bytes)

`s32 chara[16]`, lido até o primeiro 999 ou até o 16º (:134); o adversário é `chara[Rand_Range(n)]` (:140), então
n >= 1. O pool é o 0, o 1 na rodada 3 e o 2 na rodada 5 (:124..:133): **3 registros**. Cada personagem antes do 999
tem de ter retrato (0..0xA3); depois do 999 não há leitor (s32 pela forma; no disco os três pools estão cheios, sem
999). A seção tem exatamente 0xC0 bytes.

#### 3.4 Eventos ao acaso (`SimEventRow`, seção 28, 0x14 bytes)

`s32 weight[5]`; `SimDay_PickEvent` (:801) soma a coluna `eventSet` (0..3, `SimDay_ListItems` :207..:221) das 32 linhas
(`SIMDAY_EVENT_ROWS`) contra `Rand_Range(256)`. A coluna 4 não tem leitor (s32 pela forma; zerada no disco). Uma
coluna que somasse 256 ou menos não quebra o leitor (o evento 37 passa da tabela de roteiros e `SimEvent_Run` devolve
1): sem conferência de valor. No disco as quatro colunas lidas somam 256. A seção tem exatamente 0x280 bytes.

#### 3.5 Treinos (`SimTrainTbl`, seção 29, 0xC0 bytes)

| +0x | Campo | Largura | Leitor |
|---|---|---|---|
| 00 | `weight[3][6]` | s32 x 18 | `SimEv0n_Roll`: `weight[repeat][i]` (repeat 0..2, `SimDay_PickEvent` :835..:842) |
| 48 | `hp[6]` | s32 x 6 | `SimEv0n`, a mudança da vida |
| 60 | `atkMin[6]`, 78 `atkMax[6]` | s32 x 12 | `Rand_Range(max - min + 1) + min` |
| 90 | `defMin[6]`, A8 `defMax[6]` | s32 x 12 | idem |

Um registro por treino (`train[0..2]`): **3 registros**, a seção tem exatamente 0x240 bytes. Conferência: mínimo <=
máximo em cada resultado (senão `Rand_Range` recebe 0 ou menos), lida antes da troca.

#### 3.6 Níveis (`SimLevel`, seção 30, 8 bytes)

`s32 max` (o teto de ataque e defesa) e `s32 need` (os pontos acima dos quais o nível vem). O jogo lê `level[nível]`
(0..6, `SIM_LEVEL_MAX`) e `level[nível + 1]` com nível até 6 (`SimDay_RefreshBoard` :860, `SimDay_Update` :1532,
`SimDay_Cmd(33)` :662..:666): índices 0..7, **8 registros**; a seção tem exatamente 0x40 bytes (o registro 7, que só
serve de teto da comparação, é zero no disco).

### 4. As conferências do conversor

- N = 33 exato (`Conv_Secoes`); nenhuma seção pode ser vazia (cada peça recusa a sua).
- `TexFile.count` (+0, lido antes de converter) pelo menos o mínimo da tabela da seção 2.
- Textos: a contagem de linhas (+0) pelo menos 216 na seção 24 e 113 na 32, antes do `Conv_Textos`.
- Tabelas: o tamanho cabe os registros mínimos; os valores da seção 3; o resto é zero.
- Filmes, janela de mensagem e diálogo: as peças de sempre (`Conv_Flash`, `Conv_JanelaMsg`, `Conv_Dialogo`).

### 5. Os retratos do anúncio da luta (`menu_texfile`)

Na rodada, depois do roteiro do décimo turno, `SimDay_SetupBattle` escolhe o adversário e `loadState` vai a
`SIMDAY_LOAD_REQUEST`; `SimDay_UpdateFaceLoad` (:1949) pede os dois arquivos (`File_Request` em `faceFile[k]`,
0x16800 bytes cada), espera e descomprime os dois em `faceRes[k]` (0x20800 cada, alocados uma vez no `SimDay_Init`
:901..:904), depois reloca e lê **só o registro 0**: `tex6[14] = res->tex` (o do jogador) e `tex6[13] = res->tex` (o
do adversário), e `res->tex` é `MTEX(res, 0)` (include/menu/menu_a.h:49, :52). O `menu_texfile` (mínimo 1) serve.

No disco (os 164 retratos 760..923): o maior arquivo tem 0x160C0 bytes (o 00766), abaixo dos 0x16800 do `faceFile`;
o maior conteúdo descomprimido tem 0x20560 bytes (o 00760), abaixo dos 0x20800 do `faceRes`. Cada `Sprite_Unpack`
reescreve o buffer inteiro, então converter depois de cada um não converte duas vezes, nem quando os dois retratos são
do mesmo personagem (buffers diferentes). Os personagens possíveis: o do jogador (0..0xA0) e o adversário (seção 3.2:
105..108 no disco, ou os sorteios, 2..155): todos na faixa 760..923, que o `--disco` já converte inteira.

### 6. Mudanças fora do conversor do pacote

#### 6.1 Três códigos de ação no Flash (peça comum, `menus.c`, `acoes`)

O filme do evento (seção 18) tem duas listas de ação (tag 12, DoAction) com os códigos 0x96, 0x1C e 0x9F, fora da
lista de `menus.md` 7.6, e o conversor dava erro ("ação 0x96 ... sem leitor"). O leitor do jogo (`FlashTl_TagActions`
e `FlashTl_SkipActions`, src/sys/gfxm_c.c:250, :301) não tem `default`: um código desconhecido consome só o próprio
byte e a leitura segue no byte seguinte, que é lido de novo como código. O conversor passa a aceitar esses três
códigos assim (byte, sem dado), e o quadro fecha exatamente no tamanho do bloco: as seis ocorrências estão todas na
seção 18 do 00474 (as duas listas lidas pelo caminho do jogo). Qualquer outro código continua erro. Conferido: com
qualquer código desconhecido aceito como um byte, o `--disco` inteiro (com este formato) dá o mesmo resultado e acha
só essas seis ocorrências, então a lista explícita não esconde nada do que se converte hoje (os pacotes dos outros
agentes da 9b podem trazer outros: a lista cresce com eles, ou vira o `default` do jogo). A seção 7.6 de `menus.md` ganha a frase.

#### 6.2 `CONV_JOGO_MAX` (conv.h)

O mapa de bits da conversão em jogo se reserva uma vez com `CONV_JOGO_MAX / 8` bytes, e um conteúdo maior para o jogo
com o nome (conv_jogo.c:199). O 00474 tem 0x81C580 bytes: o `--disco` acusava ("acima do mapa da conversão em jogo").
Feito: `CONV_JOGO_MAX` = 0x900000 (9 MB de conteúdo, 0x120000 bytes de mapa, 128 KB a mais que antes, reservados no
boot, fora do heap do jogo: `malloc` no `conv_jogo.c`). O guia e o `menus-historia.md` 2 dizem 9 MB.

### 7. Escritas do jogo depois da conversão

- Só as relocações de sempre: `Res_RelocateOffsets` grava `TexFile.ent` (+0x10) e `pix`/`clut` (+0x38, +0x3C) de
  cada `TexFile` relocado (seções 3..8, 16, 17, 19..23, 31) e as peças (janela de mensagem, diálogo) relocam as suas
  (`menus-pecas.md` 5.1.5).
- As tabelas 25..30 só são lidas: nenhuma escrita em `round`, `enemy`, `pool`, `eventTbl`, `unkB5C`/`train` nem
  `level` (procurado em menu_q_b.c e nos roteiros de evento, menu_s_d.c, menu_t_b.c, menu_u*.c). O que muda durante o
  jogo vai a `gProgress` (`QPROG->sim`), ao save e à `SimDay`.
- Os retratos: só a relocação.

### 8. Cobertura no disco

`conv_tool --disco` (WSL, com as linhas deste formato, a mudança de 6.1 e a de 6.2):

| | Antes | Depois |
|---|---|---|
| arquivos convertidos sem erro | 3.295 | 3.296 (+ o 00474 como `menu_bpe`, 100%) |
| descomprimidos convertidos sem erro | 675 | 676 (+ o conteúdo do 00474, 8.504.704 bytes, 100%, 269.402 em números trocados) |
| maior conteúdo da conversão em jogo | 0x6BF440 (448 s2), mapa para 0x800000 | 0x81C580 (474), mapa para 0x900000 |
| censo dos menus 448..477: arquivos com conversor | 15 de 30 | 16 de 30 |
| censo dos menus: contêineres BPE com conversor | 22 de 49 | 23 de 49 |
| retratos 760..923 | 164 de 164 | 164 de 164 |

Saída 0, nenhuma linha de erro, nenhum aviso C89 do ajudante. Os caminhos do mapa de bytes e do mapa de bits
(`Conv_FormatoBits`) dão o mesmo resultado no 00474.

### 9. O conversor visto falhando

No WSL, sobre cópias do conteúdo antes da conversão (o `.orig` do `conv_tool --marcas`), cada uma estragada num ponto
e convertida com `--formato menu_sim_dia`: a cópia intacta sai com 0, cada estragada com 1 e o nome da seção.

| Estrago | Mensagem |
|---|---|
| nenhum | "convertido" (saída 0) |
| `TexFile` da seção 3 com 59 registros | "texturas em 0xCB40: 59 registros, o leitor usa 60 [seção 3 do tabuleiro do Sim Dragon]" |
| `TexFile` da seção 8 com 160 | "160 registros, o leitor usa 161 [seção 8 ...]" |
| o fundo (seção 4) com 7 | "7 registros, o leitor usa 8 [seção 4 ...]" |
| rodada 2 com o adversário 7 | "adversários em 0x7F9880: 0x140 bytes, o leitor usa 8 registros de 0x2C [seção 26 ...]" |
| um byte não nulo no enchimento depois das rodadas | "depois das rodadas: byte 0x7F9849 não reconhecido (não é enchimento) [seção 25 ...]" |
| adversário 0 com o personagem 0xA4 | "adversário 0: personagem 164 sem retrato (0x2F9 + personagem vai até 0x39C) [seção 26 ...]" |
| adversário 3 com o item 350 | "adversário 3: item 350 fora dos 350 [seção 26 ...]" |
| um byte não nulo depois dos adversários | "depois dos adversários: byte 0x7F99B6 não reconhecido (não é enchimento) [seção 26 ...]" |
| sorteio 1 começando em 999 | "sorteio 1 vazio (Rand_Range(0)) [seção 27 ...]" |
| treino 2, resultado 0, `atkMin` 1000 | "treino 2, resultado 0: mínimo acima do máximo [seção 29 ...]" |
| textos da seção 24 com 215 linhas | "textos em 0x7F0200: 215 linhas, o leitor usa 216 [seção 24 ...]" |
| textos da seção 32 com 112 | "textos em 0x802A40: 112 linhas, o leitor usa 113 [seção 32 ...]" |
| o primeiro 0x96 da seção 18 trocado por 0x97 | "Flash: ação 0x97 em 0x76675C sem leitor [seção 18 ...]" |

### 10. Hipóteses e o que fica aberto

- O personagem do jogador na seção 8 vai até 0xA0: o SoloSel o tira das células (`rowChara`, ids reais); se algum
  caminho levasse um marcador 0xA1..0xA3 ao `QPROG->member.chara`, o leitor passaria dos 161 registros do disco (o
  jogo do PS2 também).
- Os mínimos das seções 24 (216) e 32 (113) saem dos literais e das fontes de item que o código mostra; um `msgLine`
  calculado acima de 0xD7 não aparece no C (os roteiros de evento estão todos em C).
- Os registros 1 das seções 20 e 31 e 1..4 da 21, a coluna 4 dos eventos e os pools depois do 999 não têm leitor:
  convertidos pela forma (registros de `TexFile`, s32).
- O enchimento depois das rodadas (0x3C bytes) e dos adversários (12 bytes) é zero no disco e o conversor o exige.
- Os três códigos de ação do Flash (6.1) são dados do filme que o tocador do jogo lê como códigos sem efeito; se o
  filme do evento depende deles para algo, isso já não funcionava no PS2.

## Parte E: os ramos do Disc Fusion

Continuação de `menus-historia.md`, `menus-telas.md`, `menus-pecas.md` e `menus-modos.md` (seção 3.4). Aqui ficam os
dois arquivos que só os ramos do Disc Fusion leem: o ranking de 100 lugares ("Ultimate Battle", `UbRank`, modo 26) e
os cinco cursos ("Ultimate Battle Z", `UbzSel`, modo 28), mais os retratos que o ranking carrega. Conversores em
`menus_ub_ubE.c` (`Conv_MenuUbRanking`, `Conv_MenuUbCursos`), registrados em `sFormatos` e `sFaixas` de `conv.c`.
Nenhuma peça nova: só as de `conv_menus.h` (`Conv_TexFileMin`, `Conv_Flash`, `Conv_JanelaIcone`,
`Conv_FichasPersonagem`, `Conv_Textos`, `Conv_LipPack`) e quatro auxiliares internos para as tabelas de registros.

Regra da conversão (decisões 8 e 12): in-place, cada número trocado pela largura com que o código o lê, bytes e
strings intactos, enchimento exigido zero, nada muda de tamanho nem de lugar. Cada `TexFile` leva o mínimo de
registros que o leitor usa (1 + o maior índice de `MTEX`; 8 num fundo do `Sprite_DrawPicture`), o N de cada tabela é
exato (`Conv_Secoes`).

Fontes: o código da decomp (arquivo e linha de antes dos ganchos desta etapa; os ganchos deslocam as linhas seguintes
de cada arquivo, seção 5); o disco inteiro pelo `conv_tool --disco` e contagens de estrutura no WSL (só números
voltaram). Nenhum byte do jogo está aqui. **Hipótese** marca o que o código não prova.

### 1. Os arquivos

| Arquivo | Id (como o código calcula) | Índice | Forma | Formato do arquivo | Conteúdo |
|---|---|---|---|---|---|
| ranking | `baseFile + 0x1B` = 0x1DC (`UbRank_Init`, src/menu/menu_n_c.c:139) | 475 | BPE inteiro | `menu_bpe` | `menu_ub_ranking` |
| cursos | `baseFile + 0x1C` = 0x1DD (`UbzSel_Init`, src/menu/menu_n_b.c:97) | 476 | BPE inteiro | `menu_bpe` | `menu_ub_cursos` |
| retratos | `0x2F9 + personagem` (`N_FACE_FILE`, menu_n_c.c:704, :705) | 760..920 deste leitor (faixa 760..923) | BPE inteiro | `menu_bpe` (já) | `menu_texfile` (já) |

Quem chama, e o formato que cada gancho passa (`Port_ConvertMenuPack(buffer, rawSize, "formato")`):

| Chamada | Conteúdo | Buffer | Formato |
|---|---|---|---|
| `UbRank_Init` menu_n_c.c:140 | 475 inteiro | novo (`Sprite_Unpack(file, NULL, NULL)`), livre no `UbRank_Term` | `menu_ub_ranking` |
| `UbzSel_Init` menu_n_b.c:98 | 476 inteiro | novo, livre no `UbzSel_Term` | `menu_ub_cursos` |
| `UbRank_UpdateFaceLoad` menu_n_c.c:714 | retrato do jogador (`NPROG->chara`) | `faceRes[0]`, fixo de 0x20800 | `menu_texfile` |
| `UbRank_UpdateFaceLoad` menu_n_c.c:715 | retrato do adversário (`curFoe->chara`) | `faceRes[1]`, fixo de 0x20800 | `menu_texfile` |

Conteúdos descomprimidos: 475, 3.294.144 bytes (0x3243C0); 476, 2.404.608 (0x24B100); abaixo dos 8 MB de
`CONV_JOGO_MAX`. No disco: 475 com 2.059.728 bytes, 476 com 1.593.280.

### 2. As tabelas de registros (larguras)

Todas lidas por campo de struct, em leitura nativa alinhada de 4: cada campo é um s32, trocado como u32. Nenhum campo de
largura misturada.

| Struct | Tamanho | Campos (deslocamento: nome) | Quem lê |
|---|---|---|---|
| `NRankRule` (include/menu/menu_n.h:131) | 0x1C | 0x00 announcer, 0x04 unk4, 0x08 timeLimit, 0x0C stage, 0x10 bgm, 0x14 unk14, 0x18 foe | `UbRank_SetupBattle` menu_n_c.c:34..:40 (os seis primeiros; 998 = `N_RANDOM` em announcer, stage e bgm); `UbRank_SetFoe` :91, :95 e `UbRank_Draw` :330, :362, :395 (foe, índice da tabela de lutadores) |
| `NFoe` (menu_n.h:111) | 0x2C | 0x00 chara, 0x04 color, 0x08 cpuLevel, 0x0C lastItem, 0x10 item[7] | ranking: `UbRank_SetupBattle` :57, :63, :66, :74; `UbRank_Draw` :330, :362, :395, :400, :411 (chara: ficha `chara + 1` e linha `chara` dos nomes). Cursos: `UbzSel_SetupBattle` menu_n_b.c:53, :63, :73, :79, :84; `UbzSel_Draw` :218 (chara; 999 = `N_NONE` usa a ficha 165) |
| `NCourse` (menu_n.h:120) | 0x38 | 0x00 announcer, 0x04 unk4, 0x08 timeLimit, 0x0C stage, 0x10 bgm, 0x14 unk14, 0x18 foe[8] | `UbzSel_SetupBattle` menu_n_b.c:32..:38, :53, :63, :66; `UbzSel_Draw` :218 (os oito foe, mesmo depois de um `N_NONE`) |
| listas do ranking | 4 | s32 | `UbRank_SetupBattle` :49 (`stageList[Rand_Range(0x1C)]`), :52 (`bgmList[Rand_Range(0xB)]`) |

`UbScorePrice` não está nestes arquivos: é a seção 13 do pacote dos resultados (451 s3, `UbResult_Init`
menu_n_d.c:121, `UbScore_CalcPoints`), do agente do 00451.

### 3. O ranking (`menu_ub_ranking`: 00475, o contêiner inteiro)

`UbRank_Init` (src/menu/menu_n_c.c:133, modo 26). Tabela: N = 18 (o comentário do header, menu_n.h:189, diz 18; o
disco confirma). Todas as seções têm leitor.

| Seção | Tamanho | Formato | Registros mínimos | Quem lê |
|---|---|---|---|---|
| 1 | 0x7C00 | Flash | | `Flash_Create(&flash[0], ..., tex)` :216 (a lista) |
| 2 | 0x9AC0 | `TexFile`, os ícones da janela de ícone | 2 | `UR_RES(2)` :222, `IconWin_Init(pack 7, res)` :223 (registros 0 e 1; o UbRank não chama `IconWin_SetIcon`) |
| 3 | 0xCD880 | `TexFile` | 30 | `UR_RES(3)` :157, `MTEX` 0..19, 22..29 (:158..:198) |
| 4 | 0x42A40 | `TexFile`, o fundo | 8 | `UR_RES(4)` :151, `Sprite_DrawPicture` (`UbRank_Draw` :314) |
| 5 | 0x484C0 | `TexFile` | 4 | `UR_RES(5)` :153, `MTEX` 0, 1, 3 (o disco tem 10) |
| 6 | 0xDDE40 | SubpacoteTex, 165 fichas de personagem | 1 em cada | `MPACK_AT(res, 6)` :211, laço de `Res_RelocateOffsets` :212..:215; `UR_CHIP_RES(chara)` lê o registro 0 (:332, :364, :398, :400, :411) |
| 7 | 0x16580 | JanelaIcone | | `IconWin_Init` :223 |
| 8 | 0xB00 | regras dos lugares (`NRankRule`, 3.1) | 99 | `gUbRank->rule` :145 |
| 9 | 0x440 | regras dos intrusos (`NRankRule`) | 37 | `gUbRank->intruderRule` :146 |
| 10 | 0x1140 | lutadores dos lugares (`NFoe`, 3.2) | o alcance das regras: 99 no disco | `gUbRank->foe` :147 |
| 11 | 0x680 | lutadores dos intrusos (`NFoe`) | o alcance: 37 no disco | `gUbRank->intruderFoe` :148 |
| 12 | 0x80 | lista de estágios (3.4) | 28 s32 | `gUbRank->stageList` :149 |
| 13 | 0x40 | lista de músicas (3.4) | 11 s32 | `gUbRank->bgmList` :150 |
| 14 | 0x2CC0 | textos (`menus.md` 6.1), os nomes | N >= 0xA1 (3.3) | `TextBox_Init(box, text)` :227; `TextBox_AttachLine(..., chara, ...)` :328, :335, :360, :367 |
| 15 | 0x1540 | Flash | | `Flash_Create(&flash[1], ..., texB)` :218 (o painel do versus) |
| 16 | 0x1C80 | Flash | | `Flash_Create(&flash[2], ..., texC)` :220 (o filme do fim) |
| 17 | 0xB9E00 | `TexFile` | 11 | `UR_RES(17)` :199, `MTEX` 0..10 |
| 18 | 0x3D00 | LipPack (`menus-pecas.md` 4), as legendas do guia | N >= 0xA4 (3.3) | `Voice_PlayWithSubtitle(subtitles, ...)` :518, falas 0x4A, 0x4B, 0x4C, 0x50, 0x51, 0x35 (:497..:516) |

No disco: seção 3 com 30 registros, 17 com 11 e 2 com 2 (exatamente o mínimo); 14 com N = 168; 18 com N = 176 (88
falas, nenhuma vazia, de 3 a 33 chaves).

#### 3.1 As regras (`NRankRule`, seções 8 e 9)

- **Lugares (8):** o lugar `place` (0 = primeiro, 0..99) usa a regra `UbRank_PlaceToRule(place)` (:80): os lugares
  abaixo do jogador andam um para cima, e o do jogador não tem regra (ele é desenhado com `playerChip`). O maior índice
  é 98 (o lugar 99 com o jogador acima dele); `UbRank_SetFoe(NPROG->ubChoice, 0)` (:244) só roda com `ubChoice` acima do
  jogador (`NPROG_UB_UPWARD`), então também fica em 0..98: **99 regras** (`UBRANK_PLACES - 1`).
- **Intrusos (9):** `UbRank_SetFoe(Rand_Range(0x25), 1)` (:115): **37 regras** (0..36).
- Cada regra: sete s32 (seção 2); o conversor confere que `foe` cabe na tabela de lutadores da seção seguinte (o
  alcance = 1 + o maior `foe`).
- No disco: 99 regras de lugar e 0x2C bytes zerados depois; 37 de intruso e 0x34 zerados. Nas regras de lugar, stage é 998
  (`N_RANDOM`) nas 99 e bgm em 98; as de intruso têm estágio e música fixos. O conversor exige zero
  depois dos registros que o leitor usa.

#### 3.2 Os lutadores (`NFoe`, seções 10 e 11)

- Registros: os que as regras alcançam (1 + o maior `foe`): 99 e 37 no disco, cada tabela seguida de zeros (0x3C e
  0x24 bytes). Onze s32 cada.
- `chara` de 0 a 0xA0: é a ficha `chara + 1` do subpacote de 165 (seção 6), a linha `chara` dos nomes (seção 14) e o
  personagem do lado 1 no `BattleSetup_SetMember` (:74). No disco o maior é 0xA0 (um intruso). O conversor confere.
- `item[k]` e `lastItem` são ids de item menos 1, ou 999 (`N_NONE`, pulado); o código não os confere aqui (vão ao
  `BattleSetup_SetMember` como `item + 1`) e o conversor também não.

#### 3.3 Os mínimos dos textos e das legendas

- `TextBox_AttachLine(..., line, box)` lê a entrada `line + 1` da tabela de textos (src/battle/view_b.c:240): a linha
  `chara` (até 0xA0) pede N >= 0xA1. Disco: 168.
- `Voice_PlayWithSubtitle(pack, base, line)` lê as entradas `2 line + 1` e `2 line + 2` (view_b_c.c:38, :43): a fala
  0x51 pede N >= 0xA4. Disco: 176.

#### 3.4 As listas sorteadas (seções 12 e 13)

`stageList[Rand_Range(0x1C)]` e `bgmList[Rand_Range(0xB)]` (:49, :52), só quando a regra tem `N_RANDOM`: 28 e 11
s32, a lista começa em +0 (não é a ListaIds, sem contagem). Disco: 28 estágios (de 0 a 34) e 4 palavras zeradas; 11
músicas e 5 palavras zeradas. O conversor troca as 28 e as 11 e exige zero no resto.

### 4. Os cursos (`menu_ub_cursos`: 00476, o contêiner inteiro)

`UbzSel_Init` (src/menu/menu_n_b.c:91, modo 28). Tabela: N = **11** no disco; o comentário do header (menu_n.h:148)
diz "a pack of 10 sections", e o código lê 1..10. A 11 não tem leitor (4.3).

| Seção | Tamanho | Formato | Registros mínimos | Quem lê |
|---|---|---|---|---|
| 1 | 0xA1C0 | Flash | | `Flash_Create(&flash[0], ..., tex)` :148 |
| 2 | 0x9AC0 | `TexFile`, os ícones da janela de ícone | 2 | `UZ_RES(2)` :151, `IconWin_Init(pack 7, res)` :152 (0 e 1) |
| 3 | 0xAA6C0 | `TexFile` | 25 | `UZ_RES(3)` :107, `MTEX` 0..24 (:108..:132) |
| 4 | 0x42A40 | `TexFile`, o fundo | 8 | `UZ_RES(4)` :101, `Sprite_DrawPicture` (`UbzSel_Draw` :193) |
| 5 | 0x484C0 | `TexFile` | 4 | `UZ_RES(5)` :103, `MTEX` 0, 1, 3 |
| 6 | 0xDDE40 | SubpacoteTex, 165 fichas | 1 em cada | laço :143..:147; `UbzSel_Draw` :221 (`MPACK_AT(chips, N_CHIP_NUM)` para `N_NONE`, a ficha 165) e :223 |
| 7 | 0x16580 | JanelaIcone | | `IconWin_Init` :152 |
| 8 | 0x140 | cursos (`NCourse`, 4.1) | 5 | `gUbzSel->course` :99 |
| 9 | 0x700 | lutadores (`NFoe`, 4.2) | o alcance dos cursos: 40 no disco | `gUbzSel->foe` :100 |
| 10 | 0x9580 | textos, os nomes dos cursos | N >= 0x148 | `TextBox_Init` :156; `TextBox_AttachLine(..., i + 0x143, ...)` :202, i = 0..4 |
| 11 | 0x3D00 | LipPack pela forma, sem leitor (4.3) | | ninguém |

No disco as seções 4, 5, 6, 7 e 11 dos cursos são iguais byte a byte às 4, 5, 6, 7 e 18 do ranking; a 2 difere (os
dois `TexFile` têm 2 registros). A seção 3 tem 25 registros (o mínimo) e a 10, N = 350.

#### 4.1 Os cursos (`NCourse`, seção 8)

`gUbzSel->course[cursor[0]]`, cursor 0..4 (`N_COURSE_NUM`, `UbzSel_Input` :313..:323): **5 registros** de quatorze s32,
depois zeros (0x28 bytes no disco). `UbzSel_Draw` (:218) lê `foe[course.foe[i]].chara` nos oito i a cada quadro, mesmo
depois do primeiro `N_NONE`: os oito índices têm de caber na tabela de lutadores (o alcance = 1 + o maior, 40 no
disco). `UbzSel_SetupBattle` (:63) põe `foe[course.foe[0]]` no lado 1 sem olhar o `N_NONE`: o conversor exige que o
primeiro lutador de cada curso não seja 999. No disco os cursos têm 5, 6, 8, 7 e 8 lutadores antes do primeiro 999.

#### 4.2 Os lutadores dos cursos (`NFoe`, seção 9)

40 registros (o alcance) e 0x20 bytes zerados. `chara` de 0 a 0xA0 ou 999 (`N_NONE`: o fim da lista do curso; no disco
os seis registros 999 têm todos os campos 999). O resto como 3.2.

#### 4.3 A seção 11 dos cursos (sem leitor)

`gUbzSel->subtitles` nunca é atribuído (menu_n.h:151; `UbzSel_UpdateVoice` :292 toca a fala com a tabela nula), e nada
mais lê `MPACK_AT(res, 11)`. No disco ela é uma LipPack (N = 176) igual à seção 18 do ranking: converte-se pela forma
(`Conv_LipPack`), como as seções sem leitor de forma conhecida das outras telas. Vazia, o conversor a aceita.

### 5. Os retratos do ranking (`menu_texfile`)

`UbRank_UpdateFaceLoad` (menu_n_c.c:691): com `loadState` = `UBRANK_LOAD_REQUEST` (pedido pelo X na lista, :607), pede
os arquivos `0x2F9 + NPROG->chara` (o lutador do jogador, da seleção do modo 25) e `0x2F9 + curFoe->chara` (o
adversário escolhido) para `faceFile[0..1]` (0x16800 bytes cada, :141), e no estado `UBRANK_LOAD_UNPACK` descomprime os
dois seguidos em `faceRes[0..1]` (0x20800 cada, :143) e lê só `MTEX(res, 0)` de cada (`texC[11]`, `texC[10]`, :718,
:721): o `menu_texfile` de 1 registro, já na faixa 760..923.

- Os personagens: 0..0xA0 dos dois lados (o do jogador sai da seleção; o do adversário, das tabelas 3.2, conferidas):
  arquivos 760..920.
- Cabem nos buffers: no disco o maior retrato tem 0x160C0 bytes (00766), abaixo dos 0x16800 do `File_Request`; o
  maior `rawSize` é 0x20560 (00923), abaixo dos 0x20800 do `faceRes`. Nenhum dos 164 passa.
- Duas chamadas seguidas (:714, :715): um gancho logo depois de cada uma. Depois dos ganchos ficam em :720 e :727 (e o
  `UbRank_Init` continua em :140, com as linhas seguintes deslocadas em 6).
- O `faceRes` é reaproveitado: cada `Sprite_Unpack` reescreve o conteúdo inteiro, então converter depois de cada um
  não converte duas vezes.

### 6. Escritas do jogo depois da conversão

- Nos dois pacotes: só o `Res_RelocateOffsets` dos `TexFile` (os ponteiros, `menus.md` 9), inclusive as 165 fichas a
  cada `UbRank_Init` / `UbzSel_Init` (o buffer é novo a cada vez). As tabelas de registros, as listas, os textos e as
  legendas são só lidos.
- Nos retratos: o `Res_RelocateOffsets` de `faceRes[0..1]` (:717, :720).
- O que as telas gravam vai ao save e ao `gProgress`, não aos buffers: `gSaveData->rank--` no `UbRank_Input` (:660);
  `NPROG->ubChoice`, `ubFlags`, `teamSize`, `dpRule`.

### 7. Cobertura no disco (`conv_tool --disco`, 14/10/2026)

Sem erro, 100% de cobertura: 00475 e 00476 como `menu_bpe` (2.059.728 e 1.593.280 bytes, 8 em números trocados cada:
os dois s32 do cabeçalho BPE) e os conteúdos `pzs3us1/00475.bin@0x0#bpe` (3.294.144 bytes, 135.702 em números
trocados) e `pzs3us1/00476.bin@0x0#bpe` (2.404.608 bytes, 147.302), pelos dois caminhos (o da `conv_tool` e o do jogo,
`Conv_FormatoBits`), com os mesmos bytes e a mesma cobertura. Os retratos 760..923 continuam com 164 de 164.

| | Antes | Depois |
|---|---|---|
| `disco` (arquivos) | 3295 sem erro, 0 com erro | 3297 sem erro, 0 com erro |
| `disco` (descomprimidos) | 675 sem erro, 0 com erro | 677 sem erro, 0 com erro |
| censo da faixa dos menus 448..477: arquivo | 15 convertidos, 15 sem conversor | 17 e 13 |
| censo da faixa dos menus: conteúdo BPE (49) | 22 com conversor, 27 sem | 24 e 25 |

Todas as outras linhas do `--disco` são iguais às de antes (os aprovados continuam aprovados).

### 8. Os conversores vistos falhando

No WSL, sobre cópias do conteúdo antes da conversão (o `.orig` do `conv_tool --marcas`), cada uma estragada num ponto
e convertida com `--formato`: as cópias intactas saem com 0 e as estragadas com 1 e o nome da seção.

| Formato | Estrago | Mensagem |
|---|---|---|
| `menu_ub_ranking` | nenhum (cópia intacta) | "convertido", saída 0 |
| `menu_ub_ranking` | o `TexFile` da seção 3 com 29 registros | "texturas em 0x11740: 29 registros, o leitor usa 30 [seção 3 do ranking do Disc Fusion]" |
| `menu_ub_ranking` | a regra 5 com o lutador 0x200 | "lutadores dos lugares em 0x25F1C0: 0x1140 bytes, o leitor alcança 513 registros de 0x2C [seção 10 do ranking do Disc Fusion]" |
| `menu_ub_ranking` | um byte não nulo no enchimento depois das 99 regras | "regras dos lugares: byte 0x25ED55 não reconhecido (não é enchimento) [seção 8 do ranking do Disc Fusion]" |
| `menu_ub_ranking` | o lutador intruso 3 com o personagem 0xA1 | "lutadores dos intrusos: lutador 3 com o personagem 161 [seção 11 do ranking do Disc Fusion]" |
| `menu_ub_ranking` | uma palavra não nula depois dos 28 estágios | "estágios: byte 0x2609F0 não reconhecido (não é enchimento) [seção 12 do ranking do Disc Fusion]" |
| `menu_ub_ranking` | a tabela com 17 seções | "Conv_Tabela: fim 0x3206C0, arquivo 0x3243C0 [ranking do Disc Fusion]" |
| `menu_ub_cursos` | nenhum (cópia intacta) | "convertido", saída 0 |
| `menu_ub_cursos` | o `TexFile` da seção 3 com 24 registros | "texturas em 0x13CC0: 24 registros, o leitor usa 25 [seção 3 dos cursos do Disc Fusion]" |
| `menu_ub_cursos` | o curso 0 começando num lutador 999 | "cursos: o curso 0 começa sem lutador [seção 8 dos cursos do Disc Fusion]" |
| `menu_ub_cursos` | o curso 2 com `foe[7]` = 40 (há 40 lutadores) | "lutadores dos cursos em 0x23D780: 0x700 bytes, o leitor alcança 41 registros de 0x2C [seção 9 dos cursos do Disc Fusion]" |
| `menu_ub_cursos` | a legenda 1 da seção 11 com 0x7FFF chaves | "boca em 0x247700: 32767 chaves em 0x40 bytes [legenda 1] [seção 11 dos cursos do Disc Fusion]" |

### 9. Hipóteses e o que fica aberto

- A seção 11 dos cursos não tem leitor; converte-se como LipPack porque tem essa forma no disco (é igual à 18 do
  ranking). Se algum dia o código a ler, já está convertida.
- Os campos que a tela só repassa à luta (announcer, unk4, timeLimit, stage, bgm, unk14, color, cpuLevel, os itens)
  não têm limite conferido: o leitor deles é o `BattleSetup_*`, e as larguras (s32) vêm das structs.
- Um `foe` que aponte para um registro zerado dentro do alcance passa (o jogo leria um lutador com o personagem 0, sem
  cair); só o que sai da seção vira erro.
- A comparação de memória x86 contra PPC só precisa saber do `Res_RelocateOffsets` (seção 6); nenhuma escrita de dado.
