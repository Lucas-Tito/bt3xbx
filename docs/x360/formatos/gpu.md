# Pacotes da GPU e memória do GS num host big-endian

A convenção da decisão 19 (etapa 4 da F3), escrita como contrato entre quem monta os pacotes (o código do jogo, os
conversores do disco, `gs_sony.c`) e quem os lê (`port/src/gs/gs_core.c`, `gs_vu1.c`, e o backend do 360 que vier
depois). No PC little-endian tudo isto é a ordem natural e nenhuma macro muda nada.

## 1. O que vai para a GPU, na memória principal e na da VU1

| O quê | Como fica no host big-endian | Quem garante |
|---|---|---|
| Etiqueta DMA, código VIF, palavra de dado de UNPACK de 32 bits, microcódigo da VU1 | palavra de 32 bits na ordem do host | é como o código grava (`u32`) e como o conversor troca (decisão 12); os pacotes da VU1 das tabelas do executável ainda não (seção 6) |
| Valor de 64 bits: etiqueta GIF, dado de registrador em A+D e REGLIST, etiqueta DMA gravada como um número | duas palavras de 32 bits, **a baixa primeiro** (a imagem do PS2 em palavras) | `PS2_PKT64` no código do jogo; o conversor troca como `u32` |
| Campo menor que 32 bits dentro de uma palavra (byte de RGBAQ, metade de UV, NLOOP de 16 bits) | na posição que o valor tem na palavra: R no byte baixo, U na metade baixa | struct permutada em `#ifdef PORT_BIG_ENDIAN`; `PS2_LOW(u16, *(u32 *)tag)` |
| Elementos de 8 e 16 bits de um UNPACK (V2-16, V4-8, S-16...) | empacotados a partir do fim baixo de cada palavra de 32 bits | o conversor troca o fluxo VIF como `u32`; o leitor extrai por palavra |
| Dados de IMAGE (o que um upload põe na memória do GS) | **os bytes do PS2**, sem troca | decisão 12 (a imagem em bytes) |

Regra prática: quem lê uma palavra de 32 bits de um pacote lê um número certo em qualquer host; quem lê 64 bits de
uma vez lê as metades trocadas num host big-endian e passa o valor por `PS2_PKT64`.

## 2. No código do jogo (`src/`, `include/`)

- **`PS2_PKT64(x)`** (`include/types.h`; a do port em `port/include/port_compat.h`): no PS2 e no PC é `(x)`; num host
  big-endian troca as metades de 64 bits. Envolve o lado direito de toda gravação de 64 bits num pacote
  (`p[0] = PS2_PKT64(GIF_TAG(5, 1, 1));`, `p[1] = PS2_PKT64(GS_ALPHA_1);`), a cópia de um número para um pacote
  (`pkt->tex0 = PS2_PKT64(tex->tex0);`) e a leitura de um campo de pacote usado como número. `|=`, `&=` e `^=`
  aceitam o valor trocado direto (a troca das metades é uma permutação dos bits). Cópia de pacote para pacote fica
  como está.
- As macros de valor (`GIF_TAG_EX`, `GS_SET_*`) continuam dando o **número**: a maior parte das gravações nem passa
  por elas (literais e números de registrador gravados direto, `p[0] = 0x44; p[1] = GS_ALPHA_1;`), e uma troca
  dentro delas somaria duas trocas onde o valor também passa pela gravação.
- **`PS2_PKT64_FIELDS(baixa, alta)`**: declara os campos de bits de um valor de 64 bits de pacote, os da metade baixa
  primeiro; no host big-endian a ordem das metades se inverte. Só as metades: dentro de cada uma a ordem fica, porque
  o port aloca campo de bits do bit 0 para cima nos dois compiladores PPC (`#pragma bitfield_order(lsb_to_msb)` do
  `c89.py` no 360, `bitfields.py` no GCC); sem isso, o padrão deles é do bit mais alto para baixo. Usado nas 26 structs XYZF2 (`EftGsXyzf`...,
  `GfxPostSpritePkt.xyz0/xyz1`). A `EftPrim` (src/battle/eft_d.c) **não** usa: é uma visão sobre um `u64` local
  (número), copiado depois para o pacote por `PS2_PKT64`.
- Structs de pacote com campos menores que uma palavra, permutadas em `#ifdef PORT_BIG_ENDIAN`: `GfxPostSpritePkt`
  (RGBAQ e UV, em structs anônimas que o `layout.py` não abre; a etiqueta DMA já era), `EftSprPkt`, `EftTriVtx` e
  `EftNVtx` (estas três e as XYZF2 com nome constam em `layout.PERMUTADAS`).
- **Número, não pacote** (sem macro na gravação): o TEX0 guardado nas structs de textura (`TexEntry.tex0`, que o
  conversor troca como `u64`, e as cópias dele nas structs dos efeitos), flags de 64 bits, `SaveData.charaBits`,
  os registradores privilegiados do GS (`*GS_DISPFB1 = ...`, memória comum no port) e `sceGsDispEnv`.
- **Tabelas do executável:** o único pacote com `u64` vivo nelas é o `gTexBltPacket`; `elftipos.py` (`PACOTES_GPU`)
  troca esses campos como dois `u32`.
- **Conferência estática:** `port/x360/tools/pacotes64.py` (no WSL, depois da build x86) classifica todo acesso de
  64 bits à memória (pacote ou número, pelas listas `NATIVO_VAR` e `PACOTE_TIPO`) e falha se uma gravação em pacote
  estiver sem `PS2_PKT64`. Rodar depois de cada merge do upstream que mexa em desenho.

## 3. No leitor (`port/src/gs/`)

- `PS2_PKT64` também em `gs_internal.h` (os arquivos do GS não incluem o `port_compat.h`): `Gs_Gif` (etiqueta GIF,
  PACKED, REGLIST), `kick` e as versões em shader (`hle_program*`, `tag[0]`) da VU1, e `gs_sony.c` (a etiqueta e os
  registradores do `sceGsDBuff`, gravados e lidos).
- UNPACK de 8 e 16 bits (`GsVu1_Unpack`): `unpack_elem` tira o elemento da palavra de 32 bits, a partir do fim baixo.
  A memória da VU1 continua sendo palavras do host (o UNPACK grava `u32`, o interpretador lê `u32`).
- `vertex`: S é a metade baixa do registrador ST (antes era `memcpy` dos 4 primeiros bytes do `uint64_t`).

## 4. A memória do GS (`sVram`)

**Os bytes do PS2**, como a memória de 4 MB do GS: um pixel de 32 bits é a palavra little-endian dos seus 4 bytes, um
de 16 bits a metade little-endian, os índices de 8 e 4 bits o byte e o nibble que o PS2 endereça.

- Os uploads (`transfer_data`) copiam os bytes do IMAGE para lá sem mudar nada, em qualquer host (os caminhos
  rápidos de 32, 16, 8 e 4 bits e o geral).
- `vram_rw`, que lê e grava um pixel como número, passa a palavra por `GS_LE32` (troca de bytes no big-endian, nada
  no PC). É o único lugar que lê a `sVram` como palavra.
- `Gs_BlockPtr` devolve os bytes do PS2: quem lê textura a partir dele lê little-endian. Hoje só o leitor de pacote de
  texturas do PC (`gs_texpack.c`), para um hash de bytes, igual em qualquer host.
- A decodificação de textura dos backends de GPU (`gs_draw.c`, `texture_get` e `clut_texture`) e a cópia de um quadro
  da memória (`gs_gl.c`, `gs_gpu.c`) passam pela `Gs_VramRead`: já saem certas num host big-endian. O backend do 360
  (decisão 20) recebe delas o u32 `R | G<<8 | B<<16 | A<<24` e o põe numa textura `D3DFMT_LIN_A8B8G8R8` sem troca.
- `Gs_PageHash` é chave de cache e só se compara no mesmo host: o valor muda entre x86 e PowerPC, sem efeito.
- A captura (`BT3_SHOT`) grava R, G, B em bytes.

## 5. Como conferir (WSL, sem console)

1. `pacotes64.py`: nenhuma gravação em pacote sem a macro.
2. `memdiff.py --estrito` no quadro 3000 dos menus (como o `menus.sh`): nos buffers de pacotes (heap#24 e #25, de
   `Dma_InitBuffers`), em `gGfx` e em `gTexBltPacket`, nenhuma palavra `u64`, `u64-com-zero`, `2xu16` nem
   `igual-sem-inverter` (byte gravado fora do lugar).
3. O rasterizador de software (`BT3_GS=1 BT3_SHOT=N`, que grava `port/build/shots/frame_NNNNN.ppm` relativo à pasta
   de trabalho; `BT3_GS_FROM=Q` só desenha a partir do quadro Q) com imagens iguais byte a byte entre x86 e PPC. O GS
   da PPC compila com `-ffp-contract=off` (`headless.py`): com `fmadd` a borda de um polígono muda um pixel aqui e ali.
   Quadros do GS nos menus (não são vblanks): cartão 0 a 233, logos 234 a 1745, título 1748 a 4243, carregamento da
   demo 4244 a 4335.

## 6. Fora daqui

- Vetores `u8 rgba[4]` em structs de vértice e de pacote dos efeitos da luta (`EftGsVtx`, `EftStripPkt`,
  `EftLinePkt`, `EftSteamPkt`, `EftWaterGsVtx`, `EftWaterSpritePkt`, `EftTGsVtx`, `EftAeGsVtx`, `EftPGsVtx`,
  `EftSGsVtx`, `EftUGsVtx`): o índice do byte não se permuta na declaração; cada gravação `rgba[i]` precisa de um
  índice trocado (cerca de 150 em `src/battle`). Fase da luta (F5).
- Os pacotes da VU1 nas tabelas do executável (`vu1_micro.data`, `D_002BF6B0`..., `include/sys/vu1_packet.h`: códigos
  VIF, microcódigo e constantes) são declarados `u8[]`, e o mapa das tabelas os deixa como vieram do disco (15.568
  bytes de largura 1); o VIF e a VU1 os leem como `u32`. Têm de ser tipados como palavras antes da F5. **Feito na F4,
  etapa 6:** o `elftipos.py` trata o objeto como fluxo VIF (`FLUXOS_VIF`: cada rótulo um vetor de u32 até o próximo),
  e o mapa das tabelas troca as 3.892 palavras. Antes, a VU1 da PPC rodava instruções inexistentes ("unimplemented
  lower instruction", programas que não terminavam) e a imagem da luta saía com 229 mil pixels diferentes da x86.
- `sceGsExecStoreImage` (leitura da memória do GS de volta, `ScrXfade`) e o `BT3_GS_ZDUMP` (depuração) gravam na
  ordem do host.
