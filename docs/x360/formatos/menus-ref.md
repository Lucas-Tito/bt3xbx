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
