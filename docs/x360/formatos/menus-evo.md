# Evolution Z e loja (F4, tarefa 9c-1)

Continuação de `menus-telas.md`, `menus-pecas.md` e do inventário `menus-modos.md`.
Fontes: `EvoTop_Init` (src/menu/menu_w_c.c), `Shop_Init` (menu_v_d.c), `EvoZ_Load`
(menu_u_h.c), `EvoZ_Init` (menu_u_g.c), `EvoZ_UpdateFace` (menu_v_b.c) e
`PassWin_Init` (menu_za_b.c). Conversores em `port/x360/conv/menus_evo.c`.

O pacote `gMenuArc7`, id `baseFile + 4` = 453, índice 452 da partição 1, tem três
seções BPE: 1 `menu_evo_topo` (modo 48), 2 `menu_loja` (50), 3 `menu_evoz` (49).
O arquivo recebe `menu_pacote`. A tabela única de `conv.c` escolhe cada conteúdo.
Cada gancho converte logo após o `Sprite_Unpack`, antes do primeiro leitor.

Cada pacote tem uma tabela de deslocamentos, conforme `menus.md` 2.1. N é exato;
limites, ordem e fim são conferidos por `Conv_Secoes`. Números são trocados no
lugar por sua largura, bytes e texto do Flash ficam intactos, enchimento exige zero.
As peças são as já especificadas, salvo a janela de senha abaixo. `TexFile` usa
o mínimo de registros que o código alcança, indicado nas tabelas (8 no fundo
lido por `Sprite_DrawPicture`). Os textos têm o mínimo de linhas alcançadas e
as legendas duas entradas por fala, como `Voice_PlayWithSubtitle` as lê.

## Topo: `menu_evo_topo`, 452 s1, N = 11

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile, fundo | 8 | `EvoTop_Init`, `EvoTop_Draw` |
| 2 | TexFile, guia | 4 | `EVO_RES(2)`, MTEX 0, 1, 3 |
| 3 | TexFile, ícones | 2 | `IconWin_Init`, registros 0 e 1 |
| 4 | TexFile | 8 | `EVO_RES(4)`, MTEX 0..7 |
| 5 | TexFile | 2 | `EVO_RES(5)`, MTEX 0, 1 |
| 6 | Flash | | `Flash_Create` |
| 7 | Textos | 18 | `MsgWin_Draw`, `voiceLine` 0..17 |
| 8 | JanelaMsg | | `MsgWin_Init4` |
| 9 | JanelaIcone | | `IconWin_Init` |
| 10 | LipPack | 36 entradas | `EvoTop_PlayVoice`, falas 0..17 |
| 11 | Diálogo | | `DcSave_Init`, `Dialog_Init(pack, NULL, 0)` |

O DcSave recebe a seção 11 já convertida. `DcSave_Init` chama `McFlow_Init(1)`;
este descomprime o arquivo 477 e já passa pelo gancho `menu_mcflow` em
`src/sys/mcflow_a.c`. Portanto não precisa de um novo gancho no DcSave.
Ao sair com `gProgress->flags & 1`, `EvoTop_Input` chama `DcSave_Start`, que
inicia `McFlow_Start(0)`. A rodada deve alterar uma customização e aceitar o save.

## Loja: `menu_loja`, 452 s2, N = 33

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile, fundo | 8 | `SHOP_RES(1)`, `Shop_Draw` |
| 2, 3, 4, 5 | TexFile | 3, 2, 3, 3 | `SHOP_RES`, MTEX máximo 2, 1, 2, 2 |
| 6 | Flash | | flash 0 |
| 7, 8 | TexFile, guias | 4, 4 | `Shop_Init`, `Shop_SwapGuide`, MTEX 0, 1, 3 |
| 9 | Flash | | flash 3 |
| 10, 11, 12, 13, 14, 15, 16 | TexFile | 1, 3, 3, 5, 4, 2, 1 | `SHOP_RES` |
| 17 | Flash | | flash 1 |
| 18 | TexFile | 2 | MTEX 0, 1 |
| 19 | Flash | | flash 2 |
| 20 | TexFile, ícones | 2 | `IconWin_Init` |
| 21 | JanelaMsg | | `MsgWin_Init` |
| 22 | JanelaIcone | | `IconWin_Init` |
| 23 | sem leitor | | bytes preservados, sem forma conhecida |
| 24 | TexFile | 2 | MTEX 1 |
| 25 | ItemHelp | | `ItemHelp_Init` |
| 26 | LipPack | 60 entradas | dois guias, 15 falas cada |
| 27, 28 | TexFile | 4, 2 | MTEX 0..3, 0..1 |
| 29 | Flash | | flash 4 |
| 30 | Textos, itens | 351 | `TextBox_AttachLine`, id 0..349 e vazio 350 |
| 31 | TexFile | 4 | MTEX 0..3 |
| 32 | Textos, falas | 30 | `MsgWin_Draw`, guia * 15 + fala |
| 33 | Flash | | flash 5 |

## Customização: `menu_evoz`, 452 s3, N = 39

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | sem leitor | | bytes preservados, sem forma conhecida |
| 2 | ItemHelp | | `ItemHelp_Init` |
| 3..12 | TexFile | 5, 4, 4, 2, 6, 3, 3, 5, 3, 1 | `EZ_RES`, MTEX máximo + 1 |
| 13 | Flash | | flash 0 |
| 14..22 | TexFile | 3, 1, 1, 3, 2, 5, 1, 1, 2 | `EZ_RES` |
| 23 | Flash | | flash 1 |
| 24..30 | TexFile | 2, 5, 2, 1, 1, 1, 1 | `EZ_RES` |
| 31 | Flash | | flash 2 |
| 32 | Diálogo | | `Dialog_Init`, textos da 39 |
| 33 | Células | | `ChrGrid_Build`, cabeçalho s32 e registros de 9 s32 |
| 34, 35 | Textos, nomes | 161 cada | `TextBox_AttachLine`, personagem 0..160 |
| 36 | SubpacoteTex, fichas | 165 subseções | laço de relocação; `EvoZ_SetRowChips` |
| 37 | Textos, itens | 351 | id do item e vazio 350 |
| 38 | Janela de senha | | `PassWin_Init` |
| 39 | Textos, diálogo | 2 | `EvoZ_UpdateDialog`, mensagens 0 e 1 |

A janela de senha é um pacote de N = 3: seção 1 Flash; seção 2 TexFile de pelo
menos 3 registros (MTEX 0, 1, 2); seção 3 TexFile de pelo menos 1 (MTEX 0).
`PassWin_Init` reloca as duas listas. `PassWin_Draw` só muda o filme em memória,
e o texto da senha vem de `ChrPass_GetText`, não do pacote.

O retrato de `EvoZ_Init` e `EvoZ_UpdateFace` usa `menu_texfile`, registro 0,
id `0x2F9 + personagem` (índices 760..920; faixa já convertida 760..923), buffers
0x16800 para arquivo e 0x20800 para conteúdo. Cada nova carga reescreve o buffer.
O fundo de `EvoZ_Load` usa id 0x3C0, índice 959, BPE inteiro, `menu_fundo` (8
registros) da faixa existente. A chamada de `Sprite_Unpack` recebe o gancho
`menu_fundo`, pois o desenho usa `Sprite_DrawPicture`.

Depois da conversão, o jogo reloca os TexFile e as 165 fichas. Tabelas de texto,
grade e descrições são lidas; customização, pontos e compras são gravados no save,
fora dos pacotes. Os conteúdos são conferidos também por `Conv_FormatoBits`.

As duas seções sem leitor (1 do EvoZ e 23 da loja) têm 14.016 bytes idênticos
no disco. Não têm a forma das peças conhecidas. `Conv_RestoBytes` as preserva,
como a seção 7 das Opções (`menus-telas.md` 5.3). Isso só vale enquanto nenhum
leitor existir; não se infere uma largura a partir da aparência de números.

## Conferência do disco e recusas (15/10/2026)

O `conv_tool --disco gamedata` terminou com 3.300 arquivos e 687 conteúdos
convertidos, zero erros. Os dois caminhos (marcas por byte e por bit, usado em
jogo) dão os mesmos bytes, resultado e cobertura. O maior conteúdo continua
0x81C580, dentro de `CONV_JOGO_MAX` = 0x900000.

| Conteúdo | Tamanho | Bytes em números trocados | Cobertura |
|---|---:|---:|---:|
| 452 s1, `menu_evo_topo` | 1.277.312 | 52.176 | 100% |
| 452 s2, `menu_loja` | 2.815.936 | 172.570 | 100% |
| 452 s3, `menu_evoz` | 4.050.368 | 273.996 | 100% |
| 452, `menu_pacote` | 3.466.880 | 88 | 100% |

Cada conversor foi executado sobre cópias intactas (saída 0) e estragadas
(saída 1 com a seção na mensagem). Os dados ficam em `~/9c-evo/estragos`,
o procedimento e as saídas em `~/9c-evo/prova_conversores.py` e
`~/9c-evo/provas-conversores.log`, fora do git.

| Formato | Estrago | Recusa |
|---|---|---|
| topo | N = 10 | fim da tabela diferente do arquivo |
| topo | seção 4, 7 texturas | leitor usa 8 |
| topo | seção 7, 17 linhas | leitor usa 18 |
| topo | seção 10, 34 entradas | leitor toca 18 falas |
| topo | diálogo da seção 11, N = 5 | fim da tabela diferente |
| loja | seção 31, 3 texturas | leitor usa 4 |
| loja | seção 30, 350 linhas | leitor usa 351 |
| loja | N = 32 | fim da tabela diferente |
| EvoZ | seção 3, 4 texturas | leitor usa 5 |
| EvoZ | N = 38 | fim da tabela diferente |
| EvoZ | janela de senha, N = 2 | fim da tabela diferente |
| EvoZ | grade, 9.999 células | grade cabe em 165 |
| EvoZ | seção 39, 1 linha | leitor usa 2 |

Contra o disco anterior, 3.983 linhas de arquivo/conteúdo ficaram idênticas.
Só entraram o 452 e suas três seções: 3.299 para 3.300 arquivos e 684 para 687
conteúdos. No censo dos menus (448..477), 19 para 20 arquivos convertidos e
31 para 34 conteúdos BPE com conversor, dos 49. Retratos e estágios continuam
inteiros cobertos.

O mapa já guardado do roteiro `evolution` foi julgado com a ferramenta nova:
saída 0, sem largura divergente nos três pacotes. Isso é conferência sobre um
rastreio anterior, não uma rodada final do roteiro ampliado.

## Memória do topo: o dono das timelines

`EvoTop_Run` declara `EvoTop m` na pilha e passa `&m.flash[0]` a `Flash_Create`.
`FlashTl_Init` grava esse endereço diretamente em `tl->owner` (`gfxm_c.c`),
tanto na raiz quanto nas timelines dos clips. Não é um número vindo do disco.
Os layouts de `gfxm_c.h` explicam exatamente os 19 trechos da tela 48:

| Alocação | Tipo e campo | Deslocamento nos dados | Trechos |
|---|---|---|---:|
| 0x18 | `FlashTl.owner` | 0 | 1 |
| 0x1008 = 18 * 0xE4 | `FlashClip[i].tl.owner` | 0xCC + i * 0xE4, i de 0 a 17 | 18 |

As pilhas nativas têm endereços diferentes nas duas arquiteturas e ficam fora
da faixa de imagem/heap que o memdiff reconhece como ponteiro do port.
`roteiros.py memoria` explica somente os 4 bytes desses campos na tela 48,
com tamanho pedido, tipo e cadeia de alocação até `EvoTop_Init` conferidos.
`campos.py --donos` preserva os três chamadores no TSV, pois outras raízes de
0x18 bytes da mesma tela também vêm de `Flash_Create`. Sem esse contexto,
a diferença permanece falha. Nenhum conversor, gancho ou teto muda por isso.

Na cópia dos dumps do vblank 3.242, o verificador de `4aa0099` reproduziu as
19 falhas informadas pela revisão; o novo passou a mesma entrada intacta.
Os testes em `~/9c-evo-provas/memoria.py` refazem o memdiff sobre cada cópia
binária estragada antes de julgá-la. Recusas observadas:

| Cópia estragada | Resultado |
|---|---|
| Cadeia sem `EvoTop_Init`, outro tipo, outro alocador ou outro tamanho pedido | os owners continuam não explicados |
| `FlashTl.prev` da raiz ou `FlashClip[0].tl.frame` alterados | número diferente não explicado |
| `owner` da raiz do IconWin alterado, também 0x18 e `Flash_Create` na tela 48 | ponteiro diferente não explicado |
| `owner` e `slots` juntos, trecho de 8 bytes | trecho inteiro recusado |
| u32 da contagem do pacote 452 s1 vivo sem troca de bytes | marcas com um número fora do formato e memória recusada |

`limites.py` também recusa 14 variantes fora do escopo, incluindo outra tela,
o fim, falta do contexto, campo vizinho e índice após o último clip.
`conversores.py` repetiu as treze recusas da tabela anterior, com as três
entradas intactas passando. Dados, scripts e logs ficam em `~/9c-evo-provas/`.

## Roteiro ampliado: evidências por emulação

No código `f127d03`, x86 em `qemu-i386` e PPC sem mapa em `qemu-ppc` chegam ao
fim no vblank 6.403, quadro 6.397, com 69 linhas de rastro idênticas. Nas duas,
o dinheiro passa a 4.849.900 e a experiência da célula 7 a 100. O DcSave percorre
12, 13 e 15, termina em 0, e os arquivos persistidos têm esses mesmos valores.
`roteiros.py save` confere os dois cartões campo a campo: iguais. A coleção e o
ItemHelp da loja são visitados; o desbloqueio padrão já deu todos os itens, por
isso não se afirma provar compra.

As 17 imagens geradas por `roteiros.py imagens`, com janela de 8 quadros, são
idênticas byte a byte, com os quadros e vblanks conferidos. Incluem as telas
48/49/50, o retrato atualizado, a senha, os pontos e a pergunta do cartão. Os
rastros das rodadas com imagens também têm as mesmas 69 linhas. Uma cópia da
imagem da senha com um canal de um pixel alterado é recusada (saída 1).

A x86r emulada também chegou ao fim com o mesmo rastro: 5.910.419 posições em
97 arquivos. O mapa novo, em três partes, passa no `conv_tool --mapa` com saída
0: topo 10.357 posições iguais; loja 14.061; EvoZ 14.545 iguais e uma cadeia de
texto lida por inteiro (14.546 posições). Nenhuma divergência de largura.

São evidências adicionais, separadas da certificação integral pedida pela tarefa:
a execução nativa da x86 termina com SIGSYS no ambiente restrito, `ptrace` é
recusado e o depurador do qemu não abre socket. O fluxo padrão `roteiros.sh`
não fica aprovado aqui, e a memória completa por tela não foi provada. Não se
ampliaram tolerâncias para fazê-la passar. A tarefa permanece com a certificação
pendente de um ambiente que permita essa coleta.
