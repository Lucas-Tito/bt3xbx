# Preparação da 1.1.0, sem publicação

Branch `release-1.1.0`, base `459f9659`. Nenhuma tag, acesso ao console, build XDK,
publicação ou envio a main nesta tarefa. Relatório de execução fora do Git em
`~/entrega-release-1.1.0.md` para não invalidar os carimbos da conferência final.

## Integração

- Nitidez: merge de `troca/blur-analise` em `1e959c63`, preservando o fechamento da decisão 42.
- Mipmap: cherry-picks de `3c2dd577` e `6038dd30` do pacote, preservando os autores.
  O VMX128 faz a média por bytes com arredondamento para cima; alfa da folhagem a 50%.
- VU0: merge de `troca/vu0-amostra` em `07bdf95b`. Nativa ligada por padrão;
  `BT3_VU0_NATIVO=0` desliga. Amostrador desligado. Mantêm-se guardas e recuos.
- De `fd39f36c`: gancho em gs_on, saída 7, descarga do diário e ensaio com API simulada.
  A saída direta por sFalhaVideo foi substituída pelo aviso público após a desmontagem.
  Ficaram fora as notas de F9/02 e o trecho do guia que afirmava não haver caixa,
  além de todo o código Tag Team. Nenhum campo wall foi trazido para GsDraw.
- Decisões 40, 41 e 42 conservam seus números. As implementações de BT4 e do novo
  conversor da decisão 41 não entram por copiar seus registros históricos.

## Decisão 43, item 2: proposta aplicada

`BT3_TESTE_FALHAS=0` na compilação é o padrão público. Só a compilação com valor 1
permite cores por fase. BT3_LOG não muda essa política. A main espera os produtores
antes de retomar o D3D e apresenta enquanto a caixa está aberta. Fechar o aviso leva
à desmontagem restante e ao dashboard. Nunca se espera botão sobre fundo preto no
caminho fatal público. Sem relatório de exceção gravado, o aviso alternativo não
promete crash.txt. A caixa Unexpected error existente continua quando o relatório existe.

Inventário dos caminhos que chegavam ao laço de cor (fases capturadas antes da saída):

| Fase / origem | Caminho anterior | Texto público proposto |
|---|---|---|
| Preparação: reserva de memória, leitura de SLUS/DBZP e tabelas, mapa de trocas | plat_mem.c, exit(2), antes de Jogo | The game could not prepare its memory or load its program data. |
| Inicialização e execução: arquivo/cache, guarda e conversores, conteúdo incompatível | exit(2) ou Port_Abortar, inclusive conv_jogo.c | Código 2: The game could not load required data or complete the requested script. Código 3: The game stopped after an internal consistency check failed. |
| Roteiro: arquivo ausente, sintaxe, espera ou condição recusada | roteiro.c, exit(2) | The game could not load required data or complete the requested script. |
| Inicialização do renderer: layout, shaders, declarações e recursos recusados | GsGpu_Init=0, antes seguia sem vídeo; Port_Sair(7) após o gancho | The graphics renderer could not be initialized. |
| Verificação interna: abort, pilha de cortes e invariantes | Port_Abortar, código 3 | The game stopped after an internal consistency check failed. |
| Retorno não zero do jogo e demais códigos fatais | Port_Sair(código) | The game stopped because of a fatal error. |
| Exceções: jogo, gravação, desenho ou áudio | AoMorrer, vermelho; relatório pela auxiliar | Unexpected error e texto existente com crash.txt quando a gravação termina; sem relatório, The game stopped because of a fatal error. |
| Saída de teste bem-sucedida: fim do roteiro, medidas, __real_main=0 | Port_Sair(0), verde | Sem caixa: dashboard na build pública. Verde só na de teste. |

A caixa nova se chama **Unable to continue**, com botão **OK**. Todos os textos da
tabela recebem `Phase: <fase copiada>` e `Press OK to return to the dashboard.`
A fase é contexto técnico útil para identificar onde os dados falharam; o texto
não afirma uma causa de disco ou memória que o código de saída não distingue.
A recusa de ISO já tinha sua caixa própria e conserva os textos de iso_avisos.h.

Exceções ao aviso possíveis por limite da plataforma: Direct3DCreate9 nulo ou
CreateDevice recusado não deixam dispositivo para Present e saem diretamente ao
dashboard; produtores que não pararam não permitem tomar o D3D com segurança.
Recusa da UI por 10 s ou falha da API também termina no dashboard. São recuos sem
caixa, não uma aprovação fictícia de que toda falha consegue desenhar um aviso.

## Conferência reduzida por risco

Estimativa antes de armar: 4 a 8 h de máquina, mais espera por vaga. Sem rodada grande
abaixo de 40 GB livres. Serviço próprio numa vaga de `~/vez.sh`, sem recusar outros
qemu. Marcador de espera e relatório seguem a regra das esperas longas do AGENTS.md.

| Roteiro / prova | Risco coberto |
|---|---|
| f5_duelo, SEMMAPA=1 IMG=1 SOM=1 | imagem da luta, efeitos, ações, memória, resultado, retorno e áudio |
| f7_dividido, BT3_DOIS=1, SEMMAPA=1 IMG=1 SOM=1 | duas vistas e portas, técnica cheia, pausa e retorno |
| historia, SEMMAPA=1 IMG=1 SOM=1 | menus de História e conversões próprias |
| opcoes, SEMMAPA=1 IMG=1 SOM=1 | menus, persistência e estado do cartão |
| save_mc.sh e confere_save_mc.py | criação, reabertura, escrita recusada e recuperação atômica |
| Variante PPC com BT3_TESTE_VU0_NATIVO=1 | núcleo nativo, ABI preservada e guardas |
| Variante PPC GPU e C89 | Vtx 28, GsDraw 128, sintaxe e provedores matemáticos |

Ficam fora treino, duelo_time, ultimate, missao, sobrevivencia, simdragon,
discfusion, torneio, evolution, datacenter, referencia, shenlong e os demais
f5/f6/f7: não mudaram navegação, receitas nem conversores. Não se repete censo de
elenco, transformações, sessão longa, filmes ou toda a História. A amostra cobre
os consumidores diretamente afetados e não prova esses conteúdos omitidos.

A imagem D3D, mipmap, custo VMX e ganho da VU0 continuam sustentados pelas sessões
já aprovadas no console e exigem a build XDK do revisor. O rasterizador de software
não reproduz os shaders nem o VMX do 360. Som e estado são comparados com uma
reconstrução isolada do código aprovado da vu0-amostra; imagens do Duelo também
contra a blur-analise aprovada. A igualdade da .text plana com as duas bases é
conferida junto dos objetos sem depuração; uma igualdade de hash sozinha não
substitui os roteiros oficiais. Os verificadores novos exigem cópias sabotadas.

## Rascunho público em inglês, sujeito à auditoria do Lucas

Título: **BT3 Xbox 360 1.1.0**

> Version 1.1.0 improves the picture and performance. Generated mipmaps reduce
> shimmering on scenery, distance blur is reduced, and foliage uses a sharper
> alpha level. Native VU0 arithmetic is enabled in safe ranges, with the original
> calculation used for other inputs. Fatal errors display a system message and
> return to the dashboard after OK whenever the graphics device is available.
> Logs, self tests and profiling remain off during normal play.
>
> Known issues: battles can pause briefly while mipmaps are generated. Split screen
> and planet explosions can still slow down. The existing dark night stages and
> brief black areas during the Namek dragon animation remain under investigation.
> System message boxes may close immediately on consoles running Bad Avatar.
> Not every character, transformation or story battle has been played on this version.

Pendente com Lucas: listras nas sombras dos lutadores com mipmap. Comparar ao jogo
original antes de incluir como problema confirmado. Não introduzir a hipótese
como known issue definitivo no README do pacote.

O README raiz e o do pacote são rascunhos desta versão. O título e a versão vêm
dos arquivos env. A montagem real, depois de conferir XDK/console e aprovar os
textos, usa `pacote.py --estrito` com default.xex/dat/swap da mesma build pública.
Não há pacote distribuível fabricado com arquivos simulados nesta tarefa.
