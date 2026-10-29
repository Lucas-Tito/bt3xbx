# F7: escopo, redes de cobertura e critério

[Índice do plano](../../plano.md) | anterior: [F6](../F6/00-escopo.md)

[Lido] Estudo de 28/10/2026, base `c57e807f`, branch `codex/f7-escopo`. **Escopo aprovado em 28/10 (decisão 28; as
respostas estão no fim).** O estudo inicial só alterou documentação: não executou jogo, emulador, build ou console. As
sete seções seguem o modelo da F6.

[Medido] **Etapa 1, parte sem emulador, concluída em 28/10:** [manifesto e denominadores](01.md), gerados tanto
da árvore original quanto da ISO convertida, iguais por `cmp`; verificador de diferenças nominais com negativos;
auditoria de um instantâneo parcial da F6, reprovada por ausências sem justificativa; três pilotos escritos,
**ainda sem executar naquela entrega**, por determinação da tarefa. Em 29/10, os [pilotos no Deck](02-pilotos.md)
aprovaram Missão 0, marco 29/30 e Episódio 0; a transformação foi executada e reprovou na memória.
A reserva revisada é 360 a 500 h de máquina serializada. Não altera o aceite do console nem fecha a F7.

[Medido] **Etapa 2, parte x86, feita em 28/10:** [receitas de cartão nativo e verificador de save](02.md): 14
receitas, 11 marcos gerados pelo fluxo do jogo, salvos, reabertos pela carga normal e conferidos, com negativos
recusados; a PPC fica especificada, sem rodar.

[Medido] **Etapa 4, plano e primeira família, 29/10:** [plano por famílias, lote retomável e censo de carga do
elenco](04.md): 35 casos de luta em time carregam os 349 pares id/traje, os rostos e as vozes dos dois lados e 33
estágios; a x86 percorreu os 35, e a rodada com todas as redes está armada no WSL e no Deck.

[Lido] Legenda: **[medido]** é contagem nos dados ou resultado de log identificado, inclusive de uma rodada
anterior; **[lido]** é comportamento encontrado em fonte ou documento, sem prova nova de execução;
**[estimado]** é proposta de trabalho, hipótese ou limite de aceite ainda sujeito à aprovação. A marca no início
de uma linha ou parágrafo vale para a afirmação inteira. Medição de arquivo não equivale a medição no 360.

## 1. Escopo e ponto de partida

[Estimado] A entrega proposta é **todo o conteúdo do BT3 USA SLUS-21678 e a tela dividida local de dois jogadores
do jogo base**: entrar, jogar, obter o resultado, salvar, reiniciar e continuar. Não basta abrir os menus.

[Lido] `MainMenu_Input` (`src/menu/menu_a.c`) e `Progress_Main` (`menu_a_b.c`) despacham História, Ultimate Battle,
Torneio, Duelo, Evolution Z, Treino, Data Center, Referência, Opções e Shenlong. Ultimate Battle contém Sim Dragon,
Missão 100, Sobrevivência e Disc Fusion. Título, abertura, final, resultados, recompensas, aulas, loja, senhas,
customizações e replays também têm fluxos próprios. Dragon Net Battle está oculto no 360 pela decisão 22. Modos,
personagens ou cenários de `mods/`, rede e Tag Team não entram: decisão 24 e F9/decisão 16. O Disc Fusion já recebeu
a adaptação local que libera os dois discos pelo X na placa, provada na F4; não se propõe pedir discos adicionais.

### 1.1 O que os roteiros existentes alcançam

[Lido] Os arquivos estão em `port/x360/roteiros/`. `roteiros.sh todos` lista 17 roteiros; somando `f5_duelo`,
`f5_treino`, `f6_final` e `f6_final_start`, a rodada final da F6 programa **21** (`ROTEIROS` do
`f6_final_confere.py` no commit `a989c9b5`). `save_*` e `boot*` são auxiliares, não outros modos completos.

[Medido] Corte da rodada alheia: **28/10 às 09:43:41, UTC-3**, em `/home/codex/f6-final-a989c9b5`, somente leitura.
`origem.json` identifica `a989c9b534954a9416762eaeb03ecd332eae914b`, base `4c85c039`. O instantâneo de `casos.json`
tem nove roteiros concluídos com saída 0; `ultimate` tinha log em formação e ainda não tinha resultado final.
Os outros onze não tinham resultado registrado. Não se atribuem esses testes a `c57e807f` ou ao commit deste estudo.
Os números de imagens abaixo vêm dos `imagens_compara.txt` dos nove concluídos.

| Família e roteiros | Caminho já descrito no roteiro [lido] | Prova disponível no corte e limite |
|---|---|---|
| Duelo: `duelo`, `duelo_time` | 1P contra COM, seleção simples e time de dois contra um, retratos, cenário, início da luta | [Medido] F6: ambos saída 0, 9/9 e 7/7 imagens x86/PPC iguais. Não há segundo jogador ativo, troca de membro comandada nem fim de time completo nesses dois roteiros. |
| Combate: `f5_duelo`, `f5_treino` | Goku (Early) e Kuririn, traje 0, ações conferidas, pausa, janelas, resultado no Duelo e opções do Treino | [Medido] F6: saída 0, 13/13 e 11/11 imagens iguais. [Lido] Pela decisão 24, a dupla foi escolhida sem transformação/fusão; não cobre esses caminhos. |
| Treino: `treino`; Opções: `opcoes` | Primeira aula, seis marcas de página, treino livre até lutar; opções e gravação pelo McFlow | [Medido] F6: saída 0, 13/13 e 11/11 imagens iguais. Não cobre as 398 páginas, todas as aulas ou opções independentes do controle 2. |
| História: `historia`, `historia_novo`, `historia_perde` | Primeiro episódio: vitória com tudo aberto, fim da saga 0 e save; New Game sem desbloqueio, ações de evento 10001/10002, recompensa de episódio; derrota e retorno | [Medido] F6: saída 0, 14/14, 14/14 e 10/10 imagens iguais. [Lido] `termina luta` dá dano pelo caminho do jogo; não prova vencer jogando, outros episódios ou todos os comandos dos roteiros de evento. |
| Ultimate: `ultimate` | Visita seleção de missão/time, Sobrevivência, placa Disc Fusion e tabuleiro Sim Dragon | [Medido] F6: em andamento no corte, sem aceite. [Lido] O roteiro termina no tabuleiro, sem resultados desses modos. |
| Missão: `missao` | Missão 0, dois membros contra quatro, vitória dada, resultado e byte de missão cumprida | [Lido] Existe e passou na rodada histórica da F4. Sem resultado F6 no corte. Não prova as outras 99 missões, derrotas ou reabertura do cartão avançado. |
| Sobrevivência: `sobrevivencia` | Curso 0: três adversários derrubados, derrota do jogador, recorde de três | [Lido] F4 registrou sucesso. Sem resultado F6 no corte. Não chega aos 50 adversários nem aos outros dois cursos. |
| Sim Dragon: `simdragon` | Dez turnos com treino/descanso, anúncio, vitória dada, continuar no tabuleiro | [Lido] F4 registrou sucesso. Sem resultado F6 no corte. Não prova todos os eventos, continuação depois de reiniciar ou término da campanha. |
| Disc Fusion: `discfusion` | Dois ramos: sobe do lugar 100 ao 99 e vence o curso 0 contra cinco adversários | [Lido] F4 registrou sucesso. Sem resultado F6 no corte. `funde discos` prepara flags de sessão, não prova o X real da adaptação nem todos os cursos/lugares. |
| Torneio: `torneio` | Torneio de treino, inscrição, primeira vitória dada e chave atualizada | [Lido] F4 registrou sucesso. Sem resultado F6 no corte. Não chega a campeão, prêmio, eliminação, demais tipos ou confronto humano/humano. |
| Evolution Z: `evolution` | Retrato, senha exibida, troca de 100 pontos, coleção/ajuda e DcSave | [Lido] F4 registrou sucesso. Sem resultado F6 no corte. Não compra item novo com estoque limitado, equipa todos os tipos ou prova efeitos na luta. |
| Data Center: `datacenter` | Digita senha válida, grava registro 0, lista/detalhes, abre lista de replay vazia, DcSave | [Lido] F4 registrou sucesso. Sem resultado F6 no corte. Não grava/carrega/reproduz replay nem preenche os 14 registros. |
| Referência: `referencia` | Lista, perfil, modelo de Goku (Early) traje 0 aberto e girado | [Lido] F4/F5 documentam as provas. Sem resultado F6 no corte. Não cobre vozes/perfis e modelos de todo o elenco. |
| Shenlong: `shenlong` | Sorteio do dragão 1, três desejos de 5.000, ShenSave e esferas consumidas | [Lido] F4/F5 documentam as provas. Sem resultado F6 no corte. O preparo zera dinheiro; não fixa o sorteio nem exercita os outros dragões ou recompensas inéditas. |
| Filmes: `f6_final`, `f6_final_start` | Final pela seleção de sagas; segunda variante pula a abertura com START | [Lido] Sem resultado F6 no corte. O WSL usa `BT3_NOMOVIE=1`: prova pedido/retorno pelo estado de som, não exibição ou escuta dos filmes. |

[Lido] A rodada completa anterior está em [F4/10](../F4/10.md): 17 roteiros, 228 imagens iguais, save e memória
por tela, sem queda; é prova daquele commit (`68d6c0c`), anterior ao desenho e ao áudio finais. O histórico
`72aafc6c` (vitória nova/derrota da História) e `f41c5733` (quatro modos Ultimate) mostra o modelo a repetir:
um fluxo pequeno, resultado concreto e campo persistente conferido, sem chamar uma visita de modo completo.

[Medido] No corte F6 também há `--disco` com **3.325 arquivos convertidos, zero erros** e os dois filmes com
2.810/4.200 hashes x86/PPC iguais. Há reprovação registrada no teste do leitor ISO (constantes de erro ausentes
no teste de host), comparação independente com `mpeg2test` ausente e uma primeira chamada do teste de filme com
saída 126, depois refeita com `bash` e saída 0. Portanto a rodada F6 inteira **não estava aprovada**. Esses casos
pertencem à outra tarefa; não foram corrigidos nem repetidos aqui.

### 1.2 Inventário, arquivos não exercitados e save avançado

[Medido] `python3 port/x360/tools/unpacks.py --lista` nesta base: **82 chamadas em 62 funções**, todas com gancho,
zero pendências/erros, mais um `Bpe_Decode` direto com `Port_ConvertAnim`. Isso prova a ligação estática aos
conversores; não prova execução dos 82 sítios ou todos os arquivos que cada sítio pode abrir.

[Medido] `roteiros.py soma` sobre os dez `gdb.txt` já presentes na F6, usando o binário x86 daquela rodada,
encontrou **43/82 chamadas**, incluindo observações do `ultimate` ainda parcial. Limitando aos nove roteiros
concluídos, são **31/82**. As 39 chamadas ainda não observadas no conjunto parcial ficam abaixo; isso não quer
dizer que os outros roteiros existentes nunca as alcancem. A diferença entre os dois conjuntos é o Ultimate.

| Chamadas ainda sem observação no corte parcial [medido] | Conteúdo do inventário [lido] | Roteiro existente que se propõe conferir [lido] |
|---|---|---|
| `menu_h_b.c:49,125,861` | 463 s1 e retratos | `referencia` |
| `menu_i_d.c:98,172,346`; `menu_j_b.c:59,123`; `menu_k_d.c:24`; `menu_k_f.c:31,811,818,848,855` | 450 s1/s2, convite, fundo, chave e retratos | `torneio` |
| `menu_n_b.c:98`; `menu_n_c.c:140,720,727`; `menu_n_d.c:53` | cursos, ranking, resultado e retratos | `discfusion` |
| `menu_p_b.c:67`; `menu_q_b.c:1976,1983`; `menu_r_c.c:38`; `menu_s_c.c:30` | resultado Missão/Sim/Sobrevivência e retratos do anúncio | `missao`, `simdragon`, `sobrevivencia` |
| `menu_u_g.c:60`; `menu_u_h.c:542,550`; `menu_v_b.c:759`; `menu_v_d.c:146`; `menu_w_c.c:133` | 452 s1/s2/s3, retratos e estágio | `evolution` |
| `menu_y_b.c:957,1008,1020`; `menu_z_c.c:211`; `menu_z_d.c:535,958`; `menu_za_d.c:254` | 459 s1/s2/s3/s4 e retratos | `datacenter` |
| `late_a.c:555`; `late_a_c.c:52` | 472 s2, lista e save | `shenlong` |

[Lido] **A saída 0 de `soma` não exige 82/82.** O código só reprova pendências do essencial não alcançadas; depois
de todos os ganchos instalados esse conjunto é vazio. Ele imprime a quantidade das chamadas com gancho, mas não
lista as ausentes nem reprova essa diferença. Para a F7, exigir separadamente a diferença nominal entre
`unpacks.levantar()` e as chamadas observadas, com cada ausência explicada; o controle negativo deve retirar uma
chamada observada. Nenhuma chamada foi declarada inalcançável em todo o jogo com base nessa soma parcial.

[Lido] Conteúdos **sem chamada no código**, distintos dos sítios ainda sem observação: `00462.bin` contém três
seções vazias; `00472.bin` s1 e `00473.bin` s1 são `menu_inativo` em `conv.c`. Nenhum roteiro de entrada pode
exercitar um leitor inexistente. Preservar a distinção e a cobertura estrutural do `--disco`, sem inventar fluxo.

[Medido] A união dos `lidos.txt` dos nove concluídos tem 469 nomes; dos 3.325 arquivos convertidos pelo `--disco`,
**3.252 não aparecem nessa união**. Os 469 incluem outras partições, especialmente som. Nos intervalos abaixo a
contagem é de nomes de arquivo, não de cenas distintas, imagens renderizadas ou comandos executados:

| Faixa da partição 1 | Lidos nos nove [medido] | Significado e lacuna [lido] |
|---|---:|---|
| 01423..03032 | 13/1.610 | Slots de modelos/variantes e dois arquivos auxiliares por id; há slots vazios. Outros trajes e formas não foram carregados nessa amostra. |
| 00368..00445 | 3/78 | Cenários normais, divididos e fundos 3D; dentro de 00407..00442, **0/36**. Nenhum dos 21 roteiros atuais seleciona uma luta local de dois humanos. |
| 00510..00559 | 1/50 | Roteiros de evento da História. Os leitores de menu escolhem scripts 0..47; não confundir os 50 arquivos com 50 episódios jogáveis. |
| 00560..00659; 00660..00759 | 1/100; 1/100 | Textos e bocas das duas séries de voz. Não se executaram todas as cenas nem a outra série de todas elas. |
| 00760..00923 | 8/164 | Retratos de 161 ids e três marcadores. Exemplo ausente: 00761. |
| 00924..00959 | 7/36 | Miniaturas de 35 estágios e aleatório. Exemplo ausente: 00925. |
| 01025..01422 | 5/398 | Páginas de aula. Seis marcas no roteiro não significam seis arquivos diferentes. Exemplo ausente: 01028. |

[Lido] `--disco` valida todos os bytes das faixas reconhecidas e também o conteúdo BPE, mesmo sem roteiro de
controle. O censo F6 registrou 30/30 arquivos de menu e 49/49 conteúdos BPE, 164 retratos, 36 miniaturas,
dez convites/fundos, 52 imagens de episódio/resultado e 398 páginas, todos convertidos. Não valida o uso desses
números pela lógica, uma transformação, o desenho no D3D ou a sequência de save. A F7 cruza três conjuntos:
**arquivo convertido, chamada observada, comportamento concluído**. O que nenhum roteiro atual prova inclui
tela dividida, transformações/fusões comandadas, todos os episódios, replays não vazios e conclusão dos cursos.

[Medido] Leitura curta de `/srv/bt3/dados/gamedata/pzs3us1/00003.bin`, seção 1, registros `ChrTblEntry` de 0x3C,
campo `costumes` em +0x0A, e cabeçalhos dos modelos: **161 ids, 349 pares id/traje, 698 modelos com as duas
variantes**. Todos forneceram 0x100 bytes na entrada 0x12 de parâmetros. Há 76 ids com alvo de transformação e
oito com resultado de fusão: **101 relações distintas (id, slot, alvo) de transformação e 20 de fusão**,
ignorando 0xFF e deduplicando roupa/variante. São relações de dados, não 121 ações comprovadas nem contagem de
personagens base. Os tipos de transformação presentes são 0,1,2,3,4,7,8,9,10,11.

[Lido] Reprodução: índice de modelo `id*10 + traje + 0x590 + variante*4 - 1`; na entrada 0x12, alvos +0x98,
tipos +0xA4, resultados de fusão +0xB4 e parceiros +0xBA (`btl_tech_a.h`, `btl_obj.c`, `btl_act_j.c`).
`transCost`, `fusionCost`, membros do time, lua, permissão e bits disponíveis do lado condicionam as ações.
As 101/20 relações precisam virar casos executáveis ou exclusões justificadas pelo leitor, preservando a lista
original. Ids do save 0..160 incluem formas; 161..164 da grade são marcadores/registros, não lutadores novos.
`SAVE_STAGE_COUNT=35` e `CHARSEL_STAGE_RANDOM=0x23` delimitam os 35 estágios reais; 36 miniaturas não são 36 arenas.

[Lido] O save é um bloco de **0x4000** (`include/sys/save.h`), com campos ainda opacos: nove slots de História,
161 bits de personagens, 35 de estágios, 350 itens, 97 customizações com três conjuntos, 14 registros de senha,
opções/chaves de dois controles, dinheiro, missões, recordes, cursos e flags de loja/torneio. O ReplayMenu
(`menu_za_d.c`) usa ainda **sete arquivos de replay do cartão**, com gravação pelo McFlow modo 5 e carga pelo 6.
Save completo inclui esse cartão e a persistência dos modos, não apenas os bits de personagem.

[Lido] `Save_UnlockAll` não é uma partida 100%: põe os bits dos episódios e libera itens, mas esvazia os registros
de senha, não gera replay e não prova o ganho de nenhuma recompensa. O preparo do roteiro acrescenta liberações
de teste (por exemplo, acesso à Sobrevivência); não substitui progressão. `historia_novo` só ganha a recompensa
do primeiro episódio. O final acessível com tudo liberado não prova chegar a ele pela campanha.

[Estimado] Estados avançados necessários: antes da última vitória de cada saga/ramo e do final; recompensa
inédita de personagem, estágio, item e episódio; 29/30 missões antes de abrir Sobrevivência; última missão;
fim de cada curso de 50; ranking perto do topo e fim dos cinco cursos Disc Fusion; fases finais dos torneios;
Sim Dragon perto de seu encerramento; loja com estoque bloqueado/liberado; desejo ainda disponível em cada dragão;
cartão com 14 registros e sete replays. A etapa 1 deve ligar cada estado a campos e condições do código, sem
preencher regiões desconhecidas com 0xFF.

### 1.3 Duas vistas e o segundo controle

[Lido] A decisão está em `BattleSetup_SetRule`/`Battle_IsSplitScreen` (`battle_load.c`): `rule.screenMode == 1`.
`BattleSetup_FixForMode` só mantém isso nos modos de batalha 0 (Duelo) e 4 (Torneio). `CharSel_Run` escolhe pelo
número de jogadores; `TeamSel` usa `TEAMSEL_PLAYERS_TWO`; `Bracket_SetupBattle` liga controle 0 ao lado 0 e
controle 1 ao lado 1 quando os dois inscritos são humanos. Duelo simples, time e torneio entre humanos precisam
de casos próprios, além de cancelamento, revanche, escolha de cenário e inscrição dos participantes.

[Lido] `BtlCam_InitViews` e `View_InitLayout` (`btl_cam.c`) montam metades **esquerda/direita**: tesouras x=0..254
e 257..511, y=0..447. `Battle_Update` atualiza as duas câmeras e a visibilidade por vista; prioridade de câmera
(`BtlCam_UpdateOverride`) pode voltar a uma vista cheia nas sequências especiais. `Battle_Loop` escolhe
`Battle_DrawSplit` ou `Battle_Draw`, mantendo `Gfx_EndFrame(2)`. No dividido, cenário/culling e
`BtlScene_Draw`/`Ot_Draw` passam pelas duas vistas; `BtlObjDraw_Draw`, efeitos e HUD têm organização própria,
não duas chamadas da função inteira. Testar também transição dividida/cheia/dividida de golpe e transformação.

[Lido] `BtlLoad_StepInit` e a troca de cenário carregam `0x198 + estágio` em tela dividida, no lugar de
`0x171 + estágio`: o disco tem modelos mais leves para esse caso. O custo não se deduz só da metade da área.
Alfa, profundidade, contorno, cópia de ids, brilho, HUD e 16:9 ainda passam pelo backend e suas heurísticas
(`gs_draw.c`, `gs_d3d.cpp`); conferir isolamento das metades, tesouras, alvos persistentes e liberação dos recursos.

[Lido] O XInput já tem duas entradas em `port/x360/src/f2/pad360.c`. A porta lógica 2 é `socket=1`, não quadrante
fixo 2: decisão 21, primeiro controle ligado para jogador 1, próximo livre para jogador 2, quadrante reservado
mesmo desconectado. `Pad_Update` lê os dois; `scePad2GetState` guarda a leitura e `scePad2Read` a converte.
Com `BT3_ROTEIRO`, porém, o XInput é desviado, ambos contam como conectados e `Roteiro_Pad` só fornece entrada
para `socket=0`. Será preciso estender o teste com comandos/estado de conexão por jogador e relógio único por
quadro, sem avançar o roteiro duas vezes. Depois, testar fisicamente sem `BT3_ROTEIRO`.

[Medido, resultado anterior] [F5/08-gravacao, seção 29](../F5/08-gravacao.md#29-a-rodada-dos-cortes-de-2710-duas-imagens-longas-e-a-cadeia-até-o-prazo)
registra duas imagens de três vblanks na base e nos dois cortes juntos no Duelo, durante mudanças de cena dentro
do combate. No pior alvo, a cadeia entrega/gravação/desenho/fim GPU leva cerca de 42,4 ms para 32,8 ms de prazo;
a GPU isolada não excede o período. Não eram texturas novas. O zero da decisão 27 continua reprovado.

[Estimado] A recuperação proposta na F5 é de pelo menos 10,6 ms nessa cadeia, incluindo 1 ms de margem.
**Não é folga disponível para a F7.** Duas vistas repetem parte da geometria/preparo, mas podem usar modelos
menores e menos pixels por vista; som, lógica, HUD e algumas passadas são compartilhados. Dobrar o tempo total
da F5 ou dividi-lo por dois seria previsão sem medida. Medir por vista os lotes/vértices, gravação, alvos,
resolves, GPU, fim da cadeia e histograma da tela; comparar simples/dividido na mesma luta e configuração.

[Lido] A seção 30 da F5 abriu a classe comum por shader/alvo e contou cobertura nas listas x86gv, inclusive
duas cadeias de brilho. Área sobreposta não é escrita descartável, e reconstrução x86gv não é tempo no Xenos.
Essas ferramentas ajudam a localizar custo, mas não há ganho novo de tela dividida medido. Manter 720p de saída,
escala 1,33, 16:9 e efeitos dos padrões atuais (decisões 20/26), até decisão específica do Lucas.

### 1.4 Dependências herdadas

| Dependência | Antes de qual entrega da F7? | Pode correr junto? |
|---|---|---|
| [Lido] F5: teto de memória, ISO convertida, GPU/VU1/efeitos e cache corretos | Base das correções de conteúdo e do desenho dividido | [Estimado] Inventário, receitas de save e roteiros podem adiantar; integrar sobre base comum antes das provas. |
| [Lido] F5, etapas 8/10: decisão 27, três janelas e sessão livre, mínimo de 32 MiB físicos livres | Aceite de desempenho F7 e comparação simples/dividido | [Estimado] Medir cedo o dividido sem chamá-lo aprovado; não somar otimização concorrente ao orçamento sem repetir a base. |
| [Lido] F6, etapas 7/8/9: filme com som, deriva, Guide, escuta e rodada final | Aceite completo de modos, cenas e estabilidade | [Estimado] Rastro lógico e dados avançados podem adiantar. Aceite do som no dividido e final da campanha exige a F6 integrada e conferida. |
| [Lido] F8/pendencias, itens 4/5: preto intermitente de Namek e deserto noturno escuro | Continuam no fechamento de gaps no fim do plano por ordem do Lucas | [Estimado] Registrá-los nas capturas e tabela, sem travar a F7 nem declará-los corrigidos. Defeito novo de conteúdo ou metade faltando não herda essa dispensa. |
| [Lido] Importar/exportar saves, acabamento de vídeo/remapeamento, avisos e abertura do repositório | F8; quatro jogadores, F9 | [Estimado] Não são pré-requisitos para montar cartões de teste nativos nem para dois controles com o mapeamento atual. |

## 2. Redes de cobertura

[Estimado] Cada resultado terá commit, ambiente, hash da receita/cartão, roteiro, ids e limites explícitos. A F7
deve fechar o inventário de caminhos; não tentar o produto cartesiano de personagens, roupas, cenários e itens.

1. [Estimado] **Inventário estático e disco (WSL ou Deck).** `unpacks.py --lista`, tabela `sFaixas` de `conv.c`,
   `conv_tool --disco` e comparação da ISO convertida. Acrescentar manifesto textual de modo/episódio/personagem/
   traje/variante/transformação/fusão/estágio e condição de desbloqueio, com origem no leitor. Conferir a diferença
   nominal das chamadas contra `roteiros.py soma` e dos arquivos contra `LEU`; listar cada pendência mesmo que
   `soma` saia 0. Slots vazios/inativos separados, com prova do leitor. Nenhum arquivo de bytes do jogo versionado.
2. [Estimado] **Fluxos repetíveis (WSL x86 e PPC sem mapa).** Dois rastros x86 iguais e iguais à referência;
   PPC com `SEMMAPA=1`, arquivos, estado da luta, ações de evento, memória viva e save equivalentes. Usar
   `roteiros.sh`, `roteiros.py`, `compara.py`, `memdiff.py`/`memoria_luta.py`, imagens `IMG=1` ou `IMG=luta`
   conforme o ponto e som `SOM=1`. Criar casos `f7_*` por fluxo da seção 3 e suporte aos dois controles no teste.
   `termina luta` serve a menus/resultados; transformações, fusões, troca de membro e golpes devem ser observados
   pelo estado real após comando, não pelo nome do passo. Um caso com ação forçada não recebe crédito de execução
   por entrada. A guarda de conversão e stubs chamados precisam de contagem explícita e zero ocorrências novas.
3. [Estimado] **Cartões avançados nativos e progressão.** Versionar receitas e manifestos de campos, sem saves,
   dumps, senhas extraídas ou replays do usuário. Gerar fora do git, em `~/f7/...` ou `port/build/`, partindo de
   `Save_SetDefaults` e setters já identificados, em código de teste. Preparar estado imediatamente anterior ao
   evento; o jogo ganha, aplica recompensa e grava pelo McFlow. Reiniciar pela carga normal e conferir os valores.
   Gerar x86 e PPC separadamente na ordem nativa, sem transformar cartão PC em cartão 360. Para campos opacos,
   obter o estado pelo próprio fluxo ou tipar o leitor antes de editar. Registrar semente, commit, receita e hashes.
   `save_mc.sh` e `save_entre.sh` continuam obrigatórios, ampliados aos registros/replays que o teste novo usar.
4. [Estimado] **Imagens, listas e recursos.** Rasterizador x86/PPC byte a byte nos pontos ancorados, memória viva
   antes/depois de carga, transformação/fusão, mudança de arena e troca de time. Lista x86gv/PPC e verificadores
   de divergência nas otimizações, com `BT3_SCALE=1` ao reconstruir área para o backend falso. No dividido, exigir
   conteúdo nas duas metades, duas câmeras distintas e retorno correto de uma vista cheia. Zero diferença viva
   inexplicada, tolerância numérica herdada só nos casos nomeados do verificador.
5. [Estimado] **Console e sessão com Lucas.** Build XDK e revisão no Linux, mídia convertida e cartão de teste
   isolado. Capturas por vista/cena, input real de dois controles, mistura/vozes, Guide, desconexão, troca de
   quadrante, gravação/retomada, memória e tempo. Só o console prova Xenon/Xenos, latência física, falta de dado
   do XAudio2, apresentação a 30 fps e percepção das metades/HUD. O float de hardware pode divergir do replay
   x86/PPC; não prometer hash da luta do 360 idêntico ao softfloat.

[Estimado] Todo verificador novo será visto falhando em cópia: retirar chamada/arquivo/caso do manifesto, mudar
id/traço de transformação, deixar a porta 2 neutra, suprimir metade da imagem, adulterar recompensa, truncar ou
trocar a ordem do cartão/replay e omitir imagem longa no log. O intacto passa e cada estrago sai com 1; não vale
apenas imprimir aviso. Nas implementações, upstream só sob `PORT_BIG_ENDIAN`/`PORT_X360`, com original no `#else`;
objetos planos sem depuração iguais (`objcopy --strip-debug`, `cmp`), diferenças de `.text` só nos testes.

[Lido] Referência atual da `.text` x86: **`43444ea4d03a6d448aa1ae74a631e35b356a7caa`**, registrada no guia e nos
`text-base.bin`/`text-final.bin` da F6. Este estudo não cria build nem muda objeto. Provas F7 futuras terão de usar
as builds do commit final, sem `DESENVOLVIMENTO`, com referências anteriores preservadas.

[Lido] Nesta tarefa não se roda nenhuma dessas execuções. Na implementação futura: um ciclo de emulador por
máquina, `pgrep -af "qemu|roteiros"` antes, `SEMMAPA=1`, nunca `PPC=1`; um script de passos dependentes dentro de
`flock ~/wsl.lock`, logs longos por `tee` em `~/`. Espera bloqueante pelo shell, sem consultas periódicas aos logs.

## 3. Critério de fim proposto

[Estimado] Os números são propostas de aceite, não resultados já atingidos. A etapa 1 fixa os denominadores
exatos pelo conteúdo selecionável; divergência com o censo inicial exige explicação e atualização do manifesto.
Nenhum caso ausente vira aprovado por equivalência presumida. Toda família terá ao menos uma conclusão manual
no console, além das provas de lote; uma falha nova abre correção/regressão, sem transferi-la automaticamente à F8.

[Medido] Denominadores fechados em [01](01.md): 48 episódios; 100 missões (25 iniciais, expansão pelo SimEv17);
três cursos de 50; cinco cursos Disc Fusion com 5/6/8/7/8 adversários; 100 postos, 99 regras de oponente e 37
intrusos; 37 eventos Sim e sete rodadas; 161 ids, 349 pares e 698 modelos; 101/20 relações admitidas sob
condições dos leitores, sem exclusão estática; 35 versões normais e 35 divididas; 350 itens, com 114 compráveis
e 20 só na coleção; 34 aulas e 374 páginas selecionáveis, 347 imagens carregáveis entre 398 arquivos;
cinco tipos de torneio em três níveis; 14 registros, sete replays e 15 desejos em três listas.
Os critérios abaixo passam a usar esses denominadores, sem afirmar que todas essas ações já foram executadas.
O estado corrente da campanha Sim só persiste na sessão: `SimTop_Init` o zera; após reinício conferir os
campos originais salvos (ranking, conclusão, páginas, prêmios). Retomada nova da campanha exigiria decisão do Lucas.

| Item | Critério mensurável proposto [estimado] |
|---|---|
| Inventário e conversão | 82/82 sítios observados na união final, ou cada ausência justificada pelo leitor e aprovada; zero pendência/erro de gancho; `--disco` e ISO sem erro, 100% dos bytes reconhecidos; todos os arquivos selecionáveis ligados a formato e caso, zero arquivo sem classificação. Conteúdo inativo preservado e separado. |
| Elenco | 161 ids selecionáveis/formas classificados; carregar os 349 pares id/traje e suas 698 variantes válidas em lote, conferir modelo/textura/animação e retorno sem vazamento. Luta por cada id, alternando lados e classes de arena; golpes normais, Blast 1/2/Ultimate e dano por família de animação/efeito, com cada família única identificada no manifesto. Zero stub desconhecido/fallback sem explicação. |
| Transformações e fusões | Classificar as 101 relações de transformação e 20 de fusão; executar cada relação alcançável por controle, observar id antes/depois, custo, parceiro, recursos, recarga e retorno ao jogo. Casos negativos de parceiro ausente, estoque insuficiente, lua ausente e forma bloqueada. Pelo menos uma reversão, uma forma gigante, uma fusão de time e as dez classes de tipo presentes cobertas; relações impossíveis justificadas pelo código/dados. |
| Cenários | Os 35 estágios reais em luta e cada versão dividida selecionável; percorrer mudança/destruição existente e testar água, voo, lua/dia/noite e limite da arena por família. Zero modelo, textura ou colisão ausente; transições ligadas aos arquivos lidos. Aleatório e marcadores não contados como arenas extras. |
| História | Todas as sagas e os 48 índices de script selecionáveis inventariados, todos os episódios selecionáveis iniciados e concluídos, finais de saga e final do jogo pelo fluxo normal. Ambas as séries de texto/boca exercitadas por cena em lote; comandos/ramos únicos de evento, R3, troca de personagem/arena, vitória/derrota e recompensas com estado conferido. Exercitar cada classe de recompensa inédita e reler o cartão. Os dois scripts adicionais em disco precisam de leitor identificado ou declaração de inatividade. |
| Missão 100 | As 100 missões iniciadas e ganhas em lote, resultado e byte persistido; derrota/cancelamento e trava/destrava da Sobrevivência conferidos. Vitória dada pode abreviar o resultado, mas cada regra especial de missão exige caso que a execute. |
| Sobrevivência | Três cursos de 50 adversários até o fim em lote, substituição de membros e recorde conferidos; derrota parcial e posterior recorde menor sem apagar o maior. Sem concluir que os três primeiros adversários provam os 50. |
| Sim Dragon | Os 37 handlers classificados, cada evento alcançável executado em lote com semente/estado conhecido, treino, descanso, item, luta, derrota, continuar e término nas sete rodadas; conservar estatísticas entre dias/lutas da sessão e reler os campos originais persistentes após salvar/sair (ranking, conclusão, páginas e prêmios). `SimTop_Init` reinicia a campanha; não exigir serialização nova sem decisão específica ([01](01.md)). |
| Disc Fusion | Dois ramos acessíveis pelo X real no console, cinco cursos concluídos e ranking até o topo por sequência do jogo, com persistência conferida e derrota incluída. Cartões próximos dos marcos podem abreviar regressões, sem substituir a primeira prova da progressão. |
| Torneio | Cada tipo e restrição da tabela até campeão e premiação; derrota, eliminação, regeneração/vida carregada quando houver, ring-out e chave avançada conferidos. Confronto de dois humanos efetivamente dividido e torneio com participantes humanos alternados sem trocar seus controles. |
| Duelo e Treino | Duelo simples/time, 1P/COM, COM/COM e 1P/2P, trocas de membros, resultado, revanche e saída; aulas/páginas selecionáveis percorridas (398 arquivos classificados), treino livre e respectivas opções. Regressão `f5_*` integral. |
| Evolution Z e loja | Compra com dinheiro/estoque válidos, recusa sem saldo, equipamento dos tipos distintos entre 350 itens, limite de slots e três conjuntos, experiência/nível, senha e efeito na luta; reler customização/dinheiro/itens do cartão. Censo de todos os itens; um caso por regra distinta, sem alegar todas as combinações. |
| Data Center e replays | Senha válida/inválida, 14 registros com inserir/substituir/apagar e restauração; gravar, substituir, carregar e reproduzir nos sete slots de replay, tratar ausente/truncado. Comparação de campos/entradas entre arquiteturas e percurso completo de um replay de time e um dividido, com resultado e término conferidos. |
| Referência e Shenlong | Perfis/vozes/modelos e trajes do elenco acessíveis; três dragões com lista, prêmio e save, cada tipo de desejo disponível/inédito e consumo das esferas conferidos. Sorteio reproduzido por semente de teste; console com sorteio real. Exceções visuais já adiadas mantidas nominadas. |
| Desbloqueios e save completo | New Game sem desbloqueio de teste, obter ao menos uma recompensa real de cada classe e cada condição única inventariada; cartões antes/depois, reinício e Continue. Cartão avançado com todos os modos, customizações, registros, opções dos dois controles e replays preservados; `save_mc.sh`/`save_entre.sh` sem falha, queda simulada preserva último estado íntegro. Save do PC continua recusado, sem importador novo. |
| Dois jogadores | Dois rastros de entrada distintos confirmados no estado dos lados; escolhas/cancelamentos simultâneos, simples/time/torneio, pausa por ambos e prioridades de câmera. No console, ligar primeiro em quadrante diferente de 1, entrar jogador 2, desconectar/religar cada um e abrir Guide; nenhuma entrada cruzada, botão preso ou tomada do controle reservado. |
| Imagem e som no 360 | Pontos fixos de cada família comparados à referência com os limites da F5: ao menos 98% dos pixels até 8 e PSNR de 30 dB nos casos controlados com amostragem equivalente, mais inspeção; publicar a região 3D/2D e as duas metades separadas, para uma metade não diluir o erro da outra. Não alargar tolerância sozinho; efeitos, HUD, boca e câmera sem omissão. Limites de latência, mixer e deriva da F6 mantidos, zero falta de dado, escuta da luta dividida por ambos. |
| Desempenho e memória | Proposta conservadora: decisão 27 também no dividido, **zero imagens de três vblanks ou mais** em três janelas de ao menos 2.700 passos, fria e duas aquecidas, com som e padrões finais, sem aumentar fila/latência. Testar duelo simples e time, mais cenário pesado medido; trabalho por passo informativo em p50/p95/p99/máximo. Mínimo 32 MiB físicos livres, sem recusa de recurso nem crescimento após dez ciclos menu/luta. Revalidar a luta simples da F5. |
| Vida do console e final | Sessão livre de 30 min com dois jogadores e mudanças de arena/forma/time, três Guides na luta e uma na carga, dez saídas ao dashboard, sem queda ou espera ilimitada. Rodada final no commit entregue, tabelas por rede/roteiro, falhas provocadas, hashes/objetos e pendências explícitas. F5/F6 integradas e aprovadas antes de fechar a F7. |

[Estimado] Cobertura de todos os arquivos/modelos e todos os fluxos enumerados é em lote; a inspeção visual manual
no console será por família e pelos piores casos descobertos. Não se promete validar manualmente cada combinação
de dois lutadores, item, roupa e arena. A decisão 4 abaixo fixa essa divisão antes de implementar os testes.

## 4. Etapas e dependências

[Estimado] Um dia útil representa 6 h efetivas de uma pessoa familiarizada com o port, incluindo teste e correção
local. Filas de máquina/console e resolução do desempenho ainda aberto da F5 são separadas. A etapa 1 tem sua parte
sem emulador concluída em [01](01.md); os [pilotos](02-pilotos.md) estão medidos, com reprovação na memória
da transformação. Os pilotos não substituem os critérios das demais etapas.

| # | Onde | Entrega e dependência | Critério de saída | Trabalho [estimado] |
|---|---|---|---|---|
| 1 | WSL/Deck, só leitura inicialmente | Manifesto, progresso, denominadores e verificador concluídos; decisões 1 a 4 aprovadas; três pilotos executados e medidos ([02-pilotos](02-pilotos.md)) | Parte sem emulador concluída; Missão 0, marco 29/30 e Episódio 0 aprovados; transformação com memória reprovada | 2 a 3 dias, previsão inicial |
| 2 | WSL/Deck | Receitas de cartões e verificadores de cobertura/save, após 1 | Cartões nativos repetíveis, progressão inédita salva/recarregada, negativos recusados; sem bytes do jogo no git | 2 a 4 dias |
| 3 | WSL, depois 2; console curto | Entrada dos dois jogadores no teste, menus e primeiro Duelo dividido | Rastros x86/PPC, duas câmeras/metades, resultado e controles reais conferidos; perfil comparativo cedo | 2 a 4 dias |
| 4 | WSL/Deck, depois 1/2 | Elenco, trajes, transformações/fusões, estágios e transições | Denominadores da seção 3 fechados, memória/listas/imagens e ações reais iguais, lote retomável por caso | 4 a 7 dias |
| 5 | WSL/Deck, depois 2 | História, Ultimate, torneios e progresso completo | Episódios/missões/cursos/eventos e seus resultados/save; exceções por leitor documentadas | 4 a 7 dias |
| 6 | WSL/Deck, depois 2; integra 4/5 | Loja, itens, senhas, replays, Referência e desejos restantes | Cartão completo, sete slots, ambos os controles/configurações, cada família da seção 3 coberta | 3 a 5 dias |
| 7 | Linux/console, apoio WSL; após 3, F5/F6 estáveis | Desenho/perfil dividido, memória, som, sessão com dois controles; corrigir pelo maior custo real | Imagens por metade, desempenho e vida do console no aceite aprovado; repetir simples após correção | 3 a 6 dias |
| 8 | WSL/Deck e console; integra 4 a 7 | Rodada final única do commit de entrega e documentação | Todas as redes, negativos, objetos e sessão final; nenhuma pendência escondida como cobertura | 2 a 3 dias |

[Estimado] Total indicativo **22 a 39 dias úteis, 132 a 234 h de trabalho**, sem filas e sem um novo projeto de
otimização se o dividido não couber. Reestimar ao fim da etapa 1 e do primeiro Duelo dividido da 3. Parser ausente,
região opaca do save, instrução de evento/VU1 inédita, divergência do replay ou falta de margem na GPU podem
acrescentar dias por família. Mais de duas vezes o prazo de uma etapa: registrar estado/hipóteses, commitar apenas
o provado e parar conforme o AGENTS, sem manter um emulador preso.

[Medido] Os nove roteiros concluídos da F6 gastaram, somados, **aproximadamente 6,8 h** segundo `casos.json`,
incluindo execuções repetidas por memória/imagem/som. O primeiro `f5_duelo` levou 5.392 s e `f5_treino` 4.124 s.
Esses tempos são daquela máquina/rodada; o custo de um novo caso curto não é o de um roteiro longo completo.

[Estimado] Reserva de máquina revista em 29/10 pela [medição dos pilotos](02-pilotos.md):
**360 a 500 h serializadas** no ritmo do Deck; com Deck e WSL à mesma velocidade e 20% serial,
**216 a 300 h de parede**, sem velocidade relativa comprovada. Substitui as **120 a 240 h** iniciais
para expansão e regressões de lote; as hipóteses e parcelas estão na nota. Não multiplicar 21 roteiros
por 698 modelos com todas as redes: fazer censo e cargas curtas em lote, rastrear os caminhos novos e conservar
casos longos representativos. Depois de provar a equivalência, dividir casos independentes entre WSL e Deck pode
aproximar a duração de parede da metade; não reduz o trabalho nem autoriza dois emuladores no WSL.

[Estimado] Ordem: 1 -> 2 -> 3; 4, 5 e 6 podem ser preparadas em paralelo e partilhar o contrato dos cartões, mas
seus emuladores entram em fila. A sonda curta da 7 vem logo após 3, para descobrir orçamento inviável antes dos
lotes grandes; seu aceite final espera 4 a 6 e F5/F6. Mudança no backend integra primeiro, seguida das provas de
conteúdo e regressão. A 8 só usa commits integrados e builds finais.

[Lido] O guia, seção 1.2b, documenta o Deck em distrobox Ubuntu, dados locais, referências e `.text` x86 iguais
ao WSL como prova inicial. [Estimado] Na F7, o Deck pode executar a metade dos casos de conteúdo da 4/5/6 e
censos/negativos enquanto o WSL roda outra metade ou prepara entrada. Gerar `port/build/gen` localmente, conferir
commit e ferramentas, um emulador por vez no Deck, dados e cartões próprios. Não usar Deck como medidor do 360.
Não se acessou ou reservou o Deck nesta tarefa, nem se alteraram outras worktrees ou serviços do WSL.

## 5. Decisões para o Lucas

[Estimado] Só perguntas que mudam trabalho. Recomendações ainda abertas; o escopo usa a opção conservadora
descrita, sem introduzir mudança no jogo nem afrouxamento de decisão vigente.

1. [Estimado] **Aceite da tela dividida.** (a) Mesmo alvo da decisão 27, 30 fps, zero imagens longas nas três
   janelas, sem perda de imagem nem latência adicional. (b) Limite próprio de imagens longas, a escolher depois
   de ver o perfil e uma sessão de dois jogadores. (c) Permitir redução de escala/efeitos só no dividido após
   comparar imagens e custo. **Recomendação: a como alvo, medir na etapa 3/sonda da 7; se falhar, trazer os
   números antes de escolher b/c.** b/c mudam o teste e podem exigir política específica no backend. Sem resposta,
   manter a e não declarar o dividido aprovado.
2. [Estimado] **Todos os modos nesta fase?** (a) Todo o jogo base da seção 3, incluindo campanhas completas,
   cursos, loja, replays e dois humanos no Torneio. (b) Entregar primeiro Duelo/conteúdo de luta, movendo modos
   avançados para uma fase explicitamente nova antes do release. **Recomendação: a**, porque o nome da F7 e a
   decisão 22 já apontam conteúdo completo; b encurta a primeira entrega, mas não elimina o trabalho nem fecha
   o jogo base. Sem resposta, não excluir nenhum desses modos; manter só os gaps já adiados pelo Lucas na F8.
3. [Estimado] **Desbloqueios no produto e nas provas.** (a) Progressão original, cartões avançados gerados só
   para teste e casos de ganho real sem desbloqueio. (b) Acrescentar também uma opção visível de liberar tudo,
   sem substituir a progressão. (c) Liberar tudo por padrão e dispensar a campanha de desbloqueio no aceite.
   **Recomendação: a.** b exige interface, persistência e testes adicionais; c muda o jogo e mascara recompensas.
   Sem resposta, não criar opção do produto nem levar `BT3_DESBLOQUEIO` para uso normal. Importador de PC segue F8.
4. [Estimado] **Profundidade da prova no console.** (a) Lote completo de arquivos/ações/fluxos no WSL/Deck, e no
   console inspeção por família, todos os casos problemáticos e os piores cenários de desempenho, mais sessão
   livre. (b) Repetir manualmente no console cada traje/variante, relação e missão/episódio. **Recomendação: a**,
   com lista fechada na etapa 1 e estados avançados também no 360; b aumenta muito as sessões e o custo depende
   da disponibilidade do Lucas. Sem resposta, planejar a, sem alegar inspeção manual exaustiva ou equivalência
   de todas as combinações. Os critérios de estado em lote não são reduzidos por essa escolha.

## 6. Riscos e medições que os revelam cedo

| Risco ou incerteza | Medição antecipada | Encaminhamento proposto |
|---|---|---|
| [Lido] `soma` verde com chamadas não observadas | [Estimado] Diferença nominal contra 82 sítios e arquivos `LEU`, antes da etapa 2; retirar uma chamada de uma cópia | [Estimado] Exigir cobertura, não só código de saída; não transferir o resultado parcial da F6 para a F7. |
| [Lido] Liberação de teste elimina recompensa ou não cria conteúdo avançado | [Estimado] Cartão antes/depois, sem `desbloqueia`, com valor persistente esperado; reinício | [Estimado] Receitas de pré-condição e jogo produzindo o resultado; preservar original do usuário. |
| [Lido] Save contém bytes opacos e replays têm arquivos separados | [Estimado] Campo/consumidor por receita, caso truncado e nas duas ordens; medir comparação integral do cartão | [Estimado] Não converter região desconhecida por palpite; seguir o fluxo que a grava. |
| [Lido] Só a porta 1 recebe comandos de roteiro | [Estimado] Caso em que cada jogador anda para lado diferente, escolhe/cancela e pausa; desligar entrada 2 em cópia | [Estimado] Relógio comum, comandos por jogador e conexão simulada; XInput real só se aprova no console. |
| [Lido] Duas vistas repetem trabalho e os modelos são diferentes | [Estimado] Primeiro Duelo dividido: tempo/listas por vista, memória, alvos e histograma sem marcas; comparação pareada com simples | [Estimado] Reestimar antes dos lotes; não supor custo 2x, não reduzir escala nem efeitos sem resposta 1. |
| [Lido] F5 ainda tem cauda tardia nas trocas de cena | [Estimado] Mesmos alvos e três janelas sem marcas, mais Blast/transformação/arena no dividido | [Estimado] Separar custo CPU/GPU sobreposto e prazo da cadeia; diagnóstico instrumentado não aprova apresentação. |
| [Lido] 16:9/HUD, DATE/FBA e persistência de alvos foram provados em amostra pequena | [Estimado] Capturas das duas metades, passagem cheia/dividida e duas câmeras sob água/efeito | [Estimado] Comparar por metade; correção com causa e lista igual, sem tolerância genérica nova. |
| [Lido] Parâmetro existente não garante transformação alcançável | [Estimado] Conferir 101/20 relações com custos, parceiros, lua e flags; caso negativo antes do positivo | [Estimado] Preservar os dois denominadores, dados e executáveis, com exclusões justificadas. |
| [Estimado] Elenco/campanha alcançam comandos, VU1, efeitos ou leituras nunca vistos | [Estimado] Guarda, stubs chamados, instruções desconhecidas e fallback por arquivo/caso desde a primeira carga | [Estimado] Parar no primeiro caso, especificar leitor/formato e correção protegida; estender guia e regressão. |
| [Estimado] Dois humanos sobrecarregam som/leitura ou deixam voz/controle presos na Guide | [Estimado] Duas vozes e efeitos simultâneos, p99/máximo do mixer, falta de dado, Guide/desconexão na carga e luta | [Estimado] Reusar redes F6, medir leitura por thread e não pôr E/S no mixer; confirmar no console. |
| [Medido] Nove roteiros F6 já custaram 6,8 h de máquina | [Estimado] Três pilotos cronometrados e bytes livres antes dos lotes | [Estimado] Casos retomáveis com carimbo e resultado próprio; WSL/Deck independentes; não aprovar arquivo velho ou log cortado. |
| [Lido] Namek/deserto já adiados podem reaparecer nas novas capturas | [Estimado] Identificar caso/imagem e comparar com o defeito conhecido | [Estimado] Manter exceção só se for o mesmo defeito; o restante continua exigido e o gap segue bloqueando o release, não a fase. |

## 7. Divisão futura e pontos que pedem o console

[Lido] Este estudo foi feito por uma pessoa, sem agentes auxiliares. [Estimado] Na implementação, divisão por
contrato como a 9a/9b, com responsáveis futuros e integração serial do backend e do suporte a cartões/controles:

| Frente futura | Contrato/arquivos | Independência e integração [estimado] |
|---|---|---|
| Manifesto e cartões | Ferramentas de leitura em `port/x360/tools`, teste em `headless/`, documentação por campo | Depois da etapa 1, fornecer receitas versionadas e resultados esperados antes dos casos dependentes. |
| Entrada e menus de dois jogadores | `headless/roteiro.c`, `pad360.c`, leitores de seleção/Torneio | Um responsável pelo relógio/formato; merge antes dos roteiros divididos; `.text` só teste. |
| Personagens/cenários | Conversores, parâmetros e ações, cargas/transições, imagens/listas | Pode preparar casos no Deck enquanto os modos rodam no WSL; upstream sempre protegido. |
| História e Ultimate/Torneio | Receitas por pré-condição, eventos/recompensas e conclusão | Partilhar contratos de cartão, não arquivos de saída; unir manifestos e conferir ausentes na integração. |
| Loja/Data Center/Referência/desejos | Customização, registros e sete arquivos de replay, recompensas restantes | Independente do lote de modelos depois dos cartões; novos consumidores opacos exigem tipagem antes do merge. |
| GPU, desempenho e console | `gs_draw.c` protegido, backend 360 e ferramentas de perfil | Uma linha de integração com F5/F6; sonda cedo, aceitar só no commit integrado e sem marcas de diagnóstico. |
| Rodada final | Verificadores, logs e relatório por caso | Um executor por máquina, origem única; revisar lista completa e negativos antes da sessão final com Lucas. |

[Estimado] Exigem console: mapeamento e reconexão reais do segundo controle, Guide, vibração conforme o suporte
vigente, latência física, divisão/HUD em 720p/16:9, memória física/EDRAM, persistência de profundidade/alfa,
XAudio2 e escuta simultânea, apresentação em três janelas e sessão livre. O WSL prepara, mede cobertura e prova
ordem de bytes, mas não aprova esses itens. Build/deploy pertencem à revisão Linux; nenhuma ação por FTP aqui.

[Medido] Conferências deste estudo, sem execução do jogo: inventário estático saída 0; `soma` parcial 43/82;
leitura do disco/logs e censo de 698 parâmetros; nove resultados F6 preservados como instantâneo em
`~/f7-escopo/`, junto dos nomes não observados e arquivos não lidos. Nenhum verificador novo foi criado, portanto
não há controle negativo novo a certificar nesta entrega. Os controles futuros estão especificados na seção 2.

[Estimado] Relatório final de cada etapa F7: tabela por modo/receita/roteiro com cobertura, ações, resultado,
save/reabertura, imagens por metade, som, percentis/máximos, memória, erros e casos ausentes; controles negativos
vistos falhando e SHA1/objetos da x86. Não fechar F7 com a rodada F6 incompleta, com desempenho só médio ou com
os únicos dados avançados gerados pelo desbloqueio total de teste.

## Respostas do Lucas (28/10/2026, decisão 28)

1a, 2a, 3a, 4a: as recomendações da seção 5. Texto em [decisoes.md](../../decisoes.md), decisão 28.
