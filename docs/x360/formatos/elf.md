# Tabelas de dados do executável: tipagem estática

Projeto e protótipo da tipagem das tabelas de dados do `SLUS_216.78` (e do `DBZP.BIN`) que o port liga a partir de
assembly. Responde ao achado 8 da revisão nº 3: o mapa de larguras dessas tabelas tem de ser completo e estático,
tirado das declarações da decomp, das structs e dos alvos de ponteiro, e não do rastreio de uma luta.

Ferramenta: `port/x360/tools/elftipos.py` (roda no WSL, na raiz do clone, depois de `headless.py --alvo x86`).
Nenhum byte do jogo está neste documento nem nas saídas da ferramenta: só posições, larguras, nomes de símbolos,
tipos do código e contagens. A única informação tirada dos valores é a marca "valor zero" por diretiva, usada para
medir o risco do que fica sem tipo.

## 1. Como as tabelas chegam ao programa hoje

### 1.1 Do ELF ao assembly

- O splat (`config/SLUS_216.78.yaml`, `config/DBZP.yaml`) corta o executável do usuário. As faixas que a decomp
  ainda não tem em C (`data`, `sdata`, `rodata`, `lit4`, `bss`, `sbss`) viram `asm/data/*.s` com um rótulo
  (`dlabel`) em cada endereço que algum código ou dado referencia. O tipo de cada diretiva é palpite do splat pelo
  valor: `.word` para quase tudo, `.float` quando o número parece float, `.asciz` quando os bytes parecem texto,
  `.byte`/`.short` em trechos que ele não alinhou, e `.word Símbolo` quando o valor bate com o endereço de um
  símbolo.
- `port/tools/gen_data.py` reescreve esses arquivos para o assembler do host (`port/build/gen/data/*.s`):
  `.word` vira `.long`, rótulo de dentro de função vira 0, cada pedaço começa no mesmo resto módulo 16 que no PS2.
  Junta também as tabelas que os `.c` puxam com `INCLUDE_RODATA` (no port a macro some, e a tabela vem como
  `inc_<nome>.rodata.s`) e os microprogramas da VU1 (`vu1_micro.data.s`, um `.incbin`).
- São 72 objetos e 2.242 rótulos. O maior, `cod_1C3400.data` (160 KB, 852 rótulos), é a seção `.data` inteira do
  programa do PS2, inclusive as tabelas das bibliotecas da Sony, do CRI e da libc. Diretivas: 31.393 `.long`
  (1.872 delas `.long Símbolo`), 42.416 `.byte`, 3.378 `.short`, 1.288 `.float`, 1.116 `.asciz`, 9 `.incbin`.

### 1.2 Do assembly ao programa

- Build de PC e builds da F1: cada `.s` é montado e ligado (`headless.py`, `ligar`). O linker preenche os
  `.long Símbolo` com os endereços do port; o resto são os bytes do disco, na ordem do PS2.
- Na PowerPC (F1), `elfdados.retipar` reescreve cada diretiva pela largura que o rastreio viu ler antes de montar.
- Programa distribuído pelo upstream (sem dados do jogo): `make_skeleton.py` escreve `port/data/*.s`, versionado,
  com a forma de cada tabela sem os valores (rótulos, `.space N` onde entram valores do disco, `.zero N`,
  `.long Símbolo`) e `port/data/index.txt`. `make_dat.py` grava a lista (endereço no programa, arquivo, deslocamento,
  tamanho), e `Port_LoadGameData` (`port/src/plat_mem.c`) copia esses trechos do `SLUS_216.78` e do `DBZP.BIN` do
  usuário na partida e confere um hash.

### 1.3 Como o C enxerga as tabelas

- O objeto é definido no `.s`; o C só declara: `extern T nome;` num header ou no `.c` (às vezes dentro da função),
  ou definição tentativa sem inicializador (`-fcommon` junta com a definição do `.s`).
- Uma tabela pode ser declarada com tipos diferentes em unidades diferentes (por exemplo `D_002EC2C0` como `Vec4`,
  `EftDetVec` e `StgColVec`), declarada como bytes e lida por cast (`extern u8 gEftGlowClass[0x18]` passado como
  classe de tarefa), ou não ter declaração nenhuma (só o assembly não decompilado a usa).
- Ponteiro: o `.s` já diz (`.long Símbolo`), para código ou dado.

## 2. A fonte da verdade

**Escolha: as declarações C da decomp, lidas pelo AST do clang de cada `.i` da build, com o layout calculado pelo
próprio compilador (DWARF de uma sonda compilada com as flags de layout da build), ancoradas nos rótulos e nos
ponteiros do `.s` (ou do esqueleto `port/data`).**

| Fonte | O que dá | Por que sim ou não |
|---|---|---|
| AST JSON do clang dos `.i` (o mesmo texto pré-processado que a build compila) | toda declaração de cada rótulo, em cada unidade, e todo cast do rótulo para outro tipo de ponteiro | é o único lugar com as declarações `extern` e as visões por cast; `tipos.py` já percorre esse AST |
| DWARF de uma sonda: o `.i` com `__typeof__(X) *__elftipo_N;` no fim, compilado com `-g -m32 -malign-double` | deslocamento e largura de cada campo, vetores, uniões, campos de bits, tipos apontados | o layout sai do mesmo compilador e das mesmas flags da build que liga as tabelas; nada de reimplementar as regras de layout do C |
| DWARF do `bt3_x86` sozinho | só os tipos | recusado como fonte: o clang não grava as declarações `extern` de objeto definido em assembly (conferido: `llvm-dwarfdump --name=D_002C3550` não acha variável), nem os casts |
| `-fdump-record-layouts` | layouts em texto | recusado: texto frágil de interpretar, e não liga rótulo a tipo |
| Palpites do splat (`.float`, `.asciz`, `.byte`) | um tipo para tudo | só como último recurso: é heurística pelo valor, e erra (seção 4.3: texto que o C diz ser número) |
| Rastreio (`elfdados.py`) | a largura lida numa luta | não é fonte, é conferência: cobre só o que uma luta leu |

O layout de struct do x86 é o que vale porque `layout.py` já exige que a PowerPC dê o mesmo layout em todas as
1.964 structs. O compilador do 360 ainda não está nessa conferência (seção 6).

## 3. O protótipo: regras e casos difíceis

Etapas (detalhe no cabeçalho de `elftipos.py`):

1. **Objetos.** Cada `.s` vira rótulos (nome, deslocamento) e diretivas (deslocamento, tamanho, tipo). Os
   deslocamentos são os mesmos do rastreio e do `elfdados.py`.
2. **Declarações e usos.** Em cada `.i` (318), toda `VarDecl` com nome de rótulo que não é definição, e todo uso
   do rótulo convertido para outro tipo de ponteiro: cast explícito, ou conversão implícita entre ponteiros que o
   `-w` cala. `(T *)&x[k]` e `(T *)(x + k)` guardam o deslocamento. Cast para `void *`, `u8 *` ou `char *` não diz
   largura (é `memcpy` ou repasse).
3. **Layouts.** Uma sonda por unidade que tem pedidos (133 compilações, 584 layouts, 457 tipos apontados).
4. **Atribuição.** Candidatas de cada rótulo, nesta ordem de confiança: declaração tipada, visão por cast, alvo de
   ponteiro, declaração genérica. Mesma posição e largura: concordam. Larguras diferentes no mesmo byte: conflito.
5. **Alcance no port.** Vivo é o rótulo que algum objeto C da build referencia (`nm -u`) ou que se alcança de um
   vivo por `.long Rótulo`.
6. **Decisão por diretiva**: `fica`, `2x2` (`.long` de dois u16), `4x1`, `misto` (por exemplo u16 e dois u8),
   `u64` (par de `.long` que troca de lugar), `2x1`, `junta` (diretivas menores que o campo), `texto->num`,
   `conflito`, ou `nd` com o motivo.

Os casos difíceis:

- **Várias declarações do mesmo símbolo (visões entre unidades).** Todas entram e se conferem byte a byte. Nos
  dados com valor (`data`, `sdata`, `rodata`) não houve nenhum conflito: os 37 rótulos com mais de um tipo
  concordam na largura (por exemplo `D_002EC2C0`: `Vec4`, `EftDetVec` e `StgColVec`, todos quatro floats;
  `gMathfSinCoef`: `f32[4]` e `float[4]`; `gBtlCamView`, com 30 declarações diferentes, 29 ponteiros e um `s32`,
  todas de 4 bytes). Todos
  os 1.792 bytes em conflito estão em `.bss`, que começa zerado e não tem o que converter: três uniões com membros
  de larguras diferentes (`LoadScreen`, `SndRpcBuf`, `BtlScriptWork.talk.requests[].u`), `BtlScriptWork` contra a
  visão `BtlScriptCmdWork`, e `Pad` contra `LoadPad`. São visões em tempo de execução (classe 4 do guia), assunto
  do detector de visões, não desta conversão.
- **`extern u8 x[]` usado com cast.** O cast vira candidata (10 rótulos têm visão tipada; 152 usos em 85 rótulos
  são casts genéricos). Vetor de u8 que nenhum código converte para outro tipo, nem para `void *`, conta como bytes
  (24 rótulos; por exemplo `gEftBladeColor`, uma cor RGBA lida por `u8 *`). Vetor de u8 com cast genérico (23
  rótulos) não decide nada: vale o que o `.s` diz, e o `.long` fica "não determinado".
- **Tabela sem declaração em C.** 1.374 rótulos só aparecem no assembly que o port não compila (`asm/cod/*.s`:
  biblioteca da Sony, CRI, libc e rotinas trocadas pelo port), mais 85 sem referência nenhuma. A análise de alcance
  separa esse dado morto do resto. Rótulo sem declaração apontado por um campo tipado ganha o tipo apontado.
- **Rótulo no meio de objeto.** Tipo completo cobre o `sizeof`; rótulo dentro desse alcance é interior e só
  confere. Vetor sem tamanho (`extern T x[]`) e alvo de ponteiro vão até o próximo rótulo, qualquer um. A primeira
  versão estendia até o próximo rótulo *declarado*, e `extern s32 gAdxChannelGainTbl[]` (indexado pelo canal em
  `src/sys/adx.c`) cobriu 70 KB de tabelas alheias, com 40 mil `.byte` marcados como "junta" por engano. Com a regra
  certa, 11 vetores param num rótulo sem declaração; os 5 rótulos assim estão todos na parte morta.
- **Ponteiros.** `.long Símbolo` é ponteiro, com ou sem tipo no C. Nenhum campo ponteiro do C tem número não nulo
  no `.s`. Campo `T *` tipado cujo valor é `.long Rótulo` dá ao rótulo o tipo T (119 rótulos tipados assim, em 4
  rodadas). `void *`, `u8 *` e ponteiro de função não propagam: 51 rótulos ficam "alvo de ponteiro sem tipo",
  quase todos classes de tarefa (`BtlTaskClass`, seis ponteiros de função) passadas como `void *cls` a
  `BtlTaskList_AddTail`.
- **Uniões.** Membro só de bytes (`u8 raw[N]`) não manda se outro membro tem números; membros com larguras
  diferentes viram conflito.
- **Campos de bits.** A unidade do tipo declarado (u8, u16, u32), que é o valor que o código lê no 360 com
  `#pragma bitfield_order(lsb_to_msb)` (e na F1 com `bitfields.py`).
- **Diretiva do splat menor que o campo.** `.byte` ou `.short` dentro de um campo maior: `junta`. `.asciz` onde o
  C diz número: `texto->num`.

## 4. Resultados

Execução no WSL: cerca de 16 s (12 núcleos). Uma sonda caiu: `gStgColMesh`, ponteiro para struct anônima em
`port/src/headless.c`, em `.bss`.

### 4.1 Cobertura em bytes

| Classe | Bytes | Comentário |
|---|---:|---|
| total dos 72 objetos | 436.830 | |
| zero (`.space`: `bss`, `sbss`, alinhamento) | 219.816 | nada a converter |
| morto no port | 181.726 | 1.624 dos 2.242 rótulos; tabelas que só o assembly não compilado usa |
| **vivo** | **35.288** | 618 rótulos: 448 referenciados pelo C do port, 170 alcançados por ponteiro |
| vivo, tipado pelo C | 11.742 | |
| vivo, ponteiro | 7.268 | |
| vivo, texto só pelo `.s` | 138 | `.asciz` sem declaração (bytes: nada a trocar) |
| vivo, float só pelo `.s` | 24 | 6 `.float` que só o splat tipou |
| vivo, não determinado com valor zero | 540 | sem risco: zero é igual nas duas ordens |
| vivo, não determinado e não nulo | 15.576 | 15.568 do microcódigo da VU1 e 8 bytes depois do fim de `D_002EC2C0` |

Os objetos inteiramente vivos e tipados: as tabelas `inc_*` usadas (`D_002EDA70`, `D_002EDC70`..`D_002EDEE8`,
`D_002F2420`, `gEftShotFxSegs`, `gEftShotFxRows`), e a parte viva das `sdata` (ponteiros globais) é toda tipada. De `cod_1C3400.data` ficam vivos
cerca de 16 KB; de `cod_1F44C0.rodata` (textos e tabelas de salto), 264 bytes.

O não determinado vivo, por motivo: microcódigo da VU1 (15.568, um `.incbin`; regra própria, seção 6), alvo de
ponteiro sem tipo (372, todos zero), depois do fim do tipo do rótulo anterior (116, dos quais 8 não nulos:
`D_002EC2C0` ocupa 32 bytes até o próximo rótulo e os três tipos declarados têm 16), só declaração genérica (60,
todos zero).

### 4.2 Rótulos

| Situação | Rótulos |
|---|---:|
| tipado por declaração | 419 |
| tipado por declaração e visão | 10 |
| tipado como alvo de ponteiro | 119 (+1 com declaração) |
| só declaração genérica (u8 com cast) | 23 |
| alvo de ponteiro sem tipo (`void *`, `u8 *`, função) | 51 |
| sem declaração, referenciado só por assembly não decompilado | 1.374 |
| sem declaração, apontado por tabela sem tipo | 153 |
| sem declaração e sem referência | 85 |
| sem declaração, logo depois de um vetor sem tamanho | 5 |
| interior de outro rótulo | 2 |

### 4.3 O que muda nas diretivas vivas

A maioria das diretivas vivas fica como está (`.long` de 4 bytes, ponteiro, float, byte). As que precisam de outra
forma no assembler big-endian, por struct:

| Decisão | Struct (campo) | Diretivas |
|---|---|---:|
| `.long` → 2 u16 | `FontIconName` (`chars`), `BtlMenuItem` (`text`), `BtlMenu`, `BobjTalkStep` (`frame`), tabelas `u16[]` | 273 |
| `.long` → 4 u8 | `HudPromptIconDef`, `FontIconDef`, `EftVolleyAimStep`, `gEftBladeColor` | 148 |
| `.long` → u16 + u8 + u8 | `HudPromptIconDef` (`frames`), `FontIconDef` (`frameCount`), `FontTagDef` (`len`) | 65 |
| `.byte` → u16 (junta) | `gHudPromptCueDefs` (`HudPromptIconDef.frames`, que o splat escreveu em bytes) | 8 |
| par de `.long` → u64 | `TexBltPacket` (`gifTag`, `bitbltbuf`, `reg`) | 4 pares |
| `.asciz` → número | `EftSparkTbl.v` (s32, três tabelas), `StgPlace.pos.x` (f32) | 4 |

## 5. Comparação com o rastreio

A decisão do rastreio é a que `elfdados.retipar` toma em cada diretiva, pela mesma regra dele. Hoje ele retipa 12
`.long` em dois u16 (todos `BobjTalkStep.frame`) e tem 1 conflito.

| Categoria | Diretivas | Explicação |
|---|---:|---|
| lidas pela luta e com a mesma decisão | 1.490 | 547 ponteiros, 430 bytes, 274 `.long` de 4, 223 floats, os 12 `.long` → 2 u16, 2 textos, 2 `.short` |
| só o rastreio decide | 89 | todas com valor zero. 84 são posições de ponteiro de função de classes de tarefa que o C só vê como `void *` (lidas por `BtlTaskList_Update`, `_Draw`, `_PostUpdate`, `_Reset`, `BtlTask_Kill`); 5 são de classes declaradas como `u8[0x18]` e passadas a `void *`. Sem risco; o certo é tipar essas tabelas no C |
| a estática decide um conflito do rastreio | 1 | `gEftEmitNodeSlot` (`s32[8]`): lido como s32 e como byte (`EftAnimPart_Draw`, `EftAnimPart_Update`, `EftBill_Update`, `EftEmit_Spawn`). A tabela é s32; a leitura de byte é uma visão no código (classe 4) |
| discordam | 0 | antes da regra "declaração genérica não contradiz o `.s`" eram 17: posições `.long Símbolo` dentro de classes declaradas `u8[24]` |
| só a estática decide | 6.219 | 494 delas vivas e com forma diferente de "fica" (a tabela de 4.3 inteira, menos os 12 `BobjTalkStep`): menus, ícones nos textos, avisos do controle na HUD, roteiros de mira, cor da lâmina, o pacote de envio de textura e as tabelas de faísca. Com o mapa do rastreio, sairiam erradas no 360 |
| nenhum dos dois | 71.792 | quase tudo na parte morta |

Toda posição que a luta leu está na parte viva (1.580 de 1.580): a análise de alcance não chamou de morto nada que o
jogo usou.

## 6. O que falta decidir

**Proposta: o mapa é gerado, não escrito à mão, e se aplica onde os valores entram no programa.**

Rodado sobre o esqueleto versionado do upstream (`elftipos.py --dados port/data`), sem disco, o mapa sai idêntico
ao das tabelas do usuário (as mesmas 49.629 posições): ele só depende dos rótulos, dos `.long Símbolo` e do código.
Então pode ser gerado no build, sem nenhum byte do jogo, e conferido contra o rastreio quando houver um. Na partida,
`Port_LoadGameData` copia os trechos `.space` do SLUS; no 360, depois da cópia e da conferência do hash (que é sobre
os bytes originais), cada posição do mapa troca de ordem pela largura. Os `.long Símbolo` não entram na cópia: o
linker do 360 já os escreve big-endian.

Pendências:

1. **Onde aplicar.** Na carga (`Port_LoadGameData`, cerca de 35 KB vivos por partida, o mesmo caminho do upstream)
   ou no instalador (uma vez, gravando a imagem convertida em `gamedata/`). A carga parece melhor: o hash continua
   sobre o original e não há segundo formato no disco.
2. **Gerado no build ou versionado.** Gerar a cada build mantém o mapa em dia com cada merge do upstream; versionar
   dá um diff revisável. Pode ser os dois: gerado no build e conferido contra uma cópia versionada (que só tem
   posições e larguras).
3. **O esqueleto no build do 360.** `port/data/*.s` é sintaxe do GNU as. O `cl`/`link` do XDK precisa de outro
   jeito de pôr cada rótulo no deslocamento certo dentro do objeto (por exemplo seções `.dados$NNNN` ordenadas pelo
   linker, ou uma struct por objeto com os rótulos como membros).
4. **O resíduo vivo.**
   - Microcódigo da VU1 (15.568 bytes): pela convenção da decisão 12 para fluxos VIF, palavras u32. O emulador do port
     (`gs_vu1.c`) lê `lo`/`hi` com `memcpy` de 4 bytes; confirmar na F3.
   - `D_002EC2C0`: 16 bytes depois dos tipos declarados, 8 não nulos. Declarar o tamanho real no C ou confirmar que
     ninguém lê.
   - 6 floats e 138 bytes de texto que só o splat tipou: declarar no C. Gravar o palpite do splat no mapa exigiria
     o disco na geração.
   - Classes de tarefa vistas como `void *`: hoje sem risco (ponteiros ou zero), mas o certo é declará-las como
     `BtlTaskClass` na decomp.
5. **Dado morto.** Fica como veio (little-endian). Se o port passar a compilar uma função que o lê (o upstream
   decompila mais código), o rótulo vira vivo. Proteção: o build falha quando um rótulo vivo fica sem tipo ou em
   conflito.
6. **u64 em pacote da GPU** (`TexBltPacket`): o mapa segue o tipo declarado (u64), como manda a decisão 12 para campo
   de struct u64. Confirmar com o GS do 360 na F3.
7. **Layout do compilador do 360.** O mapa usa o layout do clang x86 (`-malign-double`), que `layout.py` garante
   igual ao da PowerPC GCC. Falta a mesma conferência contra o `cl` do XDK (alinhamento de u64 e double em 8, campos
   de bits com o pragma).
8. **Validação na F1.** Trocar o `elfdados.py` (rastreio) pelo mapa estático na build PPC (`headless.py`) e rodar o
   `f1.sh`: a luta tem de continuar idêntica e o `memdiff.py` deve mostrar menos palavras diferentes na área de
   dados. Não feito aqui (sem recompilar). A comparação da seção 5 pode virar teste de regressão do ciclo.
9. **Registro.** Se a proposta for aceita, ela entra como decisão numerada em `docs/x360/decisoes.md`.

## 7. Como rodar

```
BT3_RAIZ=$PWD python3 port/x360/tools/elftipos.py [--dados port/build/gen/data | port/data] [--saida DIR] [--sem-rastreio]
```

Saídas em `DIR` (padrão `/tmp/elftipos`): `mapa.tsv` (objeto, deslocamento, largura, tipo, origem, campo),
`diretivas.tsv` (decisão estática, decisão do rastreio, categoria, motivo, vivo ou morto, marca de valor zero, dono)
e `relatorio.txt`. As saídas ficam no WSL: o `mapa.tsv` só tem posições e larguras, mas as outras duas têm a marca
de valor zero por diretiva.
