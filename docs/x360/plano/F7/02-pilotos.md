# F7: pilotos de conteúdo e custo do lote

[Etapa 1](01.md) | [Escopo](00-escopo.md) | [Índice](../../plano.md)

[Medido] Rodada de 29/10/2026 no Deck, fontes `84cd35b4`: Missão 0, marco 29/30 e Episódio 0
aprovados; transformação executada, **reprovada na memória**. O lote terminou com RC=1.
As medidas permitem reestimar a máquina; não aprovam a etapa inteira nem o conteúdo completo.

## Ambiente e reprodução

[Medido] Contêiner `bt3`, worktree `~/bt3xbx-f7p`, fila única sob `~/deck.lock`,
`SEMMAPA=1 IMG=1`. A prova de 28/10 na base `1287ec9d` terminou com `duelo` RC=0 e `.text`
`43444ea4d03a6d448aa1ae74a631e35b356a7caa`, igual à do WSL. O jogo não mudou nesta tarefa:
324 dos 326 objetos x86, comparados por `objcopy --strip-debug` e `cmp`, são iguais à base;
só `x360_roteiro.o` e `x360_passos.o`, suporte de teste sem janela, diferem.
A `.text` final mede `cddde5217225d506aa9a2ed699b99d8ac47ddb6b`.

[Medido] O Deck foi desligado em 28/10 e voltou em 29/10. A espera própria do WSL foi
encerrada; as medidas finais abaixo são **do Deck**, sem incluir fila, preparação, ensaio
ou rodadas abandonadas. Artefatos em `~/f7-pilotos-final-84cd35b4`, no Deck e copiados
ao WSL; executor `~/f7-deck-final.sh`, log `~/f7-deck-final.log`. O log termina `RC=1`,
`FIM`: não houve aceite global às 10h57. A última rede foi carimbada às 10h56min22s.

```sh
F7_MAQUINA=Deck F7_BASE_OBJ=$HOME/bt3xbx/port/build/x86/obj \
  F7_SAIDA=$HOME/f7-pilotos-NOVA bash port/x360/tools/f7_pilotos.sh lote
```

[Lido] No Deck, executar dentro de `distrobox enter bt3`, conforme guia 1.2b. O executor
toma a trava, exige a prova inicial, verifica espaço e preserva um caso por vez.
`f7_pilotos.py` complementa `roteiros.sh` com comportamento, cartões e negativos;
seu `tudo certo` isolado não aprova imagem, memória ou equivalência.

## Receitas e resultados

[Medido] `missao.txt` e `historia_novo.txt` ficaram intactos. `f7_missao_fluxo.txt`
conserva a sequência da Missão 0 e os pilotos acrescentam salvar por Opções.
O marco 29/30 nasce de New Game, preparando só os bytes cleared das missões 1..29;
a 0 permanece inédita. História usa New Game e exige `gsc acao 10001/10002`,
resultado, recompensa e HistSave. `termina luta` dá crédito ao resultado/progresso;
não certifica combate por controle nem execução de todas as regras de missão.

| Caso | `roteiros.sh` | Hashes de luta x86/PPC iguais | PPM iguais | Salvar/reabrir nas duas builds |
|---|---|---:|---:|---|
| Missão 0 | RC=0, tudo certo | 1.566/1.566 | 17/17 | cleared 0 passa a 1; uma concluída |
| Marco 29/30 | RC=0, tudo certo | 1.604/1.604 | 17/17 | 30 concluídas, Sobrevivência aberta |
| Episódio 0 com eventos | RC=0, tudo certo | 6.520/6.520 | 14/14 | saga 0: flags 73, abertos 3, ganhos 1, dinheiro 1.000 |
| Transformação 1 para 2 | RC=1, quatro falhas de memória | 1.728/1.728 | 14/14 | não se aplica à troca durante a luta |

[Medido] Cartões da x86 e PPC equivalentes campo a campo, após Continue pelo McFlow normal,
sem a queda anterior. A x86 reabre com os dados originais; só a PPC recebe a árvore convertida.
Missão 0 simples também termina com `flags +0x208=1`, pois seu roteiro original usa desbloqueio;
**só o marco nativo 29/30 prova a abertura inédita**, com bit antes 0 e depois 1.

[Medido] Transformação: id 1, traje/variante 0, adversário 25, arena 5. R3 sem recurso
mantém id 1 (Blast 0 antes, 8.820 depois); após acumular naturalmente, pede id 2,
classe 2, custo 100.000. O jogo debita 116.130 para 16.130; id 2 aparece no vblank 6804
e DIREITA volta à ação 0x0D no 6926. Todas as sete execuções observadas pelo verificador
complementar cumprem negativo, pedido, débito, forma e retorno, sem chamar diretamente
a função de transformação. A amostra cobre uma relação entre 101, sem fusão.

[Medido] A reprovação tem quatro registros: no fim, `memdiff` sai 2 por visões conflitantes
de `EftDisc_Init` (`EftDisc`/`EftODisc`); em `ponto_antes`, `ponto_transformacao` e
`ponto_depois`, o julgamento recusa diferenças não explicadas no heap de tamanho 0x1A0800,
incluindo a região de dados +0x21110..0x2112B. Após a troca, recusa também classes do buffer
de tamanho 0xCE800 (15.514 palavras igual-sem-inverter, 32 de 2xu16 e 26 de u64).
Seleção, carga e `ponto_carga` passam. Nenhuma classe foi dispensada ou limite ampliado.

[Lido] São hipóteses para investigação: reutilização do trabalho de efeitos no contexto vivo,
conteúdo anterior no buffer do modelo e tipagem/valores da região de luta. Hashes e PPM iguais
não explicam essas regiões. Não se conclui erro de byte order nem diferença inofensiva.
O caso reprovado permanece isolado no Deck para diagnóstico; não se inicia outro lote sobre ele.
O diagnóstico está na seção seguinte.

## Diagnóstico da memória da transformação

[Medido] Análise de 29/10/2026 à noite, só sobre os dumps preservados no Deck
(`~/f7-pilotos-final-84cd35b4/atual/f7_transformacao`), sem emulador. **Nenhuma das quatro
reprovações vem de dado não convertido ou mal convertido.** Os 122 arquivos e conteúdos lidos
saem do `conv_tool` com 100% de cobertura e 0 posições não trocadas contra o mapa, e no fim os
19 blocos vivos com arquivo convertido conferem 9.477.248 números e bytes pelas marcas, nenhum
fora do formato. Não é a classe do Broly (`00446`/`00447` sem conversor, [queda](queda-relocate.md)):
não há formato a acrescentar nem censo de arquivos a fazer. São uma falha da ferramenta e três
diferenças explicáveis da execução. Scripts e saídas da análise: `~/f7tr-diagnostico` no Deck.

### 1. Fim: duas vistas do trabalho do disco (ferramenta)

[Lido] O trabalho de 0x4C0 bytes de uma tarefa de disco tem duas declarações: `EftODisc`
(`include/battle/eft_o.h`), usada por `EftDisc_Init` e `EftDisc_Update` em `eft_o_c.c`, e
`EftDisc` (`include/battle/eft_p.h`), usada pelos outros seis callbacks em `eft_p.c`. Um disco
estava vivo no fim, `memoria_luta.py` (linha 373) recusou os dois tipos e o `memdiff` saiu 2
antes de comparar: **a memória do fim não chegou a ser julgada**.

[Medido] As duas marcações nunca discordam: onde ambas tipam um byte, a largura é a mesma.
`EftODisc` tipa 8 bytes que `EftDisc` deixa opacos (`unk3AC` e o ponteiro `tex` em +0x3F8);
`EftDisc` tipa 98 que `EftODisc` deixa opacos.

Correção: em `memoria_luta.py`, tratar `EftDisc_Init` como já se trata `EftBlade_Init`, aceitando
só `{EftDisc, EftODisc}` e comparando pela união das marcações (o byte tipado vence o opaco; larguras
diferentes no mesmo byte continuam recusadas). Com a troca mais simples (escolher `EftDisc`) numa
cópia das ferramentas, o `memdiff` do fim sai 0 e o julgamento mostra só os itens 2, 3 e 4.

### 2. Bloco 0x1A0800: arredondamento da libm nos quads do brilho

[Medido] O bloco é a arena do `BtlPool`. Os 13 trechos de `ponto_antes` (e os 3 que restam em
`ponto_transformacao`, `ponto_depois` e no fim) caem no vetor `struct EftPGlowPart` de
`EftGlowMgr_Init` (140 x 0xC0), só em `dir` (+0x90) e `worldDir` (+0xA0) das partes 16, 21,
27, 32, 36 e 40: 34 palavras `float-proximo`, de 0 a 56 ULP. Posição, ponto, cor e o resto das
partes são iguais.

[Medido] O perfil `gEftGlow->dir[20]` já difere pela libm nos y/z das amostras 8, 11, 12, 18 e
19: são as 8 palavras `libm-host` aceitas desde `ponto_carga` (`memoria_luta.py`, linha 156).
`EftGlow_GetShapeDir` copia `dir[shape]` e `EftGlow_StepParts` (`src/battle/eft_p_b.c`,
linhas 856 a 867) gira por `Mtx_RotateZ` (sinf/cosf), leva ao mundo e normaliza. **As seis partes
diferentes têm `shape` 8; as outras 64 (formas 2 a 7 e 14) são iguais byte a byte.** Cinco estão
na lista livre (vida 0 ou menos) e a sexta, na ativa com vida -0,251, sai no passo seguinte.
É a diferença da libm já conhecida, propagada pelo jogo.

Regra: em `memoria_luta.py`, no vetor `EftPGlowPart`, marcar `dir.xyz` (+0x90..+0x9B) e
`worldDir.xyz` (+0xA0..+0xAB) como 0x54 (libm-host, 64 ULP), em todas as partes, porque as duas
entradas vêm da libm. A margem é curta (56 ULP num componente de 0,0086). Se estourar, usar
tolerância absoluta de vetor unitário em vez de ampliar a de ULP.

### 3. Bloco 0xCE800: resto do arquivo anterior no buffer fixo

[Lido] O bloco é `slot->file[2].buf` de um slot fixo de lutador (`BTL_RES_ANIM1_SIZE`,
`src/battle/btl_obj.c:1198`, zerado no `BtlObj_Init`). Antes da troca guarda
`pzs3us1/01442.bin` (741.568 bytes); a transformação recarrega no mesmo buffer
(`BtlRes_SetFileId`, linha 1133) `pzs3us1/01452.bin` (638.720 bytes), e a leitura só grava o arquivo.

[Medido] Em `ponto_depois` e no fim:
- `[0, 0x9BF00)` confere com as marcas do 01452 (541.655 números e bytes);
- `[0x9BF00, 0xB50C0)` (25.712 palavras) é o resto do 01442. Na x86, 25.658 palavras são
  iguais ao original; as outras 54 são campos que o jogo gravou com ele vivo. Pela marcação do
  01442, os 78.641 números e bytes da PPC são todos conformes, nenhum fora do formato;
- `[0xB50C0, 0xCE800)` é zero nas duas.

**As classes suspeitas do bloco estão todas no resto: exatamente 15.514 igual-sem-inverter, 32 2xu16
e 26 u64.** É o mesmo padrão do `stageFile` 0x3B800, já em `CLASSES_CONHECIDAS`, e prova de quebra
que o 01442 estava bem convertido.

Regra: no `memdiff`, um bloco com marcas vivas cujo conteúdo é menor que o de um arquivo lido antes
na rodada confere a cauda `[tamanho atual, tamanho anterior)` pelas marcas do anterior, que se acha
pelo conteúdo da x86 como no `--marcas-vivas` (o `.orig`, metade dos trechos de 64 bytes), em
camadas se houver mais de um. O que sobrar depois do maior tem de ser igual nas duas. Vale para os
três buffers do slot fixo (modelo 0xCE000, anim0 0x160800, anim1 0xCE800): voltará em toda troca
para um arquivo menor, inclusive nas 101 relações. A alternativa fraca é uma linha de
`CLASSES_CONHECIDAS` com teto pelo menor arquivo e só depois de uma troca de forma. Essa
alternativa não prova o conteúdo do resto.

### 4. Fim: cauda não iniciada do argumento do disparo

[Medido] Só aparece com o item 1 corrigido: 16 bytes em `heap#176+0x2FD70`, que é o trabalho da
tarefa de disco 0 (lista de 0x5F00 = 20 x 0x4C0), em +0xD0..+0xDF: `EftDisc.atk` +0x40..+0x4F.
`BtlFx_FireKiBlast` (`src/battle/btl_char_fx.c:1328`) preenche um `FxHitArg2` local até +0x3C e
`unk41`, sem gravar `unk40` nem `unk42[0xE]`. `EftDisc_Throw` e `EftDisc_SpawnFromNode` copiam
os 0x50 bytes por valor (`eft_p.c`, linhas 309 e 697). Na x86 ali estão ponteiros do host
(0x20090112, 0x019966A4); na PPC, outros valores (0x20045E24, 1.0f). O byte +0x41, gravado,
vale 0x01 nas duas. É lixo da pilha, outro em cada build e também no PS2.

[Lido] Nenhum leitor: os registros de golpe veem `&w->atk` como `EftHitAtk` (0x34 bytes), e a
única outra leitura do `atk` é `objId` (+0x12).

Regra: em `memoria_luta.py`, `sem_leitor` para `EftDisc_Init` nos bytes +0xD0 e +0xD2..+0xDF.
O código do jogo fica como está.

### Para fechar

Implementar o item 1 e as três regras, e ver cada uma falhando de propósito numa cópia: um byte do
resto do 01442 trocado, um z de quad do brilho a mais de 64 ULP, um byte tipado do `atk` (+0x3C)
diferente, e larguras conflitantes entre `EftDisc` e `EftODisc`. Depois, rejulgar os dumps preservados
no Deck sem emulador (`memdiff` e `roteiros.py memoria` nos quatro pontos e no fim). Isso é prova
antecipada; o aceite continua a ser o `roteiros.sh` do piloto com RC=0.

### Novo julgamento com as quatro regras (29/10/2026, noite)

[Medido] As regras acima estão no `memoria_luta.py` (1 a 3) e no `memdiff.py` (4), com o `roteiros.py` contando as
linhas da cauda à parte. Rejulgamento só dos dumps preservados no Deck, sem emulador, com os executáveis de
`~/bt3xbx-f7p` (`c1e8ff7`, que só difere de `84cd35b4` em documentação): as ferramentas antigas com eles reproduzem
byte a byte o `memdiff.txt` guardado do fim, do `ponto_carga` e do `ponto_antes`. Executor `~/f7vl/py/tudo.sh`, saídas em
`~/f7vl/{base,novo,neg}` no Deck.

| Ponto | `memdiff` | Julgamento | `libm-host` | Observação |
|---|---|---|---:|---|
| fim (`semmapa`) | sai 0 | RC=0 | 13 | rastro igual, 1.728/1.728 quadros de luta; disco pela união; cauda do 01442 conforme |
| tela 38 | sai 0 | RC=0 | 0 | como antes |
| tela 39 | sai 0 | RC=0 | 0 | como antes |
| `ponto_carga` | sai 0 | RC=0 | 8 | como antes (o perfil) |
| `ponto_antes` | sai 0 | RC=0 | 42 | 8 do perfil e as 34 das partes |
| `ponto_transformacao` | sai 0 | RC=0 | 13 | |
| `ponto_depois` | sai 0 | RC=0 | 13 | cauda do 01442 conforme |

[Medido] No `anim1` (`heap:0xce800`) do fim e do `ponto_depois`: `[0, 0x9BF00)` é o 01452 (conforme, como antes);
`cauda pzs3us1_01442.bin.marcas [0x9bf00, 0xb50c0): conforme 78641, fora-do-formato 0`; `resto [0xb50c0, 0xce800):
conforme 26064, fora-do-formato 0`. As 15.514 palavras igual-sem-inverter, 32 2xu16 e 26 u64 sumiram da classe por
palavra, porque agora são números conferidos pela marcação do arquivo que os gravou.

[Medido] Os negativos, cada um numa cópia (dump da PPC ou base do `--donos`), todos recusados com RC=1 e a falha
nomeada:

| Regra | Estrago | Recusa |
|---|---|---|
| 1 | `EftODisc` +0x40: o campo de 4 bytes que as duas vistas tipam igual vira dois u16 | `memdiff` sai 2: "vistas com larguras ou tipos conflitantes em +0x40" |
| 2 | `dir.z` da parte 0 do brilho (normal e igual nas duas) somado de 100 ULP, `ponto_antes` | `heap#176+0x1f338 (heap:0x1a0800, dados+0x1f318, +0x4 bytes) diferente e não explicado` |
| 3 | `EftDisc.atk` +0x3C (trabalho +0xCC), um número tipado do argumento | `heap#176+0x2fd6c (+0x4 bytes) diferente e não explicado` |
| 3 | o byte gravado +0x41 (trabalho +0xD1), que fica comparado | `heap#176+0x2fd71 (+0x1 bytes) diferente e não explicado` |
| 4 | um byte da cauda do 01442 (dados +0x9C001) | `marcas heap#36 cauda ...: fora-do-formato 1` e o trecho |
| 4 | um byte depois do 01442 (dados +0xC0000) | `marcas heap#36 resto ...: fora-do-formato 1` e o trecho |
| 4 | as marcas sem o `.orig` do 01442 (a camada não se acha) | as 15.514 igual-sem-inverter voltam, "classe não explicada ali" |

[Medido] O `teste_memoria_luta.py` (um número errado em cada subalocação e trabalho vivo, slot, geração e dono
estragados): no fim, 1.470 estragos recusados e intacto conforme, incluindo o trabalho vivo do `EftDisc_Init` pela
união e o vetor das partes do brilho; no `ponto_antes`, 1.473 recusados e intacto conforme.

[Lido] Isto é prova antecipada sobre os dumps de `84cd35b4`; o aceite da transformação continua a ser o
`roteiros.sh` do piloto com RC=0, que espera o emulador (`entrega-f7-verificador-libm.md`). A regra 2 cobre a
diferença da dobra de constantes da x86 (`docs/x360/pesquisa/06-libm.md`): com a chave `BT3_SEM_DOBRA=1` ligada por
padrão, ela e a do perfil saem.

## Relógio de parede e disco

[Medido] Segundos por rede, calculados entre marcas consecutivas de `*-tempos.tsv`.
`x86` inclui duas execuções e comparações; `memória` inclui novas execuções das duas builds,
gdb, tipagem e comparação por ponto; `imagens` inclui rasterização e comparação dos PPM.
A preparação comum das ferramentas antecede a primeira marca e fica fora das colunas.

| Caso | x86 | gdb | x86r | mapa | conversão | PPC sem mapa | memória | imagens | Total das redes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Missão 0 | 49,10 | 25,85 | 117,20 | 131,43 | 4,28 | 395,72 | 1.053,00 | 1.437,34 | 3.213,91 s |
| Marco 29/30 | 50,20 | 25,95 | 118,75 | 136,49 | 3,83 | 398,79 | 1.056,73 | 1.479,23 | 3.269,98 s |
| Episódio 0 | 91,82 | 46,59 | 203,92 | 73,52 | 7,24 | 721,07 | 2.299,48 | 2.847,84 | 6.291,49 s |
| Transformação | 39,88 | 20,35 | 77,62 | 87,42 | 3,79 | 310,47 | 1.745,65 | 1.771,74 | 4.056,92 s |

[Medido] Soma: **16.832,30 s, 4h40min32s**. Memória mais imagens: **81,34%**.
Com preparação por caso, gravação/reabertura e julgamento complementar: Missão 0,
3.220,63 s; marco, 3.276,13 s; História, 6.297,60 s. Reabrir as duas builds leva,
respectivamente, 0,68/0,66/0,67 s, já dentro desses totais. O fim da transformação
não teve julgamento complementar na mesma janela: não inventar uma medida desse trecho.

| Caso | Bytes antes de compactar | GiB |
|---|---:|---:|
| Missão 0 | 3.659.641.341 | 3,41 |
| Marco 29/30 | 3.654.725.167 | 3,40 |
| Episódio 0 | 2.261.362.865 | 2,11 |
| Transformação reprovada | 2.734.890.565 | 2,55 |

[Medido] Espaço livre antes do lote: 25.250.205.696 bytes; antes da transformação:
25.170.079.744. Os três casos aprovados foram compactados e seus intermediários apagados
antes do seguinte. Há no máximo um roteiro com saídas completas. Não somar os quatro
tamanhos como necessidade simultânea de disco; manter margem para rasterizador e temporários.

## Cobertura e verificadores

[Medido] A falha interrompeu o executor antes da auditoria final e da prova do cartão.
Em 29/10, 11h43 a 11h49, `~/f7-fechar-pilotos.sh` completou esses passos sob `~/deck.lock`,
com os mesmos binários e as saídas já medidas, sem repetir os pilotos. Seu RC=0 é da
auditoria complementar; o RC=1 da transformação e do lote original permanece.

| Conferência | Resultado |
|---|---|
| `f7_cobertura.py conferir`, justificativas versionadas vazias | RC=1 esperado: 27/82 chamadas e 85/3.325 arquivos observados; faltam 55 chamadas e 3.240 arquivos, todos sem dispensa |
| `conv_tool --disco` | 3.325/3.325 resultados presentes, sem falta nem extra |
| `teste_f7.py` | RC=0, tudo certo; controle de laboratório passa mantendo lacunas pendentes |
| `f7_manifesto.py` | RC=0, 5.718 linhas, tudo certo; leitura estática |
| `f7_pilotos.py`, quatro casos | RC=0 no complemento de comportamento/cartão, sem alterar o julgamento de memória |
| `save_mc.sh` | RC=0; 28 casos x86 e 28 PPC com zero falhas; criação A, erro de escrita F, recuperação B1/B2 conferidos |

[Medido] Os negativos recusados em cópias externas foram: sem fim e sem ganho nos dois
pilotos de missão; sem fim e sem evento 10002 na História; sem fim, sem débito e forma
errada na transformação. Para cobertura: retirar uma chamada, um LEU, um resultado de
`--disco` ou uma justificativa causa RC=1 e nomeia a ausência. O manifesto recusa traje
zero; BPE truncado também é recusado, enquanto o controle de cabeçalho big-endian com
conteúdo little-endian passa. As justificativas de laboratório ficam fora do git.

[Medido] `save_mc.sh` criou e carregou pelo fluxo normal: A igual à primeira x86;
B1/B2 iguais à segunda, `.tmp` removido, `.new` recuperado idêntico ao save de A.
F chegou ao erro de escrita do McFlow e depois ao menu sem queda. Os 28 casos por
arquitetura incluem falhas injetadas na camada atômica. A análise conferiu os estados
e arquivos, além do retorno do script.

[Lido] A cobertura inclui observações do piloto reprovado, identificado com saída 1
em `coleta/casos.json`. Esses totais não são cobertura aprovada de ações ou memória.
O JSON e `cobertura.log` preservam todos os nomes ausentes. O ramo permanece reprovado
na auditoria de conteúdo completo, como esperado de quatro casos de amostra.

## Estimativa revisada da F7

[Estimado] Para planejar sem depender de agrupamento ainda não medido, **360 a 500 horas
de máquina serializada no ritmo do Deck**, em lugar das 120 a 240 h iniciais. É uma reserva
de execução e regressões, não medição do conteúdo ausente nem prazo de trabalho humano.

| Parcela de referência | Hipótese explícita | Horas |
|---|---|---:|
| Missões | 100 vezes a rede da Missão 0, 53,57 min | 89,28 |
| História | 48 vezes a rede do Episódio 0, 104,86 min | 83,89 |
| Transformações | 101 vezes a rede medida, 67,62 min, incluindo a reprovada | 113,82 |
| Fusões | 20 vezes o custo da transformação, ainda sem piloto próprio | 22,54 |
| Subtotal de referência | uma passagem completa por caso, sem agrupamento | **309,52** |
| Demais famílias, marcos e regressões | reserva arredondada, ainda sem medida própria | **50 a 190** |

[Estimado] A extrapolação supõe casos semelhantes ao piloto; episódios longos, eventos,
trocas adicionais e correções podem excedê-la. O marco 29/30 é uma regressão separada,
não um multiplicador adicional de todas as missões. Sobrevivência, Disc Fusion, Sim,
torneios, cartões, replays e o primeiro dividido ainda precisam de amostras. A faixa
residual não fecha esses denominadores. Resolver a memória da transformação pode exigir
mais passagens; o teto de 500 h não é garantia diante de novas famílias de defeitos.

[Estimado] A redução deve vir de compartilhar boot/carga, acumular ações e fazer cargas
curtas de modelos, rastreando redes completas nos caminhos novos. **Não multiplicar
698 modelos pela rede de transformação.** O subtotal acima é um cenário de planejamento,
não piso matemático: agrupar casos pode reduzi-lo. A reserva antiga só deve voltar após
medir esse ganho; os pilotos não demonstraram aceleração da linha de produção.

[Estimado] Deck e WSL, cada qual com uma fila e velocidade suposta igual: o limite ideal
para casos divisíveis é **180 a 250 h de parede**, 50% menos. Reservando 20% serial para
dependências e integração, `T*(0,2 + 0,8/2)` dá **216 a 300 h**, redução de 40%
(144 a 200 h). A simultaneidade não reduz as horas de máquina somadas nem o trabalho humano.
Se a velocidade do WSL for `r` vezes a do Deck, a parcela paralela usa `T/(1+r)`.
**Essa razão não foi medida**; os números de duas máquinas são cenários, sem filas,
desligamentos ou uso concorrente por outras tarefas. Não há aceleração real de duas
máquinas provada nesta rodada. A previsão humana de 132 a 234 h do escopo não foi recalibrada
por relógio de máquina. Reestimar após agrupamento e o primeiro Duelo dividido.

[Lido] Permanecem sem aceite: a memória da transformação, demais relações/formas e conteúdo,
combate real das missões abreviadas, som, tela dividida, GPU e desempenho do Xbox 360.
A opção conservadora preserva a progressão original, não cria serialização do Sim Dragon
e mantém cada lacuna nominal aberta. Nenhum console foi acessado.
