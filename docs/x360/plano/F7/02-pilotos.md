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
