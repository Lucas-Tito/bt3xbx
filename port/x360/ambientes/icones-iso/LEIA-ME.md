# Aberturas dos ícones e das caixas, ainda não executadas

Compilar no Linux o commit entregue de `codex/icones-iso`, pelo `build_f2.sh`.
Subir juntos `default.xex`, `default.dat` e `default.swap`, e a ISO nova da rodada
final indicada em `~/entrega-icones-iso.md`, como **`game:\bt3-x360.iso`**.
Manter a convertida versão 1 já existente no console para a abertura 05, com o
nome `bt3_convertida.iso`. Esse nome antigo só é usado nesse controle negativo.
Não substituir a nova por uma ISO que tenha apenas a marca adulterada.

Cada ambiente substitui `game:\ambiente.txt`, um por abertura. Saves e capturas
usam seis nomes novos e diferentes com prefixo `ic2810k_`. Criar o cartão pelo
New Game, aceitando salvar: são pastas vazias, sem save emprestado nem dados do
jogo no git. O preparo local correspondente fica em `~/icones/console/`.
Se qualquer pasta já existir no console, usar um sufixo novo em **ambos** os campos.
Não mesclar saves nem capturas. A cópia explícita de `BT3_ISO` é só deste preparo
do projeto; nenhum texto da interface manda editar essa variável.

Captura automática a cada 17 quadros do GS nas primeiras quatro aberturas, para
mostrar também estados diferentes da animação de seis quadros. A captura custa
tempo, então estas aberturas não medem desempenho. Fechar após obter a prova,
baixar as PPM e os logs para a pasta local de mesmo nome, antes de abrir outra.
As caixas são do sistema e não entram na captura do alvo do GS: conferir seu texto
na TV e registrar o resultado no relatório da revisão. Nenhuma captura entra no git.

| Abertura | Ação manual e prova pedida |
|---|---|
| 01 HUD | New Game, Duelo 1P contra COM, Goku (Early) contra Kuririn, trajes 0, mirante do Kami. Provocar esgotamento de ki durante a luta e aguardar o prompt de recuperação (`BtlCtrl_GetChangePrompt`, flag 0xE7). Capturar o botão de ação nos dois estados e conferir o mapeamento físico B/X/Y/A. O prompt precisa estar visível, não basta capturar o HUD comum. |
| 02 pausa | New Game e a mesma luta. START, deixar a pausa aberta com suas dicas. Capturar e conferir as letras novas; A confirma e Y volta. |
| 03 lição | New Game, Treino, aulas, primeira classe e primeira aula. Avançar até uma página com marcas de botão; deixar aberta e capturar. Navegar também por páginas de carga/segurar para conferir o ▼ amarelo. O ícone SELECT e o texto "SELECT button" ficam originais. |
| 04 golpes | New Game e a mesma luta, START, lista de golpes. Percorrer até aparecerem botões de ação, ombros, gatilhos, cliques e variantes com ▼. Capturar páginas suficientes; conferir os rótulos sem cortar e espaçamento dentro da linha. |
| 05 antiga | Abrir com a convertida versão 1 já existente no console, selecionada explicitamente. Exigir `Unsupported converted ISO version`, reconversão com `convert_iso.py` e destino `bt3-x360.iso`; o jogo não começa e OK volta ao dashboard. |
| 06 ausente | O ambiente aponta para um nome novo que deve estar ausente, mesmo com a ISO nova instalada. Exigir `ISO not found`, raiz do jogo, `bt3-x360.iso` e `default.xex`, sem variável nem arquivo de configuração no texto. Confirmar também uma abertura **sem ambiente e sem nenhuma ISO na raiz**, retirando temporariamente as imagens por procedimento do Lucas, para provar a busca padrão ausente. OK volta ao dashboard. |

Comparações contra imagens do WSL devem marcar as áreas dos ícones como diferença
esperada; fora delas, não ampliar tolerâncias. A folha de contato aprovada como
fonte fica em `~/icones/contato_kenney.png`, ainda aguardando o Lucas ver a redução.
A visibilidade real dos prompts, a legibilidade e os tempos de carga só ficam
provados depois destas aberturas. Nenhuma delas rodou no WSL nem no console.
