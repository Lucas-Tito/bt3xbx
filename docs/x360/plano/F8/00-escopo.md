# F8: escopo do acabamento e da release

[Índice do plano](../../plano.md) | anterior: [F7](../F7/00-escopo.md) | itens anotados: [pendências](pendencias.md)

[Lido] Estudo de 29/10/2026, base `2fd53d03` (main), branch `claude/f8-escopo`. Só documentação: nada compilado, rodado
ou enviado ao console. **Escopo escrito, aguardando as decisões do Lucas (seção 6).** Segue a forma das F5 a F7, com
duas seções próprias: a release em si (4) e a abertura do repositório (5, só listada, a pedido do Lucas).

[Lido] Legenda, a mesma da F7: **[medido]** é resultado de log ou contagem identificada; **[lido]** é o que está no
código, nos documentos ou nos branches, sem prova nova; **[estimado]** é proposta, hipótese ou tamanho de trabalho. A
marca no início de uma linha ou parágrafo vale para a afirmação inteira. Arquivos citados com o nome de um branch
ainda não estão na main.

## 1. O que a F8 entrega

[Estimado] **A primeira release do bt3xbx**: um pacote que um usuário comum, com um Xbox 360 desbloqueado (RGH ou JTAG)
e a própria ISO USA SLUS-21678, instala seguindo só o README, e que joga o jogo base inteiro (F7) a 30 fps (F5), com
som e filmes (F6), avisos em inglês, queda com aviso claro, nenhum log de desenvolvimento e nenhum byte do jogo no
pacote. Junto, cada item anotado nas [pendências](pendencias.md) termina de um de três jeitos: feito, decidido fora
pelo Lucas, ou listado como problema conhecido no README.

[Lido] A ordem do plano não muda: a primeira release vem depois da F7 inteira (estimativa revista em 29/10: 216 a 300 h
de máquina em parede, [F7/02-pilotos](../F7/02-pilotos.md)), e a F5 ainda fecha o desempenho (alfa exato e prefixos em
prova, decisão 32). A F8 não espera por elas para começar: as etapas 1 a 3 e 5 da seção 3 andam antes; só a 4 (os
gaps de imagem) espera a cadeia de desenho da F5 assentar, e a 6 (o candidato) espera as duas fases fechadas.

### 1.1 Critério de fim: a release "pronta"

| Item | Critério [estimado] |
|---|---|
| Build | `.xex` Release do commit marcado, árvore limpa (o carimbo do `passos` sem diferença, `da39a3ee`), pela cadeia normal (`build_f2.sh`); todas as fontes sem erro e a lista de stubs igual à conhecida, nenhum chamado na sessão final. `default.dat` e `default.swap` da mesma build. `imagexex -dump` confere Title ID `0xFFFF4254`, nome e ícone do SPA e a ausência de `<dvdxgd2/>`. |
| Padrões do produto | Sem `environment.txt`, o jogo roda só com os padrões de `Ambiente_Carregar` (`posix360.c`). Nenhuma chave de teste ativa (desbloqueio, roteiro, captura, medida, queda de teste), conferido contra o inventário de 173 chaves (`04-chaves.tsv`, branch `teste/queda-console`). Dragon Net Battle escondido (decisão 22). |
| ISO | A convertida da versão vigente (hoje a 3) é aceita, venha da conversão no console ou do `convert_iso.py`. A original USA leva à pergunta da conversão no console (textos aprovados em 29/10). Outra versão, imagem ilegível e ausência mostram as caixas de [01-avisos](01-avisos.md) e [02-icones](02-icones.md). |
| Textos | Todo texto visível em inglês; nenhuma caixa cita `environment.txt`, chave ou caminho de desenvolvimento (pendências 3 e 9). |
| Queda | Caixa `Unexpected error`, `crash.txt` gravado, volta ao painel limpo e nenhum laço da FreeStyle, vistos na build da release (com a queda de teste ligada só num ambiente da prova, apagado depois). |
| Logs | Depois da sessão longa sem `environment.txt`, a pasta do jogo só tem o que o pacote trouxe, a ISO e `saves\` (mais `crash.txt` se houve queda). Nenhum `bt3xbx.log`, `output.txt`, medida ou captura (pendência 7). |
| Desempenho | O aceite da F5 (decisão 27; ou a exceção E da decisão 29, se o Lucas a aprovar com os números) e o da tela dividida da F7 (decisão 28), medidos no commit da release ou num commit cujo diferencial para ele não toque jogo, port, backend nem shaders, provado por `git diff` e pelos objetos. |
| Conteúdo | A F7 sem as etapas 4, 5 e 6 (os lotes de elenco, história, modos e loja ficam para depois da primeira release, decisão 35). Namek e deserto conforme a pergunta 10: corrigidos, ou no README como problemas conhecidos (o deserto à noite já é problema conhecido, pendência 5). |
| Vida do console | A sessão longa (uns 30 min, ciclos de luta e menu, Guide algumas vezes, memória estável, som sem desandar) e a saída para o painel pela Guide (pendência 11), na build da release. A sessão final da F7 vale por esta se usar o mesmo commit. |
| Pacote | Lista branca de arquivos, `SHA256SUMS.txt`, licenças; verificador que sai com 1 quando se planta um arquivo proibido numa cópia; nenhum byte do jogo. |
| Documentação | README do usuário em inglês, seguido literalmente pelo Lucas numa pasta nova do console, da cópia dos arquivos até a primeira luta, sem abrir o repositório. |

### 1.2 Fora da F8

- [Lido] A abertura do repositório (pendência 8): última parte do plano, só listada na seção 5.
- [Lido] Otimizar a GPU para subir a resolução (pendência 14): depois da primeira release, a partir da build dela.
- [Lido] As provas da F6 que o Lucas tirou do plano (pendência 10); o Tag Team de 4 (F9, decisão 16) e o online.
- [Estimado] Pela recomendação da seção 6: opções do port, remapeamento próprio, importar e exportar saves e `mods/`
  documentados para o usuário.

## 2. Inventário

[Lido] Situação em 29/10/2026. "Em branch" é o que está pronto esperando o teste do Lucas no console e o merge; todos
esses commits estão empilhados em `teste/queda-console`, que é o branch subido para o teste. Os tamanhos são
[estimados], em dias úteis de 6 h, sem filas de máquina.

| Pend. | Item | Situação | Depende de | Tamanho |
|---|---|---|---|---|
| 1 | Mensagens da ISO por caso | feito, na main ([01-avisos](01-avisos.md)) | console: aparência das caixas | na sessão da etapa 1 |
| 9 | Caixa de ISO ausente para o usuário comum, nomes em inglês | feito, na main ([02-icones](02-icones.md)) | console: aparência | na sessão da etapa 1 |
| (d. 30) | Ícones do 360 (Kenney, CC0, créditos) na conversão | feito, na main | console: os quatro usos visuais | na sessão da etapa 1 |
| (d. 26) | 16:9 por padrão (`BT3_WIDE=1` nos padrões) | feito, na main | nada | 0 |
| 3, 13 | Queda com caixa `Unexpected error` e `crash.txt` (texto aprovado) | em branch (`codex/queda-elegante-logs`, `04-queda-e-logs.md`) | console | na sessão da etapa 1 |
| 13 | Laço da FreeStyle depois de uma queda | em branch (`codex/queda-freestyle`, `F7/queda-freestyle.md`) | console: painel respondendo e reabertura | na sessão da etapa 1 |
| 7 | Logs desligados por padrão (`BT3_LOG=1` liga) | em branch (`04-queda-e-logs.md`) | console: árvore de arquivos e custo no quadro | na sessão da etapa 1 |
| 3 | Nomes em inglês (`environment.txt`, `output.txt`) | em branch (`codex/arquivos-ingles`, `05-arquivos-ingles.md`) | console | na sessão da etapa 1 |
| 2 | Conversão da ISO no próprio console (textos aprovados, releitura completa, porcentagem única) | em branch (`codex/conv-console`, `03-conversao-console.md`) | console: FATX, Guide, tempo real (o texto promete até 40 min, sem medida) | sessão de 1 a 2 h |
| (F7) | ISO versão 3 (transições de efeito convertidas; a queda do Final Flash no Broly) | em branch (`14ea9f97`, `4b68feaf`) | console: reconversão e o Broly | na sessão da etapa 1 |
| 6 | O 360 preferir a convertida quando há original e convertida | feito ([01-avisos](01-avisos.md)) | nada | 0 |
| 6 | Numeração repetida das seções das notas da F5 | por fazer | F5 fechada (não renumerar com notas abertas) | 1 a 2 h |
| 6 | Pastas temporárias do WSL | por fazer | WSL sem rodada; listar e mostrar ao Lucas antes de apagar | 1 a 2 h |
| 6 | Propor ao upstream as correções do cache (`f5-clut`, `codex/f5-clut16`) | por fazer | a abertura, ou um fork público só dessas correções (pergunta 11) | 0,5 a 1 dia |
| 4 | Preto intermitente de Namek no Shenlong | por fazer; rodada de 4 aberturas preparada ([F5/09](../F5/09.md), `f5-shenlong`, `codex/f5-namek`) | F5 com a cadeia de desenho assentada (decisão 32); console | caixa de tempo, pergunta 10 |
| 5 | Deserto à noite escuro demais | por fazer; sem captura | idem; a primeira captura | caixa de tempo, pergunta 10 |
| 11 | Sessão longa e saída pela Guide | por fazer | a build da release | 1 sessão de 1 h |
| (d. 21) | Vibração (`Pad_SendActuator` para `XInputSetState`) | por fazer, nunca implementada | pergunta 4; console | 1 a 2 dias |
| (16:9) | Seguir o ajuste de tela do painel (`XGetVideoMode().fIsWideScreen`) | por fazer ([16x9](16x9.md), etapa 4) | pergunta 3; console | 0,5 a 1 dia |
| (d. 15) | Opções do port e remapeamento | por decidir; recomendação: nada | pergunta 2 | 0, ou 3 a 6 dias |
| (d. 22) | Importar e exportar saves | por decidir; recomendação: fora | pergunta 5 | 0, ou 5 a 10 dias (o `SaveData` não está tipado) |
| novo | Auditoria dos padrões do produto | por fazer | itens em branch integrados | 0,5 dia |
| novo | Versão visível (no `crash.txt`, no log e no README) | por fazer | pergunta 7 | 2 a 4 h |
| novo | Workflow `release.yml` do upstream rodando no fork | por decidir (seção 4.3) | pergunta 8 | 1 h |
| novo | Pacote, verificador e avisos de licença | por fazer | seção 4 | 1,5 a 3 dias |
| novo | README do usuário em inglês | por fazer | perguntas 6 e 9; conversão no console integrada | 1 dia |
| 8 | Abertura do repositório | só listada | a release | seção 5 |
| 14 | GPU para resolução maior | depois da release | a build da release | fora da F8 |
| 10 | Provas da F6 fora do plano | fora do plano | nada | 0 |
| (F5) | Desempenho da luta (alfa, prefixos, zero imagens longas) | em andamento na F5 | F5 | fora da F8, mas bloqueia a 6 |
| (F7) | Conteúdo completo e tela dividida | em andamento na F7 | F7 | fora da F8, mas bloqueia a 6 |

[Lido] Achados desta leitura que entram no inventário:

- Os branches da etapa 1 mexem em código comum (`port/src/gs/gs_core.c`, `gs_draw.c`, `gs_vu1.c`), não só em
  `port/x360/`. O merge exige as provas da seção 28 de `F5/08-gravacao.md` (`.text` da x86, objetos, roteiros e
  save), que usam o WSL ou o Deck ao mesmo tempo que os lotes da F7.
- [Medido] O `release.yml` do upstream (`.github/workflows/`) roda a cada push na main do fork privado: as três
  últimas execuções levaram de 8 a 12 min cada, gastando minutos de Actions do repositório privado. Um tag `v*` cria
  um rascunho de release com os pacotes **de PC** do upstream.
- O `convert_iso.py` sozinho não basta: ele chama o `conv_tool`, que se compila dos fontes de `port/x360/conv` com
  `gcc` (`formatos/iso-convertida.md`), e a marca guarda o SHA256 do executável usado. Fora do repositório, o
  usuário precisaria dos fontes ou de um binário pronto (pergunta 6).
- O `crash.txt` traz `Iar`, `Lr` e a cadeia de retornos em endereços: sem o `.map` **da mesma build**, um relatório
  de usuário não se resolve. O `.map` e o `.pdb` da release precisam ser guardados.

## 3. Etapas e dependências

[Estimado] Um dia útil são 6 h de trabalho de quem conhece o port, com teste e correção local. Pela divisão em vigor
desde 28/10, o desenvolvimento vai para o Codex; o Claude orquestra, revisa, compila no XDK e prepara o console; o
Lucas testa no console e aprova.

| # | Onde | Entrega e dependência | Critério de saída | Trabalho [estimado] |
|---|---|---|---|---|
| 1 | Linux (XDK), console; WSL ou Deck para as regressões | **Integrar o que está em branch.** O Lucas testa `teste/queda-console` no console: queda e caixa, `crash.txt`, laço da FreeStyle, logs desligados, nomes em inglês, conversão com a original, ISO versão 3 com o Final Flash no Broly, aparência das caixas e dos ícones. Depois os merges na ordem em que estão empilhados, sem os commits "Teste no console". Pode começar já. | Cada item aprovado pelos roteiros de `04-queda-e-logs.md` e `03-conversao-console.md`; a build do 360 sem erro; as provas da seção 28 da F5 passando no código comum | 1 a 2 dias, mais uma sessão de console de 2 a 3 h e uma janela de máquina para as regressões |
| 2 | Linux e WSL; console curto | **Acabamentos pequenos** conforme as respostas: 16:9 pelo painel (pergunta 3), vibração (4), auditoria dos padrões do produto, versão visível (7), workflow do upstream (8). Depois da 1. | x86 com `.text` igual onde o upstream não muda; no console, 4:3 e 16:9 trocando o ajuste do painel, motores parados na Guide, na pausa e na queda | 1 a 4 dias, conforme as respostas |
| 3 | Linux e WSL, sem emulador | **Arrumação** (pendência 6): numeração das seções da F5 (depois da F5 fechar), pastas temporárias do WSL (listadas e mostradas antes de apagar), o pacote das correções do cache para o upstream preparado e provado na build de PC | Nada apagado sem lista aprovada; o patch aplicado sobre o upstream compila e mantém o replay do PC | 0,5 a 1,5 dia |
| 4 | Deck ou WSL, console | **Gaps de imagem** (pendências 4 e 5), depois que a F5 assentar o alfa e os prefixos (decisão 32), porque eles mudam o caminho do desenho que os dois gaps atravessam. Primeiro a captura do deserto e a rodada de 4 aberturas de Namek já preparada. | Corrigidos com a prova de imagem igual no resto (decisão 29, item 2), ou a caixa de tempo vencida com estado e hipóteses escritos e o item no README (pergunta 10) | caixa de tempo de 3 a 6 dias |
| 5 | Linux | **Pacote e README** (seção 4): script de empacotamento com lista branca, `SHA256SUMS.txt`, licenças e README em inglês; verificador visto falhando com um arquivo proibido plantado numa cópia (uma `.iso`, um `.bin` de AFS, um `environment.txt`, um arquivo acima do teto de tamanho). Pode andar junto da 2 a 4. | Dois empacotamentos do mesmo commit com o mesmo conteúdo; cada estrago sai com 1 | 1,5 a 3 dias |
| 6 | Console, Linux | **Candidato e checagem final** (seção 4.4). Depende de F5 e F7 fechadas e das etapas 1 a 5. Junta a sessão final da F7 (etapa 8 dela) e a pendência 11 numa sessão só, no mesmo commit. | Todos os itens de 1.1 | 1 a 2 dias, mais duas sessões de console: instalação do zero com a conversão (até 1 h) e sessão longa com a Guide (1 h) |
| 7 | GitHub | **Publicação** (seção 4.3): tag, release, anexos e notas | O pacote baixado da release confere com o `SHA256SUMS.txt` | 0,5 dia |
| 8 | só listada | **Abertura do repositório** (seção 5) | a planejar quando chegar | fora desta estimativa |

[Estimado] Total das etapas 1 a 7: **9 a 19 dias úteis, 54 a 114 h de trabalho**, mais uns 10 a 30 h de máquina
para as regressões dos merges (sem os lotes da F7) e quatro a cinco sessões de console. Os extremos vêm das
respostas (vibração, 16:9 pelo painel) e da caixa de tempo dos gaps. Opções, remapeamento ou importar saves, se
entrarem, somam os tamanhos do inventário.

[Estimado] Ordem: 1 primeiro (destrava o resto e só pede o Lucas no console); 2, 3 e 5 depois, em paralelo, porque
não usam emulador além das regressões; 4 quando a F5 assentar; 6 por último. Regra das máquinas: um ciclo de emulador
por vez no WSL, e as regressões da F8 entram na fila dos lotes da F7, não ao lado deles.

## 4. A release em si

### 4.1 O pacote

[Estimado] Um `.zip` com uma pasta pronta para copiar para o console:

| Arquivo | Origem | Observação |
|---|---|---|
| `default.xex` | `build_f2.sh`, Release | Title ID `0xFFFF4254`, nome `Budokai Tenkaichi 3` e ícone de arte própria (`make_icon.py`) no SPA (decisão 2) |
| `default.dat`, `default.swap` | `dat360.py`, da mesma build | Só endereços, deslocamentos, tamanhos e o hash do índice: nenhum byte do jogo. Os três arquivos andam sempre juntos |
| `README.txt` | escrito na etapa 5 | Em inglês, texto puro, para abrir em qualquer lugar (seção 4.2) |
| `LICENSE.txt`, `THIRD-PARTY.txt` | o `LICENSE` do port (MIT) e as licenças do que entra na build do 360 | A biblioteca matemática da newlib com certeza; o resto conferido pela lista de objetos da build. Créditos do Kenney (CC0) e o aviso legal do README do upstream |
| `tools/convert_iso.py` | conforme a pergunta 6 | Com os fontes do `conv_tool` ou binários prontos; ou fora do pacote |
| `SHA256SUMS.txt` | o script de empacotamento | De todos os arquivos acima |

[Estimado] Fica de fora: qualquer ISO, AFS ou arquivo extraído, `environment.txt`, `saves\`, logs, `.map`, `.pdb`. O
`.map` e o `.pdb` da build da release ficam guardados pelo Lucas (ou anexados à parte na release privada), para
resolver os `crash.txt` que chegarem.

[Lido] O `.xex` leva o código do jogo compilado a partir da decomp (`src/`), que pertence aos titulares, como os
pacotes de PC que o upstream publica pelo `release.yml`. Não leva nenhum asset. O aviso legal do README do upstream
("not affiliated", "you need your own legally obtained copy") vai também no README do usuário.

### 4.2 Versão, nome e README do usuário

[Estimado] Proposta (pergunta 7): numeração `MAJOR.MINOR.PATCH`; a primeira release completa é `1.0.0`, uma prévia
antes da F7 seria `0.9.0`. Pacote `bt3xbx-x360-1.0.0.zip`; tag `x360-v1.0.0`, de propósito fora do padrão `v*` que
dispara o `release.yml` do upstream. O nome no painel continua `Budokai Tenkaichi 3`. A versão aparece no
`crash.txt` (ao lado do carimbo do commit), no log e no README.

[Estimado] Conteúdo proposto do README (pergunta 9), em inglês e curto:

1. O que é e o que não é (port nativo; nenhum dado do jogo incluído; precisa da ISO USA SLUS-21678 do próprio
   usuário, com o MD5 e o SHA256 publicados para conferência).
2. Requisitos: console RGH ou JTAG que abre `.xex` sem assinatura; espaço livre (3 GB da ISO convertida, mais a
   original durante a conversão); a configuração testada (kernel 2.0.17150, FreeStyle 3.0.780, HD interno), dizendo
   o que não foi testado (Aurora, USB, outros kernels).
3. Instalação: copiar a pasta, copiar a ISO original para a raiz dela, abrir o jogo, aceitar a conversão, esperar o
   reinício. O caminho do PC, se a pergunta 6 o mantiver, numa seção "Advanced".
4. Atualizar: trocar os três arquivos juntos (`.xex`, `.dat`, `.swap`), manter `saves\` e a ISO; se a nova versão
   pedir reconversão, o jogo avisa. Recomendar guardar a ISO original.
5. Controles: os do jogo, com os ícones do 360; Y volta nos menus; a configuração de controle do próprio jogo.
6. Problemas conhecidos (os gaps que restarem, o 16:9 do HUD na escala atual, o que a F7 deixar nomeado).
7. Se o jogo cair: o que a caixa diz, onde fica o `crash.txt` e para onde mandá-lo (depende da pergunta 11).
8. Créditos (decomp e port de PC do z3xox, Kenney), licença e aviso legal.

### 4.3 Onde publicar

[Lido] O repositório é privado até a abertura (seção 5). [Estimado] Recomendação (pergunta 11): uma release do GitHub
no próprio repositório privado, com o `.zip` anexado, visível só a quem o Lucas der acesso; a publicação aberta a todos
acontece na abertura. Antes do primeiro tag, resolver o `release.yml` do upstream (pergunta 8): no fork privado ele
gasta minutos a cada push na main e, com um tag `v*`, criaria um rascunho com os pacotes de PC. Desligar o workflow
pelo GitHub (`gh workflow disable`) não mexe no arquivo, e o merge do upstream continua sem conflito.

### 4.4 Checagem final (etapa 6)

[Estimado] Na ordem, sobre o commit candidato; qualquer falha volta ao passo que a causou e reinicia a lista:

1. Árvore limpa, carimbo sem diferença, build do 360 sem erro, stubs iguais aos conhecidos; `imagexex -dump` com o
   Title ID, nome, ícone e mídias certas; `.dat` e `.swap` da mesma build (`dat_compara.py` contra a anterior quando
   houver).
2. x86: `.text` e objetos iguais à referência onde o upstream não muda; PPC sem mapa (`SEMMAPA=1`) e save
   (`save_mc.sh`, `save_entre.sh`) como na rodada final da F7.
3. Pacote: o empacotamento duas vezes com o mesmo conteúdo; o verificador do pacote passa e sai com 1 nas cópias
   estragadas; `SHA256SUMS.txt` confere.
4. Console, pasta nova, sem `environment.txt`, seguindo o README ao pé da letra: original USA, pergunta,
   conversão, reinício, primeira luta. Medir o tempo real da conversão e corrigir o README se passar do prometido.
5. Console: a convertida pelo PC (se a pergunta 6 a mantiver) aceita pelo mesmo `.xex`; uma ISO de outra região,
   uma truncada e a pasta sem ISO mostram as caixas certas.
6. Console: sessão longa com a Guide e saída para o painel pela Guide (pendência 11), junto da sessão final da F7.
7. Console: a queda de teste num ambiente só da prova mostra a caixa, grava o `crash.txt` e volta ao painel sem laço;
   o ambiente é apagado depois.
8. Console: a pasta do jogo depois da sessão só tem o pacote, a ISO e `saves\` (mais o `crash.txt` da prova 7).
9. Desempenho: os números de aceite da F5 e da F7 valem para este commit (mesmo commit, ou diferencial sem código de
   jogo, port, backend nem shaders).
10. Busca por bytes do jogo no pacote e no texto da release: nenhum arquivo fora da lista branca, nenhum acima do teto
    de tamanho, nenhum com assinatura de ISO 9660, AFS ou PSS.

[Estimado] Depois da 1.0, **evitar mudar a versão da marca da ISO**: cada mudança obriga todo usuário a reconverter a
partir da original. Mudanças na conversão entram antes do candidato ou esperam uma versão que justifique.

## 5. A abertura do repositório (só listada)

[Lido] A última parte do plano (pendência 8). O Lucas pediu para não planejar a fundo agora; fica a lista do que
terá de ser decidido e feito quando chegar a hora:

- Documentos em inglês: decidir o que se traduz (o README do port, o guia de instalação, o de contribuição) e o que
  fica em português como histórico (`docs/x360/` inteiro hoje está em português).
- Guia de instalação (a partir do README do usuário) e guia de contribuição (build, ferramentas, regras de prova).
- GitHub configurado para receber contribuições: modelos de issue e PR, proteção da main, Actions (o `release.yml`
  do upstream e um build próprio do 360, que depende do XDK e por isso não roda nas máquinas do GitHub).
- Revisão do histórico inteiro antes de tornar público: bytes do jogo, caminhos e dados pessoais, arquivos de
  agentes (`AGENTS.md`, `CLAUDE.md`).
- **A licença.** O fork é MIT: a base é o upstream de 10/10/2026 (`7043a3e4`, o ancestral comum com
  `upstream/main`). O upstream passou a AGPL-3.0-only depois de 14/10 (`06e4671` é o último MIT; a mudança está em
  `9cdcdcb`, [pesquisa/05-tag-team](../../pesquisa/05-tag-team.md)). Um `git merge upstream/main` com código posterior
  torna o conjunto AGPL. Até a abertura, nenhum merge do upstream sem essa decisão; as correções que mandarmos para
  lá (pendência 6) são MIT e entram no AGPL deles sem problema.

## 6. Decisões para o Lucas

**Respondidas em 29/10/2026: ver a decisão 33** (`decisoes.md`). Em resumo: release única `1.0.0` junto com a abertura
do repositório, pacote `bt3-x360-1.0.0.zip`, vibração dentro, as duas conversões no README, só o deserto entre os gaps
(Namek fica para depois), opções, remapeamento, 16:9 pelo painel e saves fora da primeira release, relatórios de queda
nas issues do GitHub com regras contra spam. As opções abaixo ficam como registro do que foi perguntado.

[Estimado] Só perguntas que mudam trabalho. Cada uma tem a recomendada e a conservadora; sem resposta, o escopo segue
a conservadora.

1. **Uma prévia antes da F7 completa?** (a) **Recomendada:** sim, uma `0.9.0` privada para poucos convidados quando a
   F5 fechar e as etapas 1, 2 e 5 estiverem prontas, com o README dizendo o que da F7 falta; ela testa a instalação em
   consoles, kernels e painéis que não são o do Lucas, enquanto a F7 roda nos lotes. (b) **Conservadora:** nenhuma
   prévia; uma release só, depois da F7, como no plano.
2. **Opções do port e remapeamento na primeira release?** (a) **Recomendada e conservadora:** nada; padrões fixos e a
   configuração de controle do próprio jogo. (b) Um arquivo de opções documentado (16:9, vibração), 1 a 2 dias.
   (c) Uma tela de opções e remapeamento próprios, 3 a 6 dias e mais prova de console.
3. **16:9 seguindo o ajuste de tela do painel?** (a) **Recomendada:** sim, `XGetVideoMode().fIsWideScreen` como
   padrão, como a decisão 26 (item 4) já previa: TV marcada 4:3 recebe 4:3. (b) **Conservadora:** 16:9 fixo para
   todos, como hoje.
4. **Vibração?** (a) **Recomendada:** entra; o jogo original vibra, o trabalho é pequeno, e os motores param na
   Guide, na pausa e na queda. (b) **Conservadora:** fica fora e vai para os problemas conhecidos.
5. **Importar e exportar saves (decisão 22)?** (a) **Recomendada e conservadora:** fora da primeira release; volta
   se alguém pedir. (b) Entrar agora, 5 a 10 dias, porque exige tipar o `SaveData` de 0x4000 bytes.
6. **Caminho de conversão no pacote.** (a) **Recomendada:** a conversão no console é o caminho do README; o pacote
   leva o `convert_iso.py` com os fontes do `conv_tool` numa seção "Advanced" para Linux ou WSL com `gcc`.
   (b) **Conservadora:** só a conversão no console; o script fica no repositório até a abertura. (c) Binários prontos
   do `conv_tool` para Windows e Linux, 1 a 2 dias, e cada binário muda os 64 bytes de procedência da marca.
7. **Nome e versão.** (a) **Recomendada:** `1.0.0` na primeira completa (`0.9.0` numa prévia), pacote
   `bt3xbx-x360-1.0.0.zip`, tag `x360-v1.0.0`, nome no painel `Budokai Tenkaichi 3`. (b) **Conservadora:** a mesma
   numeração, com o Lucas escolhendo só o nome do pacote.
8. **O `release.yml` do upstream no fork.** (a) **Recomendada:** desligar o workflow no GitHub agora, sem mexer no
   arquivo. (b) **Conservadora:** deixar como está e só usar tags fora de `v*`.
9. **O que entra no README do usuário.** (a) **Recomendada:** os oito pontos da seção 4.2. (b) **Conservadora:** só
   requisitos, instalação, atualização e aviso legal.
10. **Os gaps de Namek e do deserto.** (a) **Recomendada:** caixa de tempo de 3 a 6 dias para os dois, depois que a
    F5 assentar a cadeia de desenho; o que não fechar vai para os problemas conhecidos do README e para depois da
    release. (b) **Conservadora:** bloqueiam a release até corrigir, como a F7 registrou (seção 6 dela).
11. **Onde publicar e para onde vão os relatórios de queda.** (a) **Recomendada:** release do GitHub no repositório
    privado, acesso a convidados, relatórios por um canal que o Lucas escolher; as correções do cache vão ao upstream
    na abertura. (b) **Conservadora:** nenhum convidado; o pacote fica com o Lucas até a abertura.

## 7. Riscos e como aparecem cedo

| Risco | Como aparece cedo | Encaminhamento [estimado] |
|---|---|---|
| [Lido] A F7 longa empurra a release por semanas | O ritmo dos lotes no Deck e no WSL | A prévia da pergunta 1; as etapas 1 a 3 e 5 da F8 não esperam a F7 |
| [Lido] Os merges da etapa 1 tocam `port/src/gs` | `git diff --stat` fora de `port/x360` | Regressões na fila dos lotes da F7, uma janela marcada |
| [Estimado] A conversão no console demora mais que os 40 min prometidos, ou falta espaço | A primeira conversão real na sessão da etapa 1 | Corrigir o texto (aprovado pelo Lucas) ou o README antes do candidato |
| [Estimado] Consoles, kernels ou painéis diferentes (Aurora, USB) se comportam diferente | A prévia, se houver | README diz o que foi testado; o resto é relatado e corrigido na 1.0.x |
| [Lido] Um `crash.txt` de usuário sem o `.map` não se resolve | Ao receber o primeiro | `.map` e `.pdb` guardados por release |
| [Lido] Mudança na marca da ISO depois da 1.0 obriga todos a reconverter | Diff da conversão contra o tag | Congelar a conversão no candidato |
| [Lido] Os gaps de imagem podem não fechar | O fim da caixa de tempo | Pergunta 10 |
| [Lido] O workflow do upstream gasta minutos e um tag `v*` cria release de PC | Já medido: 8 a 12 min por push | Pergunta 8 |
