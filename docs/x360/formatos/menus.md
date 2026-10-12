# Arquivos de menu do boot até a tela de título: especificação para a conversão de ordem de bytes

Especificação, por formato e por tipo, dos três arquivos de menu que o jogo lê do boot até a tela de título:
`pzs3us1/00473.bin` (id 0x1DA, cartão e logos), `pzs3us1/00477.bin` (id 0x1DE, recursos do cartão de memória) e
`pzs3us1/00449.bin` (id 0x1C2, título e menu principal). Para cada um: quem carrega, o pacote externo, o que o jogo
descomprime dele e quem descomprime, e o formato de cada seção descomprimida, campo a campo, com a largura e quem
lê. Três peças aparecem aqui pela primeira vez: o **arquivo de diálogo**, o **filme Flash** ("FOD") e a **tabela de
textos**.

Regra da conversão (decisões 8 e 12): in-place, cada número de 2, 4 ou 8 bytes trocado de ordem pela largura com
que o código o lê, bytes e strings intactos, enchimento zero, nada muda de tamanho nem de lugar; dado comprimido se
converte depois de descomprimido, no caminho de quem usa o formato.

Peças já especificadas que este documento referencia em vez de repetir: a tabela de deslocamentos
(`personagem.md` seção 2), `TexFile`/`TexEntry` e os pacotes de imagem e paleta (`personagem.md` 4.4), o
contêiner BPE (`personagem.md` 6.2, `som-e-sprites.md` 2.2), o texto u16 (`personagem.md` 5.13) e a regra de id
para arquivo (`comuns.md` 1.1: o arquivo NNNNN da partição 1 é o id NNNNN + 1).

Fontes: o código da decomp (arquivo e linha citados em cada seção; as linhas dos quatro arquivos do jogo com gancho
já contam o gancho) e, como conferência, o rastreio x86 do boot até antes da primeira demo no WSL
(`port/build/menus/rastreio.tsv`, cerca de 3.600 vblanks, e o mapa gerado dele, `port/build/menus/mapa.tsv`) e os
arquivos de `gamedata/pzs3us1/` lá. Nenhum byte do jogo está neste documento: só contagens, deslocamentos e
conclusões. Convenções de largura e da coluna M (posições de número do mapa, expandidas pelas repetições) como em
`comuns.md` 1.4; para os menus não foi gerado o `tipos.tsv`, então não há coluna T.

## 1. Os três arquivos e o caminho do boot

### 1.1 Id e arquivo

`Progress_Init` grava `gProgress->baseFile` = 0x1C1 (src/battle/view_a_e.c:47; `MenuProgress.baseFile`,
include/menu/menu_a.h:60; a visão de `mcflow_a.c` chama o mesmo campo de `fileBase`, src/sys/mcflow_a.c:21).

| Arquivo | Id | Quem carrega | Buffer | Conteúdo | Tamanho | Seção |
|---|---|---|---|---|---|---|
| 00473 | `baseFile + 0x19` = 0x1DA | `FirstRun_Main` (src/menu/menu_i_c.c:170), uma vez por boot | `gMenuArc0`, novo do heap, liberado em menu_i_c.c:185 | 3 seções em BPE: sem leitor, tela do cartão, logos | 217.856 | 2, 5.1, 5.2 |
| 00477 | `fileBase + 0x1D` = 0x1DE | `McFlow_Init` (src/sys/mcflow_a.c:2635), a cada tela que usa o cartão | `gMcFlow->pack`, novo, liberado em `McFlow_Term` | um contêiner BPE inteiro: os dois ícones do cartão | 65.488 | 3, 5.3 |
| 00449 | `baseFile + 1` = 0x1C2 | `Progress_Main` (src/menu/menu_a_b.c:100), `Title_Init` (src/menu/menu_a_c.c:51) e `MainMenu_Init` (src/menu/menu_a.c:50), quando `gMenuArc1` é nulo | `gMenuArc1`, novo, residente enquanto o título e o menu principal se alternam | 2 seções em BPE: título, menu principal | 1.544.768 | 2, 5.4 |

Nenhum outro leitor desses ids foi achado (busca pelas constantes e por `baseFile`/`fileBase` mais o deslocamento).
Os índices 449, 473 e 477 não colidem com nenhuma faixa já reconhecida por `formato_de`
(port/x360/conv/conv.c): 368..445 é cenário, 960..962 folha de carregamento, 1423..3032 slot de lutador.

### 1.2 O caminho

`main` chama `Progress_Main(0)` (src/main.c:157). Com `MPROG_FIRST_RUN`, `Progress_Main` chama `FirstRun_Main`
(menu_a_b.c:69), que:

1. lê 00473 em `gMenuArc0` (menu_i_c.c:170);
2. roda `BootCard_Run(2)` (menu_i_c.c:172): `BootCard_Init` (src/menu/menu_i_b.c:20) descomprime a **seção 2** do
   00473 (`Sprite_Unpack(MPACK_AT(gMenuArc0, 2), NULL, NULL)`, menu_i_b.c:24), passa a seção 1 dela a `Dialog_Init`
   (menu_i_b.c:31) e chama `McFlow_Init` (menu_i_b.c:32), que lê e descomprime 00477; o fluxo 2 do cartão
   (`McFlow_Start(2)`, menu_i_b.c:68) é a carga do boot;
3. descomprime a **seção 3** do 00473 (menu_i_c.c:173) e mostra os seis logos (`Logo_ShowAll`, menu_i_c.c:113:
   seções 6, 7, 8, 3, 4, 5, cada uma relocada por `Logo_Init`, menu_i_c.c:20..21);
4. libera tudo, espera a partição 2 atrás da tela de carregamento (`FirstRun_WaitPartition`, menu_i_c.c:123;
   folha da tela de carregamento, `som-e-sprites.md` 2) e toca o filme de abertura.

De volta, `Progress_Main` lê 00449 em `gMenuArc1` (menu_a_b.c:100) e roda `Title_Run(1)` (menu_a_b.c:115), que chama
`Title_Init(1)` (menu_a_c.c:300): descomprime a **seção 1** do 00449 (menu_a_c.c:56), reloca as texturas, cria o
filme (menu_a_c.c:99), o diálogo (menu_a_c.c:101) e chama `McFlow_Init` de novo (menu_a_c.c:102). Depois de 1.800
quadros parado o título prepara a demo (`TITLE_IDLE_FRAMES`, include/menu/menu_a.h).

A **seção 2** do 00449 é o menu principal (`MainMenu_Init`, src/menu/menu_a.c:45), fora do caminho até o título
(seção 11).

## 2. Pacote externo de 00449 e 00473

### 2.1 Tabela de seções

A tabela de `personagem.md` seção 2: `buf[0]` = N, `buf[1..N]` = deslocamento em bytes da seção, `buf[N+1]` = tamanho
do arquivo, zeros até a seção 1. Quem lê: `MPACK_AT(pack, n)` (include/menu/menu_a.h:55), que faz
`buf[n] >> 2 << 2`; ninguém lê `buf[0]`.

| Arquivo | N | Seção 1 | Seção 2 | Seção 3 |
|---|---|---|---|---|
| 00473 | 3 | 0x40, 0x9740 bytes | 0x9780, 0xAEC0 bytes | 0x14640, 0x20CC0 bytes |
| 00449 | 2 | 0x40, 0xAB580 bytes | 0xAB5C0, 0xCDC80 bytes | |

Conversão: a tabela inteira (`buf[0]` até a seção 1) como u32 (`Conv_Tabela`), com as validações de lá (fim igual
ao tamanho, ordem, seção 1 depois da tabela).

### 2.2 Cada seção: contêiner BPE

Cada seção é um contêiner BPE (`BpeHeader`, include/sys/bpe.h:7) que o jogo passa direto a `Sprite_Unpack`
(src/sys/sprite.c:10, que só chama `Bpe_Decode`, src/sys/bpe.c:6):

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | s32 `rawSize` | `Bpe_Decode` (bpe.c:21); o gancho de tempo de jogo (seção 4) | u32 |
| 0x04 | s32 `packSize` | `Bpe_Decode` (bpe.c:23) | u32 |
| 0x08 .. 0x08 + packSize | fluxo comprimido | `Bpe_Decode`, byte a byte | bytes |
| 0x08 + packSize .. fim da seção | cauda | ninguém | bytes, **sem exigir zero** |

As caudas têm 35, 9 e 59 bytes no 00473 (2, 4 e 6 deles não nulos) e 38 e 34 no 00449 (4 e 1 não nulos): restos do
compressor, como nas folhas de carregamento (`som-e-sprites.md` 2.2). Tamanhos descomprimidos: 00473 seção 1
0x18500, seção 2 0x13E00, seção 3 0x129200; 00449 seção 1 0x1A1540, seção 2 0x296C80. Em todas a decodificação
produz exatamente `rawSize` bytes e consome exatamente `packSize`.

Conversor: `Conv_MenuPacote` (port/x360/conv/menus.c), formato `menu_pacote`.

## 3. 00477: um contêiner BPE inteiro

`McFlow_Init` passa o arquivo inteiro a `Sprite_Unpack` (mcflow_a.c:2636), sem tabela em volta: o formato de 2.2 com
o contêiner começando no deslocamento 0. `rawSize` 0x15BC0, `packSize` 0xFFC7, cauda de 1 byte (zero). Conversor:
`Conv_MenuBpe`, formato `menu_bpe`.

## 4. Onde converter o conteúdo descomprimido: um gancho por chamador

Os números das seções só existem depois de `Bpe_Decode`, então a conversão é em tempo de jogo (decisão 12, como as
animações e a folha da tela de carregamento). O gancho não pode ficar em `Sprite_Unpack` nem em `Bpe_Decode`: os
dois servem a formatos diferentes, e **quem sabe o formato do resultado é quem chama** (a mesma tabela externa tem
uma tela de cartão numa seção e seis logos na outra; o mesmo `Sprite_Unpack` descomprime animações, folhas e
dezenas de telas de menu).

**Escolha: um gancho em cada chamador**, logo depois do `Sprite_Unpack`, sob `#ifdef PORT_BIG_ENDIAN`, com o nome
do formato:

```
Port_ConvertMenuPack(buffer, ((s32 *)contêiner)[0], "formato");   /* port/x360/conv/conv_jogo.c */
```

| Chamador | Linha | Buffer | Formato | Seção |
|---|---|---|---|---|
| `BootCard_Init` | src/menu/menu_i_b.c:25..30 | `gBootCard->res` | `menu_cartao` | 5.1 |
| `FirstRun_Main` | src/menu/menu_i_c.c:174..179 | `res` | `menu_logos` | 5.2 |
| `McFlow_Init` | src/sys/mcflow_a.c:2637..2642 | `gMcFlow->res` | `menu_mcflow` | 5.3 |
| `Title_Init` | src/menu/menu_a_c.c:57..62 | `gTitle->res` | `menu_titulo` | 5.4 |

- O tamanho é o `rawSize` do contêiner, já convertido na carga do arquivo (2.2, 3): os quatro chamadores passam
  `NULL` como terceiro argumento de `Sprite_Unpack`, como `Load_InitScreen` (src/sys/loading.c:170).
- Os quatro passam `dst = NULL`: cada `Sprite_Unpack` aloca um buffer novo, e cada um é liberado no `_Term` da tela.
  Não há conversão dupla, nem buffer reaproveitado.
- O formato depende do chamador e não do número da seção que ele recebe: `Title_Init(section)` e
  `BootCard_Init(section)` leem um layout fixo de seções de dentro (5.1, 5.4), qualquer que seja `section`.
- Formato que não bate é falha fatal com a mensagem do conversor (`falha` em conv_jogo.c), como nos outros ganchos.

**Recusado: a origem anotada** (cada buffer lido do disco registrado com arquivo e deslocamento, e o
`Sprite_Unpack` olhando a origem do ponteiro numa tabela "arquivo, seção → formato"). Precisaria de um registro de
buffers no caminho de leitura do jogo (`File_LoadSync`, `File_Request`, cargas em segundo plano), que hoje não
existe no 360, e de uma tabela que repete o que cada chamador já sabe; quebraria com seções de seções e com
buffers reaproveitados (o `loadPack` fixo da tela de carregamento). Um gancho por chamador é uma linha onde a
informação já está, e a lista de chamadores (seção 11) diz exatamente o que falta.

Na ferramenta de teste a mesma escolha está numa tabela (`formato_bpe` em port/x360/conv/conv_tool.c), para
`--mapa` e `--disco` converterem o conteúdo descomprimido pelo nome do rastreio (`pzs3us1/00449.bin@0x40#bpe`).

## 5. Os pacotes de tela descomprimidos

Todos começam com a tabela de 2.1 (N próprio, base = início do buffer). Quem lê as seções: `MPACK_AT` sobre o
buffer descomprimido (ou `DIALOG_PART`, a mesma conta, src/sys/dialog.c:55).

### 5.1 Tela do cartão (`menu_cartao`: 00473, seção 2)

N = 1. A seção 1 (0x40..0x13E00) é o arquivo de diálogo (seção 6), passado a `Dialog_Init` (menu_i_b.c:31). Mais
nada lê o buffer (`BootCard`, include/menu/menu_i.h:237: "section 1 is the dialog file").

### 5.2 Logos (`menu_logos`: 00473, seção 3)

N = 9. Seções 1 e 9 vazias. Seções 2..8: `TexFile` de `personagem.md` 4.4, 8 texturas cada (0x20C40 ou 0x42A40
bytes). `Logo_Init` reloca a seção (`Res_RelocateOffsets`, menu_i_c.c:21; src/sys/gfxm_b_b.c:244) e
`Sprite_DrawPicture` (sprite.c:27) desenha as 8 texturas como uma imagem 512x448, lendo `TexEntry` +0x24
(`cbpOfs`, a visão `SpriteTex` em include/sys/sprite.h chama o s32 de +0x10 de `cbpOfs`; os dois são s32) e `tex0`
(u64) e enviando os pacotes por `TexFile_UploadOne`. Ninguém lê 0x28..0x2F do registro: bytes.

`Logo_ShowAll` mostra 6, 7, 8, 3, 4, 5. **A seção 2 não tem leitor** (nem na decomp, nem por id): ela tem a forma
de `TexFile` conferida pelo conversor e é convertida como tal (o mesmo critério de `comuns.c`, "TexFile pela forma").

### 5.3 Recursos do cartão (`menu_mcflow`: 00477 descomprimido)

N = 3 (`McFlow.res`, include/sys/mcflow_a.h:98: "word 1 system-save icon, word 2 replay icon").

| Seção | Faixa | Quem lê | Conteúdo | Conversão |
|---|---|---|---|---|
| tabela | 0x00..0x40 | `McFlow_SetupRequest` (mcflow_a.c:160, :166) lê `res[1]` e `res[2]` como u32 | | u32 |
| 1 | 0x40..0x9D00 | ninguém lê como número: `McCardFile` grava `iconSize` = 0x9C9A bytes no cartão com `sceMcWrite` (src/battle/stgm_a_b.c:323, :494) | ícone do jogo salvo, no formato de ícone do PS2 | bytes; depois de 0x9C9A, zeros |
| 2 | 0x9D00..0x15BC0 | idem, com `iconSize` = 0xBE8A | ícone do replay | bytes; depois de 0xBE8A, zeros |
| 3 | vazia | | | |

O ícone é dado do cartão de memória, não do jogo: tem de chegar ao arquivo do cartão byte a byte como no PS2.
Quem interpretava os números dele era o navegador do PS2. No 360 o save vai para o sistema do console (plano, F4);
se algum dia o ícone for convertido para outro formato, será pelo código do save, não aqui.

### 5.4 Título (`menu_titulo`: 00449, seção 1)

N = 14 (`Title.res`, include/menu/menu_a.h, "a pack of 15 sections" conta a palavra 0).

| Seção | Bytes | Formato | Quem lê |
|---|---|---|---|
| 1 | 0x42A40 | `TexFile`, 8 texturas | `Title_Init` (`TT_RES(1)`, menu_a_c.c:63): o fundo, por `Sprite_DrawPicture` |
| 2 | 0x69FC0 | `TexFile`, 6 | `TT_RES(2)` (:65): `TexEntry` dados ao filme (`gTitle->tex[17..22]`) |
| 3 | 0x1E500 | `TexFile`, 7 | `TT_RES(3)` (:72): `tex[10..16]` |
| 4 | 0x8580 | `TexFile`, 1 | **ninguém** (`Title_Init` pula a 4): `TexFile` pela forma |
| 5..7 | 0x20580, 0x4580, 0x4580 | `TexFile`, 1 cada | `TT_RES(5..7)` (:82..:86) |
| 8 | 0x10AC0 | `TexFile`, 2 | `TT_RES(8)` (:88) |
| 9 | 0x10C40 | `TexFile`, 3 | `TT_RES(9)` (:91) |
| 10 | 0x49000 | `TexFile`, 3 | `TT_RES(10)` (:95) |
| 11 | 0x5F00 | filme Flash (seção 7) | `Flash_Create(&gTitle->flash[0], MPACK_AT(gTitle->res, 11), gTitle->tex)` (:99) |
| 12 | 0x13DC0 | arquivo de diálogo (seção 6) | `Dialog_Init` (:101) |
| 13 | vazia | | ninguém |
| 14 | 0x20C40 | `TexFile`, 8 | `TT_RES(14)` (:80): o logo dos primeiros 300 quadros |

`TT_RES(n)` reloca a seção (`Res_RelocateOffsets`) e guarda `MTEX(res, k)` (o `TexEntry` k) em `gTitle->tex[]`, a
tabela de texturas do filme: o filme lê esses registros em `FlashDraw_UploadTex` (src/sys/gfxm_c.c:672: `pixSize`,
`pixBlt`, `pix`, `tbpStep`, `clutBlt`, `clut`, `clutSize`, `tex0`), todos já na largura de `personagem.md` 4.4.

## 6. Arquivo de diálogo (`Dialog_Init`)

`Dialog_Init(file, msgTbl, size)` (src/sys/dialog.c:237) lê seis partes por `DIALOG_PART(file, i)` (dialog.c:55, a
conta de `MPACK_AT`). É o mesmo arquivo na tela do cartão e no título (os dois são idênticos byte a byte); a
estrutura vale para qualquer chamador de `Dialog_Init`.

| Parte | Bytes | Formato | Quem lê |
|---|---|---|---|
| tabela | 0x40 | 2.1, N = 6 | `DIALOG_PART` |
| 1 | 0x3C0 | `TexFile`, 1 textura | reloca e guarda `ent` em `res.tex0` (dialog.c:242..245) |
| 2 | 0x4AC0 | `TexFile`, 2 | `res.tex2` e `res.clut2` = segundo registro (dialog.c:250..253) |
| 3 | 0x4AC0 | `TexFile`, 2 | `res.tex3`, `res.clut3` (dialog.c:254..257) |
| 4 | 0x4AC0 | `TexFile`, 2 | `res.tex1`, `res.clut1` (dialog.c:246..249) |
| 5 | 0x35C0 | filme Flash (seção 7) | `Flash_Create(gDialog->flash, DIALOG_PART(file, 5), &gDialog->res)` (dialog.c:258) |
| 6 | 0x23C0 | tabela de textos (6.1) | `gDialog->titleTbl` (dialog.c:260) |

`DialogFlashRes` (include/sys/dialog.h) é a tabela de texturas do filme, com os `TexEntry` dessas quatro partes.

### 6.1 Tabela de textos

`DIALOG_TEXT(tbl, i)` (dialog.c:54) = `tbl + tbl[i + 1] / 4 * 4`, lido como `u16 *`: o texto i é a entrada i + 1 de
uma tabela no formato de 2.1. Leitores: `Dialog_DrawTitle` e `Dialog_DrawBody` (dialog.c:136, :170) com a tabela
do arquivo, `Dialog_DrawText` (dialog.c:58) com a tabela do chamador (`msgTbl`; nula na tela do cartão e no
título), e as funções de fonte (`Font_CountLines`, `Font_PrintAt`) sobre `u16 *`.

| Parte | Tipo | Conversão |
|---|---|---|
| `tbl[0]` = N | u32 (ninguém lê) | u32, com a tabela |
| `tbl[1..N]`, `tbl[N+1]` = tamanho | u32 | u32 |
| enchimento até a entrada 1 | zeros | u32 |
| cada texto | u16 até um u16 zero, depois zeros até a entrada seguinte | u16 (a entrada inteira, como `personagem.md` 5.13) |

No arquivo: N = 66, entrada 1 em 0x140, deslocamentos múltiplos de 4, todo texto termina num u16 zero e o que
sobra até a entrada seguinte (0 a 26 bytes) é zero.

## 7. Filme Flash ("FOD" 0x11 "LIT")

Um SWF convertido (comentário de include/sys/gfxm_c.h:9). Leitores: `src/sys/gfxm_b_c.c` (cabeçalho, tags, blocos
nomeados, bits, matriz) e `src/sys/gfxm_c.c` (o tocador). **Nada é alinhado**: toda leitura de número de 2 ou 4 bytes
é um `memcpy` para uma variável local ou uma struct local, então a ordem dos bytes no buffer é a única coisa que
importa (no 360, o `memcpy` não alinhado não tem problema; a troca in-place serve). Base dos deslocamentos: o
início do filme.

### 7.1 Cabeçalho

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | 'F' 'O' 'D' 0x11 | `Flash_CheckHeader` (gfxm_b_c.c:16), byte a byte | bytes |
| 0x04 | "LIT\0" | `Flash_CheckHeader` (`strcmp`) | bytes |

`Flash_Create` (gfxm_c.c:1022) guarda `data = file + 8`: o resto é uma lista de tags.

### 7.2 Tags e registros (`FlashTag`, include/sys/gfxm_b_c.h:13)

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | u8 `code` | `Flash_FindTag` (gfxm_b_c.c:33) | byte |
| 0x01 | u8 `unk01` | ninguém (0 em todos) | byte |
| 0x02 | u16 `count` | `FlashClipList_Init` (gfxm_c.c:1449, `hdr.count`); `FlashTl_Init` (gfxm_c.c:404) num registro | u16 |
| 0x04 | u32 `size` | `Flash_FindTag` (o passo até a próxima tag), `Flash_SkipRecords` (gfxm_b_c.c:62) num registro | u32 |
| 0x08 | `count` registros (tag) ou os dados (registro) | | |

Cada tag tem `count` registros com o mesmo cabeçalho de 8 bytes, um atrás do outro, e os registros fecham a tag
exatamente (`size` = soma dos registros). A lista termina numa tag com `code` 0 (`Flash_FindTag` para nela); nos
filmes do caminho ela tem `count` e `size` zero, e o que sobra da seção depois dela (23 e 45 bytes) é zero. Nos
registros, `code` é o próprio índice e ninguém o lê; `count` só é lido nos registros das tags 6 e 7 (o número de
profundidades), mas é u16 pela struct em todos.

Tags presentes nos filmes do caminho: 2, 3, 4, 6, 7 e a de fim. Nenhuma outra: uma tag de código desconhecido é
erro do conversor (não há como saber a largura do que ela traz).

| Tag | Registro (depois dos 8 bytes) | Quem lê | Conversão | Diálogo | Título |
|---|---|---|---|---|---|
| 2 | u16 id do registro da tag 6 (tamanho 2) | `FlashClipList_Init` (gfxm_c.c:1459) | u16 | 10 | 11 |
| 3 | u16 largura, u16 altura (tamanho 4) | `FlashShape_Draw` (gfxm_c.c:1414..1415) | u16, u16 | 11 | 26 |
| 4 | u16 n, n x `FlashShapeQuad` (7.3) | `FlashShape_Draw` (gfxm_c.c:1396, :1404) | 7.3 | 11 | 25 |
| 6 | linha do tempo de um sprite (7.4) | `FlashTl_Init` via `Flash_GetRecord` (gfxm_c.c:1465) | 7.4 | 7 | 8 |
| 7 | linha do tempo da raiz (7.4), um registro | `Flash_Create` (gfxm_c.c:1034) | 7.4 | 1 | 1 |

### 7.3 Quadrilátero de forma (`FlashShapeQuad`, include/sys/gfxm_c.h:198, 0x18 bytes)

Copiado inteiro por `memcpy` (gfxm_c.c:1404) a partir de `registro + 10 + i x 0x18`; o registro tem exatamente
2 + n x 0x18 bytes em todos os registros dos três filmes.

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | s16 `image` (registro da tag 3, negativo = sem textura) | `FlashShape_Draw` | u16 |
| 0x02 | u16 `unk02` | ninguém | u16, pela struct |
| 0x04 | u32 `rgba` (r no byte alto) | `FlashShape_Draw` (deslocamentos de 24, 16, 8, 0) | u32 |
| 0x08..0x17 | s32 `x0`, `x1`, `y0`, `y1` | `FlashShape_Draw` | u32 x 4 |

### 7.4 Linha do tempo: blocos nomeados de quadros

Os dados de um registro das tags 6 e 7 são uma lista de blocos (`Flash_SkipNamed`, gfxm_b_c.c:86;
`Flash_FindName`, gfxm_b_c.c:106; `FlashTl_RunFrame`, gfxm_c.c:77):

| Parte | Tipo | Quem lê | Conversão |
|---|---|---|---|
| rótulo do quadro | cadeia terminada em zero (quase sempre vazia) | `Flash_FindName`, `Flash_ReadString` | bytes |
| tamanho | u16 (`memcpy`, gfxm_b_c.c:94, :120; gfxm_c.c:90) | os três | u16 |
| tags do quadro | `tamanho` bytes (7.5) | `FlashTl_RunFrame` | 7.5 |

Um bloco de tamanho 0 fecha a lista (`Flash_SkipNamed` devolve NULL, `Flash_FindName` para, `FlashTl_RunFrame`
para a linha do tempo). Depois dele, até o fim do registro: zeros (nos filmes do diálogo, do título e do menu
principal o bloco de fim fecha o registro exatamente).

### 7.5 Tags de um quadro

`FlashTl_RunFrame` (gfxm_c.c:77) lê um código u16 (`memcpy`, gfxm_c.c:101) e os dados dele, até o código 1
(ShowFrame). Um código que não está na lista abaixo não consome nada no jogo (o `switch` não tem `default`), o que
desalinha o resto: o conversor o trata como erro. Nos filmes do diálogo, do título e do menu principal as tags de
todo quadro fecham o bloco exatamente no `tamanho`, e o conversor exige isso.

| Código | Nome SWF | Dados, na ordem | Quem lê | Conversão |
|---|---|---|---|---|
| 1 | ShowFrame | nada | | |
| 4 | PlaceObject | u16 profundidade, u8 tipo, u16 id; tipo 4: matriz; tipo 6: matriz e cor | `FlashTl_TagPlace` (gfxm_c.c:126) | u16, byte, u16, 7.7 |
| 26 | PlaceObject2 | u8 flags, u16 profundidade; com flag 2: u8 tipo, u16 id; flag 4: matriz; flag 8: cor; flag 0x20 **só com a 2**: nome | `FlashTl_TagPlace2` (gfxm_c.c:155) | byte, u16, byte, u16, 7.7, bytes |
| 5 | RemoveObject | u16 profundidade, u8 tipo, u16 id | `FlashTl_TagRemove` (gfxm_c.c:211) | u16, byte, u16 |
| 28 | RemoveObject2 | u16 profundidade | `FlashTl_TagRemove2` (gfxm_c.c:241) | u16 |
| 12 | DoAction | ações até a ação 0 (7.6) | `FlashTl_TagActions` (gfxm_c.c:301), `FlashTl_SkipActions` (gfxm_c.c:250) | 7.6 |

Nesses três filmes só aparecem ShowFrame, PlaceObject2, RemoveObject2 e DoAction; nenhum PlaceObject nem
RemoveObject.

### 7.6 Ações (DoAction)

Um código u8 por ação, até o 0. As duas funções consomem os mesmos bytes (uma executa, a outra pula):

| Código | Ação | Dados | Conversão |
|---|---|---|---|
| 0 | fim | | byte |
| 4, 5, 6, 7 | NextFrame, PrevFrame, Play, Stop | nada | byte |
| 0x81 | GotoFrame | u16 quadro (`memcpy`, gfxm_c.c:333) | u16 |
| 0x83 | GetURL | duas cadeias ("pad", "trig", "se", "trigger" e o argumento) | bytes |
| 0x8A | WaitForFrame | 3 bytes que o jogo pula sem ler | bytes |
| 0x8B, 0x8C | SetTarget, GotoLabel | uma cadeia | bytes |

Um código fora dessa lista não consome nada no jogo; o conversor o trata como erro. Vistos no caminho: 0, 6, 7,
0x83, 0x8C.

### 7.7 Matriz e transformação de cor: campos de bits

`Flash_ReadMtx` (gfxm_b_c.c:189): um byte de flags; para cada par presente (flags 0x11 escala, 0x0A rotação, 0x24
translação) uma largura de 5 bits e um ou dois campos com sinal dessa largura; o fim vai ao próximo byte inteiro.
`Flash_ReadCxform` (gfxm_c.c:43): um byte de flags (bits 0..3 multiplicadores, 4..7 somas), largura de 4 bits se
flags não é zero, e um campo por bit ligado. Os bits se leem do mais alto para o mais baixo, byte a byte
(`Flash_ReadBits`, gfxm_b_c.c:136): **independem da ordem da máquina, ficam como bytes**. O conversor só mede o
tamanho (a mesma conta do jogo) para achar a tag seguinte.

### 7.8 A alternativa ao conversor do Flash (não implementada)

O conversor de 7.1..7.7 é um analisador inteiro do formato (o mesmo caminho do tocador, cerca de 300 linhas em
menus.c). Ele se mostrou proporcional: o formato é fechado pelo código, as tags e os quadros fecham exatamente nos
tamanhos, e a validação acha qualquer desvio. Fica registrada a alternativa, para o dono decidir se um dia preferir
trocar:

- **Ler little-endian no código, sob `#ifdef PORT_BIG_ENDIAN`.** As leituras de número do Flash são poucas e todas
  por `memcpy`: o tamanho do bloco (3 lugares), o código e os campos das tags de quadro (cerca de 10), os campos de
  `FlashTag` (`Flash_FindTag`, `Flash_SkipRecords`, `FlashTl_Init`, `FlashClipList_Init`), o id do sprite, largura e
  altura da imagem e o `FlashShapeQuad`. Uma macro `PS2_LE16`/`PS2_LE32` nesses pontos e o filme fica como está no
  disco.
  - Prós: nenhum analisador; o filme não precisa ser percorrido na carga (hoje cada criação de tela passa por todos
    os quadros); um filme com tag desconhecida não derruba a tela.
  - Contras: muda cerca de 20 leituras em `gfxm_b_c.c` e `gfxm_c.c` do jogo; quebra a regra de que todo dado chega
    ao código em ordem nativa (decisão 8), o que obriga a lembrar que o buffer do Flash é a exceção; a `conv_tool`
    deixa de validar a estrutura do filme no disco inteiro; e `FlashShapeQuad` é copiado inteiro para uma struct,
    então cada campo teria de ser trocado depois do `memcpy`.

Proposta: manter o conversor por formato.

## 8. `TexFile` nos menus

O formato de `personagem.md` 4.4 sem mudança (`Conv_TexFile`, `hud` = 0): base no início da seção, `entOfs` = 8
(registros em 0x20) em todas as seções de textura do caminho, 0x08..0x0F não lido, 0x10 ponteiro, `TexEntry`
+0x28..0x2F bytes (ninguém lê nos menus), `tex0` u64, pacotes de imagem e paleta de 4.4. Leitores: `Res_RelocateOffsets`
(gfxm_b_b.c:244), `Sprite_DrawPicture` (sprite.c:27), `TexFile_UploadOne`, `FlashDraw_UploadTex` (gfxm_c.c:672).

## 9. Escritas do jogo

`Res_RelocateOffsets` grava `TexFile.ent` (+0x10) e `pix`/`clut` (+0x38, +0x3C) no próprio buffer descomprimido,
sobre campos zerados. Nada mais grava nos pacotes de tela: o tocador do Flash e o diálogo guardam o estado nas suas
structs (`Flash`, `FlashTl`, `Dialog`). Nada se grava nos arquivos de 2 e 3.

## 10. Cobertura conferida

### 10.1 Contra o rastreio (`conv_tool --mapa port/build/menus/mapa.tsv`)

| Nome no rastreio | O que é | Cobertura do conversor | M (iguais / total) |
|---|---|---|---|
| `00449.bin` | pacote externo | 100% | 3 / 3 |
| `00473.bin` | pacote externo | 100% | 6 / 6 |
| `00477.bin` | contêiner BPE | 100% | 2 / 2 |
| `00473.bin@0x9780#bpe` | tela do cartão (5.1) | 100% | 1.426 / 1.426 |
| `00473.bin@0x14640#bpe` | logos (5.2) | 100% | 402 / 402 |
| `00477.bin@0x0#bpe` | recursos do cartão (5.3) | 100% | 1 / 1 |
| `00449.bin@0x40#bpe` | título (5.4) | 100% | 3.800 / 3.800 |

Nenhuma posição com outra largura, nenhuma não trocada, nenhum conflito no `mapa.tsv.conflitos` para esses nomes.
"Cobertura" é a parte do buffer que algum conversor reconheceu (número, byte ou enchimento conferido).

Por parte, no que o rastreio viu (M por largura):

- título: tabela 12 x u32; texturas 1, 2, 3, 5..10, 14 (relocação, desenho e envio: u32 e `tex0` u64); filme 11:
  3.270 u16 e 141 u32; diálogo: tabela 6, partes 1..4 (relocação) 22 u32, filme 19 u16 e 8 u32; textos do diálogo:
  nada (o título não abriu o diálogo);
- tela do cartão: diálogo inteiro exercitado (o fluxo de carga do boot abriu a janela): tabela do pacote 1 u32,
  tabela do diálogo 6 u32, texturas 37 u32 e 3 u64, filme 1.216 u16 e 31 u32, textos 130 u16 e 2 u32;
- logos: 6 seções x (58 u32 + 8 u64), a tabela 6 u32;
- recursos do cartão: só `res[1]` (a carga do boot pede o ícone do jogo salvo; o do replay e os ícones em si só se
  leem ao gravar).

Todos os outros arquivos lidos do boot até antes da demo também passam: `conv_tool --mapa` sobre os 61 nomes do
rastreio (arquivos e conteúdos `#bpe`, fora as tabelas do executável) converte todos, sem nenhum "sem conversor", e
as 321.639 posições do mapa batem.

### 10.2 Contra o disco (`conv_tool --disco gamedata`)

2.383 arquivos convertidos sem erro (os 2.380 de antes, mais 00449, 00473 e 00477), 100% de cobertura em cada um.
O `--disco` agora converte também o conteúdo descomprimido que o jogo converte em tempo de jogo: as três folhas da
tela de carregamento (00960..00962) e as quatro seções dos menus desta especificação, as sete sem erro e com 100%
de cobertura.

### 10.3 O que o rastreio não confere

- O que o caminho não exercitou: a seção 4 do título e a 2 dos logos (sem leitor), os textos do diálogo do título,
  o ícone do replay, os bytes dos ícones (só se leem ao gravar no cartão), as ações GotoFrame, WaitForFrame e
  SetTarget e as tags PlaceObject e RemoveObject (não existem nesses filmes). Para essas partes a garantia é a do
  código e a da validação estrutural no disco.
- As larguras dentro dos campos de bits (7.7): o rastreio vê leituras de byte, e o conversor deixa bytes, mas o
  tamanho de cada campo só a análise confere (as tags e os quadros fecham exatamente: um erro de medida desalinharia
  o resto e viraria erro).
- A seção 1 do 00473 e a 2 do 00449 (menu principal) não foram descomprimidas por ninguém no caminho; o rastreio
  não as tem (seção 11).

## 11. O que fica pendente

**Outros chamadores de `Sprite_Unpack`/`Bpe_Decode`.** Cada um descomprime um formato próprio e precisa do seu
gancho e da sua especificação. Convertidos: `BtlObjAnim_Load` (animações), `Load_InitScreen` (folha da tela de
carregamento), `BootCard_Init`, `FirstRun_Main`, `McFlow_Init` e `Title_Init`. Pendentes (arquivo:linha da chamada,
função), 76 chamadas:

- `src/menu/menu_a.c:55` `MainMenu_Init` (o menu principal, seção 2 do 00449: 28 seções, com filme, janelas de
  mensagem e de ícones, legendas e textos; o próximo da fila, porque START no título leva a ele);
- `src/menu/menu_a_d.c:76` `ModeMenu_UpdateImage`, `:126`, `:128` `ModeMenu_Init`;
- `src/menu/menu_b_c.c:79`, `:107` `HistOutro_Init`; `src/menu/menu_b_d.c:147` `HistSel_Init`;
- `src/menu/menu_c_c.c:180`, `:182`, `:191` `HistResult_Init`; `src/menu/menu_c_d.c:25` `HistSave_Init`;
- `src/menu/menu_c_e.c:226` `CharSel_UpdateFaceLoad`, `:294` `CharSel_UpdateStageLoad`, `:388`, `:391`, `:722`,
  `:729` `CharSel_Init`;
- `src/menu/menu_e_b.c:290` `TeamSel_UpdateFaceLoad`, `:361` `TeamSel_UpdateStageLoad`, `:457`, `:753`, `:760`
  `TeamSel_Init`;
- `src/menu/menu_g_c.c:195` `DuelMenu_Init`;
- `src/menu/menu_h_b.c:49`, `:119` `CharRef_Init`, `:849` `CharRef_UpdateImage`; `src/menu/menu_h_d.c:650`
  `Train_Init`, `:1215` `Train_UpdateImage`;
- `src/menu/menu_i_d.c:98` `EntrySel_UpdateImage`, `:166`, `:334` `EntrySel_Init`;
- `src/menu/menu_j_b.c:59`, `:117` `TourMenu_Init`; `src/menu/menu_k_d.c:24` `TourBg_Init`;
  `src/menu/menu_k_f.c:31` `Bracket_Load`, `:805`, `:806` `Bracket_UpdateImages`, `:830`, `:831`
  `Bracket_LoadImages`;
- `src/menu/menu_l_b.c:83` `SoloSel_UpdateImage`, `:148`, `:254` `SoloSel_Init`;
- `src/menu/menu_m_b.c:184` `UbTeamSel_UpdateFaceLoad`, `:266`, `:421` `UbTeamSel_Init`;
- `src/menu/menu_n_b.c:98` `UbzSel_Init`; `src/menu/menu_n_c.c:140` `UbRank_Init`, `:714`, `:715`
  `UbRank_UpdateFaceLoad`; `src/menu/menu_n_d.c:53` `UbResult_Init`;
- `src/menu/menu_o_b.c:63` `DiscFusion_Init`; `src/menu/menu_o_d.c:146` `MisSel_Init`;
- `src/menu/menu_p_b.c:67` `MisResult_Init`; `src/menu/menu_p_c.c:70` `UbMenu_Init`;
- `src/menu/menu_q_b.c:899` `SimDay_Init`, `:1970`, `:1971` `SimDay_UpdateFaceLoad`;
- `src/menu/menu_r_c.c:38` `SimResult_Init`; `src/menu/menu_r_d.c:53` `SimTop_Init`;
- `src/menu/menu_s_b.c:93` `SurvSel_Init`; `src/menu/menu_s_c.c:30` `SurvResult_Init`;
- `src/menu/menu_u_g.c:60` `EvoZ_Init`; `src/menu/menu_u_h.c:542`, `:544` `EvoZ_Load`;
- `src/menu/menu_v_b.c:759` `EvoZ_UpdateFace`; `src/menu/menu_v_d.c:146` `Shop_Init`;
- `src/menu/menu_w_c.c:133` `EvoTop_Init`;
- `src/menu/menu_x_c.c:39` `Option_Init`;
- `src/menu/menu_y_b.c:957` `DcList_UpdateFace`, `:1002`, `:1008` `DcList_Init`;
- `src/menu/menu_z_c.c:211` `DcMenu_Init`; `src/menu/menu_z_d.c:535` `DcPass_UpdateFace`, `:952` `DcPass_Init`;
- `src/menu/menu_za_d.c:254` `ReplayMenu_Init`;
- `src/sys/late_a.c:555` `Shen_Init`; `src/sys/late_a_c.c:52` `ShenSave_Init`.

As peças desta especificação (diálogo, Flash, tabela de textos, `TexFile`) se reaproveitam: `Dialog_Init` aparece
em outras telas (`menu_x.h`, `DcSave_Init`), e o Flash é o mesmo tocador para todas.

**Os outros arquivos de menu.** Os arquivos `baseFile + k` dos outros grupos de modos (`gMenuArc2..12`,
include/menu/menu_a.h) provavelmente têm o mesmo pacote externo (2), mas não estão no registro: cada um entra com a
sua especificação, pelo id que o código usa.

**A seção 1 do 00473** (0x18500 bytes descomprimidos: tabela de 4, um `TexFile` de 3 texturas e três vazias) não tem
leitor achado; o pacote externo cobre o cabeçalho BPE dela.

**Na F1:** a build PPC aplica o mapa ao conteúdo `#bpe` só de arquivos que não estão na lista dos convertidos
(port/x360/headless/inverter.c). Com 00449 na lista, o menu principal (seção 2) fica sem conversão nenhuma na PPC
até ganhar o seu gancho; o caminho do título até a demo não passa por ele. A comparação da PPC sem mapa com a x86
nos menus (critério da etapa 3 no plano) depende da build sem janela dos menus da etapa 2, que ainda não está no
repositório.

## 12. Não determinado

- Quem lê a seção 1 do 00473, a seção 2 dos logos e a seção 4 do título (nenhum leitor na decomp; podem ser restos
  da versão japonesa ou de outra tela).
- A divisão dos 3 bytes de WaitForFrame (no SWF, u16 quadro e u8 ações a pular): o jogo não os lê, ficam bytes.
- `FlashShapeQuad.unk02`: sem leitor; u16 pela struct.
