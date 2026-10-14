# Os arquivos da luta de um episódio da História (F4, etapa 9a)

Três arquivos que a luta de um episódio da História lê inteiros na carga e que nenhum conversor conhecia até aqui (a
tabela `CONHECIDOS` do `roteiros.py` os listava pelos do primeiro episódio: 00510, 00560 e 00710). Escrito do código
da decomp e conferido no disco inteiro (`conv_tool --disco`) e contra o rastreio do roteiro `historia` (o da etapa 4,
que chega à luta do primeiro episódio). Conversor: `port/x360/conv/historia_luta.c`; formatos `roteiro_luta`,
`textos_luta` e `labio_luta` na tabela única (`sFaixas`, `conv.c`), todos `CONV_ARQ`: o arquivo se converte inteiro
na leitura (`conv_tool --saida` para a PPC, `iso360.c` no 360).

Convenção: índice = id - 1 na partição 1 (`pzs3us1/NNNNN.bin`). "Lido como" diz a largura com que o jogo lê.

## 1. Os arquivos e as faixas

| Formato | Ids | Índices | Quem pede | Quem lê |
|---|---|---|---|---|
| `roteiro_luta` | 0x1FF..0x230 | 510..559 | `BtlLoad` estado 0x5A: `File_Request(job->kind + BTL_FILE_SCRIPT)`, `kind = Battle_GetScript()` (battle_load.c:612, :810) | `Gsc_LoadFile`, `Gsc_RunAction(1000)`, `BtlScript_ScanEvents` (battle_load.c:620), o motor (src/sys/gsc.c) e os 46 comandos (src/battle/btl_script_cmd.c) |
| `textos_luta` | 0x231..0x294 | 560..659 | comando 701 (`BtlScriptCmd_LoadText`): 0x231 + história com `gProgress->unk0` 0, 0x263 + história com 1 | `BtlScript_GetText` (btl_script.c:288), desenhado por `Font_GetHeight` e `Font_PrintAt` (texto u16, src/battle/col_c.c) |
| `labio_luta` | 0x295..0x2F8 | 660..759 | comando 702 (`BtlScriptCmd_LoadFile8`): 0x295 + história, ou 0x2C7 + história com `SAVE_FLAG_VOICE` | `BtlScript_GetFile8Entry` (btl_script.c:300), entregue ao rosto do lutador que fala (`BtlFacade_SetObjSubState14`, `BOBJ_MOUTH_LIP_PTR` em src/battle/bobj_a.c:2120) |

- **O número do roteiro:** só o `ModeMenu_Run` o escolhe (`BattleSetup_SetScript(ModeMenu_GetLine(subMenu, item))`,
  menu_a_d.c:703), e o `ModeMenu_GetLine` dá 0..47 (3 + 4 + 7 + 5 + 16 + 5 + 4 + 4 episódios, com 20..23 permutados).
  O disco tem 50 roteiros (510..559, todos GSCF); os dois últimos nenhum menu pede, e se convertem igual.
- **O número da história** (comando 2, `BtlScript_SetStory`) vem do próprio roteiro e escolhe os textos e as bocas.
  As quatro faixas de 50 vão de 0x231 até os retratos em 0x2F9 (0x2F9 - 0x231 = 4 x 50), e o disco confirma: 560..759
  são todos a tabela de 300 entradas das seções 3 e 4.
- O índice 0x2C7 - 1 = 710 do primeiro episódio é a boca com o conjunto de vozes do save (`SAVE_FLAG_VOICE`); 660..709
  é a do outro conjunto. Os dois têm o mesmo formato.

## 2. O roteiro de evento (GSCF)

### 2.1 Os blocos

Todo bloco (`GscChunk`, include/sys/gsc.h) é um cabeçalho de quatro palavras u32 {etiqueta, headSize, dataSize, arg},
os dados (dataSize bytes, a partir de bloco + headSize) e um bloco "EOFC" de 0x10 bytes depois deles
(`GSC_NEXT_CHUNK` = bloco + 0x10 + headSize + dataSize). O jogo lê:

- a etiqueta como u32 e a compara depois de `GSC_SWAP` com `GSC_TAG_GSAC` (`Gsc_FindAction`, `BtlScript_ScanEvents`,
  os dois `Gsc_Dump*`). Trocada como número, a palavra dá o mesmo valor nas duas ordens, e o `GSC_SWAP` também: a
  etiqueta se converte como u32;
- headSize e dataSize como u32 (`Gsc_LoadFile`, `GSC_NEXT_CHUNK`), arg como s32 (o id da ação em `Gsc_FindAction` e
  no `BtlScript_ScanEvents`).

O arquivo: GSCF { GSHD, EOFC, GSCD { GSAC, EOFC, GSAC, EOFC, ... }, EOFC, GSDT, EOFC }, EOFC. O `Gsc_LoadFile` não
procura os blocos pela etiqueta: o primeiro bloco dentro do GSCF é o cabeçalho, o seguinte o código, o terceiro os
valores. O laço das ações para na primeira etiqueta que não é GSAC: a do EOFC do GSCD, logo depois da última ação.
No disco (50 arquivos): headSize 0x10 em todo bloco, os blocos sempre nessa ordem, cada EOFC {"EOFC", 0x10, 0, 0}, o
GSCF fechando exatamente no fim do arquivo menos o EOFC final, e as ações fechando exatamente no fim dos dados do
GSCD. O conversor exige tudo isso.

### 2.2 GSHD

16 bytes de dados que ninguém lê (o `Gsc_LoadFile` guarda o ponteiro em `GscFile.head` e nada o usa): bytes.

### 2.3 GSCD: as ações e as palavras

Cada GSAC é uma ação (arg = id; -1 a principal, 1000 a de carga, 10000..10051 os eventos) com palavras de 32 bits,
lidas pelo `Gsc_StepTask`, `Gsc_NextCommand`, `Gsc_FindOptionN`, `Gsc_SeekOption`, `Gsc_CountOption(s)`,
`Gsc_NotifyAll` e `BtlScript_ScanEvents`. O tipo é o nibble baixo:

| Tipo | Palavra | Campos e como o jogo os lê |
|---|---|---|
| 0 | fim da ação | `*cmd & 0xF` (u32) |
| 1 | comando | tipo (u32 `& 0xF`), byte 1 = número de operandos simples (`((u8 *)cmd)[1]`), bytes 2..3 = id do comando (`((u16 *)cmd)[1]`) |
| 2, 3 | chamada (3: espera a ação acabar) | tipo (u32), bytes 2..3 = id da ação (`((s16 *)cmd)[1]`) |
| 8 | opção | tipo e o bit 3 (u32 `& 0xF`, `& GSC_WORD_ARG`), byte 1 = a letra (`word[1]`), byte 2 = quantos operandos seguem (`word[2]`) |
| 10 | operando | u32: tipo, o bit 3, o tipo do valor nos bits 4..7 (que nenhum código lê) e o índice do valor nos bits 8..31 (`word >> 8`, `GscTask_PeekOperand`; `cmd[1] >> 8` no `BtlScript_ScanEvents`) |

Depois de um comando vêm os n operandos simples e então toda palavra com o bit 3 ligado (opções com os seus
operandos, e operandos avulsos), como no `Gsc_NextCommand`. O conversor anda cada ação por esse caminho até a palavra de
fim e troca cada palavra como u32; depois do fim, até o fim dos dados da ação, o disco tem só zeros (1.472 palavras em
745 das 1.010 ações), e o conversor os exige. No disco: 14.595 comandos (bits 4..7 sempre 0), 240 chamadas (só tipo
2), 18.187 opções (bits 4..7 e byte 3 sempre 0), 62.516 operandos (40.425 int, 22.089 float, 2 cadeia), todo
operando aponta um valor do GSDT e nenhum valor é apontado com dois tipos.

### 2.4 GSDT: os valores

Palavras de 4 bytes apontadas pelos operandos. Os comandos as leem por `Gsc_GetInt` (s32), `Gsc_GetFloat` (f32) e as
variantes `...Or`, todos uma leitura de 4 bytes: u32. O `Gsc_GetString` (o endereço do valor, uma cadeia de bytes)
nenhum comando chama. O conversor dá a cada valor o tipo dos operandos que o apontam: int e float como u32; cadeia
(tipo 2) em bytes até o NUL, arredondada a 4, sem nenhum outro operando apontando dentro dela; valor que nenhum operando
aponta tem de ser zero (o enchimento até múltiplo de 4 valores). No disco há duas cadeias, de 6 bytes, uma em cada um
de dois roteiros, operandos do comando 1301 (`BtlScriptCmd_Nop1301`, que não lê nada); e 73 valores sem operando, todos
zero ou a segunda palavra dessas cadeias.

### 2.5 As leituras de byte e de meia palavra: correção no código (pendente)

A palavra do roteiro é lida com duas larguras: o tipo, o bit 3 e o índice do operando como u32; a contagem, a letra e a
contagem da opção como bytes; o id do comando e da ação como a metade de cima. Nenhuma ordem dos bytes no arquivo serve
às duas leituras no big-endian (com a palavra trocada como u32, o byte 1 da memória é o byte 2 do número; sem trocar,
o tipo do comando sairia do byte do id). O rastreio mostra isso: 1.779 posições do 00510 lidas como byte ou u16 dentro
de uma palavra lida como u32, que o `rastreio.py` deixa nos conflitos (825 de 1 byte, 279 de 2 e 675 de 4 na lista,
por `Gsc_NextCommand`, `Gsc_StepTask`, `Gsc_FindOption(N)` e `BtlScript_ScanEvents`).

Escolha: o conversor troca a palavra inteira como u32 (as leituras de u32 são a maioria, e o índice do operando ocupa
24 bits), e o código lê os campos de byte e de meia palavra pelo deslocamento do número, só em `PORT_BIG_ENDIAN`: em
include/sys/gsc.h, `GSC_BYTE1(w)` = `(u8)(*(u32 *)w >> 8)`, `GSC_BYTE2(w)` = `(u8)(*(u32 *)w >> 16)`, `GSC_HALF1(w)`
= `(u16)(*(u32 *)w >> 16)` e `GSC_SHALF1(w)` = `(s16)(...)`, definidas só em `PORT_BIG_ENDIAN`; em cada lugar, um
`#ifdef PORT_BIG_ENDIAN` com a leitura pela macro e, no `#else`, a linha original intacta (o upstream só muda dentro do
`#ifdef`; os objetos da x86 saem iguais aos de antes, a menos da informação de depuração, que muda com as linhas).
Aplicada na etapa 9a (24 lugares: 23 em `gsc.c` e 1 em `btl_script.c`). Os lugares: `Gsc_NotifyAll`, `Gsc_StepTask` (três), `Gsc_FindOptionN` e
`Gsc_SeekOption` (três cada), `Gsc_CountOptions` (duas), `Gsc_CountOption` (três), `Gsc_NextCommand` (três), os dois
`Gsc_Dump*` (sem efeito: a impressão foi compilada fora) e `BtlScript_ScanEvents` (btl_script.c:119). Desses, 14 estão em funções que nada chama (`Gsc_NotifyAll`, `Gsc_SeekOption`, `Gsc_CountOptions`, `Gsc_CountOption` e os dois `Gsc_Dump*`, sem chamador em `src/` nem em `port/`): nenhuma rodada os executa, e foram conferidos só por leitura. **Sem essa
correção a PPC e o 360 leem a contagem e o id errados no primeiro comando e o roteiro da luta da História se perde.**

### 2.6 Os ids de item: `BtlFacadeItem` (correção no código, aplicada na etapa 9a)

Os ids de item de um membro são s32 no roteiro e o jogo os lê pela metade baixa (u16) em dois lugares:

- o comando 12 (`BtlScriptCmd_SetMember`) lê os oito ids por `Gsc_GetInt` numa cópia local (`BtlScriptCmdItems
  items`, os operandos 7 a 13 e o 5), entrega essa cópia ao `BattleSetup_SetMemberByItemIds`, que lê cada um com
  `PS2_LOW(u16, itemIds[i])` (battle_load.c:1579, certo nas duas ordens), e a guarda em `work->items[lado][membro]`;
- os comandos 13 e 14 (`BtlScriptCmd_SetStatus0/1`) mudam ids nessa cópia guardada (as opções `-0`..`-6` e `-a`) e a
  entregam ao `BtlFacade_SetMemberItems` como `BtlFacadeItem *`, que lê `id` como o u16 do começo de cada s32. No
  big-endian esse u16 é a metade alta (0): o item viraria o 1.

O segundo é visão com outra largura no código, não no arquivo (o valor do GSDT está certo como u32): a correção é
permutar `BtlFacadeItem` em `PORT_BIG_ENDIAN` ({unk2, id}) e acrescentá-la a `layout.PERMUTADAS`.

Efeito no `--mapa`: as posições desses valores que o rastreio só viu ler pela metade saem "com outra largura" (o
rastreio da x86 leu 2 bytes no começo do valor, o conversor trocou 4); as que ele viu também por um leitor de 4 bytes
estão nos conflitos ([2, 4], seção 5). No 00510 são duas, 0x2368 (301) e 0x236C (125): no GSDT, cada uma é operando
só do comando 12 (os operandos 8 e 9, `items.id[1]` e `id[2]`), e o rastreio da x86r as viu ler só pelo
`BattleSetup_SetMemberByItemIds` e pelo `BtlFacade_SetMemberItems`. É o resultado certo para um valor s32 lido pela
metade baixa **por esses dois leitores**, e o `--mapa` não tem classe para isso: o `roteiros.py larguras` aceita a
posição uma a uma, na lista `LARGURA_CONHECIDA`, com o porquê e os leitores, e confere no rastreio da rodada
(`rastreio.tsv`, os pcs que ele grava por posição, pelo programa da x86r) que nenhuma outra função a leu; com outro
leitor, com a lista de pcs cheia (4: pode haver um leitor não anotado) ou sem a posição no rastreio, falha. Qualquer
outra posição com outra largura continua erro.

Por que o leitor e não só a posição: o GSDT é um pool de constantes deduplicadas (0x2310 = -1 é operando de 44
lugares, 0x2300 = 0 de 96), e a mesma posição pode ser operando de vários comandos; um leitor novo de 2 bytes sem
`PS2_LOW` nem struct permutada leria a metade errada no big-endian e passaria por uma lista só de posições. E o
`--mapa` não vê o endereço lido no big-endian: o rastreio é da x86. Na 9b e na 9c isso pesa (das 1.846 leituras de id
de item nos 48 roteiros, 891 distinguem as metades).

## 3. Os textos das falas (`textos_luta`)

A tabela de deslocamentos de `personagem.md` 2 (contagem, deslocamentos, o fim igual ao tamanho, enchimento até
alinhar a 16) com exatamente 300 entradas (`BtlScriptTalk.speakers[300]`: uma por fala), e em cada entrada um texto u16
terminado por um u16 zero (`Font_PrintAt`, `FONT_CMD_STR16`). O `BtlScript_GetText(n)` lê a palavra n + 1 da tabela
como u32 e a arredonda a 4 (`/ 4 * 4`). No disco: os 100 arquivos com 300 entradas, nenhuma vazia, todas em múltiplo de
4, e depois do NUL só zeros; o conversor troca a entrada inteira em u16 (`Conv_Texto`).

## 4. As bocas das falas (`labio_luta`)

A mesma tabela de 300 entradas; cada entrada vazia (a fala sem boca: 10.120 e 10.124 nas duas faixas) ou um
`BObjLipData` (include/battle/bobj_b.h, `personagem.md` 6.6): 8 bytes que ninguém lê, u16 `count`, 6 bytes que ninguém
lê, `count` chaves `BObjLipKey` {u16 quadro, u16 boca} a partir de +0x10 (`BOBJ_MOUTH_LIP_PTR` lê o `count`, as chaves
e o quadro da última), e zeros até o fim da entrada. No disco: toda entrada não vazia cabe, tem `count` maior que 0, os
quadros em ordem e zeros depois das chaves; +0x0A..+0x0F sempre zero, +0x00..+0x07 não (bytes sem leitor, como no
lutador). O conversor é o `Conv_Labio` do lutador.

## 5. O que o disco e o rastreio deram

- `conv_tool --disco`: 3.295 arquivos convertidos sem erro (os 250 novos, 510..759, com 100% de cobertura cada), e os
  675 conteúdos descomprimidos sem erro.
- `conv_tool --mapa` (rastreio do `historia` da etapa 4): 00510, cobertura 10.768 de 10.768 bytes, 722 posições, 720
  iguais, 2 com outra largura (2.6), 0 não trocadas; 00560, 27.520 de 27.520, 148 posições, todas iguais; 00710, 13.440
  de 13.440, 70 posições, todas iguais. Na rodada final da 9a (`93227ed`), o 00510 com 752 posições, 750 iguais e as
  mesmas 2 com outra largura.
- **Os conflitos do GSDT do 00510** (`mapa.tsv.conflitos` da rodada final da 9a; o `rastreio.py` não inverte conflito,
  e o `conv_tool --mapa` não os confere): além das 1.779 posições das palavras do GSCD (2.5), dez valores do GSDT. Os
  leitores são os pcs do `rastreio.tsv` (até 4 por posição; os de byte na última coluna), resolvidos na x86r; os
  comandos, da análise dos operandos do arquivo. Nenhum é erro de ordem: cada leitura estreita é de uma cópia do
  número ou de uma leitura que dá o mesmo valor nas duas ordens.

  | Posição | Valor | Larguras | Leitores (rastreio) | Operando de | Por que está certo |
  |---|---|---|---|---|---|
  | 0x2300 | 0 | [1, 4, 16] | `Gsc_GetInt`, `Gsc_GetIntOr`, `BtlLoad_StepInitial`, `BtlScript_GetStory` (lista cheia); byte: `McCard_Checksum` | 96 operandos (comandos 2, 7, 8 a 13, 15, 801, 807, 901, 1302...) | o valor é 0: toda leitura, de qualquer largura e em qualquer ponto, dá o mesmo nas duas ordens. A de 16 bytes é uma cópia em bloco que a lista cheia não nomeia |
  | 0x2314 | 1 | [1, 4] | `Gsc_GetInt`, `Gsc_GetIntOr`, `BtlScript_ScanEvents`, `BtlLoad_StepInitial`; byte: `McCard_Checksum` | 90 operandos | as leituras de 4 são do número (as do roteiro por `Gsc_GetInt` e as das cópias dele em campos s32); o único leitor de byte é o `McCard_Checksum` (stgm_a_b.c:939), que soma os bytes de um bloco de propósito (o save, o cabeçalho de slot ou o replay) e sai na ordem da build (etapa 8) |
  | 0x2330 | 2 | [1, 4] | `Gsc_GetInt`, `Gsc_GetIntOr`, `BtlScript_ScanEvents`, `File_UpdateRequests`; byte: `McCard_Checksum` | 37 operandos | o mesmo: 4 bytes do número, o byte só pelo `McCard_Checksum` |
  | 0x233C | 3 | [1, 4] | `Gsc_GetInt`, `BtlScriptCmd_Talk`, `Port_AdxTick`; byte: `McCard_Checksum` | 6 operandos (comandos 10, 12, 15, 1302, 1603) | o mesmo; o `Port_AdxTick` (port/src/plat_sndstate.c:30) lê campos `int` do `PortAdxView` (`stat`, `left`), aonde o rastreio levou uma cópia do valor |
  | 0x2348 | 15 | [1, 4] | `Gsc_GetInt`, `Battle_GetBgm`, `McCard_Checksum`; byte: `McCard_Checksum` | comando 10 (`SetRule`, a música) e 15 | o `Battle_GetBgm` devolve `SETUP()->rule.bgm` como s32 (battle_load.c:1810); o byte só pelo `McCard_Checksum` |
  | 0x2310 | -1 | [2, 4] | `BattleSetup_SetMemberByItemIds` (os 4 pcs, lista cheia) | 44 operandos: itens do comando 12, recompensas do 16, comandos 8 e 9 | o valor é -1 (0xFFFFFFFF): toda metade dá 0xFFFF nas duas ordens. A leitura de 2 é o `PS2_LOW` dos itens (item -1 vira 0, sem item) |
  | 0x2370 | 102 | [2, 4] | `Gsc_GetInt`, `BattleSetup_SetMemberByItemIds`, `BtlFacade_SetMemberItems` | comando 12 (item `id[3]`), 7 e 15 (`-v`, número de fala) | as leituras de 2 são o `PS2_LOW` e o `BtlFacadeItem` permutado (2.6), as de 4 o número da fala por `Gsc_GetInt` |
  | 0x2374 | 104 | [2, 4] | os mesmos três | comando 12 (`id[4]`), 7 e 15 | idem |
  | 0x2378 | 95 | [2, 4] | os mesmos três | comando 12 (`id[5]`) e 15 | idem |
  | 0x25C4 | 10031 | [8], desalinhado | `BtlScript_StartPending` | comando 7 (`-a`, a ação do gatilho) | a cópia da struct `BtlScriptStart` {s32 action, ponteiro} inteira (`*cur = work->pending->u.start`, btl_script.c:638) numa leitura de 8 bytes que começa no `action`: cópia de campos nativos, cada um na ordem da build; nenhum campo é lido por outra largura |

  Regra para os próximos: conflito de arquivo convertido também se explica, leitor a leitor, como as
  `DIFERENTES_CONHECIDAS`; a coluna de pcs de byte do `rastreio.tsv` diz quem lê o byte.
- Visto falhando (cópias estragadas no WSL, cada uma com saída 1 e a mensagem com o trecho): a etiqueta da primeira
  ação trocada ("etiqueta em 0x50 não é GSAC [ação 0]"), o dataSize do GSCD aumentado ("etiqueta em 0x2300 não é
  GSDT"), um operando com índice fora do GSDT ("operando em 0x104 com valor 65535 de 444 [ação 3]"), a contagem da
  tabela de textos 299 ("fim 0x6B40, arquivo 0x6B80"), um deslocamento de texto fora de ordem ("entrada 3 em 0x5C0
  fora de ordem"), a contagem de uma boca 0x7FFF ("lábio em 0x4C0: 32767 chaves [boca 1]"). As cópias intactas
  convertem com 100%.

## 6. F4 ou F5

Os três são dados do roteiro da História: eventos, falas e a boca de quem fala, sem modelo, efeito ou desenho. A boca
é a mesma faixa de lábio que o arquivo do lutador já converte. Convertíveis já (F4). O que falta para a luta da História
andar igual no big-endian são as duas correções de código de 2.5 e 2.6, não a F5.

## 7. Hipóteses

- Que 0x263 + história seja a outra língua e 0x2C7 o outro conjunto de vozes: o formato é o mesmo, e o disco confirma
  as quatro faixas pela forma.
- Os dois roteiros sem episódio (558 e 559) nenhum menu pede; se convertem pela mesma forma.
