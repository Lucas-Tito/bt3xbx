# F8: aviso de queda, relatório público e diagnóstico optativo

[Pendências 3, 7 e 13](pendencias.md) | [Índice](../../plano.md)

Implementado sobre `651224b9`, sem XDK, console ou emulador. Compilação e aceite no 360 continuam na revisão.

## Queda e caixa

O filtro copia a primeira exceção para armazenamento estático: fase, thread, quadro, vblank, código,
tipo e endereço do acesso, IAR, LR e os 32 registradores gerais completos. Percorre até 12 elos de r1,
com a leitura protegida por SEH e os limites do `pilha360.h`. Os retornos são candidatos: folhas,
chamadas de cauda e pilhas danificadas podem encurtar a cadeia. Não é um desenrolamento por símbolos.

Uma auxiliar na thread de hardware 1 formata `game:\crash.txt`, grava, confere a quantidade de bytes,
dá flush e fecha. Independe de `BT3_LOG`; o carimbo vem do mesmo `kCarimbo` usado pelo passos, inclusive
commit e diferença. Não usa o relógio do console. Outra queda não sobrescreve o primeiro contexto.
O filtro não faz E/S, nem chama a descarga síncrona do diário, inclusive numa origem no áudio.

A main primeiro pede parada e espera os produtores gráficos com os tetos existentes. Só com os
handles sinalizados recupera o D3D e remove a callback. Aguarda o relatório por até 3 s mantendo a
apresentação quando já tem a posse. Depois abre `XShowMessageBoxUI` com buffers estáticos:

- `Unexpected error`
- `The game ran into an unexpected error and needs to close. A report was saved to crash.txt in the game folder. Press OK to return to the dashboard.`
- `OK`

A main continua apresentando durante a caixa. Depois do OK, a mesma desmontagem encerra cargas e som,
espera a GPU, libera o D3D, fecha o diário e solicita o painel por outra thread. A separação entre parar
os produtores e desmontar recursos é necessária para a caixa ter apresentação segura.

`ERROR_ACCESS_DENIED` tem tentativas por até 10 s com apresentação. Outra falha de abertura ou de
resultado volta ao caminho de tela e botão solto/pressionado. Só mostra a caixa aprovada se a gravação
foi confirmada; falha de arquivo, escrita curta ou flush não produz uma afirmação falsa de relatório
salvo. Threads pendentes impedem retomar o D3D e levam ao painel pelo caminho de emergência. Se a
própria main cair, a auxiliar tenta gravar antes de lançar o painel, sem caixa. Falta de memória para
a auxiliar, disco indisponível ou E/S travada podem impedir o relatório; nenhum software garante
persistência nesses casos. Os tetos continuam tendo prioridade sobre esperar para sempre.

## Chaves e padrões

O [inventário completo](04-chaves.tsv) cobre 173 chaves dos fontes selecionados pela build do 360,
do backend e dos cabeçalhos de diagnóstico, incluindo nomes indiretos de `plat_extras.c`.
A coluna da tabela de ambiente distingue o valor injetado no arranque de um padrão local no consumidor.
O inventário é deliberadamente conservador: inclui ramos de PC presentes nos mesmos fontes, sem
confundir a presença textual de uma chave com a execução desse ramo no console.

| Chave | Antes | Agora sem ambiente |
|---|---|---|
| `BT3_LOG` | Diário e saída abertos incondicionalmente | Só o valor exato `1` abre os dois arquivos |
| `BT3_TEMPOS` | Ligado no backend e nas famílias da gravação | Desligado; `1` liga com log |
| `BT3_ESTADO_TEMPOS` | Resumo por vblank incondicional | Nova chave, desligada; `1` restaura com log |
| `BT3_FILME_MEDIDAS` | Estatísticas por imagem incondicionais | Nova chave, desligada; `1` restaura com log |
| `BT3_SOM_MEDIDAS` | Histogramas de latência e custo incondicionais | Nova chave, desligada; `1` restaura com log |

Picos (`BT3_PICOS`, `BT3_PICOS_TELA`), cortes (`BT3_CORTES`, `BT3_CORTES_GPU`, `BT3_COMUM_GPU`),
CPU (`BT3_CADEIA_CPU`), deriva (`BT3_SOM_DERIVA`), sincronismo (`BT3_FILME_LOG`), medição isolada
(`BT3_MPEG_MEDE`) e capturas (`BT3_SHOT`, `BT3_SHOT_LISTA`, `BT3_FILME_SHOTS`) já eram optativos.
Continuam desligados por padrão. Agora exigem também `BT3_LOG=1`, assim como os demais arquivos de
rastro e medidas listados em `diagnostico360.h`. Não confundir `BT3_FILME`, a reprodução normal,
com suas medidas. `BT3_TEX_ORIGEM` é otimização de cache, não diagnóstico; não foi desligada por essa regra.
A variante de compilação `BT3_PERFIL_CADEIA` continua optativa.

Os 69 ambientes versionados e os três geradores de ambientes do console têm `BT3_LOG=1` e as
três novas chaves de medidas ligadas. Onde `BT3_TEMPOS` era explícito, seu valor foi preservado;
nos demais ambientes foi acrescentado `1` para conservar o comportamento anterior. A capacidade da
tabela passou de 64 para 128 entradas, para essas chaves não descartarem opções no fim de ambientes maiores.
O ambiente é registrado depois de abrir o diário; antes disso ele já precisa ser lido para decidir se abre.

Sem log, diário e saída retornam antes de formatar; os embrulhos de `fprintf`, `fputs`, `fputc`,
`fwrite` e `fflush` aceitam a saída nula e preservam arquivos reais. Não há escritora do diário.
A abertura comum de arquivos também recusa escritas auxiliares sem log, permitindo saves e
configurações explicitamente escolhidas. Isso cobre inclusive despejos de erro do código comum.
Estado e filme não leem seus cronômetros nem alocam estatísticas. Os cronômetros de diagnóstico
de texturas, entrega, VU1, backend, ISO e blocos retornam sem medir. Permanecem os relógios de
vblank, sincronização, prazos e travas necessários ao funcionamento. Há ramos e contadores inteiros
residuais no código comum: custo literalmente zero em ciclos não foi provado no WSL.

## Arquivos públicos

Sem ambiente, o título só tem gravações normais em `game:\saves\card1\BASLUS-21678DBZT3\`,
com os nomes do cartão americano (`BASLUS-21678DBZT3`, `icon.sys` e arquivos de ícone), além de
`crash.txt` na queda. Nomes da árvore já estavam em inglês e não foram trocados; os dados binários
continuam no formato do jogo. `.tmp` e `.new` são estados transitórios da gravação atômica.
As configurações do port ficam desativadas pelo padrão `BT3_SETTINGS` vazio. Capturas, hash,
gravações de controle e rastros exigem chaves de diagnóstico. Não se cria `environment.txt`.

Com diagnóstico, usam-se os nomes de projeto `bt3xbx.log`, `output.txt` e o arquivo de entrada
`environment.txt`, com conteúdo em português permitido. Capturas usam a pasta configurada ou `shots`,
e outros rastros usam os caminhos configurados. A tradução dos nomes foi aprovada pelo Lucas.
As cores de fase só são apresentadas com `BT3_LOG=1`; sem ele, o fundo dos avisos e da saída é preto.

## Provas locais e sabotagens

Na raiz, com `set -euo pipefail` e `tee` para arquivos em `~/elegante-provas/`:

| Comando | Critério |
|---|---|
| `python3 port/x360/tools/teste_queda360.py` | Main real compilada; jogo, desenho, áudio, log, caixa ocupada/negada/falha, resultado com erro e três falhas de disco; 7 sabotagens acusadas |
| `python3 port/x360/tools/teste_logs360.py` | Ambiente e saída reais recortados; estado e filme completos; ausência de arquivos, formatação, relógios e alocações nos caminhos ensaiados; 7 sabotagens acusadas |
| `python3 port/x360/tools/audita_logs360.py --mutantes` | 173 chaves, 69 ambientes, 56 ambientes gerados conferidos; falta de log e chave nova acusadas |
| `port/x360/tools/teste_diario.sh` e `--mutantes` | Diário real, retorno sem formatar antes da abertura; 5 sabotagens acusadas |
| `python3 port/x360/tools/teste_saida360.py --mutantes` | Tetos e sequência da saída; 8 sabotagens acusadas |
| `python3 port/x360/tools/teste_paradas360.py` | Paradas de blocos, fluxos, som e checkpoints; controles negativos acusados |
| `python3 port/x360/tools/teste_iso_avisos.py` | Regressão das recusas e busca de ISO sintética |
| `port/x360/tools/teste_som360.sh` | Regressão de núcleo, Guide e leitura, com controles negativos |
| `python3 port/x360/tools/som_ambiente.py --mutantes` | Inventário do som atualizado e seis sabotagens acusadas |
| `python3 port/x360/tools/c89.py port/build/c89-elegante` | 317 fontes preparados sem falhas, com o shim de saída novo |

A main é compilada inteira contra uma API determinística, sem SEH ou driver real. O ensaio observa
criação, escrita, flush e fechamento antes da caixa; apresentações durante a caixa; OK antes de
cargas/som/GPU e chegada à solicitação de painel. O áudio de teste usa hardware 4 como origem e
exige toda E/S do relatório na auxiliar 1. Não é prova de concorrência, AV real, FATX ou Guide.

Para a x86, gerar primeiro as tabelas com `python3 port/tools/gen_data.py --asm "$PWD/asm"`,
com `asm` e `assets` apontando aos dados somente leitura, e compilar `headless.py --alvo x86`.
`f5_final_confere.py objetos ~/queda-base/x86 port/build/x86` compara os 399 objetos por
`objcopy --strip-debug` e `cmp`: 398 iguais e só `obj/x360_passos.o`, de teste, diferente.
A `.text` permanece com SHA1 `43444ea4d03a6d448aa1ae74a631e35b356a7caa`.
Sem mudanças de save ou conversão, não foi necessário emulador nem `save_mc.sh` nesta tarefa.

## Aceite na revisão e no console

Compilar a revisão final no XDK e levar `.xex`, `.dat`, `.swap` e mapa da mesma build. Em pasta limpa,
sem ambiente, conferir boot, menu, save e saída; listar a árvore antes e depois. Só devem surgir os
saves. Usar uma segunda abertura com ambiente mínimo contendo somente `BT3_QUEDA_TESTE=180` e
`BT3_QUEDA_TESTE_THREAD=jogo`, sem `BT3_LOG`; repetir com `desenho`. Exigir `crash.txt` legível,
carimbo do commit, contexto correto, caixa exata, Guide responsiva, OK, painel respondendo e reabertura.
Repetir três vezes cada origem. O ambiente de queda é mínimo de propósito; os ambientes versionados
continuam sendo os de desenvolvimento com log.

Para desempenho, usar a mesma build Release e o mesmo roteiro, save e trecho, alternando sem log e
com o controle de diagnóstico, três repetições após aquecimento. Fazer uma variante temporária que
acumule apenas o tempo externo dos quadros em RAM e descarregue depois da janela. Comparar essa
variante com o mesmo ramo substituído por retorno vazio, sem ligar picos, tempos finos ou capturas;
comparar mediana, p99, pior e imagens longas. O resultado sem diagnóstico deve ficar no ruído da
medição. Essa sonda não faz parte do release. Conferir também que não há handles dos logs nem
escritora. O WSL prova ausência das operações nos caminhos ensaiados; o custo no Xenon e a ausência
integral de outros arquivos numa sessão real dependem dessa rodada.
