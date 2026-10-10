# Bancos de som, folhas da tela de carregamento e rostos dos membros: especificação para a conversão de ordem de bytes

Especificação, por formato e por tipo, de três famílias de arquivos que a luta usa e que ainda não tinham
documento: os **bancos de som** (`SndBankFile`: 00329, 00330, 00334, 03199 e 03231 na demo, 361 no disco), as
**folhas de sprites da tela de carregamento** (00961 na demo, comprimida em BPE, e as irmãs 00960 e 00962) e as
**folhas de rosto dos membros** (00017 e 00082 na demo, 322 no disco). Para cada uma: quem carrega e como o id
sai, o formato campo a campo com a largura e quem lê (o jogo ou o port), o que vira ponteiro na carga, as partes
opacas e como delimitá-las, e a cobertura conferida contra o rastreio e contra todos os arquivos do mesmo tipo.

Regra da conversão (decisões 8 e 12): in-place, cada número de 2, 4 ou 8 bytes trocado de ordem pela largura com
que o código o lê, bytes intactos, nada muda de tamanho nem de lugar; pacotes da GPU como palavras u32, salvo campo
de struct u64 lido como u64; dado comprimido se converte depois de descomprimido, no caminho de quem usa o formato.

Peças já especificadas que este documento referencia em vez de repetir: a tabela de deslocamentos
(`personagem.md` seção 2), `TexFile`/`TexEntry` e os pacotes de imagem e paleta (`personagem.md` 4.4), o contêiner
BPE (`personagem.md` 6.2), a visão do HUD com `mark` em `TexEntry` + 0x28 (`comuns.md` 6.3) e a regra de id para
arquivo (`comuns.md` 1.1: o arquivo NNNNN da partição 1 é o id NNNNN + 1).

Fontes: o código da decomp e do port (arquivo e linha citados em cada seção) e, como conferência, o rastreio da
luta de demonstração no WSL (`port/build/tipos.tsv`, `port/build/mapa.tsv`, `port/build/mapa.tsv.conflitos`) e
todos os arquivos de `gamedata/pzs3us1/` lá. Nenhum byte do jogo está neste documento: só contagens,
deslocamentos e conclusões. "Não determinado" quer dizer que o código não deixa claro e que não foi inventado um
tipo. Convenções de largura e das colunas T (posições distintas de `tipos.tsv`) e M (posições de número de
`mapa.tsv`, expandidas pelas repetições) como em `comuns.md` 1.4.

## 1. Bancos de som (`SndBankFile`)

### 1.1 O caminho no PS2 e no port

No PS2 o jogo não toca efeito nenhum: ele entrega o banco ao processador de som (IOP) e manda comandos. O lado do
jogo está em `src/sys/snd.c` (comentário do topo, linhas 1..64) e compila sem mudança no port:

- `Snd_LoadBank(mask, file, sync)` (src/sys/snd.c:502) e `Snd_ReloadBank` (snd.c:522) chamam `Snd_SetupBank`
  (snd.c:359), que lê o cabeçalho do arquivo e calcula ponteiros e tamanhos das três seções **no `SndBank` do
  gerenciador** (include/sys/snd.h:108), não no arquivo. Depois `Snd_SendBank` (snd.c:413) copia as seções `hd` e
  `seq` para blocos do heap do IOP por `Dma_SendToIop` (src/sys/dma.c:343, que monta um `SifDmaData` na pilha e
  chama `sceSifSetDma`) e manda a seção de amostras para a memória do chip de som (SPU2) passando pelo buffer de
  0xA4800 bytes do IOP (`Snd_SpuTransfer`, agora ou depois em `Snd_Update`). Por fim `SET_BANK` dá ao driver os
  três endereços.
- O que o jogo lê do arquivo: as quatro últimas palavras do cabeçalho, a palavra `hd + 0x30` (endereço do bloco Vagi) e,
  dentro do Vagi, `maxNumber` e o byte de laço de cada amostra (`SndHd_GetVagMax`, `SndHd_GetLoopFlags`,
  `SndHd_GetLoopBits`, snd.c:759..792). Nada mais: o resto das tabelas da Sony era lido pelo `SOUNDS.IRX` no IOP
  (MIPS little-endian), com o sequenciador (modsesq2) e o sintetizador (modhsyn) da Sony.
- No port de PC não há IOP. `port/src/gs/snd_se.c` faz as vezes do driver: `sceSifSetDma` (snd_se.c:431) é um
  `memcpy` para memória comum, `sceSdRemote` 0x80D0 (`func_002967C0`, snd_se.c:444) copia as amostras para um
  vetor de 2 MB que faz o papel da memória do SPU, e `sceSifCallRpc` (snd_se.c:458) atende os comandos. O
  `SET_BANK` (snd_se.c:472) guarda os ponteiros das cópias de `hd` e `seq`, e o tocador lê essas cópias em
  `lookup` (snd_se.c:107): sequência, timbre e amostra, **sempre com `le32`/`le16`, byte a byte**.
  `voice_own_bank` (snd_se.c:376) e `bank_parts` (snd_se.c:351) leem ainda, direto do disco com `fopen`, os
  bancos de vozes do outro conjunto (jogo em rede).

Consequência: há dois leitores do mesmo arquivo, com regras diferentes. O jogo lê seus campos como número nativo
(precisam ser invertidos no 360); o port lê os seus byte a byte (não podem ser invertidos, a menos que o port mude).
Os campos lidos pelos dois são os pontos de decisão (1.8).

### 1.2 Quem carrega e como o id sai

Todos os bancos estão em `pzs3us1`, nenhum em `pzs3us0` nem em `pzs3us2` (varredura de todos os arquivos do
disco no WSL pelos blocos "SCEI" da Sony: 361 arquivos, todos nas faixas abaixo, todos com a tabela de 3 entradas).

| Arquivos (índice) | Ids | Máscara (slot) | Quem carrega | Buffer | Na demo |
|---|---|---|---|---|---|
| 00329 | 0x14A (`BTL_FILE_SND_COMMON`, include/battle/battle_work.h:115) | 4 (slot 2) | `BtlLoad_StepInitial`: lê em src/battle/battle_load.c:501, `Snd_LoadBank` em :514, libera em :524 depois do envio | novo, do heap | sim |
| 00330 | 0x14B (`COMMON_FILE_SND_BANK`, include/sys/common.h:11) | 1 (slot 0, `SND_BANK_COMMON`) | `Common_Reload` (src/sys/common.c:37 e :41, síncrono), uma vez na partida; libera em :42 | novo | sim |
| 00331 | 0x14C (constante em src/menu/menu_a_b.c:103) | 2 (slot 1) | `Snd_LoadBankFile` (snd.c:555: `File_LoadSync`, carga síncrona, libera); descarregado em menu_a_b.c:218 | novo | não (menu) |
| 00332 | 0x14D | | **nenhum leitor achado**: nem no C nem por nome nas funções ainda em assembly (`asm/` do WSL) | | não |
| 00333..00367 | 0x14E + cenário, cenário 0..0x22 (`BTL_FILE_SND_STAGE`, battle_work.h:116; macro `BTL_STAGE_BANK`, battle_load.c:23) | 8 (slot 3, tamanho fixo) | `BtlLoad_StepInitial` (:502, :515); recarga no mesmo slot por `BtlLoad_StepStageReload` (:216, :223) e `BtlLoad_StepStageChange` (:273, :280) com `Snd_ReloadBank` | inicial: novo; recarga: `BattleRes.bank`, fixo de 0x90800 bytes (:565..567) | 00334 (cenário 1) |
| 03033..03193 | 0xBDA + personagem (`BTL_FILE_VOICE`, battle_work.h:123) | 0x10 (lado 0) ou 0x20 (lado 1), slots 4 e 5, tamanho fixo | `BtlLoad_StepInitial` (:503..504, :516..517) e `BtlLoad_StepChara` (:410, recarga em :456/:458) | idem | não |
| 03194..03354 | 0xC7B + personagem (`BTL_FILE_VOICE_ALT`, battle_work.h:124) | idem | idem; o conjunto sai de `gSaveData->flags & SAVE_FLAG_VOICE` | idem | 03199 (personagem 5) e 03231 (personagem 37) |

Notas:
- 161 personagens em cada conjunto de vozes; a faixa começa logo depois do último arquivo de slot de lutador
  (índice 3032, `port/x360/conv/conv.c` `formato_de`), então não colide com nenhum formato já reconhecido.
- No port, cenário acrescentado de fora do disco (id >= 0x24) pede o banco do cenário 0 (battle_load.c:23).
- Os nomes dos dois conjuntos de vozes divergem entre `battle_work.h` (0xBDA padrão, 0xC7B com o bit) e o
  comentário de `snd_se.c` (0xC7B padrão). Para a conversão não importa: as duas faixas têm o mesmo formato.
- 00332 (0x14D) tem a forma de um banco de cenário (11 amostras, seções `hd` e `seq` do mesmo tamanho). Converter
  igual aos outros.

Recomendação para o conversor: reconhecer pelos índices **329..367 e 3033..3354** e validar a estrutura (1.11)
antes de converter.

### 1.3 Cabeçalho do arquivo: a tabela de 3 entradas

O cabeçalho é a tabela de deslocamentos de `personagem.md` seção 2 com N = 3. A decomp o declara como
`SndBankFile` (include/sys/snd.h:99):

| Desl. | Campo | Tipo | Quem lê | Significado |
|---|---|---|---|---|
| 0x00 | `unk00` | u32 | port (`bank_parts`, `le32`) | N = 3 (o jogo não lê) |
| 0x04 | `bodyOfs` | u32 | jogo (`Snd_SetupBank`, snd.c:362, :369); port (`bank_parts`, `le32`) | início das amostras = 0x40 em todos |
| 0x08 | `hdOfs` | u32 | jogo (snd.c:362, :364, :369, :382, :760); port | início da seção `hd` |
| 0x0C | `seqOfs` | u32 | jogo (snd.c:364, :366, :382, :395); port | início da seção `seq` |
| 0x10 | `endOfs` | u32 | jogo (snd.c:366, :395); port | tamanho do arquivo |
| 0x14..0x3F | | | ninguém | zeros |

Tamanhos das seções (`Snd_SetupBank`): amostras `[bodyOfs, hdOfs)`, `hd` `[hdOfs, seqOfs)`, `seq`
`[seqOfs, endOfs)`. Nos slots fixos o jogo reserva tamanhos fixos (máscara 8: SPU 0x13800, `hd` 0x800, `seq`
0x800; máscaras 0x10 e 0x20: SPU 0x8F000, `hd` 0x1000, `seq` 0x800; snd.c:369..404). Conversão: as cinco palavras
como u32 (a tabela inteira, como em `personagem.md` 2).

### 1.4 Seção de amostras (`body`)

Amostras ADPCM do chip de som do PS2 (formato VAG sem cabeçalho), em blocos de 16 bytes: byte 0 = deslocamento
(4 bits baixos) e filtro (4 bits altos), byte 1 = flags (1 = último bloco, 2 = e repete, 4 = início do laço), bytes
2..15 = 28 amostras de 4 bits, a de baixo primeiro. **Tudo é byte.** O jogo não lê nada daqui (só copia); o port
decodifica byte a byte (`decode_block`, snd_se.c). Conversão: nenhuma, a seção inteira fica intacta.

Delimitação: o fim útil é `Head.bodySize` (1.5); dali até `hdOfs` há zeros. O primeiro bloco é zero em todos (o
bloco de silêncio que a Sony põe no começo). Cada amostra começa em `body + vagOffset` (1.5, Vagi).

### 1.5 Seção `hd`: o cabeçalho HD da Sony

Blocos ("chunks") da Sony, cada um começando por `creator` = "SCEI" e `type` gravados como u32 little-endian (no
arquivo aparecem invertidos, "IECS", "sreV" e assim por diante). Deslocamentos a partir do início da seção.
Os endereços dentro de `Head` são deslocamentos a partir do início da seção; os de dentro de cada bloco, a partir
do início do bloco.

**Vers (0x00, 0x10 bytes)**: 0x00 `creator`, 0x04 `type`, 0x08 tamanho do bloco = 0x10, 0x0C versão (o mesmo valor
nos 361 arquivos; a divisão em campos não é determinada pelo código). Ninguém lê no jogo nem no port.

**Head (0x10, 0x40 bytes)**, deslocamentos dentro do bloco:

| Desl. | Campo (nome Sony) | Tipo | Quem lê | Nos 361 arquivos |
|---|---|---|---|---|
| 0x00, 0x04 | creator, type | u32 | ninguém (o port procura os nomes byte a byte) | "SCEI", "Head" |
| 0x08 | chunkSize | u32 | ninguém | 0x40 |
| 0x0C | headerSize | u32 | ninguém | fim do último bloco (Setb); a seção é ele arredondado a 0x40 |
| 0x10 | bodySize | u32 | ninguém | bytes de amostras; a seção `body` é ele arredondado a 0x40 |
| 0x14, 0x18, 0x1C | programChunkAddr, samplesetChunkAddr, sampleChunkAddr | u32 | ninguém | -1 (não há) |
| **0x20** | **vagInfoChunkAddr** | **u32** | **jogo**: `SndHd_GetVagInfo` lê `head[0xC]` (snd.c:755), duas vezes por banco | 0x50 |
| 0x24 | seTimbreChunkAddr | u32 | ninguém (o port acha o Setb procurando o nome) | início do Setb |
| 0x28..0x3F | reservado | u32 x 6 | ninguém | -1 |

**Vagi (0x50)**: `SndVagInfoChunk` + `SndVagParam` (include/sys/snd.h:143, :136).

| Desl. | Campo | Tipo | Quem lê |
|---|---|---|---|
| 0x00, 0x04 | creator, type | u32 | port, só o nome, byte a byte (`chunk`, snd_se.c:94) |
| 0x08 | size | u32 | ninguém |
| **0x0C** | **maxNumber** | **u32** | **jogo** (`SndHd_GetVagMax`, snd.c:760, e `GetLoopFlags`, snd.c:766) **e port** (`lookup`, `le32(vagi + 0x0C)`) |
| 0x10 | offset[maxNumber + 1] | u32 | port (`le32`), deslocamento do registro a partir do bloco |
| 0x10 + 4 x (maxNumber + 1) | registros de 8 bytes, um por amostra | | |
| +0x00 | vagOffset | u32 | port (`le32`): início da amostra em `body` |
| +0x04 | sampleRate | u16 | port (`le16`): Hz |
| +0x06 | loop | u8 | **jogo** (`SndHd_GetLoopFlags`, snd.c:770) e port: 0 ou 1 |
| +0x07 | | u8 | ninguém: 0 ou 0xFF |
| até `size` | enchimento | bytes | ninguém: 0xFF (0 a 12 bytes) |

O jogo não usa `offset[]`: acha os registros em `&offset[maxNumber + 1]` (snd.c:766). Nos 361 arquivos os
registros vêm exatamente ali (`offset[i]` = 0x10 + 4 x (maxNumber + 1) + 8 x i), então o jogo lê os bytes certos.
`maxNumber` vai de 8 a 0x54 (9 a 85 amostras), dentro do limite de 0x7F que o vetor `flags[0x80]` de
`SndHd_GetLoopBits` impõe (snd.c:782) e dos 152 bits de `SndBank.loopBits`. `GetLoopBits` só leva os laços de 0 a
`maxNumber - 1` (o da última amostra não entra): comportamento do jogo, sem efeito na conversão.

**Setb (logo depois do Vagi, em `Head.seTimbreChunkAddr`)**, deslocamentos dentro do bloco:

| Desl. | Tipo | Quem lê | Nos 361 arquivos |
|---|---|---|---|
| 0x00, 0x04 | u32 | port, só o nome | "SCEI", "Setb" |
| 0x08 | u32 | ninguém | tamanho do bloco; o bloco termina em `Head.headerSize` |
| 0x0C | u32 | ninguém | 0 |
| 0x10 | u32 | ninguém | 0x14 |
| 0x14 | u32 | ninguém | 0 |
| 0x18 | u32 | ninguém | 0x1C |
| 0x1C | u32 | ninguém | 8 em todos (não é o número de amostras) |
| 0x20 | u8 | port (`setb[0x20]`) | tamanho do registro de timbre = 0x32 |
| 0x21 | u8 | ninguém | 0 |
| 0x22 | u8 | port (`setb[0x22]`) | maior número de timbre = `Vagi.maxNumber` |
| 0x23 | u8 | ninguém | 0xFF |
| 0x24 + 0x32 x i | registro de timbre i (maxNumber + 1 deles) | | |
| +0x00 | u16 | port (`le16`) | número da amostra = i em todos |
| +0x02..+0x31 | 0x30 bytes | ninguém (o sintetizador do IOP no PS2) | parâmetros do timbre: idênticos nos 18.924 registros do disco |
| até o tamanho | enchimento | ninguém | 0xFF (2 a 12 bytes) |

As larguras de 0x0C..0x1F seguem o padrão dos outros blocos (palavras de deslocamento e contagem), mas nenhum
código as lê: não determinado pelo código. Os 0x30 bytes de parâmetros também não.

Depois de `Head.headerSize`, zeros até o fim da seção (alinhamento 0x40).

### 1.6 Seção `seq`: o cabeçalho SQ da Sony

**Vers (0x00, 0x10 bytes)**: como na seção `hd` (mesmo valor de versão nos 361).

**Sequ (0x10, 0x20 bytes)**: 0x00/0x04 nome; 0x08 tamanho = 0x20; 0x0C tamanho útil da seção (`fileSize`: a seção é
ele arredondado a 0x40, zeros no resto); 0x10 e 0x14 -1; 0x18 = 0x30 (endereço do Sesq); 0x1C -1. Todos u32,
ninguém lê.

**Sesq (0x30)**, deslocamentos dentro do bloco:

| Desl. | Tipo | Quem lê | Nos 361 arquivos |
|---|---|---|---|
| 0x00, 0x04 | u32 | port, só o nome | "SCEI", "Sesq" |
| 0x08 | u32 | ninguém | tamanho; o bloco termina em `Sequ.fileSize` |
| 0x0C | u32 | ninguém | 0 |
| 0x10 | u32 | ninguém | 0x10 |
| 0x14 | 4 bytes | ninguém | o mesmo valor em todos; divisão em campos não determinada |
| 0x18, 0x1C | u32 | ninguém | -1 |
| 0x20 | u32 | ninguém | 0x24 |
| 0x24 | u32 | ninguém | `Vagi.maxNumber` |
| 0x28 | u32 | ninguém | 0xC |
| 0x2C | 4 bytes | ninguém | o mesmo valor em todos (como 0x14) |
| 0x30 | u32 | ninguém | -1 |
| 0x34 | offset[maxNumber + 1] | port (`le32(sesq + 0x34 + 4 x id)`) | deslocamento da sequência a partir do bloco; crescentes, a primeira logo depois da tabela |
| sequência | | | uma por efeito (o "id" do jogo) |
| +0x00 | u32 | ninguém | 0x10 |
| +0x04 | 4 bytes | ninguém | o mesmo valor em todas |
| +0x08 | u32 | ninguém | comprimento dos comandos (7 em 18.922 efeitos, 0xD em 2) |
| +0x0C | u32 | ninguém | 0 |
| +0x10 | comandos (bytes) | port lê +0x11 (= 0x90, nota ligada) e +0x13 (timbre = id) | sequência MIDI de uma nota |
| até a próxima | enchimento | ninguém | 0 ou 0xFF (1 a 13 bytes) |

Medido nos 361 bancos: todo efeito é uma nota no timbre do próprio número, e todo timbre usa a amostra do próprio
número (o que o port já documenta em snd_se.c, linhas 9..16).

### 1.7 Quem lê o quê: resumo

| Campo | Largura | Jogo | Port | Conversão (proposta, 1.8) |
|---|---|---|---|---|
| cabeçalho 0x04..0x13 (`bodyOfs`..`endOfs`) | u32 | nativo | `le32` em `bank_parts` | u32 |
| cabeçalho 0x00 (N) | u32 | não | `le32` em `bank_parts` | u32 (tabela inteira) |
| `hd` + 0x30 (`Head.vagInfoChunkAddr`) | u32 | nativo | não | u32 |
| `Vagi.maxNumber` | u32 | nativo | `le32` em `lookup` | u32 |
| `Vagi.offset[]`, `vagOffset`, `sampleRate` | u32, u32, u16 | não | `le32`/`le16` | intactos (bytes) |
| `Vagi` loop | u8 | byte | byte | byte |
| `Setb` 0x20, 0x22 | u8 | não | byte | byte |
| `Setb` número da amostra no registro | u16 | não | `le16` | intacto |
| `Sesq.offset[]` | u32 | não | `le32` | intacto |
| bytes de comando das sequências | u8 | não | byte | byte |
| nomes dos blocos, tamanhos, endereços da Sony, parâmetros, enchimentos | u32 e bytes | não | não (o nome é comparado byte a byte) | intactos |
| amostras ADPCM | bytes | não | byte | byte |

### 1.8 A regra para os bancos e o ponto que depende de decisão

Pela regra "largura com que o código lê", as leituras `le32`/`le16` do port são leituras byte a byte (independem
da máquina) e não pedem inversão; as leituras do jogo são nativas e pedem. Dois grupos de campos são lidos pelos
dois: o cabeçalho de 5 palavras (jogo nativo, port `le32` em `bank_parts`) e `Vagi.maxNumber` (jogo nativo, port
`le32` em `lookup`). Conflito do tipo "byte e número": não se resolve no conversor sozinho. Duas saídas:

- **A (proposta): converter só o que o jogo lê, mais a contagem da tabela (7 números por banco), e mudar o port nesses dois pontos.**
  O conversor inverte as 5 palavras do cabeçalho, `hd + 0x30` e `Vagi.maxNumber`; todo o resto do arquivo fica
  como está. No port, sob `#ifdef PORT_BIG_ENDIAN`, `lookup` passa a ler `maxNumber` em ordem nativa e
  `bank_parts` lê o cabeçalho em ordem nativa; o resto do tocador continua little-endian byte a byte, como hoje.
  Vantagens: toda largura vem do código (nenhum campo da Sony com largura não determinada precisa ser
  adivinhado), sete posições por arquivo, e o backend de áudio do 360 (F6) reaproveita o tocador quase inteiro.
- **B: converter as tabelas da Sony inteiras para big-endian** (nomes, tamanhos, endereços, `offset[]`,
  `vagOffset`, `sampleRate`, número da amostra do Setb) e o port passa a ler tudo em ordem nativa. Custa: o port
  tem de trocar todas as leituras e a busca por nome (o nome invertido como u32 vira "SCEI" na ordem de
  leitura); e campos que ninguém lê (Vers 0x0C, Sesq 0x14/0x2C, sequência +0x04, parâmetros do Setb) teriam de
  ganhar largura sem prova no código.

Recusado: deixar o arquivo como está e trocar as leituras do jogo por uma macro little-endian. São campos de
struct lidos por código casado do jogo, e o resto da conversão segue a regra contrária.

Decisão do Lucas. Este documento descreve as duas; o rastreio não distingue (só viu as leituras do jogo, 1.11).

### 1.9 Escritas e quando converter

O jogo **não grava nada no arquivo de banco**: `Snd_SetupBank` escreve só no `SndBank` do gerenciador (ponteiros
EE para as seções, tamanhos, `vagMax`, `loopBits`); as cópias no heap do IOP e na memória do SPU são feitas do
arquivo já convertido. O port também não grava no arquivo. Nada vira ponteiro dentro dele.

Momento: com o instalador da decisão 12, uma vez no disco, sem dependência de ordem. Em tempo de jogo seria antes
de cada `Snd_LoadBank`/`Snd_ReloadBank`, porque o buffer `BattleRes.bank` é relido a cada troca de personagem ou
de cenário e os bancos iniciais são buffers novos. Atenção: `voice_own_bank` lê bancos com `fopen`, por fora da
camada de arquivos do jogo; com conversão em tempo de jogo esses bancos chegariam sem converter. Com o instalador
o problema não existe.

### 1.10 O que não é do conversor (código)

- `sceSifCallRpc`, comando `QUEUE` (snd_se.c:507): lê `count`, `handle` e `pitch` da fila de comandos com `le32` e
  `le16`. A fila é memória gravada pelo jogo em ordem nativa (`SndQueue`, include/sys/snd.h), não dado do disco:
  numa build big-endian essas três leituras saem trocadas. É classe 4 do guia (visão com outra ordem), a corrigir
  no port junto com a F6. A build sem janela da F1 não passou por ali (sem dispositivo de áudio, `stream_ready` é
  falso e o laço não roda).
- `SifDmaData` do port (snd_se.c:428) guarda os ponteiros em `uint32_t`: serve no 360 (ponteiros de 32 bits).

### 1.11 Cobertura conferida

**Contra o rastreio**, com a especificação da saída A (7 números, o resto bytes):

| Arquivo | Id | Papel | T | M | Largura igual |
|---|---|---|---|---|---|
| 00329 | 0x14A | comum da luta (máscara 4) | 87 | 6 | 83 de 87 em T; 6 de 6 em M. As 4 restantes são os conflitos abaixo |
| 00330 | 0x14B | partida (máscara 1) | 14 | 6 | todas |
| 00334 | 0x14F | cenário 1 (máscara 8) | 16 | 6 | todas |
| 03199 | 0xC80 | vozes do personagem 5 (0x10) | 62 | 6 | todas |
| 03231 | 0xCA0 | vozes do personagem 37 (0x20) | 62 | 6 | todas |

Por papel: em T, 4 posições do cabeçalho (`bodyOfs`..`endOfs`, catálogo `SndBankFile`), `hd + 0x30`
(`u32 *[]`), `Vagi.maxNumber` (`SndVagInfoChunk`) e os bytes de laço (`u8 *[]`, vistos pela cópia local em
`SndHd_GetLoopBits`, snd.c:788). Em M, as mesmas 6 palavras (`mapa.tsv`: 4 a partir de +4, e as duas do `hd`).
Em 00334 o slot fixo (máscara 8) não lê o tamanho das seções `hd` e `seq` (snd.c:382, :395), por isso há menos
linhas por campo, sem mudar as posições. Em 00329 o rastreio viu 81 dos 84 laços lidos (os índices 81..83 não
aparecem); as posições vistas batem.

**Os 4 conflitos de `mapa.tsv.conflitos` (00329 +533274, +533306, +533338, +533370)**: são os bytes de laço das
amostras 68, 72, 76 e 80, e a "leitura como u32" vem de `Dma_SendToIop` (src/sys/dma.c:347..350) e de
`sceSifSetDma` (snd_se.c:435) lendo os campos do `SifDmaData` local, que ocupa na pilha o lugar onde
`SndHd_GetLoopBits` tinha o vetor `flags[0x80]` com a origem desses bytes. Artefato de pilha do rastreio, como o
01801 737884/737890 (`personagem.md` 6.3.1): não há leitura de número no arquivo; a especificação os mantém como
bytes e o mapa corretamente não os inverte. São os "? 4" e "SifDmaData 3" do catálogo.

O rastreio não vê nenhuma leitura do port nos bancos: sem dispositivo de áudio o tocador não roda, e
`voice_own_bank` sai cedo fora do jogo em rede.

**Contra os 361 bancos do disco** (no WSL, só contagens): todos com N = 3, fim = tamanho, `bodyOfs` = 0x40, as
quatro marcas múltiplas de 0x40 e em ordem, zeros em 0x14..0x3F; amostras com tamanho múltiplo de 16, primeiro
bloco zero, `Head.bodySize` múltiplo de 16 e zeros dali ao fim da seção; Vers na seção `hd` com tamanho 0x10, Head
em 0x10 com tamanho 0x40, `vagInfoChunkAddr` = 0x50 com o Vagi lá, `seTimbreChunkAddr` apontando o Setb logo
depois do Vagi, o Setb terminando em `headerSize` e zeros até o fim da seção; registros do Vagi logo depois dos
deslocamentos, `vagOffset` crescente, múltiplo de 16 e menor que `bodySize`, `sampleRate` de 8.000 a 24.000 Hz,
laço 0 ou 1 (0 a 3 amostras com laço por banco), `maxNumber` <= 0x54; Setb com registros de 0x32 e número da
amostra = índice; seção `seq` com Vers, Sequ (tamanho 0x20, Sesq em 0x30) e o Sesq terminando em `fileSize`,
zeros até o fim da seção, deslocamentos crescentes, toda sequência com a nota ligada no timbre do próprio índice.

Tamanhos por grupo (cabem nos buffers e slots do jogo):

| Grupo | Arquivos | Tamanho | Amostras (`body`) | `hd` | `seq` | Amostras por banco |
|---|---|---|---|---|---|---|
| 00329 | 1 | 540.224 | 0x81F00 | 0x1540 | 0x9C0 | 85 |
| 00330 | 1 | 36.480 | 0x8A00 | 0x2C0 | 0x180 | 9 |
| 00331 | 1 | 679.360 | 0xA4040 (cabe no buffer de 0xA4800 do IOP) | 0x1400 | 0x940 | 80 |
| 00332 | 1 | 51.904 | 0xC580 | 0x340 | 0x1C0 | 11 |
| cenários 00333..00367 | 35 | 25.792 a 80.576 | até 0x13580 (slot 0x13800) | 0x340 (slot 0x800) | 0x1C0 (slot 0x800) | 11 |
| vozes 03033..03193 | 161 | 123.904 a 591.104 (buffer 0x90800) | até 0x8EF80 (slot 0x8F000) | 0xE80 (slot 0x1000) | 0x6C0 (slot 0x800) | 57 |
| vozes 03194..03354 | 161 | 121.536 a 414.016 | até 0x63BC0 | 0xE80 | 0x6C0 | 57 |

Validação que o conversor deve fazer antes de converter um banco: as condições da tabela de deslocamentos (N = 3,
fim = tamanho, ordem), o nome "Head" em `hd + 0x10`, `hd + 0x30` dentro da seção com o nome "Vagi" lá, e
`maxNumber` <= 0x7F com os registros dentro do bloco. Enchimento: dentro dos blocos da Sony ele é 0xFF (Vagi,
Setb, Head reservado, byte 7 dos registros) ou misto (Sesq); como tudo isso fica em bytes na saída A, o conversor
não exige zero ali. Exige zero em 0x14..0x3F do cabeçalho e no fim de cada seção.

### 1.12 Não determinado

- Quem carrega 00332 (id 0x14D).
- Largura dos campos que só o IOP lia: Vers 0x0C, Setb 0x0C..0x1F e os 0x30 bytes de parâmetros, Sesq 0x14 e
  0x2C, sequência +0x04. Só importa na saída B.
- Por que o rastreio não viu os laços 81..83 de 00329 (as outras posições batem).

## 2. Folhas da tela de carregamento: `00960`, `00961`, `00962` (BPE com `TexFile` dentro)

### 2.1 Quem carrega e como o id sai

`Load_InitScreen` (src/sys/loading.c:158) sorteia o tipo da tela (0..2, nunca o da vez anterior) e lê o id
`LOAD_FILE_FIRST + tipo` = 0x3C1..0x3C3 (include/sys/loading.h), ou seja os arquivos **00960, 00961 e 00962**:

```
gProgress->loadPack = File_LoadSync(gLoadScreen.type + LOAD_FILE_FIRST, gProgress->loadPack, LOAD_PACK_MAX); /* :165 */
Sprite_Unpack(gProgress->loadPack, gProgress->loadRes, NULL);                                                 /* :166 */
gLoadScreen.res = gProgress->loadRes;
Res_RelocateOffsets(&gLoadScreen.res, gLoadScreen.res, gLoadScreen.res);                                      /* :168 */
```

- `loadPack` (0x3000 bytes) e `loadRes` (0x6800 bytes) são alocados uma vez em `Progress_Init`
  (src/battle/view_a_e.c:48..49) e reaproveitados a cada tela de carregamento.
- `Sprite_Unpack` (src/sys/sprite.c:10) é só `Bpe_Decode`: **a descompressão acontece em `Load_InitScreen`, para o
  buffer fixo `loadRes`**, e cada tela decodifica de novo por cima.
- Quem lê o resultado: `Res_RelocateOffsets` (src/sys/gfxm_b_b.c:244) e o desenho em `Load_DrawScreen`
  (loading.c:282) por `Sprite_DrawList` (sprite.c:91), que chama `TexFile_UploadOne` (gfxm_b_b.c:294) →
  `Tex_UploadPixels`/`Tex_UploadClut` (gfxm_b_b.c:262, :271) e lê `res->tex[i]` pela visão `SpriteTex`
  (include/sys/sprite.h:27). Os sprites usados vêm de `Load_BuildSprites` (tipo 0: texturas 0 e 1; tipo 1: 0 e 2;
  tipo 2: 0 e 1).

Na demo: 00961 (tipo 1). No rastreio o resultado decodificado aparece como o pseudoarquivo
`pzs3us1/00961.bin@0x0#bpe` (o contêiner começa no deslocamento 0 do arquivo).

### 2.2 O arquivo: contêiner BPE

O arquivo inteiro é um contêiner BPE (`BpeHeader`, include/sys/bpe.h, especificado em `personagem.md` 6.2), sem
tabela de deslocamentos em volta:

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | s32 `rawSize` | `Bpe_Decode` (src/sys/bpe.c) | u32, no arquivo |
| 0x04 | s32 `packSize` | `Bpe_Decode` | u32, no arquivo |
| 0x08 .. 0x08 + packSize | fluxo comprimido | `Bpe_Decode`, byte a byte | bytes |
| 0x08 + packSize .. fim | cauda | ninguém (o decodificador para em `packSize`) | bytes, **sem exigir zero** |

Os três arquivos: tamanhos 7.840, 11.008 e 9.312 bytes (todos <= 0x3000 de `loadPack`, múltiplos de 0x10);
`rawSize` 0x4320, 0x64A0 e 0x64A0 (<= 0x6800 de `loadRes`); a decodificação produz exatamente `rawSize` bytes nos
três. A cauda tem 15, 1 e 8 bytes; em 00960 e 00962 ela tem bytes não nulos (8 e 4), restos do compressor que
ninguém lê. O conversor deixa a cauda em bytes sem exigir zero (mesma situação que o guia, 5.5, registra para o fim
de fluxos BPE).

### 2.3 O conteúdo decodificado: `TexFile`

Formato `TexFile`/`TexEntry` de `personagem.md` 4.4, base = início do buffer decodificado:

| Parte | Formato | Quem lê aqui |
|---|---|---|
| 0x00 `count` | u32 | `Res_RelocateOffsets` (2, 3 e 3 texturas) |
| 0x04 `entOfs` | u32 | `Res_RelocateOffsets` (= 8 nos três: registros em 0x20) |
| 0x08..0x0F | 2 x u32 (não lido, não zero; como em `personagem.md` 4.4) | ninguém |
| 0x10 `ent` | u32 (ponteiro gravado na carga) | zero no arquivo |
| 0x14..0x1F | zero | |
| `TexEntry` (0x40 cada) | `pixOfs`, `clutOfs`, `pixSize`, `clutSize`, 0x10, 0x14, `pixBlt`, `clutBlt`, 0x20, 0x24 (u32/s32); 0x28..0x2F u8 x 8; 0x30 `tex0` u64; 0x38, 0x3C ponteiros | `Res_RelocateOffsets` (`pixOfs`, `clutOfs`), `Tex_UploadPixels`/`Clut` (`pixOfs`, `clutOfs`, `pixSize`, `clutSize`, `pixBlt`, `clutBlt`), `Sprite_DrawList` (+0x10 como s32 e `tex0` como u64) |
| pacotes de imagem e paleta | `personagem.md` 4.4 (cabeçalho de 0x60 e fim de 0x20 como u32, miolo bytes) | DMA |

Diferenças de visão, sem efeito na conversão: `SpriteTex` chama de `cbpOfs` o s32 em +0x10 (é o `tbpStep` de
`TexEntry`; `Sprite_DrawList` o soma à base da paleta), e cobre 0x14..0x2F com `unk14[0x1C]`. Ninguém lê
0x28..0x2F nesta folha: bytes, como no modelo de personagem (zero nos três).

Conferido nos três arquivos decodificados: `entOfs` = 8, 0x08..0x0F não zero, 0x10..0x1F zero, `pix`/`clut` zero,
0x28..0x2F zero, todos os pacotes no formato de 4.4 (DIRECT cobrindo o pacote, tamanho = 0x60 + NLOOP x 16 + 0x20),
8 texturas, todas PSMT4. O arquivo decodificado é coberto inteiro pelo cabeçalho, registros e pacotes (nenhum byte
de enchimento).

O que vira ponteiro: `Res_RelocateOffsets` grava `TexFile.ent` (+0x10) e `pix`/`clut` (+0x38, +0x3C) de cada
registro, no próprio `loadRes`.

### 2.4 Onde converter

Os números da folha só existem depois de `Bpe_Decode`, então a conversão é em tempo de jogo, como a das animações
(decisão 12), **no caminho da tela de carregamento**: em `Load_InitScreen`, entre `Sprite_Unpack` (loading.c:166)
e `Res_RelocateOffsets` (:168), sob `#ifdef PORT_BIG_ENDIAN`, sobre `gProgress->loadRes` com o tamanho `rawSize`.
Cada chamada de `Load_InitScreen` decodifica de novo por cima do buffer, então não há conversão dupla. O
cabeçalho BPE do arquivo (`rawSize`, `packSize`) é convertido no disco pelo instalador, como o do file[1] do
personagem; o tamanho para o gancho sai do terceiro argumento de `Sprite_Unpack` (hoje `NULL`) ou de
`((s32 *)gProgress->loadPack)[0]` já convertido.

O gancho **não** pode ficar em `Sprite_Unpack` nem em `Bpe_Decode`: `Sprite_Unpack` descomprime também os pacotes
dos menus, que são de outros formatos (tabelas de deslocamentos com cursos, textos de diálogo e várias folhas:
src/menu/menu_n_b.c:98, menu_s_b.c:93, src/sys/late_a_c.c:52, entre outros); só alguns chamadores aplicam
`Res_RelocateOffsets` direto no resultado. Cada um desses é um formato e um gancho próprios, fora deste documento.

Na build de teste da F1 o caminho já existe: `__wrap_Bpe_Decode` (port/x360/headless/inverter.c) aplica o mapa ao
pseudoarquivo `#bpe`.

### 2.5 Cobertura conferida

| Pseudoarquivo | T | M | Largura igual |
|---|---|---|---|
| `00961.bin` (o contêiner) | 10.976 (2 u32 do cabeçalho e o fluxo byte a byte) | 2 (`rawSize`, `packSize`) | todas |
| `00961.bin@0x0#bpe` (a folha) | 18 | 20 | todas |

Na folha, T: `count` e `entOfs`; dos registros 0, 1 e 2, `pixOfs`/`clutOfs` da relocação; dos registros 0 e 2
(as texturas do tipo 1), `pixSize`, `clutSize`, `pixBlt`, +0x10 (`SpriteTex.cbpOfs`) e `tex0` (u64). M tem essas
mesmas posições e mais `clutBlt` (+0x1C) dos registros 0 e 2, que `tipos.tsv` não lista.
Uma leitura em +0x20 aparece atribuída a `LoadSprite.v1` (sprite.c:140): a posição é o `pixOfs` do registro 0,
lida como u32, a mesma largura; é atribuição de linha do `tipos.py`, não leitura de `LoadSprite` no arquivo.
Sem conflitos.

Não exercitado: 00960 e 00962 (outros tipos de tela), o registro 1 de 00961 além da relocação, e o conteúdo dos
pacotes (vão por DMA).

## 3. Folhas de rosto dos membros: `00007..00328` (`TexFile`)

### 3.1 Quem carrega e como o id sai

Id `BTL_FILE_CHARA_DATA + personagem x 2 + lado` = 8 + personagem x 2 + lado (include/battle/battle_work.h:114),
ou seja o arquivo `7 + personagem x 2 + lado`. **322 arquivos, 00007..00328**, dois por personagem (um por lado).
Na demo: 00017 = personagem 5, lado 0; 00082 = personagem 37, lado 1 (os mesmos dos bancos de vozes 03199 e
03231).

- `BtlLoad_StepInitial` aloca dois blocos de `BTL_MEMBER_BUF_SIZE` = 0x1000 bytes por membro (`buf[0]`, `buf[1]`,
  battle_load.c:571..575), lê o arquivo de cada membro em `buf[0]` (:593) e, no passo seguinte, reloca com
  `Res_RelocateOffsets(&m->buf[0], m->buf[0], m->buf[0])` (:603..606). `BattleMember.data` (+0x58,
  include/battle/battle.h:144) aponta `buf[0]` (:573, e de novo em :797 no recomeço).
- Na troca de personagem, `BtlLoad_StepChara` lê o arquivo novo em `buf[1]` (:419) e, ao trocar, reloca e faz
  `data = buf[1]` (:476..477).
- Quem lê: só o painel de equipe do HUD. `HudTeam_DrawFaceA`/`DrawFaceB` (src/battle/hud_a_b.c:284, :302) pegam
  `BattleMember + 0x58` pela visão `HudTeamMember.faceRes` (hud_a_b.c:32) e chamam `HudSprite_InitTex` e
  `HudSprite_DrawAt` → `HudRes_UploadTex` (src/battle/hud_c_c.c:143, :160) com a visão `HudCTex`
  (include/battle/hud_c.h:44); `HudTeam_ClearFaceTexMark` (hud_a_b.c:272) zera `mark`. Nenhum outro leitor de
  `BattleMember.data` foi achado.

Mesmo papel em outro lugar: não há. Os rostos dos menus (seleção de personagem, ranking, listas) são outros
arquivos, comprimidos e lidos por `Sprite_Unpack`, de outro formato; ficam fora deste documento.

### 3.2 Formato

`TexFile` de `personagem.md` 4.4 com a visão do HUD de `comuns.md` 6.3 (`hud` = 1 no conversor): base = início do
arquivo, 1 textura, `entOfs` = 8, 0x08..0x0F 2 x u32 (não lido, não zero), 0x10 ponteiro (zero no arquivo),
0x14..0x1F zero; registro em 0x20 com 0x28 = s32 `mark` e 0x2C = s32 `unk2C` (2 x u32; zero nos 322); `tex0` u64
(`HudSprite_InitTex` tira largura e altura dos bits 26..33); pacote de imagem e de paleta de 4.4.

Campos lidos pelo HUD: `imageSize`/`clutSize` (+0x08, +0x0C), `imageWidth`/`clutWidth` (+0x18, +0x1C),
`imageBlock`/`clutBlock` (+0x20, +0x24), `mark` (+0x28, s32), `tex0` (+0x30, u64), `image`/`clut` (+0x38, +0x3C,
ponteiros da relocação).

Conferido nos 322 arquivos: todos com 3.424 bytes, `count` = 1, `entOfs` = 8, 0x08..0x0F não zero, 0x10..0x1F
zero, 0x28..0x2F zero, `pix`/`clut` zero, pacotes no formato de 4.4, PSMT8 com paleta, e o arquivo coberto inteiro
por cabeçalho, registro e os dois pacotes (nenhum byte de enchimento).

### 3.3 Escritas e quando converter

`Res_RelocateOffsets` grava `ent` (+0x10) e `pix`/`clut` (+0x38, +0x3C); o HUD grava `mark` (+0x28, s32) a cada
desenho. Todos esses campos são lidos antes como número (deslocamento) ou vêm zerados. Os buffers são relidos a
cada luta (`buf[0]`) e a cada troca (`buf[1]`); com o instalador, conversão uma vez no disco. Sem compressão.

### 3.4 Cobertura conferida

| Arquivo | T | M | Largura igual |
|---|---|---|---|
| 00017 | 4 | 4 | todas |
| 00082 | 4 | 4 | todas |

As quatro posições são `count`, `entOfs`, `pixOfs` e `clutOfs`, lidas por `Res_RelocateOffsets` (catálogo:
`TexFile 3, TexEntry 2`). A demo é uma luta de um contra um: o painel de equipe nunca desenhou um rosto, então os
campos de envio, `mark` e `tex0` não foram exercitados.

## 4. Reconhecimento pelo conversor (resumo)

| Índices de `pzs3us1` | Ids | Formato | Seção | Onde converte |
|---|---|---|---|---|
| 7..328 | 8..0x149 | `TexFile` com a visão do HUD | 3 | instalador |
| 329..367 | 0x14A..0x170 | banco de som | 1 | instalador |
| 960..962 | 0x3C1..0x3C3 | contêiner BPE; dentro, `TexFile` | 2 | cabeçalho BPE no instalador; a folha em `Load_InitScreen` |
| 3033..3354 | 0xBDA..0xD1B | banco de som | 1 | instalador |

Essas faixas não colidem com as já reconhecidas por `formato_de` (port/x360/conv/conv.c): 1..6 comuns, 368..445
cenários, 1423..3032 slots de lutador. O índice 367, que `cenario.md` seção 1 diz ser "de outro formato, tabela de
3 entradas", é o banco do cenário 0x22. Todos os índices daqui têm forma conferida em todos os arquivos da faixa
(seções 1.11, 2.2, 2.3 e 3.2).

## 5. O que fica para decisão ou para o código

Decisão:
- **Bancos de som: saída A ou B (1.8).** Proposta: A (7 números por banco, o resto intacto, e o port lê
  `maxNumber` e o cabeçalho em ordem nativa sob `#ifdef PORT_BIG_ENDIAN`).
- **Folhas da tela de carregamento:** o gancho em `Load_InitScreen` (2.4), de onde vem o tamanho, e o nome da
  entrada do conversor para o caminho em tempo de jogo (algo como `Conv_Animacao`, para `TexFile`).

Código (fora do conversor):
- `snd_se.c`, comando `QUEUE`: `count`, `handle` e `pitch` lidos com `le32`/`le16` de memória nativa (1.10).
- `snd_se.c`, `lookup` e `bank_parts`: leitura nativa dos campos convertidos, se a saída A for a escolhida.

Não determinado: ver 1.12. Nas folhas de textura, só o que `personagem.md` 4.4 já deixa em aberto (`TexFile`
0x08..0x0F; cabeçalho e fim dos pacotes de imagem, resolvidos pela decisão 12 como u32).

## 6. Cobertura conferida contra o rastreio (resumo)

Método de `personagem.md` 10: no WSL, cada arquivo foi percorrido como esta especificação diz (os bancos pela
saída A; a folha de carregamento decodificada com uma cópia do `Bpe_Decode`), cada posição ganhou uma largura, e
cada posição de `tipos.tsv` e de `mapa.tsv` foi comparada com ela.

| Arquivo | T (iguais / total) | M (iguais / total) | Conflitos |
|---|---|---|---|
| 00329 | 83 / 87 | 6 / 6 | 4, artefato de pilha (1.11) |
| 00330 | 14 / 14 | 6 / 6 | nenhum |
| 00334 | 16 / 16 | 6 / 6 | nenhum |
| 03199 | 62 / 62 | 6 / 6 | nenhum |
| 03231 | 62 / 62 | 6 / 6 | nenhum |
| 00961 | 10.976 / 10.976 | 2 / 2 | nenhum |
| 00961@0x0#bpe | 18 / 18 | 20 / 20 | nenhum |
| 00017 | 4 / 4 | 4 / 4 | nenhum |
| 00082 | 4 / 4 | 4 / 4 | nenhum |

E contra todos os arquivos do mesmo tipo: os 361 bancos (1.11), as três folhas de carregamento decodificadas (2.2,
2.3) e as 322 folhas de rosto (3.2), sem nenhuma exceção às invariantes.

O que a demo não exercita: nos bancos, todas as leituras do port (tabelas da Sony e amostras), os bancos do menu
(00331) e o 00332; na tela de carregamento, os tipos 0 e 2; nos rostos, tudo além da relocação; em todos, o
conteúdo dos pacotes de imagem e das amostras.
