# F8: itens anotados para o acabamento

[Índice do plano](../../plano.md)

Itens que o mantenedor pediu para o acabamento ao longo das outras fases. O [escopo da F8](00-escopo.md) (29/10/2026)
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
2. **Conversão no próprio console** (decisão 23 revista pelo mantenedor): implementada no WSL em
   [03-conversao-console](03-conversao-console.md), com os conversores C comuns, ícones, autenticação integral,
   temporário, conferência e publicação. A carga continua lendo a ISO preparada; BPE se converte na carga.
   Build XDK, sessão FATX/Guide e aprovação dos textos provisórios aguardam a revisão.


## Os textos para o usuário (22/10/2026)

3. **Todo aviso que aparece para o usuário em inglês** (pedido): as caixas do sistema (a ISO recusada, a falta de
   memória, a queda), as telas de falha e qualquer texto do port na tela. O que vai só para o log (`bt3xbx.log`,
   `output.txt`) pode seguir em português. Fazer o levantamento dos textos visíveis (as chamadas de `XShowMessageBoxUI`,
   as telas de cor por fase e os textos desenhados pelo port) antes de traduzir.
   Levantamento e tradução em [01-avisos](01-avisos.md); as telas de cor não tinham texto.
   A queda ganhou a caixa aprovada e `crash.txt` em inglês em [04-queda-e-logs](04-queda-e-logs.md).
   Saves padrão já usam nomes em inglês; cores de fase só com diagnóstico. Aceite visual no 360 pendente.
   A tradução dos arquivos de diagnóstico e dos modelos de ambiente está em
   [05-arquivos-ingles](05-arquivos-ingles.md), com inventário, provas nativas e limites de validação.
9. **A caixa de ISO ausente fala com o usuário comum** (visto pelo mantenedor no console, 28/10/2026): a recusa funciona
   (a caixa aparece e o jogo não começa), mas o texto mistura inglês com os nomes em português dos arquivos e manda
   mexer no `BT3_ISO` do `environment.txt`, que só o projeto usa. O texto deve dizer que a ISO não está na pasta raiz do
   jogo com o nome esperado, dizendo o nome. Os nomes que o usuário vê (a ISO convertida, o script de conversão)
   passam para inglês, e nenhuma caixa cita o `environment.txt`. Vale para as outras caixas da ISO também.
   Implementado no WSL em [02-icones](02-icones.md): `bt3-x360.iso`, `convert_iso.py` e marca versão 2.
   Aparência das caixas e ícones no console aguardam a revisão.

## Fechamento de gaps, antes do release (27/10/2026)

Defeitos conhecidos que o mantenedor mandou deixar para o fim do plano, para não travar as fases:

4. **O preto intermitente de Namek na animação do Shenlong** (vinha da F5, etapa 9): no começo da animação, com as
   esferas subindo, um pedaço do céu e da água fica preto e volta. O pulso do brilho do Shenlong, que era outro
   defeito, foi corrigido (o cache das tabelas, `f5-clut`). Já testado no console sem resolver: o cache da GPU
   (`BT3_INVALIDA`), os alvos limpos, o alfa (`BT3_ALFA255`, `BT3_ALFA_INT`), o alvo sobreposto (`BT3_PULA_FBP`), o
   brilho em 100% (`BT3_GLOW`) e todos os efeitos de tela fora. O céu são 32 triângulos da CPU com Gouraud e CLUT
   estável; a água, grupos da VU1 com DATE e FBA. Estado, hipóteses e a rodada de 4 aberturas preparada:
   [F5/09](../F5/09.md) e os branches `f5-shenlong` e `codex/f5-namek`.
5. **O estágio do deserto à noite escuro demais** (visto pelo mantenedor jogando, 23/10): talvez o mesmo brilho somado
   mais fraco no 360 (a H9 de [F5/09](../F5/09.md)); sem captura ainda.
   **Estado em 29/10** ([05-brilho](05-brilho.md), decisão 34: o brilho igual ao do PC a 60%): a H9 não mostrou defeito
   do 360. A referência dela, o rasterizador x86, lê no ponto a cadeia do brilho que o jogo pede bilinear, e isso
   desloca o halo dele 40 pixels em x (o modelo da cadeia prevê 40,5; as capturas dão 40) e o estreita; os 39% são os
   60% do port vezes 0,615, e o que falta nos 0,615 tem candidato comum ao PC (o alfa acima de 0x80 que o PC e o 360
   prendem). Nenhuma mudança no 360. Armado no WSL (`bt3-brilho-ref`, esperando a trava): o rasterizador com o bilinear
   do GS e o alfa do PC contra as capturas da H9. Preparada a rodada curta de console: o Shenlong na configuração de
   release a 60% e a 0%, e o deserto à noite em luta a 60, 0 e 100 (ambientes em [05-ambientes](05-ambientes/)).
   **Problema conhecido (decisão do mantenedor, 29/10):** sem mais teste. A rodada de console e a prova no WSL não serão
   feitas (o serviço `bt3-brilho-ref` foi desarmado antes de rodar). Vai para os problemas conhecidos do README da
   release: os estágios à noite podem parecer escuros demais no console. Fica em dúvida se a causa é o jogo ou a TV do
   o mantenedor (os "Níveis de referência" de vídeo do console contra a faixa que a TV espera escurecem os tons baixos); o
   brilho do 360 segue o do PC a 60% pelo que se mediu.
6. **Arrumação para a finalização** (27/10/2026): o 360 achar a ISO convertida quando há a original e a convertida na
   mesma pasta (hoje a busca pega a primeira `.iso`); a numeração repetida das seções das notas da F5; as pastas
   temporárias do WSL; propor ao upstream as correções do cache que valem também para o PC (a colisão das tabelas do
   `f5-clut` e as 16 cores do `codex/f5-clut16`).

7. **Todos os logs desligados por padrão** (pedido do mantenedor, 28/10/2026): no release, o jogo não grava nada que só
   serve ao desenvolvimento. O `bt3xbx.log` e o diário, as medidas (picos, tempos, cortes, perfis da GPU e da CPU,
   deriva do som, filme) e as capturas ficam desligados sem `environment.txt`, e continuam ligáveis por ele para teste.
   Implementado no WSL em [04-queda-e-logs](04-queda-e-logs.md): `BT3_LOG=1` obrigatório, tempos
   desligados e ambientes de teste explícitos. Ensaios e varredura passaram. Conferir no console a árvore
   completa de arquivos e o custo no quadro pelo método da nota; custo literalmente zero não foi medido.

11. **As duas provas de console que restam da F6** (decisão do mantenedor, 28/10/2026, para não repetir depois): a
    sessão longa (uns 30 min de jogo livre, com ciclos de luta e menu e a Guide algumas vezes, a memória no mesmo
    estado depois de cada ciclo e o som sem desandar) e a saída para o dashboard pela Guide (o BT3 não tem opção de
    sair e o port não criou uma), sem travar nem deixar som preso. Ficam para a sessão final de console do
    fechamento, sobre a build do release. O resto da lista de console da F6 etapa 9 saiu: filme e escuta (item 10),
    frio e aquecido (a F5 já mede) e a queda provocada.

13. **Falhar com elegância, não com uma tela vermelha solta** (pedido do mantenedor, 28/10/2026, depois da queda do Final
    Flash no Broly): hoje uma queda pinta a tela de vermelho e espera um botão, sem dizer nada. No release, a queda
    mostra um aviso claro em inglês (o jogo encontrou um erro e vai voltar ao painel), guarda o log para quem quiser
    reportar e volta ao painel limpo, sem deixar a FreeStyle em laço (o defeito do laço é tratado antes, na tarefa da
    queda). As cores por fase continuam só para o desenvolvimento.

15. **Análise do suporte a mods** (pedido do mantenedor, 29/10/2026, para o fechamento): estudar o que seria preciso para o
    port do 360 aceitar (a) os mods do BT3 que a comunidade usa (personagens, trajes, estágios, texturas, sons; hoje a
    pasta `mods/` é recusada ou convertida na carga, decisões 18, 23 e 24) e (b) o Tag Team na versão do port de PC (o fork
    nativo do autor sobre o Tenkaichi3Decomp e a branch `tagteam-exp` do upstream, `pesquisa/05-tag-team.md`). Só
    análise: formatos, conversão para big-endian, memória e desempenho no Xenon, e a licença (o código do Tag Team de PC
    é AGPL-3.0; trazê-lo muda a licença do port, decisão a tomar então). Não mexe na decisão 16 nem antecipa a F9.
16. **As regras das issues, para rever no fechamento** (mantenedor, 29/10/2026): ficam como estão por enquanto, mas ele não
    concorda com a forma atual. Hoje: dois formulários (queda e defeito sem queda), queda sem `crash.txt` fechada na
    hora pela ação e reaberta quando o arquivo chega, respostas prontas por rótulo, nada fecha por prazo
    ([06-publicacao](06-publicacao.md), seção 5a). **Fechado em 31/10/2026:** o mantenedor manteve os dois formulários e a
    triagem; a preocupação dele era spam, palavrão e baixaria, e a triagem ganhou a moderação automática (seção 5a, item 3).

## A abertura do repositório, a última parte do plano (27/10/2026)

8. **Antes de abrir o repositório** (pedido do mantenedor, só anotado, sem plano ainda): os documentos em inglês, com o
   guia de instalação e o de contribuição; e o GitHub configurado para receber as contribuições.

## Depois da primeira release (29/10/2026)

14. **Otimizar a GPU para subir a resolução** (pedido do mantenedor). **Desistido em 11/11/2026:** a resolução fica como
    está; o que segue abaixo é o registro de quando o item estava aberto. Hoje a resolução é limitada pela GPU, não pela CPU. Na
    escala 1,33 a GPU tem uns 8 ms de folga na mediana e uns 2 ms nos quadros pesados do Duelo de teste; a escala 1,5
    mediu 32 ms de GPU na mediana e 45 nos trechos pesados (22/10, `plano/F5/08-720p.md`). Caminhos já levantados: o
    desenho comum sem textura, as leituras de alvos e a cadeia do brilho (perfil fino de 28/10, seção 31 do
    `plano/F5/08-gravacao.md`), os efeitos de tela na escala 1 (`BT3_ESCALA_FX`, 3 a 5 ms estimados) e o ganho de tabela
    dos prefixos. Começa por uma rodada curta medindo a GPU em 1,33, 1,4 e 1,5 sobre a build da release.
    **30/10:** a 1,4 entrou pela chave (640 x 480 em 896 x 672) e a primeira otimização foi o desenho sem textura sem a
    leitura da branca (`BT3_OTIM_SEMTEX`, 1,6 a 3,9 ms estimados na 1,4), não o `BT3_ESCALA_FX` (até 1,3 ms); a rodada de
    console está preparada ([09-escala](09-escala.md)).
    O MSAA 2x da EDRAM no lugar ou ao lado do FXAA: estudo de custo e protótipo sob `BT3_MSAA=2` em
    [10-msaa.md](10-msaa.md) (30/10/2026, branch `claude/f8-msaa`), com a rodada de console preparada.

17. **A biblioteca da VU0 do port em float nativo no 360** (30/10/2026; o mantenedor deixou como ponto de melhoria, sem mudar
    essa lógica agora): os floats andam como bits `uint32_t` e cada conta vai e volta pela memória (load-hit-store); na luta
    comum isso é uns 11,7 ms por quadro da thread do jogo, e o conserto estimado ganharia 8 a 9 ms. Exige provar o mesmo
    resultado na x86, na PPC e no 360 pelos roteiros. Estudo em [11-cpu-vu0](11-cpu-vu0.md), item A; o item B (a projeção
    por vértice na explosão do planeta) foi feito antes da release. Implementação do item A preparada em
    `vu0-nativo` em 04/11: **aceite sem console concluído**, 2 milhões de casos por função e fastvec em cada
    variante x86/PPC, modelos inteiro/360; mutantes recusados e roteiro oficial `f5_duelo` com “tudo certo”,
    hashes e sete pontos de memória aprovados. Objetos e `.text` x86 preservados. A seção nova do estudo
    lista a guarda e as contagens GCC. **Continua pendente no XDK e no console**: desmontagem do cl sem
    fusões e medida do amostrador. Não considerar o ganho estimado como medido.
18. **A luta de tela dividida mais rápida** (31/10/2026, pedido do mantenedor): roda a 25,4 quadros por segundo, com o jogo a
    85% da velocidade; a gravação do GS (39,6 ms), o jogo (33,8) e o desenho (30,1) estão todos perto do limite. Os pontos
    de melhoria, com ganho e custo, estão em [13-tela-dividida](13-tela-dividida.md). O recorte justo do estágio (C1) e o
    desenho no núcleo do áudio (ponto A) entraram na 1.0 por padrão, com 30 quadros firmes no roteiro (decisão 39); a
    cópia especializada (D2) piorou e ficou de fora. O resto fica para a 1.1.

## Fora do plano do port (28/10/2026)

10. **As provas da F6 que ficaram por fazer no console** (o mantenedor tirou do plano: não são importantes o suficiente).
    Etapa 7: a abertura pelo modo 2 do título, o START no meio da abertura e o final da História; as capturas da
    abertura já bateram com a referência e o boot foi visto e ouvido perfeito (28/10). A medida do sincronismo da
    abertura recusa porque as duas primeiras imagens saem antes de o BGM ter posição (`geracao=0`, uns 33 ms, ninguém
    percebe); fica como está. Etapa 8: a escuta de 11 min com a Guide, o tom de 1 kHz e o Treino sem som. Os
    ambientes estão prontos em `build/sessao_2810b/subir/` (`f6e7b_03` a `08`) se um dia valer rodar.

## Recuperação da queda (29/10/2026)

13. **Queda com aviso claro e relatório independente dos logs** (aprovado pelo mantenedor): caixa do sistema
    `Unexpected error`, relatório inglês `crash.txt` com carimbo e contexto, OK seguido de desmontagem com teto
    e retorno ao painel. Implementado e ensaiado no WSL em [04-queda-e-logs](04-queda-e-logs.md), sobre
    [F7/queda-freestyle](../F7/queda-freestyle.md). Produtores gráficos param antes da caixa para a main apresentar;
    cargas, som e vídeo terminam depois do OK. Falha de caixa conserva o caminho de tela e botão.
    Compilação XDK, Guide, FATX, painel respondendo e reabertura continuam pendentes na revisão.

## Importar e exportar saves (3/11/2026)

19. **Mover o save entre o PS2 e o 360 pela pasta do jogo** (desenho aprovado pelo mantenedor): a pasta `game:\import` com `.psu`
    abre uma tela SAVE IMPORT na abertura do jogo (IMPORT ou SKIP; o save atual vai antes para `game:\backup\NNN`; os arquivos
    vão para `done`, `skipped` e `failed`, nunca apagados), e a pasta `game:\export` faz o jogo gerar
    `export\BASLUS-21678DBZT3.psu` a cada gravação do save. Feito no ramo `save-importa`: a lógica em C89 com sistema de
    arquivos trocável, o gancho em `plat_mc.c` atrás de `PORT_X360`, a tela, os textos em `iso_avisos.h` (pendentes de
    aprovação do mantenedor), a seção "MOVING SAVES TO AND FROM THE PS2" do README do pacote (texto público pendente da
    auditoria do mantenedor), os testes no Linux com mutantes e a build do XDK sem stub novo. Documento:
    [save-importacao](../../formatos/save-importacao.md).
    **Falta:** o teste no console (FATX, `MoveFile`, `FindFirstFile`, a tela na TV, o controle), com um `.psu` de verdade
    exportado de um PS2 ou do PCSX2; ver uma exportação no console abrir no PCSX2 pelo mymc++; aprovar os textos da tela e
    o README; decidir se a importação vai na 1.0.2 (a lista de problemas conhecidos diz hoje que importar não existe).
    **11/11/2026:** o mantenedor não vai fazer o teste com um `.psu` de PS2 ou PCSX2; a prova com save real fica aberta
    para a comunidade.
20. **A caixa de erro não aparece quando o vídeo é recusado** (console, 09/11/2026, rodada 5 da conferência da 1.1.0,
    ambiente falha_video): o jogo saiu direto para a FreeStyle, sem a caixa Unable to continue. O diário mostra o
    porquê: `vídeo: inicialização recusada` (código 7), e na desmontagem a thread de desenho do GS não parou no teto
    de 2000 ms (`saída: GS desenho: espera=00000102`, WAIT_TIMEOUT); com produtor pendente, a saída segue direto ao
    dashboard sem retomar o D3D, que é o recuo previsto. O mantenedor aceitou a saída limpa para a 1.1.0 e pediu para
    resolver depois: fazer a thread de desenho parar quando o renderer nunca chegou a iniciar. Log em
    `local/console/rel110/logs/r5_bt3xbx.log` (fora do git).
21. **O replay com bug desde a primeira release até a última** (informado ao mantenedor, 11/11/2026; sintoma de
    segunda mão: "sempre trocava para o Goku e travava"). **Causa provável achada no código em 12/11/2026, sem
    reproduzir:** `BattleReplay_Load` (`src/battle/battle_load.c:1714`) confere o cabeçalho com `*(u64 *)buf` contra
    `(BTL_SETUP_VERSION << 32) | BTL_SETUP_MAGIC`, constante montada para little-endian. No 360 os bytes
    `62 74 6C 73 00 00 00 07` dão `0x62746C7300000007` e a comparação nunca passa. O `McFlow_UpdateReplayLoad`
    (`src/sys/mcflow_a.c:2379`) ignora o retorno: a luta começa com o setup do `Battle_ClearWork` (personagens 0, o
    Goku) e sem as entradas gravadas. A varredura do `47a4097d` (`PS2_LOW`/`PS2_LD64`) não pegou este caso porque a luta
    de demonstração não passa por ele, e nenhum roteiro grava nem carrega replay (o `datacenter` só abre a tela 56 com
    a lista vazia). `PS2_LD64` sozinho não conserta: a comparação tem de ser pelos bytes do `magic` e pela `version`.
    **Corrigido no código em 12/11/2026** com `PS2_TAG32` (a constante na ordem do host) e `PS2_LD64` na leitura; a
    build de matching expande para a expressão original. Compilado nos dois modos do port e conferido numa conta com
    os bytes do cabeçalho, sem rodar o jogo. **Provado na PPC em 12/11/2026** pelo roteiro `replay` (`09da6fd3`,
    `SEMMAPA=1`): com a correção, "tudo certo", a luta carregada do cartão igual na x86 e na PPC e os dois `confere
    luta` com o Kuririn; sem a correção, a PPC carrega a luta do setup zerado, lê arquivos que a x86 nunca leu e para
    na guarda (a guarda da PPC sem mapa corta antes do segundo `confere luta`, então ele não foi visto falhando). Build
    do XDK do `dev` pronta (`1c042253`). Falta: o console, com a falha vista na build velha e o mesmo replay carregado
    na nova (`local/console/replay/`). **Aprovado no console em 12/11/2026:** na build gravorig (sem o conserto) o replay
    assistido no fim da partida funciona (vem da memória) e o carregado pelo Data Center falha; na build do `dev`
    (`1c042253`, com o conserto) o mesmo replay, do mesmo cartão, carrega certo. O `src/battle/eft_g.c:499`, anotado
    como da mesma classe, foi conferido em 12/11 e não é defeito: o campo em `0x19F0` é `u64 flags`
    (`include/battle/battle.h:257`) e todo acesso a ele usa a mesma largura (`eft_g.c:499`, `eft_r.c:497`,
    `gfxm_b.c:934`, `gfxm_e_b.c:702`), então a ordem dos bytes não muda o bit.
22. **Mods no console, rodada de 11/11/2026 (build `mods-telas`, `ee205cdb`)**: controle e pacote aprovados (o pacote
    não avisa na reabertura; o pacote novo avisa uma vez e segue). **Defeito:** com arquivos soltos em subpastas
    (`mods\pzs3us1\...`) o boot registra `mods: nenhum` e `mods: preparação recusada (-1)` e mostra a caixa "Cannot
    load mods"; com um arquivo solto na raiz de `mods` a tela dos soltos abre. A varredura do Xbox (`mods_pasta.c`,
    `FindFirstFile`) nunca tinha rodado no console (o WSL usa o ramo POSIX); a separação pela subpasta está no
    diário da rodada. **Pedidos do mantenedor para as telas:** a tela dos soltos no mesmo estilo da "Unsupported",
    com OK e Cancel (não segurar A e B); só a mensagem principal em vermelho, não todas as letras. Feito na
    `mods-telas` (build do XDK pronta, falta o console); o texto novo foi **aprovado** pelo mantenedor em 11/11: o
    destaque "WARNING: unconverted mod files detected." em vermelho e o resto do aviso aprovado antes, sem o "Press A to
    continue.".
    **Aprovado no console em 12/11/2026** (build `cd7b30fe`): os soltos em subpastas achados (o "Cannot load mods" de
    11/11 sumiu), Cancel cancela, OK segue e grava o `mods-vistos.bin`, reabrir não avisa, um arquivo novo avisa uma
    vez. Falta só o texto da tela (pendência 25).
23. **A troca de corpo do Ginyu no BT4** (`bt4_body_change.c`, do BT4decomp R3, `83e3bd9`; meio dia a um dia): fora da
    primeira release com BT4 por decisão do mantenedor (11/11/2026), anotada para depois (decisão 40, nota de 11/11).
24. **Sessão de console seguinte (ordem aprovada em 11/11/2026):** terminar a gravorig2 (C6 a C9; a C6 já está no
    `environment.txt` do console), depois as 4 aberturas curtas dos mods com a build corrigida da `mods-telas`
    (`cd7b30fe`; soltos em subpasta com Cancel, com OK, reabrir sem aviso, um arquivo novo avisa), depois a sessão do BT4
    (build do XDK da `bt4-etapas34`, a imagem de teste de 5,5 GB e os ambientes `bt4-etapa4`, inclusive o teste das
    recusas: arenas 11 e 46 e a técnica 4 do Broly Super Saiyan 3). Antes da sessão do BT4: a caixa de erro do
    complemento corrompido e o nome `bt4-x360.*` (decisão 40, notas de 11/11).
25. **Tela dos mods soltos, antes da release** (pedido do mantenedor no console, 12/11/2026, build `mods-telas` `cd7b30fe`):
    a tela funcionou como esperado com os soltos em subpastas (aviso, Cancel cancela). Mudar o texto: tirar o "load mods"
    e começar direto pelo aviso ("WARNING: unconverted mod files detected."); a linha de cima, "BT3 X360 <versão> ISO
    conversion", fica. Não fazer agora; entra antes da release que levar os mods.
26. **Integrar a `main` e o `dev` no branch do BT4 antes da primeira release com BT4** (12/11/2026): o `bt4-etapas34`
    saiu de `9c69c4ba` (época da 1.0.2) e não tem a release 1.1.0 nem o que entrou no `dev` depois (o conserto do replay,
    entre outros). Ensaio de fusão de 12/11 (`git merge-tree` do `dev`, 91 commits, contra o BT4, 408): 16 arquivos em
    conflito, 4 de documentos e 12 de código (`main360.cpp`, `gs_d3d.cpp`, `gs_draw.c`, `float360.c`, `headless.py`,
    `c89.py`, `rastreio.c`, `stg_c.c` e ferramentas de teste). Por pedido do mantenedor, começa já, num branch novo
    `bt4-integracao`, para os próximos testes de console serem sobre a base certa; a sessão de console do BT4 de 12/11
    roda sobre o `bt4-etapas34` como está.
27. **Menu "MODDED MODES" do Tag Team deixa a entrada passar para o menu do jogo** (console, 12/11/2026, build do
    `bt4-etapas34` `2ec1e4ea`, ambiente `bt3controle`): com a lista de modos do mod aberta (o botão que a abre e fecha
    pelo menu principal; textos em `port/src/gs/tag_text_*.inc`), mexer nela também mexe o menu principal do jogo por
    baixo. O menu do mod tem de ficar com a entrada só para ele enquanto estiver aberto. Anotado pelo mantenedor como
    importante; corrigir antes da release com BT4. Confirmado também no BT4 (ambiente `bt4menu`, 12/11).
28. **Gravação de origem: opção 2** (decisão do mantenedor, 12/11/2026, depois da gravorig2): adotar com a cópia das
    páginas de textura feita no envio (lado da gravação, que ficou com folga) em vez de na primeira leitura do desenho.
    Antes de mexer, confirmar a causa: build com um contador por quadro de páginas materializadas e de `Gs_PageHash`
    junto da linha dos cortes, C10 e C11 de novo, e a correlação com a diferença da classe `vu0`. Se confirmar, a mudança
    e depois B (c, o, o, c), C10/C11 e um A rápido. Análise em `local/console/gravorig2/LEIA-ME.md` (fora do git).
29. **BT4: a luta trava numa técnica por falta de memória de textura** (console, 12/11/2026, build `bt4-etapas34`
    `2ec1e4ea`, ambiente `bt4menu`, Gogeta Blue contra Goku Black SSJ, no golpe do meteoro): no quadro 13794 o golpe
    pediu de uma vez texturas e alvos grandes; o `gs_d3d` descartou mais de 700 texturas (uns 63 MB), ficou com 29 MB
    livres (piso 32), 90 MB pendentes e 118 protegidas, devolveu 20 texturas brancas e, no pedido de um alvo de 896x672,
    esperou a GPU e nunca voltou (o vigia registra a thread do jogo parada a partir dali; saída pela Guide). Dois
    defeitos: a demanda das técnicas do BT4 passa do teto de textura do 360, e o backend trava em vez de seguir quando a
    memória acaba. Log em `local/console/bt4e34/logs/bt4menu_bt3xbx.log` (fora do git). Bloqueia a release com BT4.
    **Segundo travamento** (ambiente `bt4equipe`, luta em equipe com tela dividida, no agarrão): a thread do jogo parou
    sem nenhuma pressão de memória. Nos dois casos ela parou logo depois da primeira vibração da luta (o primeiro
    impacto pesado); a chamada da vibração voltou normal (`XInputSetState 0`, a linha é escrita depois dela), e no BT3
    (`bt3controle`) o primeiro impacto passou. O vigia diz que a thread do jogo parou, mas não onde: próximo passo, o
    vigia registrar o `Iar` e o `Lr` da thread do jogo parada (resolvidos pelo `.map`), e repetir o agarrão em equipe,
    que reproduziu. **Vibração descartada** (12/11, terceira abertura): com a vibração desligada nas opções, o mesmo
    agarrão em equipe travou igual, sem nenhuma linha de vibração e sem pressão de memória. O agarrão em equipe trava
    sempre que tentado (2 de 2); a falta de memória do primeiro caso pode ser outro defeito no mesmo golpe.
    O mantenedor duvida da hipótese da memória: o golpe final do Gogeta Blue passou sem travar na `bt4complemento`,
    e o agarrão, que trava, não é um golpe pesado. A thread do jogo presa na lógica do golpe é mais provável.
30. **Menu do Tag Team no 360: tradução incompleta e nome de botão do PS2** (console, 12/11/2026, `bt4equipe`): parte
    dos textos do menu do mod não está traduzida, e a página pede "Cross" (nome do botão do PS2) em vez do botão do
    controle do Xbox. Os nomes de botão no 360 seguem a tabela do port (X do jogo = A, círculo = B, quadrado = X,
    triângulo = Y, Select = Back).
31. **HUD da luta em equipe com tela dividida** (console, 12/11/2026, `bt4equipe`): cada vista desenha um HUD diferente,
    quatro barras de vida no total. O mantenedor achou estranho; conferir contra o Tag Team de PC se é o desenho
    pretendido (cada metade com as barras da sua luta) ou duplicação.
    **Resolvida sem mudança (mantenedor, 12/11):** os HUDs a mais fazem sentido no Tag Team com mais lutadores; nada a fazer.
32. **Complemento do BT4 no console: a caixa não apareceu, e o diário não mostra leitura nem gravação** (12/11/2026,
    `bt4complemento`, build `2ec1e4ea`): com um `native-bt4-custom.bin` truncado de propósito (o cabeçalho e 100 bytes
    zerados, sem dados do jogo) na raiz de `game:\sv_b4e4bt4menu`, o mantenedor jogou com o Gogeta Blue (personagem do
    BT4) e nada aconteceu; em nenhuma abertura do BT4 o diário registra o complemento lido, e nenhum save do BT4 tem o
    arquivo gravado. Duas hipóteses: o `PortGame_CustomSlot` não foi chamado para um personagem acima de 96, ou o
    `fopen` do 360 não acha o caminho com barras misturadas (`game:\sv_b4e4bt4menu/native-bt4-custom.bin`, de
    `custom_path` em `plat_game.c`) e o trata como ausente (então ele também nunca seria gravado no console, e o
    `3e986921` só vale no emulador). Próximo passo junto com o da pendência 29: uma build com o diário dizendo o caminho, o
    resultado do `fopen` e a gravação do complemento.
33. **As cinco recusas do BT4: as receitas de teste não passaram no console** (12/11/2026, `bt4recusas`, build
    `2ec1e4ea`): as arenas carregam, os lutadores pisam e os modelos aparecem normais, mas a técnica 4 do Broly SSJ3 caiu
    com a caixa de queda (`crash.txt`: `C0000005`, leitura de `6B91BA08`). Cadeia pelo `.map`: `DemoCam_Update` (a câmera
    da técnica) → `BtlCam_TraceStage` → `StgCol_TraceSphere` → `StgCol_QueryZone` → `ColMesh_WalkBox` →
    `ColMesh_WalkNode` → `ColNode_TestBox+0x8`, lendo um nó da árvore de colisão do cenário num endereço com lixo na
    metade de cima (`r3`, `r30`, `r31` = `00000001_6B91B9FC`). Logo antes, a imagem abriu `pzs4us2/02209` (a câmera) e
    `pzs4us1/01568` (a arena), os dois por receita de teste. Hipótese: um campo da árvore de colisão da arena que a
    receita do cenário não converte (o andar dos lutadores não usa esse caminho, e a câmera da técnica sim). Pela decisão
    40, as receitas de teste **não** viram normais: a imagem de release continua recusando as cinco. Próximo passo:
    reproduzir na PPC (a técnica 4 do Broly SSJ3 na arena da `01568`, com um roteiro) e achar o campo.
    **Causa achada em 12/11/2026 (branch `bt4-colisao`, `f516c1e2`, no troca):** não era a árvore de colisão (conferida
    nó a nó, toda trocada), e sim a câmera da técnica, `pzs4us2/02209`: o `Conv_Camera` pulava o array de chaves de um
    canal quando o primeiro byte já estava marcado, e os arrays de canais vizinhos se sobrepõem; os canais 2 e 4 ficavam
    em little-endian e, na PPC, a câmera ia a z = -4,6e19. Fora das zonas, o `BtlStage_FindZoneNear` devolve a zona 0; na
    01568 (a arena 46, normal) a zona 0 tem malha no deslocamento 0 (o cabeçalho lido como malha), e a raiz `base - 4` em
    64 bits dá o `r3` do crash (hipótese coerente com os registros; o qemu não cai, a conta dele é de 32 bits). **A receita
    normal tinha o mesmo defeito na `02149`**: as imagens do BT4 geradas antes têm de ser refeitas. Correção: converter
    toda chave declarada; censo das 116678 entradas: só 02149 e 02209 mudam, sem recusa nova; BT3 igual; câmera da técnica
    idêntica entre a x86 e a PPC (704 linhas), zona 0 nunca mais chamada; mutantes acusados. Falta o console (imagem de
    teste regerada, a técnica 4 na arena 46) e levar o branch à integração. Uma guarda no jogo para malha de zona no
    deslocamento 0 não foi aplicada (o PS2 lê igual; esconderia o próximo defeito de dados).
    **Aprovado no console em 12/11/2026, à noite:** com a imagem de teste regerada pelo conversor corrigido e a mesma build
    da queda (`2ec1e4ea`, só a imagem mudou), a técnica 4 do Broly SSJ3 saiu e as lutas foram até o fim nas arenas 46
    (`01568`) e 11 (`01533`), sem queda nem travamento. As arenas 11 e 46 divididas (`01632`, `01667`) não foram jogadas no
    console. A decisão 40 (as receitas de teste viram normais depois do teste do mantenedor) fica com o mantenedor.
    **Luta em equipe dividida (Tag Team, dois controles), 12/11 à noite:** as duas arenas carregaram e as lutas foram até
    o fim, mas o jogo abriu os arquivos normais (`01568`, `01533`), não as variantes divididas (`01632`, `01667`, que são
    `1622 + id`): falta achar o modo que as carrega (o catálogo `plano/F9/05-arenas.md` só dá o endereço do mapeamento,
    SLUS `0x127340..0x127384`) e testá-las. Proposta ao mantenedor: aprovar as três que passaram (`01533`, `01568`,
    `02209`) e manter as duas divididas recusadas até o teste.
35. **Texturas que somem e ficam transparentes às vezes** (mantenedor, console, 12/11/2026): visto desde o começo do port,
    em todos os modos, não só no Tag Team. Fica para depois.
34. **Engasgos na seleção de personagens e de arenas: leitura de disco na thread do jogo** (console, 12/11/2026,
    `BT3_PICOS=20`, build `2ec1e4ea`, BT3): cada movimento do cursor lê do disco um arquivo novo de 27 a 60 KB
    (retrato, prévia), e a thread do jogo para esperando. Normalmente a leitura sai em 1 a 2 ms (91 MB/s), mas às vezes
    passa de 60 ms (8 de 24 leituras acima de 20 ms, pior 113,8 ms) e o quadro inteiro espera junto: quadro 2306 com o
    jogo em 116 ms e o `00816.bin` (48 KB) lido em 113,8 ms; 2275 e `00761` em 96 ms; 2437 e `00921` em 90 ms. A gravação
    e a GPU ficaram ociosas nesses quadros. No BT4 os picos são maiores (66 a 196 ms) pelo mesmo caminho. Causa provável
    da lentidão ocasional: a disputa pelo HD com a leitura da música. **Decisão do mantenedor (12/11): opção 1**, ler de
    uma vez os arquivos da grade ao entrar na seleção (uns 8 MB de personagens e 2 a 3 MB de arenas, ~0,3 s na transição de
    tela, nada na abertura do jogo), e o cursor não toca mais o disco. Log em `local/console/picos-menu/` (fora do git).
    **Feito e aprovado no console em 12/11/2026** (`efb7e8c1`, só código do 360: `iso360.c` e `estado360.c`; `BT3_PRECARGA=0`
    volta ao jeito antigo): a entrada na tela 39 lê o bloco 761 a 960 do `pzs3us1` (200 arquivos, 16,6 MB, 324 ms); ao
    mover o cursor, 29 de 29 arquivos vieram da memória e nenhum quadro passou de 40 ms (antes, 8 de 24 leituras acima de
    20 ms, até 114 ms). Custo: a entrada na seleção passou de 366 para 749 ms, uma vez, na transição. Os quadros lentos que
    sobram são das transições (depois de escolher, os modelos e a arena; o começo da luta), fora do cursor. Falta a
    rodada dos roteiros no WSL (x86 contra PPC) e levar ao BT4 (outros caminhos de arquivo, `pzs4us`).
