# Referência e Shenlong (F4, tarefa 9c-4)

Continuação de `menus-dc.md` e `menus-torneio.md`. Conversores em `port/x360/conv/menus_ref.c`,
tabela única em `conv.c`, ganchos logo após as cinco chamadas de `Sprite_Unpack`, antes do primeiro leitor.
As peças seguem `menus-pecas.md`: N exato, limites e fim conferidos por `Conv_Secoes`, números trocados
pela largura, texto e bytes preservados, enchimento exige zero. Texturas têm o mínimo que o leitor alcança.

## 1. Referência: `menu_charref`, 463 s1 (offset 0x40), N = 28

Leitores em `src/menu/menu_h_b.c`: `CharRef_Init`, `CharRef_BuildList`, `CharRef_Draw`,
`CharRef_MeasureText` e os estados dos detalhes. `CHARREF_CHARA_MAX` é 161.

| Seção | Formato | Mínimo e leitor |
|---|---|---|
| 1 | Flash | `flash[0]` |
| 2, 3, 4, 5 | TexFile | 1, 2, 2, 4 registros, respectivamente |
| 6 | bytes | sem leitor |
| 7, 8, 9, 10, 11 | TexFile | 1, 2, 6, 1, 3 registros |
| 12, 13, 14 | TexFile | 9, 4, 1 registros |
| 15 | TexFile | 8, fundo de `Sprite_DrawPicture` |
| 16 | TexFile | 6 registros |
| 17 | 161 s32 | ids da lista, `charaTbl[i]` |
| 18 | JanelaMsg | `MsgWin_Init` |
| 19 | JanelaIcone | texturas da seção 4 |
| 20 | LipPack | duas entradas por fala; pelo menos 9 + P falas |
| 21 | Textos | pelo menos 9 + P falas |
| 22, 23 | Textos | 161 nomes e formas, indexados pelo id 0..160 |
| 24 | TexFile | 1 registro |
| 25 | Textos | 322 linhas: voz e perfil de cada personagem, `chara * 2` e `+1` |
| 26 | 161 s32 | início do comentário por personagem |
| 27 | P s32 | poses dos comentários; `poseTbl[commentBase + comment - 9]` |
| 28 | 161 s32 | número de falas de comentário por personagem |

P é o maior `início + número` das tabelas 26 e 28, lidas antes da troca. Recusa intervalo negativo ou
que não comporte a soma com 9 num s32. No disco P = 216, textos = 225, legendas = 450 entradas.
As tabelas 17/26/28 têm 704 bytes (161 palavras e 60 bytes zero), a 27 tem 896 bytes (216 palavras e 32 bytes zero).
A tabela de poses não tem só 161 entradas. Os nomes e formas têm 168 linhas; a voz/perfil tem 322.
Os retratos de `CharRef_Init` e `CharRef_UpdateImage` usam o `menu_texfile` existente, registro 0,
no buffer de 0x20800. O trabalho fica no heap (`gCharRef`), não na pilha.

## 2. Shenlong: `menu_shenlong`, 472 s2 (offset 0x1DE80), N = 13

Leitores em `src/sys/late_a.c`, `late_a_b.c`, `late_a_c.c` e o assembly em
`asm/nonmatchings/sys/late_a/Shen_BuildList.s` e `Shen_DrawList.s`. Os modos 70 e 71 pedem a mesma seção 2.

| Seção | Formato | Mínimo e leitor |
|---|---|---|
| 1 | Textos | 16 falas, `Shen_Say` e `MsgWin_Draw`, linhas 0..15 |
| 2 | Desejos | quinze registros de oito bytes, prova abaixo |
| 3 | Flash | um filme da lista |
| 4 | TexFile | 7 registros, `tex[0..2,4..7]` |
| 5 | TexFile | 2 registros, `IconWin_Init` |
| 6 | Confirmação | N = 4, prova abaixo |
| 7 | bytes | 350 registros de 0x28; só o primeiro byte (`ShenItem.type`) tem leitor |
| 8 | JanelaIcone | `IconWin_Init` |
| 9 | JanelaMsg | `MsgWin_Init` |
| 10 | Diálogo | `Dialog_Init`, também no `ShenSave_Init` |
| 11 | GetWin | `GetWin_Init`, peça existente |
| 12 | bytes | sem leitor |
| 13 | Textos | 15 nomes de desejos, linha `no`, `no+4` ou `no+11` |

A seção 7 tem 14.016 bytes, como a tabela de itens do Data Center, mas aqui o único leitor é o byte
do tipo (`Shen_DrawList`, endereço 0x2BDA08 no assembly). O restante continua em bytes, sem inferir
largura pela aparência. A seção 12 tem 2.560 bytes, preservados. A confirmação, seção 6, tem quatro
subseções: Flash; TexFile com 4 registros; TexFile com 2; ItemHelp. Leitor: `ShenCfm_Init`.

### Prova do layout dos desejos

`Shen_BuildList` mantém o ponteiro do arquivo em s0 (0x2BDD9C). Cada ramo faz:

| Dragão | Base do grupo no arquivo | Número de registros | Instruções que provam |
|---|---|---|---|
| 0 | 0 | 4 | 0x2BDF04 copia s0 para s1; 0x2BDF0C passa 4 ao construtor; 0x2BDF30 limita o laço a 4 |
| 1 | 0x20 | 7 | 0x2BDE98 soma 0x20; 0x2BDEA8 passa 7; 0x2BDEC8 limita a 7 |
| 2 | 0x58 | 4 | 0x2BDE20 soma 0x58; 0x2BDE30 passa 4; 0x2BDE50 limita a 4 |

Cada iteração copia oito bytes por `ldl/ldr` em +7/+0 de s1 para +0xB/+4 do nó (`sdl/sdr`), e soma
8 a s1. Isso prova extensão e agrupamento, mas sozinho não prova a largura dos campos. Os leitores
`Shen_DrawList.s` e o código de `Shen_Update`/`Shen_SetupGetWin` leem `wish.id` em +4 do nó e
`wish.kind` em +8 como duas palavras de 32 bits. Portanto se trocam dois u32 por registro, nunca um u64.
No disco a seção tem 128 bytes: 4 + 7 + 4 registros ocupam 0x78 e os oito bytes restantes são zero.
Os tipos encontrados são dinheiro, personagem, estágio e item (3, 2, 1, 0). Os ids são positivos:
limites 350 itens, 35 estágios e 161 personagens; dinheiro cabe em s32. O conversor recusa tipo ou id
fora da faixa e enchimento não nulo. Os grupos começam com 10.000, 5.000 e 50.000 de dinheiro.

## 3. Conteúdos sem chamada e censo completo

O 462 tem uma tabela de três seções vazias: N = 3, quatro offsets 64, tamanho 64. Recebe o
`menu_pacote` existente, sem inventar conteúdo. Não tem leitor no inventário nem no código.
O 472 s1 (551.808 bytes) e o 473 s1 (99.584 bytes) não têm `Sprite_Unpack` no jogo: `Shen_Main`
chama `Shen_Run(2)` e `ShenSave_Run(2)`, `FirstRun_Main` chama `BootCard_Run(2)` e usa a seção 3
para os logos. Recebem `menu_inativo`: extensão limitada às duas conhecidas, tudo em bytes, nenhuma
interpretação de números. Não se afirma que essas telas sejam executadas. Isso fecha o censo sem
confundir conteúdo inativo com conversor numérico pendente. As três chamadas de `CharRef`, `Shen_Init`
e `ShenSave_Init` fecham o inventário: 82 ganchos, nenhuma pendente.

## 4. Roteiros e limites

`referencia.txt` percorre a lista e os detalhes, atualizando o retrato, sem abrir o modelo 3D `ChrView`.
`shenlong.txt` prepara o saldo com `libera desejo`: só no menu principal, com save na memória,
`BT3_DESBLOQUEIO=1` e cartão vazio no início, pela mesma função `so_teste` do desbloqueio e do Disc Fusion.
Só muda `SaveData.money` para zero, em código de teste. A hipótese de que todos os desejos seriam
recusados com tudo desbloqueado não vale para dinheiro: ambos os `IsOwned` devolvem zero para tipo 3.
Não é necessário mudar o sorteio. O dragão 1 concede três vezes 5.000, abre o modo 71 e grava por
McFlow 12, 13, 15 e 0. O saldo 15.000 e os bits das esferas zerados são conferidos no save em memória
e também nos arquivos das duas builds na rodada. A cena do dragão continua fora das imagens até a F5;
as capturas do modo 70 só começam nas marcas da lista e da recompensa. O modo 71 entra no critério.

## 5. Recusas dos conversores

Sobre cópias fora do git (`~/9c-ref/estragos.py`): 68 casos, seis controles intactos aceitos e 62 estragos
recusados com saída 1. Incluem N, mínimos de todas as texturas da Referência, textos e legendas, as três
tabelas de 161 entradas curtas e com cauda não zero, poses curtas e com cauda, intervalos negativos e soma
que estoura; desejos curtos, cauda não zero, tipo inválido e ids fora da faixa de cada tipo; confirmação
com N, texturas e ItemHelp inválidos; itens curtos, Flash, diálogo, GetWin, janelas; extensões dos dois
conteúdos inativos; offset inválido do pacote vazio e retrato sem textura. Nenhum dado original foi alterado.

## 6. Memória da Referência: sobreposição e buffers ainda sem arquivo

A primeira rodada na PPC (`d215210`) teve rastro e leituras iguais e quatro imagens idênticas, mas a memória
recusou cinco globais e os dois buffers do visualizador. Não era erro dos conversores: todos os conteúdos vivos
estavam conformes, inclusive o pacote da tela e o retrato.

`gCharRefState` é declarado como nove ponteiros e `CharRef_Init` grava os handlers 0..8. O esqueleto
`port/data/cod_002FF300.bss.s` reserva só 16 bytes antes de `gUbMenuPlate3Open`, `gSimTrainOutcome0/1/2`
e `gSimCardShown`, quatro bytes cada: os handlers 4..8 sobrepõem esses nomes de outros modos.
No dump do fim da rodada, os nove valores, lidos na ordem nativa, resolvem exatamente para os nove handlers
nas duas builds. Para os cinco sobrepostos: x86 201A32C0, 201A33D0, 201A36B0, 201A38C0, 201A3950;
PPC 201D6780, 201D6A90, 201D6E70, 201D6670, 201D64E0. O `memdiff.py` só usa essa visão se TODOS os nove
valores forem os endereços exatos das funções esperadas, e os cinco símbolos estiverem nas posições
`gCharRefState + 16..32` em ambas as builds. Não aceita qualquer ponteiro. Fora disso vale a comparação original.
Nenhuma reserva do esqueleto ou comportamento do jogo mudou.

`ChrView_Init` (`view_a_d.c`) aloca dois buffers de 0xCE000, grava `file[0].size = 0`, `id = -1` e
só a primeira palavra do buffer como zero. O roteiro não chama `ChrView_Show`: os 843.772 bytes depois
da palavra nunca foram inicializados e guardam restos de alocações anteriores, inclusive números já trocados.
No dump da tela 60: trabalho x86/PPC em 0xBF4460, buffers em 0xBF4580 e 0xCC25C0, slots com tamanho 0
e id -1 nas duas. A tabela de donos prova `ChrView` (0xBC), `ChrView_Init < CharRef_Init < CharRef_Run`.

O comparador só exclui esse trecho não inicializado com: tela 60 nas duas; `gChrView` apontando para um
bloco vivo de 0xBC com o tipo e dono acima; `loading`, `stageOn`, `visible` e `cur` zerados; o slot com
size 0 e id -1 nas duas; o buffer apontado por esse slot sendo um bloco vivo de exatamente 0xCE000,
pareado no mesmo deslocamento; primeira palavra zero nas duas; e sem conteúdo identificado pelas marcas.
Cabeçalho, primeira palavra, metadados e todos os outros blocos continuam comparados. Não amplia os tetos de
`BUFFER_IMAGEM_VAZIO`, nem aceita trechos diferentes de outros buffers pelo tamanho.

A prova da seção 6 foi exercitada com dumps copiados: dois controles aceitos (intacto e alteração apenas
na parte nunca inicializada), 17 recusas (cinco aliases apontando para outro handler válido; primeira palavra,
size e id de cada slot; outra tela, visible, loading, outro buffer, outro dono e outro tipo). O `memdiff`
foi refeito a partir de cada dump ou metadado estragado, e depois julgado por `roteiros.py memoria`.

## 7. Cópia da lista de desejos no rastreio

A instrumentação da x86r grava as cópias de `ShenWish` como acessos de oito bytes: `__asan_loadN_noabort`
com N = 8, no `ShenList_Fill` inline (linha 498 de `late_a.c`). No disassembly da x86r, em 0x2003C520,
o gancho é seguido por dois `mov` de 32 bits, que copiam +0x20/+0x24 do arquivo para +4/+8 do nó.
A PPC faz dois `lwz` e dois `stw` nos campos correspondentes, preservando a ordem dos dois números.
É a atribuição da struct inteira, não um número de 64 bits que deva trocar as metades.

`LARGURA_CONHECIDA` explica só as sete posições do grupo do dragão 1 (0x6A0..0x6D0, passo 8), com leitura
8 e conversão 4, e exige que TODO leitor da posição seja `ShenList_Fill`, sem a lista de PCs cheia.
Os outros grupos não foram alcançados e não recebem exceção antecipada. O controle intacto passou; cinco
cópias foram recusadas: posição ausente no rastreio, outro offset, outra largura, outro leitor e lista cheia.
As outras duas leituras largas sobre bytes nesse pacote são a regra já existente de deslocamento ímpar.

## 8. Os atores da cena do dragão

A PPC sem mapa parava no vblank 3.000, dentro da carga inicial da cena, ao receber os arquivos de atores
sem conversão. `ShenScene_StepLoad` (`view_b_d.c`) passa os ids da tabela `{0xD3C,0xD46}`,
`{0xD3D,0xD46}`, `{0xD45,0xD47}` para `BtlRes_Request`, como slots file[0], e depois
`BtlObj_Create(1/3, slot, 1)`. Os leitores são os mesmos dos modelos de personagens (`personagem.md`).
Os cinco arquivos têm N = 252 e são aceitos integralmente pelo `Conv_Personagem` existente, sem peça nova.
Associá-los a `personagem` em `sFaixas` fez a PPC passar do vblank 3.000 e continuar a cena.

| Índice | Tamanho | Bytes em números trocados | Cobertura |
|---|---:|---:|---|
| 3387 | 587.840 | 393.958 | 100% |
| 3388 | 609.152 | 364.096 | 100% |
| 3396 | 612.992 | 440.556 | 100% |
| 3397 | 262.592 | 137.346 | 100% |
| 3398 | 260.096 | 134.874 | 100% |

Os dois arquivos antes listados em `CONHECIDOS` saíram da exceção de falta de conversor. Os cinco intactos
passaram; cinco cópias com N = 251 foram recusadas. O total dos ensaios de conversores passa a ser
78 casos: onze intactos e 67 estragos, todos com a saída esperada. O roteiro exercita o dragão 1;
os outros dois grupos de atores ficam provados pelo conversor sobre os arquivos, sem afirmar execução.

## 9. Memória do Shenlong: trabalho local e cores da cena

`Shen_Run` tem `ShenWork work` na pilha. Na primeira coleta da tela 70 (vblank 7.173), a raiz e todos
os 19 clips do filme guardam o mesmo owner: x86 0xFFD13768 e PPC 0x40800240. Os cinco `overArg`
ficam em owner + 0xE4 + k * 0x8C (k = 0..4): `ShenWork.box` em +0x9C, `TextBoxFull.draw` em +0x50,
e o filme em +8. O nó cabeça está em owner + 0x370: x86 0xFFD13AD8, PPC 0x408005B0.
Os seis nós do heap têm números 1..6; o `prev` do nó 1 e o `next` do 6 apontam para essa cabeça local.
Os outros elos apontam para os mesmos blocos do heap nas duas builds.

Aqui o comparador não usa uma tolerância de endereço de pilha. `contexto_shen` exige uma raiz de 0x18
com tipo `FlashTl` e cadeia `Flash_Create < Shen_Init < Shen_Run`, um bloco de 19 clips (0x10EC), com
tipo e cadeia `FlashClipList_Init < Flash_Create < Shen_Init`, e seis nós de 0x14 com tipo `ShenNode`
e cadeia `ShenList_Build < Shen_BuildList < Shen_Init`, todos vivos e pareados. Exige os números 1..6
iguais nas duas. Todos os 19 owners devem coincidir com a raiz. Só normaliza: raiz e owners; overArg
que tenha o mesmo k entre as cinco caixas provadas; prev do 1 e next do 6 que sejam EXATAMENTE a cabeça
calculada em cada arquitetura. Qualquer outro campo segue a comparação normal.

Também apareceram 48 palavras em cada buffer de pacotes da GPU (0x100000): todas são bytes `3C 3C 3C 80`,
nos deslocamentos 0x480..0xC30. São as cores de efeitos gravadas byte a byte do erro já conhecido da F5
(`gpu.md`, seção 6), agora com o motor 3D ativo na cena, antes de uma luta. O teto específico é 48 por
buffer, somente na tela 70; não libera o teto de 2048 da luta, a tabela de ordenação ou outra tela.
A tela 71 passou sem essa exceção. Os 11 controles negativos foram recusados: owner da raiz e do clip,
overArg, os dois elos da cabeça, número e id de nó, slots do clip, outro dono, outro tipo e a 49a palavra.
O controle intacto passou. Os dumps estragados foram rejulgados desde o `memdiff`, não só na saída textual;
somente o caso do teto estraga a saída textual com a soma correspondente atualizada.
