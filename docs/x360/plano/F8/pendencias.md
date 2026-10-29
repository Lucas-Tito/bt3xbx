# F8: itens anotados para o acabamento

[Índice do plano](../../plano.md)

Itens que o Lucas pediu para o acabamento ao longo das outras fases. O [escopo da F8](00-escopo.md) (29/10/2026)
dá a situação de cada um, a etapa em que entra e as perguntas que faltam responder.

## A ISO (22/10/2026)

Hoje (decisão 23, `port/x360/src/iso360.c`): o `convert_iso.py` converte no PC e confere o tamanho, o MD5 e o SHA256
da SLUS-21678; no console, uma ISO sem a marca da convertida é recusada com uma caixa do sistema, e o jogo não começa.
A mensagem é a mesma para qualquer ISO sem a marca.

1. **Mensagens separadas por caso** (pedido): a ISO USA original sem converter ("converta no PC com o script"), uma
   versão não suportada (outra região, outra revisão: "só a USA SLUS-21678") e a imagem ilegível ou truncada. O console
   distingue pela identificação do disco (o `SYSTEM.CNF` e o volume) antes de olhar a marca.
   Implementado no WSL em [01-avisos](01-avisos.md), junto com a busca pela convertida (item 6); build e sessão no
   360 ficam para a revisão.
2. **Conversão no próprio console** (pergunta para quando a F8 for escopada; seria rever a decisão 23): o código de
   conversão já rodou no console antes da decisão 23; o custo é ler e gravar 3 GB no FATX, com tela de progresso, e os
   conteúdos comprimidos (BPE) continuariam convertidos na carga.

## Os textos para o usuário (22/10/2026)

3. **Todo aviso que aparece para o usuário em inglês** (pedido): as caixas do sistema (a ISO recusada, a falta de
   memória, a queda), as telas de falha e qualquer texto do port na tela. O que vai só para o log (`bt3xbx.log`,
   `saida.txt`) pode seguir em português. Fazer o levantamento dos textos visíveis (as chamadas de `XShowMessageBoxUI`,
   as telas de cor por fase e os textos desenhados pelo port) antes de traduzir.
   Levantamento e tradução em [01-avisos](01-avisos.md); as telas de cor não tinham texto.
9. **A caixa de ISO ausente fala com o usuário comum** (visto pelo Lucas no console, 28/10/2026): a recusa funciona
   (a caixa aparece e o jogo não começa), mas o texto mistura inglês com os nomes em português dos arquivos e manda
   mexer no `BT3_ISO` do `ambiente.txt`, que só o projeto usa. O texto deve dizer que a ISO não está na pasta raiz do
   jogo com o nome esperado, dizendo o nome. Os nomes que o usuário vê (a ISO convertida, o script de conversão)
   passam para inglês, e nenhuma caixa cita o `ambiente.txt`. Vale para as outras caixas da ISO também.
   Implementado no WSL em [02-icones](02-icones.md): `bt3-x360.iso`, `convert_iso.py` e marca versão 2.
   Aparência das caixas e ícones no console aguardam a revisão.

## Fechamento de gaps, antes do release (27/10/2026)

Defeitos conhecidos que o Lucas mandou deixar para o fim do plano, para não travar as fases:

4. **O preto intermitente de Namek na animação do Shenlong** (vinha da F5, etapa 9): no começo da animação, com as
   esferas subindo, um pedaço do céu e da água fica preto e volta. O pulso do brilho do Shenlong, que era outro
   defeito, foi corrigido (o cache das tabelas, `f5-clut`). Já testado no console sem resolver: o cache da GPU
   (`BT3_INVALIDA`), os alvos limpos, o alfa (`BT3_ALFA255`, `BT3_ALFA_INT`), o alvo sobreposto (`BT3_PULA_FBP`), o
   brilho em 100% (`BT3_GLOW`) e todos os efeitos de tela fora. O céu são 32 triângulos da CPU com Gouraud e CLUT
   estável; a água, grupos da VU1 com DATE e FBA. Estado, hipóteses e a rodada de 4 aberturas preparada:
   [F5/09](../F5/09.md) e os branches `f5-shenlong` e `codex/f5-namek`.
5. **O estágio do deserto à noite escuro demais** (visto pelo Lucas jogando, 23/10): talvez o mesmo brilho somado
   mais fraco no 360 (a H9 de [F5/09](../F5/09.md)); sem captura ainda.
6. **Arrumação para a finalização** (27/10/2026): o 360 achar a ISO convertida quando há a original e a convertida na
   mesma pasta (hoje a busca pega a primeira `.iso`); a numeração repetida das seções das notas da F5; as pastas
   temporárias do WSL; propor ao upstream as correções do cache que valem também para o PC (a colisão das tabelas do
   `f5-clut` e as 16 cores do `codex/f5-clut16`).

7. **Todos os logs desligados por padrão** (pedido do Lucas, 28/10/2026): no release, o jogo não grava nada que só
   serve ao desenvolvimento. O `bt3xbx.log` e o diário, as medidas (picos, tempos, cortes, perfis da GPU e da CPU,
   deriva do som, filme) e as capturas ficam desligados sem `ambiente.txt`, e continuam ligáveis por ele para teste.
   Conferir no console que, desligados, não custam nada no quadro nem gravam arquivo.

11. **As duas provas de console que restam da F6** (decisão do Lucas, 28/10/2026, para não repetir depois): a
    sessão longa (uns 30 min de jogo livre, com ciclos de luta e menu e a Guide algumas vezes, a memória no mesmo
    estado depois de cada ciclo e o som sem desandar) e a saída para o dashboard pela Guide (o BT3 não tem opção de
    sair e o port não criou uma), sem travar nem deixar som preso. Ficam para a sessão final de console do
    fechamento, sobre a build do release. O resto da lista de console da F6 etapa 9 saiu: filme e escuta (item 10),
    frio e aquecido (a F5 já mede) e a queda provocada.

13. **Falhar com elegância, não com uma tela vermelha solta** (pedido do Lucas, 28/10/2026, depois da queda do Final
    Flash no Broly): hoje uma queda pinta a tela de vermelho e espera um botão, sem dizer nada. No release, a queda
    mostra um aviso claro em inglês (o jogo encontrou um erro e vai voltar ao painel), guarda o log para quem quiser
    reportar e volta ao painel limpo, sem deixar a FreeStyle em laço (o defeito do laço é tratado antes, na tarefa da
    queda). As cores por fase continuam só para o desenvolvimento.

15. **Análise do suporte a mods** (pedido do Lucas, 29/10/2026, para o fechamento): estudar o que seria preciso para o
    port do 360 aceitar (a) os mods do BT3 que a comunidade usa (personagens, trajes, estágios, texturas, sons; hoje a
    pasta `mods/` é recusada ou convertida na carga, decisões 18, 23 e 24) e (b) o Tag Team na versão do port de PC (o fork
    nativo do autor sobre o Tenkaichi3Decomp e a branch `tagteam-exp` do upstream, `pesquisa/05-tag-team.md`). Só
    análise: formatos, conversão para big-endian, memória e desempenho no Xenon, e a licença (o código do Tag Team de PC
    é AGPL-3.0; trazê-lo muda a licença do port, decisão a tomar então). Não mexe na decisão 16 nem antecipa a F9.

## A abertura do repositório, a última parte do plano (27/10/2026)

8. **Antes de abrir o repositório** (pedido do Lucas, só anotado, sem plano ainda): os documentos em inglês, com o
   guia de instalação e o de contribuição; e o GitHub configurado para receber as contribuições.

## Depois da primeira release (29/10/2026)

14. **Otimizar a GPU para subir a resolução** (pedido do Lucas): hoje a resolução é limitada pela GPU, não pela CPU. Na
    escala 1,33 a GPU tem uns 8 ms de folga na mediana e uns 2 ms nos quadros pesados do Duelo de teste; a escala 1,5
    mediu 32 ms de GPU na mediana e 45 nos trechos pesados (22/10, `plano/F5/08-720p.md`). Caminhos já levantados: o
    desenho comum sem textura, as leituras de alvos e a cadeia do brilho (perfil fino de 28/10, seção 31 do
    `plano/F5/08-gravacao.md`), os efeitos de tela na escala 1 (`BT3_ESCALA_FX`, 3 a 5 ms estimados) e o ganho de tabela
    dos prefixos. Começa por uma rodada curta medindo a GPU em 1,33, 1,4 e 1,5 sobre a build da release.

## Fora do plano do port (28/10/2026)

10. **As provas da F6 que ficaram por fazer no console** (o Lucas tirou do plano: não são importantes o suficiente).
    Etapa 7: a abertura pelo modo 2 do título, o START no meio da abertura e o final da História; as capturas da
    abertura já bateram com a referência e o boot foi visto e ouvido perfeito (28/10). A medida do sincronismo da
    abertura recusa porque as duas primeiras imagens saem antes de o BGM ter posição (`geracao=0`, uns 33 ms, ninguém
    percebe); fica como está. Etapa 8: a escuta de 11 min com a Guide, o tom de 1 kHz e o Treino sem som. Os
    ambientes estão prontos em `build/sessao_2810b/subir/` (`f6e7b_03` a `08`) se um dia valer rodar.
