# Tag Team no port de PC: já existe, em beta

Data da consulta: **29/10/2026, entre 09:10 e 09:20 UTC**. [lido] significa informação encontrada na fonte;
[inferido] significa conclusão desta pesquisa. As datas dos eventos abaixo estão em UTC; a data de leitura de
todas as fontes é 29/10/2026. Publicação e código existente não significam aprovação de jogabilidade nesta pesquisa.

**Sim: o próprio The Mufti publicou um Tag Team nativo sobre o Tenkaichi3Decomp, para Windows e Linux, com
tela dividida para 2, 3 e 4 jogadores. A beta .3 saiu em 29/10/2026.** [lido: [lançamento beta .3](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/releases/tag/v0.1.0-beta.3),
[README no commit da beta](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/README.md)].
O nome `PC_Recomp` desse repositório não deve confundi-lo com o antigo BT3-Recomp: a base declarada é a
**decompilação compilada nativamente**, Tenkaichi3Decomp 0.1.37. [lido: README acima].

Há também código de quatro jogadores na branch experimental do próprio upstream e uma distribuição independente
do Pyrosea98. Portanto, a descrição de 12/10 na [decisão 16](../decisoes.md#16-mod-tag-team-depois-do-jogo-base-só-a-tela-dividida-de-4-12102026)
precisa desta atualização factual; a decisão de fazer somente quatro jogadores locais depois do jogo base permanece.
[inferido a partir das fontes das seções seguintes].

## 1. O mod que continua no PCSX2

| Aspecto | Resultado e fonte |
| --- | --- |
| Versão publicada | [lido] **Version 14.3**, tag `v0.1.0-beta.14.3`, pré-lançamento publicado em **25/10/2026 às 22:35:50**. Instaladores e atualizadores Windows e Linux x86-64: [release](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/releases/tag/v0.1.0-beta.14.3). |
| Último código | [lido] `main` e essa tag apontam para `7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d`, commit de **25/10/2026 às 22:24:19**. Não confundir `updated_at` do repositório com commit: [commit](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/commit/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d). A lista consultada de issues/PRs tinha atualização até **27/10/2026 às 01:01:56**, na [issue 11](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/issues/11). |
| Lutadores | [lido] Até **5 por equipe, 10 simultâneos**, com até **4 humanos locais**. `TEAM_CAPACITY = 5` limita a seleção, enquanto `ENGINE_ACTORS = 12` é capacidade interna de tabelas, não uma oferta de 12 lutadores: [battle_mode_policy.py](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/bt3-multifighter/tools/battle_mode_policy.py). |
| Modos locais | [lido] Equipes simultâneas, Free For All, cooperação e treino modificado; a versão 14 acrescentou Fighter Gauntlet e Tag Team Tournament, com equipes de 1 a 5 no torneio. [Política dos modos](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/bt3-multifighter/tools/battle_mode_policy.py), [release 14](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/releases/tag/v0.1.0-beta.14.0). |
| Tela e mecânicas | [lido] Tela dividida até quatro; a 14.3 oferece ao terceiro jogador a faixa inferior inteira. Há reviver aliados, fusões e ataques combinados; suporte a discos BT3 USA, Europa e Japão e BT4 B14 REV2 inglês/espanhol. Continua beta, sem cobertura humana de todo personagem/golpe: [README](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/README.md), [release 14.3](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/releases/tag/v0.1.0-beta.14.3). |
| Licença | [lido] O código original do mod é **GPL-3.0-only**; componentes têm termos próprios. [README, seção License](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/README.md), [LICENSE](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/LICENSE), [avisos dos componentes](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/THIRD_PARTY_NOTICES.md). |

[lido] O processo Python acessa a RAM **emulada do Emotion Engine** pela PINE: TCP local no Windows, socket Unix
no Linux. O cliente oferece leituras e escritas de memória; isso não é acesso a estruturas C do port de PC.
Fonte: [pine.py](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/bt3-multifighter/tools/pine.py).

[lido] Python também monta instruções MIPS, prepara blocos com endereço, bytes esperados e substitutos e instala
desvios nos pontos do executável PS2. O montador é a classe `Assembler`; `fresh_team_combat.py` produz os blocos
de combate, e `battle_modes.py` emite a política de FFA/cooperativo nesse formato.
Fontes: [prototype.py](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/bt3-multifighter/tools/prototype.py),
[fresh_team_combat.py](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/bt3-multifighter/tools/fresh_team_combat.py),
[battle_modes.py](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/blob/7f3216c9bf29f15f75ae7f0c67a39fd27f40be8d/bt3-multifighter/tools/battle_modes.py).
[inferido] Essa implementação continua dependente do PCSX2; a publicação nativa é outro projeto. A palavra
`native` em nomes de módulos do mod PS2, sozinha, não demonstra um port para PC.

## 2. O que já existe fora do PCSX2

### 2.1 O fork nativo do próprio autor

[lido] [tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-)
é identificado pela [API do GitHub](https://api.github.com/repos/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-)
como **fork de z3xox/Tenkaichi3Decomp**, criado em **27/10/2026 às 11:54:31**. Seu histórico é anterior à criação do
repositório público. A base declarada é `ec3c85a98cffd9cd22c22ac14cd62fb584d23664`, versão 0.1.37 do upstream;
`main` e beta .3 estão em `1b56a9cf1d104f6551a33da3278dd8d424dbe3ad`. O último commit é de **29/10 às 01:08:40**
e a release de **01:20:43**. Fontes: [README fixado](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/README.md),
[commit](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/commit/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad),
[release](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/releases/tag/v0.1.0-beta.3).

[lido] É um **protótipo jogável, não o mod completo**. A documentação descreve até dez lutadores, cinco por lado,
equipes desiguais, Free For All, treino, Gauntlet e torneio; quatro entradas independentes, câmeras e HUD por vista,
reviver e fusões. Relata lutas CPU em Windows/Linux e desenho Vulkan com 2/3/4 vistas, mas distingue entradas
roteirizadas e controles SDL virtuais de uma sessão humana com quatro controles físicos. Ainda faltam cobertura
ampla de golpes/personagens, sessões longas com quatro controles e mecânicas como cenários personalizados,
Potaras personalizadas e tag attacks. Fonte: [NATIVE_TAG_TEAM.md, estado e limites](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/NATIVE_TAG_TEAM.md).

[lido] A implementação está em C no ciclo normal da luta: `tag_team.c` administra participantes e alvos;
`Battle_DrawSplit` percorre `TagTeam_ViewCount()`; `TagView_Layout` calcula até quatro retângulos, incluindo a
faixa inferior larga no modo de três jogadores. Existem os arquivos correspondentes, não apenas uma promessa
no README. Fontes: [tag_team.c](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/src/battle/tag_team.c),
[battle.c](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/src/battle/battle.c),
[tag_view_layout.h](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/include/battle/tag_view_layout.h).
[inferido] Isso confirma a existência de uma implementação nativa; não comprova sua estabilidade no Xbox.

[lido] O fork usa **AGPL-3.0-only**, preservando os avisos MIT anteriores e os direitos dos titulares do jogo:
[README](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/README.md),
[LICENSE](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-/blob/1b56a9cf1d104f6551a33da3278dd8d424dbe3ad/LICENSE).
O Python incluído no instalador não implica um trainer em execução ao lado do PCSX2.
[inferido a partir do fluxo de instalação no README e do código C citado].

### 2.2 O próprio upstream já tem uma branch de quatro jogadores

[lido] O upstream tem cinco branches públicas: `main`, `char-packages`, `mac-ci`, `mac-setup` e **`tagteam-exp`**.
A última está em **`15407be567ffcd539b3c3be7da14f2dcada1e3ee`**, cujo commit é de **15/10/2026 às 23:40:57 UTC**
(16/10 no fuso +04:00 do autor). Ela não está integrada à `main`: o teste de ancestralidade retorna 1 e há
90 commits alcançáveis nela que não estão na `main`. A diferença desde o ancestral comum altera 97 arquivos,
com 8.512 inserções e 248 remoções. Fontes: [branches](https://api.github.com/repos/z3xox/Tenkaichi3Decomp/branches?per_page=100),
[branch fixada](https://github.com/z3xox/Tenkaichi3Decomp/tree/15407be567ffcd539b3c3be7da14f2dcada1e3ee),
[comparação](https://github.com/z3xox/Tenkaichi3Decomp/compare/main...tagteam-exp); medições locais descritas na seção 4.

[lido] Há código para até dez lutadores (`PORT_FIGHTER_MAX = 10`), equipes e Free For All, quatro controles e
quatro vistas. O desenho reutiliza duas câmeras em duas passadas; o menu oferece de um a quatro jogadores.
Fontes: [port_fighters.h](https://github.com/z3xox/Tenkaichi3Decomp/blob/15407be567ffcd539b3c3be7da14f2dcada1e3ee/port/include/port_fighters.h),
[battle.c](https://github.com/z3xox/Tenkaichi3Decomp/blob/15407be567ffcd539b3c3be7da14f2dcada1e3ee/src/battle/battle.c),
[seleção de jogadores, commit 549cf10](https://github.com/z3xox/Tenkaichi3Decomp/commit/549cf10).

[lido] A nota de implementação descreve controles 3/4, buffers DMA de 4 MB e o custo de quatro vistas, estimado
ali em aproximadamente duas vezes o desenho do split de duas. Declara não ter sido jogado com quatro controles
reais: [tagteam_local4.md](https://github.com/z3xox/Tenkaichi3Decomp/blob/15407be567ffcd539b3c3be7da14f2dcada1e3ee/docs/port/tagteam_local4.md).
Essa nota não acompanhou todas as correções posteriores: diz que os contadores e páginas de controles ainda
faltam, mas os commits [3f57bac](https://github.com/z3xox/Tenkaichi3Decomp/commit/3f57bac) e
[998179a](https://github.com/z3xox/Tenkaichi3Decomp/commit/998179a) já os acrescentam. A nota também antecede o
layout de três vistas corrigido em [045bc1d](https://github.com/z3xox/Tenkaichi3Decomp/commit/045bc1d) e
[15407be](https://github.com/z3xox/Tenkaichi3Decomp/commit/15407be).
[inferido] Serve de mapa técnico com limites históricos, não de lista atual e definitiva de defeitos.

[lido] A `main` consultada, **`7b43a75bd8b0aeabcce0a22c45e1ad66162ac9d7`**, publicada como **v0.1.39.1 em
28/10/2026**, ainda anuncia dois jogadores; não contém `port_fighters.h` nem o conjunto dessa branch.
Fontes: [README da main](https://github.com/z3xox/Tenkaichi3Decomp/blob/7b43a75bd8b0aeabcce0a22c45e1ad66162ac9d7/README.md),
[release](https://github.com/z3xox/Tenkaichi3Decomp/releases/tag/v0.1.39.1), busca local em todas as branches.
[inferido] O split-screen anunciado na release normal é o original de dois jogadores; não é evidência de F9 pronta.

[lido] A [issue 84](https://github.com/z3xox/Tenkaichi3Decomp/issues/84) continua aberta pedindo a combinação com
Tag Team; a [130](https://github.com/z3xox/Tenkaichi3Decomp/issues/130) também pede suporte a mods. Na
[74](https://github.com/z3xox/Tenkaichi3Decomp/issues/74#issuecomment-6140777577), em 13/10, o mantenedor defende
mods externos. Não apareceu PR específico de integração Tag Team entre os 50 PRs abertos/fechados consultados.
[inferido] Esses pedidos não substituem a evidência de código na branch e no fork já publicado.

[lido] Em 12/10 o mantenedor havia pedido autorização para estudar o mod sem copiar código e o autor concordou:
[issue 1 do mod PCSX2](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-/issues/1#issuecomment-6117847640).
[inferido] Essa conversa permite entender a proposta de referência; não constitui uma oferta de licença MIT
para os arquivos novos do fork nativo.

### 2.3 Outro projeto público: Pyrosea98

[lido] O projeto **BT3-TagTeam-Native** publicou **v0.4.1 em 28/10/2026 às 01:55:05**, com instalador Windows e
Power Scale BETA 8. A release aponta para `BT3-TagTeam-Runtime`, branch **`decomp-native`**, no commit
**`3094cfd05af195cbe142d4585a187249766a08b0`**. As versões antigas usavam recompilação estática BT3-Recomp;
essa versão migrou para Tenkaichi3Decomp. Fontes: [release 0.4.1](https://github.com/Pyrosea98/BT3-TagTeam-Native/releases/tag/v0.4.1),
[README do motor](https://github.com/Pyrosea98/BT3-TagTeam-Runtime/blob/3094cfd05af195cbe142d4585a187249766a08b0/README.md).

[lido] O manifesto do motor identifica a base upstream **`15407be...`**, precisamente a `tagteam-exp` acima, e
registra `OFFLINE_BUILD_PASSED_LIVE_PENDING`:
[SOURCE_REVISION.json](https://github.com/Pyrosea98/BT3-TagTeam-Runtime/blob/3094cfd05af195cbe142d4585a187249766a08b0/SOURCE_REVISION.json).
O [recibo publicado](https://github.com/Pyrosea98/BT3-TagTeam-Native/releases/download/v0.4.1/RELEASE_VERIFICATION.json)
declara testes automatizados e entrada em luta para mapas normal/2x, mas **não** novo aceite com três/quatro
controles físicos. [inferido] É outra implementação/distribuição relevante, sem comprovação humana completa.

[lido] Há diferença de licenças e de gerações: o [repositório do instalador](https://github.com/Pyrosea98/BT3-TagTeam-Native/blob/e2d751e6d136cf9862fc3afad3ad000ecb52a237/README.md)
declara **GPL-3.0-only**, inclusive derivação do Python original em `controller/`; o
[motor decomp-native](https://github.com/Pyrosea98/BT3-TagTeam-Runtime/blob/3094cfd05af195cbe142d4585a187249766a08b0/README.md)
pede preservar **AGPL-3.0**. A branch padrão do motor ainda é `tagteam-native`, da implementação anterior.
[inferido] Consultar somente a branch padrão ou a descrição curta indexada produz uma resposta desatualizada.
Não se deve tratar o conjunto atual como código permissivo ou trazer suas receitas/dados de Power Scale ao port.

### 2.4 A decompilação original

[lido] `BT3-Decompiled` tem uma branch pública, `main`, em
**`27a748bdac299e930a43580c08159cb90d91fc71`**, de **25/10/2026**. O trabalho autoral é CC0, com a ressalva de que
o código do jogo pertence aos titulares: [README](https://github.com/z3xox/BT3-Decompiled/blob/27a748bdac299e930a43580c08159cb90d91fc71/README.md),
[LICENSE](https://github.com/z3xox/BT3-Decompiled/blob/27a748bdac299e930a43580c08159cb90d91fc71/LICENSE),
[branches](https://api.github.com/repos/z3xox/BT3-Decompiled/branches?per_page=100).
[inferido] Não encontrei ali extensão Tag Team ou split de três/quatro na árvore nem nas mensagens do histórico
consultado. As ocorrências de `split` incluem divisão de arquivos/estruturas e a tela dividida original;
não indicam vários lutadores ativos. Os projetos nativos encontrados estão no port/forks, não nessa decomp matching.

## 3. Consequência para a F9, somente local

[inferido] A F9 deixa de partir apenas de um trainer MIPS: **já existem soluções em C para os problemas que vamos
enfrentar**. A decisão de esperar o jogo base continua adequada. O estudo não autoriza importar branches nem
mudar a licença do nosso port. O online permanece fora do escopo.

| Origem | O que a licença encontrada permite considerar para a F9 |
| --- | --- |
| Python/MIPS do PCSX2 | [lido] GPL-3.0-only. Traduzir não elimina a licença: a [FAQ da GNU sobre tradução entre linguagens](https://www.gnu.org/licenses/gpl-faq.html#TranslateCode) trata tradução como modificação. [inferido] Mantém-se a opção da decisão 16: referência de comportamento, sem tradução/copiar código. |
| `tagteam-exp` do upstream | [lido] O [README dessa revisão](https://github.com/z3xox/Tenkaichi3Decomp/blob/15407be567ffcd539b3c3be7da14f2dcada1e3ee/README.md) delimita MIT até `06e4671`, de 14/10, e AGPL-3.0-only depois. A mudança está em [9cdcdcb](https://github.com/z3xox/Tenkaichi3Decomp/commit/9cdcdcbdd7df9039a6d89a26c4c7db8a6fae830c). As quatro vistas foram acrescentadas depois. [inferido] A licença MIT das revisões antigas não cobre automaticamente esse código novo. |
| Fork nativo do autor e motor decomp-native do Pyrosea98 | [lido] Declaram AGPL conforme as fontes acima. [inferido] Podem ser estudados; incorporar/adaptar exigiria assumir os termos aplicáveis e examinar a procedência de cada componente. A opção conservadora aplicada aqui é **somente referência**, até uma decisão explícita do projeto sobre reaproveitamento. |

[inferido] Para a reimplementação independente em C, o estudo fornece esta lista de problemas e provas futuras:

| Parte | O que o código/relato público adianta | O que falta provar no 360 |
| --- | --- | --- |
| Participantes, entrada e alvos | Listas de atores separadas do save original, associação de cada controle ao lutador e filtros por equipe. Ver `tag_team.c` do fork e `plat_fighters.c` da branch. | Quatro XInput físicos, atribuição estável, KO, espectador, troca de alvo, menus e revanche. |
| Vistas e HUD | Há duas estratégias concretas: reutilizar duas vistas em duas passadas, ou percorrer as vistas e seus retângulos. Ver os dois `battle.c` e `tag_view_layout.h`. | Projeção, recorte, pós-processamento, HUD e cinemáticas em 2/3/4 vistas no nosso D3D9. |
| Capacidade e vida útil | A [nota de dez lutadores](https://github.com/z3xox/Tenkaichi3Decomp/blob/15407be567ffcd539b3c3be7da14f2dcada1e3ee/docs/port/tagteam_many.md) enumera tabelas de dois, filas de arquivos, pools, efeitos, objetos e limites DMA que estouraram. | Alocações e alinhamento no Xenon, endian, áudio, transformações simultâneas e limpeza após lutas repetidas. |
| Orçamento | Os buffers de listas e de modelos cresceram na branch. O número de vistas aumenta trabalho de CPU e GPU. | Medir memória, pior quadro e sessão longa a 720p/30; os números de PC não aprovam desempenho do 360. |

[inferido] Começar pelo menor cenário que cumpra quatro jogadores locais e expandir somente o necessário evita
adotar como requisito os dez lutadores, a roster modificada, os modos extras ou o online. Não há nesta pesquisa
prova de que copiar qualquer uma das soluções seria uma integração simples no nosso código atual.

## 4. Alcance e reprodução da pesquisa

[lido, execução local em 29/10] Foram feitos clones completos, **sem checkout, build ou execução de código dos
projetos**, fora do repositório, em `~/pesquisa-tagteam/`. Não foram baixados instaladores, receitas grandes,
ISO, capturas ou dados do jogo. Foram lidos seis repositórios: mod PCSX2, fork PC do autor, Tenkaichi3Decomp,
BT3-Decompiled, instalador e motor do Pyrosea98. As referências fixadas nos links preservam o estado pesquisado.

[lido, execução local] No upstream foram percorridas as cinco branches e **1.065 commits únicos em todas as
referências**, não apenas os 899 da `main`; na decomp, **382**. Consultaram-se mensagens, nomes e conteúdo dos
arquivos, além do histórico de alterações das funções de quatro vistas. Comandos equivalentes:

```sh
git clone --no-checkout https://github.com/z3xox/Tenkaichi3Decomp.git ~/pesquisa-tagteam/Tenkaichi3Decomp
cd ~/pesquisa-tagteam/Tenkaichi3Decomp
git log --all --format='%H %cI %s' -i --extended-regexp \
  --grep='tag.?team|multi.?fighter|four.player|4.player|3.player|simultaneous|split'
git ls-tree -r --name-only origin/tagteam-exp
git grep -n -i -I -E 'tag.?team|multi.?fighter|four.player|4.player|3.player|split' \
  origin/tagteam-exp -- README.md docs src include port/src port/include
git log --all -G 'TagTeam_|Port_DrawQuad|PORT_FIGHTER_MAX' -- src/battle/battle.c port/include/port_fighters.h
git merge-base --is-ancestor origin/tagteam-exp origin/main
git rev-list --count origin/main..origin/tagteam-exp
git diff --shortstat origin/main...origin/tagteam-exp
```

[lido, consulta pública] A API forneceu **201 issues/PRs** do upstream em três páginas, **50 PRs** na listagem
específica e **76 forks públicos retornados** na listagem paginada. Os metadados dos forks levaram ao fork do
próprio autor, que a busca por nomes ainda não mostrava. Foram consultados releases, tags, branches e issues dos
demais projetos; [issues do upstream](https://api.github.com/repos/z3xox/Tenkaichi3Decomp/issues?state=all&per_page=100),
[PRs](https://api.github.com/repos/z3xox/Tenkaichi3Decomp/pulls?state=all&per_page=100),
[forks](https://api.github.com/repos/z3xox/Tenkaichi3Decomp/forks?per_page=100&sort=newest).
Não foi feita auditoria de todo o código e histórico de cada um desses forks.

[lido, consulta pública] A busca web também foi usada com `Tenkaichi3Decomp tag team`, `BT3-Decompiled Tag Team`
e `BT3-TagTeam-Runtime split`. Encontrou o Pyrosea98, mas suas descrições indexadas ainda misturavam gerações;
prevaleceram os arquivos e releases atuais. A wiki Git do mod PCSX2 não pôde ser lida anonimamente (`git
ls-remote` pediu credenciais); os outros repositórios iniciais não anunciam wiki pela API. O convite Discord
`RsPFr4pkaN` do [README upstream](https://github.com/z3xox/Tenkaichi3Decomp/blob/7b43a75bd8b0aeabcce0a22c45e1ad66162ac9d7/README.md)
devolveu HTTP 403 na consulta pública. Não foram consultadas conversas privadas nem feitas postagens.

[inferido] A resposta positiva está apoiada em fontes primárias e código acessível, sem depender de Discord,
vídeo ou rumor. Não demonstra inexistência de outros forks, paridade integral com o mod PS2, aprovação humana
com quatro controles, regularidade de todos os componentes de terceiros ou desempenho no 360. Estas últimas
questões ficam abertas para a F9; esta entrega é somente pesquisa e documentação.
