# F5: escopo, redes de cobertura e critério

[Índice do plano](../../plano.md) | anterior: [F4](../F4/00-escopo.md)

Estudo de 16/10/2026, sobre a base `a14baf7`. **Plano proposto, sem implementação ou execução nesta tarefa.** A
rodada completa da F4 estava em andamento no WSL; este estudo só leu documentos e código. As etapas abaixo estão
por fazer. As decisões numeradas na seção 5 são perguntas para o Lucas, não decisões novas do projeto. Antes de
implementar, registrar as respostas em `decisoes.md` e fechar o roteiro e os limites de aceite.

## 1. Escopo e ponto de partida

A entrega é uma luta jogável no Xbox 360, do Duelo 1P contra COM até o resultado e a volta ao menu, com cenário,
dois lutadores, HUD, sombras, contorno e efeitos presentes, a 30 fps. Saída 1280 x 720, lógica original em passo
fixo e menus a 60, conforme a [decisão 15](../../decisoes.md). A proposta conserva a escala interna 1 e a imagem
4:3 centralizada da decisão 20; 720p de saída não significa renderizar internamente toda a cena em 1280 x 720.
Confirmar esse recorte com o Lucas (pergunta 1).

O caso principal deve ser uma combinação fixa de personagens, trajes, estágio, dificuldade e opções, versionada
num roteiro. A demo da F2 serve de diagnóstico com histórico; o `duelo.txt` da F4 fornece o caminho jogável.
Nenhum deles sozinho cobre todos os efeitos. A proposta acrescenta trechos controlados de combate e uma janela
de Treino para medir pelo menos 90 segundos de luta contínua, mesmo se o KO do Duelo vier antes. Não se promete
todo personagem ou estágio: isso é F7. A etapa 3 identifica os ids realmente escolhidos, congela a combinação e
lista os recursos e efeitos alcançados. O Lucas escolhe a combinação, sem deduzir nomes só dos passos do cursor.

Também entram as pendências visuais que a F4 deixou explicitamente para a F5: modelo no `ChrView`, cena 3D e
imagens compostas do Shenlong. São regressões do mesmo motor; não substituem a luta como prova de desempenho.
A boca de quem fala fica registrada separadamente, pois a animação segue a voz e o áudio só entra na F6.

Fora do escopo proposto: som e filmes (F6), conteúdo completo e tela dividida (F7), opções de 16:9 e remapeamento
(F8), quatro jogadores (F9), online, outras regiões, interpolação e lógica a 60 fps. Uma transformação ou troca de
estágio não alcançada pelo caso escolhido fica como limite declarado, sem alegação de cobertura.

### Evidência já disponível e o que ela não prova

| Base | O que se sabe | Consequência para a F5 |
|---|---|---|
| [F3, riscos](../F3.md), decisões 20 e 22 | A luta chegou a 128 MB de texturas e 24 alvos, cerca de 96 MB de cor; sobraram 3 MB. Houve falhas de criação, paradas de 8 a 80 s e Guide indisponível | Primeiro o teto de memória, antes de tentar uma luta longa ou habilitar tudo |
| Decisão 23 | ISO convertida no PC, do mesmo tamanho e exigida pelo jogo; conteúdos BPE ainda convertidos após descompressão | Segundo item, já decidido. Marca da mídia e contrato de `mods/` continuam abertos |
| Decisão 17, F2 | Sem desenho e sem som: aproximadamente 15 ms por quadro, pior par de vblanks perto de 20 ms; a luta do Xenon terminou em 3.182 quadros contra 4.242 na x86 | Há uma referência de CPU, não uma medição do renderizador nem garantia dos 18 ms restantes |
| [F4, etapa 6](../F4/06.md), [GPU, seção 6](../../formatos/gpu.md) | Microcódigo das tabelas já tipado: 3.892 palavras. Depois disso ainda houve 17.673 pixels diferentes na luta, até 7 de desvio, em 26 primitivas com cores invertidas | Não refazer a correção da VU1 das tabelas; corrigir os `rgba[4]` e voltar a exigir igualdade de imagens |
| [F4, etapa 5](../F4/05.md) e [9b, resposta 4](../F4/09b.md) | Arena do `BtlPool` de 0x1A0800 bytes aceita por teto de classe; cinco trechos de `gBtlScript` sem prova adequada no meio da luta | Tipar subalocações e a união ativa, sem estender exceções de menu à luta |
| [F4, etapa 7](../F4/07.md) | Menus com desenho: mediana de 19,35 ms nas janelas medidas; boca fora de fase com a referência | A F2 não prevê o custo atual; não cobrar sincronismo de voz da F5 silenciosa |
| [F4, 9c-4](../F4/09c.md) | `ChrView` não foi aberto; lista e recompensa sobre a cena do Shenlong diferiram em 40.950 e 27.650 pixels | Provar os modelos e as composições, incluindo a UI sobreposta; as cinco imagens de menus aprovadas não provam a cena |

As tabelas de arquitetura de `plano/base.md` são históricas em alguns pontos. Para esta fase prevalecem as
decisões posteriores: C89 em C (3), float em hardware no Xenon (5 e 17), escala 1 (20), save nativo (22) e ISO
convertida no PC (23). Não reabrir essas escolhas implicitamente.

## 2. Redes de cobertura

Reaproveitar as quatro redes da F4, com uma quinta para o orçamento do console. Cada resultado deve indicar
commit, ambiente, roteiro, arquivos lidos e limites; uma rede não substitui a outra.

1. **Código e contratos.** Inventário das gravações de cores nos pacotes, chamadas e variantes da VU1, efeitos
   nativos, capacidades de buffers e donos dos recursos. Reutilizar `layout.py`, `pacotes64.py` e `unpacks.py` onde
   se aplicam. O inventário de `rgba` deve distinguir cor da CPU, cor de pacote e cópia de pacote; não basta
   substituir todo vetor de quatro bytes. Novos verificadores precisam recusar cópias estragadas antes de valer.
2. **Disco e instalação.** `conv_tool --disco`, mapa do rastreio e marcas por formato, preservando o censo da F4.
   A nova instalação valida os conteúdos que grava e os que preserva, os limites ISO/AFS e o resultado reaberto.
   Cobertura estrutural de todos os arquivos conhecidos não prova execução de todas as técnicas.
3. **Execução e memória no WSL.** Roteiros ancorados em estado, x86 repetida contra si mesma, PPC com
   `SEMMAPA=1`, sequência de leituras, estado da luta e memória nos mesmos pontos. No meio da luta, parear os
   sub-blocos vivos do `BtlPool` por slot, geração e dono, além do heap normal; interpretar a união de
   `BtlScriptRequest` pelo `kind`. Excluir apenas campos sem conteúdo vivo com contexto comprovado.
4. **Imagens.** Rasterizador de software x86 contra PPC sem mapa, idêntico byte a byte, em janelas curtas ao redor
   de marcas. Incluir a luta no critério (`IMG=luta` hoje a compara e falha) e as composições do Shenlong. Para HLSL, comparar
   entradas/saídas e imagens contra o caminho do PC com shaders e contra o interpretador nos casos em que ambos
   representam a mesma operação. Depois, capturas do console em estados controlados, com revisão visual.
5. **Console.** Contagem real de bytes, memória livre mínima, recursos aguardando a GPU, tempos de CPU/GPU,
   apresentação e ritmo da lógica, Guide e controle. Medir os quadros frios, não só o cache aquecido. O qemu prova
   ordem de bytes, não fps ou sincronização da GPU do 360.

No WSL, um ciclo de emulador por vez, sempre conferir processos antes; nunca usar `PPC=1` como critério. O GS pode
processar cargas e VU1 sem pintar todos os quadros; aproveitar as janelas de `headless/imagens.c`, validando cedo
que preservam os efeitos com histórico da luta. Se uma janela curta não reproduzir um efeito, ampliar só aquela
janela e registrar o motivo. Não planejar horas de rasterização integral por roteiro como rotina.

Todas as rodadas futuras usam builds do mesmo commit final, carimbo sem diferenças e saídas novas, sem
`DESENVOLVIMENTO`. Logs longos com `tee` em `~/`. Dados, ISOs, dumps, capturas e replays do usuário ficam fora do
git. Mudanças no upstream ficam dentro de `#ifdef PORT_BIG_ENDIAN` ou `PORT_X360`, com o original no `#else`.
Conferir objetos sem depuração por `objcopy --strip-debug` e `cmp`; só objetos de teste de
`port/x360/headless/` podem alterar a `.text` x86. Extrair seções com saída distinta, nunca reescrever um
executável em uso. Referência documental mais recente: [F4, 9c-4](../F4/09c.md), SHA1 da `.text`
`e0af7e67cfc7048af372b16eff0707be72390d35`; confirmar a referência que a rodada completa da F4 entregar.

## 3. Critério de fim proposto

Os números abaixo são uma proposta verificável para aprovação do Lucas (perguntas 1, 2 e 8). Nenhuma medição
nova neste estudo sustenta que já sejam atingíveis.

### Caminho e conteúdo

- Roteiro `f5_duelo` a criar: boot, New Game em cartão de teste, seleção fixa, introdução, combate com movimento,
  defesa, golpes físicos, carga de ki, projétil e pelo menos uma técnica com câmera/efeitos, pausa, retomada,
  resultado e volta ao menu. Cada ação exigida tem uma confirmação no estado/rastro, não só o botão apertado.
  Completar o Duelo por KO ou tempo do jogo, sem `termina luta`, dano injetado ou alteração de velocidade na
  rodada que prova jogabilidade e fps. Esses atalhos continuam úteis para regressões de menus.
- Roteiro `f5_treino` a criar com a mesma combinação: pelo menos 2.700 passos de luta contínua, cerca de 90 s,
  exercitando os mesmos tipos de ação. A janela começa no estado de combate controlável (`BtlSeqFight`, estado
  3), não na primeira linha `tela luta`, que também pode incluir apresentação. Não eliminar quadros lentos de
  técnica, efeito ou carga durante o combate. Se o Duelo já oferecer essa janela, o Treino continua como
  diagnóstico, sem duplicar a exigência de duração.
- Dois lutadores e cenário visíveis, HUD e textos completos, sombras e contorno, profundidade/oclusão corretas,
  transparência, névoa e brilho onde o jogo os pede. Zero desenhos VU1 ou efeitos nativos requeridos descartados,
  zero textura substituída por branca por falta de capacidade, zero estouro de buffer ou instrução VU1
  desconhecida. `gsSkipped` conta também passes do PS2 substituídos por equivalentes nativos: documentar essa
  correspondência, sem exigir que esse contador bruto seja zero.
- No WSL, x86 e PPC sem mapa iguais no rastro, arquivos, estado por passo e memória viva explicada nos pontos
  de carga concluída, combate, técnica, pausa e retorno. Imagens da luta e das regressões 3D idênticas entre
  rasterizadores. Revisar as permissões `SO_NA_LUTA` e da tela 70: retirar o aceite de cor invertida dos pacotes
  vivos; restos fora da lista usada só com limites e prova de que não são consumidos.

### Tempo, memória e uso do console

| Medida | Aceite proposto | Como medir |
|---|---|---|
| Ritmo da lógica | Um passo por dois vblanks; cerca de 29,97 passos/s no modo de 59,94 Hz, chamado 30 fps; tempo real da janela dentro de 1% do esperado | Contar passos e tempo de parede; confirmar a frequência de vídeo efetiva e usar `2 / frequência` como período, aproximadamente 33,37 ms |
| Trabalho por passo | p99 até 30 ms; nenhum passo acima de um período de luta, aproximadamente 33,37 ms, em cada uma de três janelas de pelo menos 2.700 passos | CPU e conclusão da GPU, sem a espera deliberada de vblank; distinguir sobreposição para não somar tempos concorrentes como se fossem seriais |
| Apresentação | Em cada janela, pelo menos 99% dos intervalos entre imagens novas dentro de um período mais 1 ms; nenhum acima de três vblanks, aproximadamente 50,05 ms | Registrar apresentação/conclusão e identidade do passo mostrado, distinguindo imagem nova de repetida; não aceitar só contagem de chamadas a `Present` |
| Cache frio e cargas | Boot, seleção, introdução e carga inicial fora do percentil de combate, cada duração publicada; cargas durante o combate entram no critério | Uma rodada fria e duas aquecidas, sem omitir o primeiro uso das técnicas; log por fase, com o vigia |
| Memória | Uso dentro dos tetos aprovados, incluindo recursos pendentes de liberação; proposta de piso de 32 MiB livres; zero falhas de criação | Bytes reais/alinhados de D3D, buffers, temporários e jogo, mínimo de `dwAvailPhys`; amostrar também entre quadros, nas alocações |
| Estabilidade | Sessão de 30 min com ao menos dez ciclos de entrar na luta e voltar, incluindo revanche e alternância com menus; sem crescimento persistente após a GPU liberar os recursos | Comparar bytes vivos e pendentes no mesmo estado após cada ciclo; teto estável e nenhuma queda, espera sem limite ou corrupção |
| Guide e controle | Abrir e fechar três vezes na luta e uma na carga; resposta visível em até 1 s; nenhuma ação de botão residual ao voltar | Sessão com o Lucas; confirmar a política de pausa e o comportamento ao desconectar/reconectar o controle (pergunta 5) |

Capturas e escrita pesada do log ficam numa rodada própria de imagem, com o mesmo binário e configuração de
desenho. A rodada de fps não usa capturas; recolhe medidas em buffer limitado e descarrega fora do trecho medido.
Registrar custo da instrumentação e os tempos de log. Pausas do usuário e Guide têm intervalos marcados, separados
da janela contínua de 90 s; a sessão ainda precisa passar no teste de resposta e retomada.

Não medir apenas a média de 60 quadros do log atual. `estado360.c` soma pares de vblanks para a F2 sem janela;
`gs_d3d.cpp` mede a lista e `Present` por quadro do GS. Precisam de fronteiras comuns de passo, percentis e prova
de conclusão da GPU. Não usar os 16,7 ms dos menus como duração do passo da luta, nem contar uma espera de vsync
como CPU trabalhando. Reservar os cerca de 3,37 ms entre p99 de 30 e o período como margem proposta para a F6;
isso não é prova de que o áudio futuro caberá.

### Aceite das imagens no console

A igualdade byte a byte vale para x86/PPC com o mesmo rasterizador. Para console contra referência, a proposta é
usar cenas curtas e controladas antes de a divergência de float alterar a luta, mais casos de desenho com entradas
fixas. A igualdade de `hash.txt` entre Xenon e x86 não é critério: a F2 diverge no primeiro quadro de luta por float.
Não reaproveitar o número do quadro do GS de uma luta x86 longa como se fosse o mesmo estado no console.

Publicar por captura pixels iguais, diferença até 2 e até 8, PSNR, retângulos de diferença e inspeção lado a lado.
Proposta inicial: no mínimo 98% dos pixels até 8 e PSNR de 30 dB nos casos controlados com amostragem equivalente,
mais aprovação visual do Lucas em todas as peças exigidas. Fechar a lista e os limiares antes de implementar os
shaders (pergunta 8). Comparações de cena com recorte ou filtro deliberadamente diferente exigem regiões definidas
antes e uma referência adequada, sem mascarar personagem, efeito, UI ou profundidade para passar.

O patamar de 82,3% da seleção na F4 não é licença para a luta: aquela diferença veio de textura reduzida filtrada
no console e amostrada no ponto no rasterizador. Medir fundo e restante separadamente, como na etapa 7. O
interpretador também não é referência perfeita para o recorte dos shaders do PC; documentar a diferença esperada
e comparar a mesma entrada contra os dois caminhos, sem chamar toda discrepância de erro de endianness.

## 4. Etapas e dependências

Estimativas de trabalho de uma pessoa familiarizada com o port, incluindo implementação, conferência e correções
comuns, sem a espera pela disponibilidade do console. Um dia útil significa 6 h de trabalho efetivo. São faixas de
planejamento pela leitura do código, não medições de execução. WSL significa código e testes locais; build XDK e
revisão do 360 são no Linux. Nenhuma dessas execuções foi feita no estudo.

| # | Onde | O que faz | Critério de saída | Tamanho estimado | Estado |
|---|---|---|---|---|---|
| 1 | WSL e console | Teto total de memória do backend e descarte seguro por uso; contagem de recursos pendentes, alvos e buffers; tratamento de pressão e falha | Travessia curta menu/carga/luta volta sem esgotar memória ou travar a Guide; bytes reais dentro do orçamento, sem usar textura branca para esconder falta de memória; descarte validado com referências ainda vivas | 2 a 4 dias | por fazer; perguntas 2 e 5 |
| 2 | WSL e console | Instalador de PC da decisão 23, gerando ISO convertida do mesmo tamanho; marca/versionamento e recusa clara da original; retirar conversão de arquivo na carga do 360 | Saída reaberta equivalente aos conversores por arquivo, offsets/tamanhos ISO e AFS preservados; original intacta; instalador recusa reconversão; jogo aceita a convertida válida e recusa original, truncada, incompleta ou incompatível; boot e luta pela nova mídia | 2 a 4 dias | [PC concluído](02.md), critério inteiro no WSL; integração do 360 após a etapa 1 |
| 3 | WSL, confirmação no console | Fechar `f5_duelo`/`f5_treino`, matriz de ações, pontos de memória e captura; instrumentar tempo por passo e contadores de descarte | Duas x86 repetíveis; ações confirmadas, ids e estado inicial fixos; verificadores recusam resultado ausente, roteiro cortado, ação não alcançada e log de outro commit | 1 a 2 dias | feita, [03](03.md); os `f5_*` dão "tudo certo" depois do conversor de fonte (etapa 5) |
| 4 | WSL | Auditoria e correção das cores byte a byte dos efeitos, em todos os usos das structs de pacote, não só nas 26 primitivas conhecidas | Inventário de gravações coberto; regras dos pacotes preservadas; `IMG=luta` e imagens do Shenlong incluídas, sem exceção de cor invertida em dado vivo; objetos x86 do upstream iguais | 1 a 2 dias | feita, [04](04.md) |
| 5 | WSL | Memória no meio da luta: arena/subalocações do `BtlPool`, união ativa de `gBtlScript`, componentes de câmera e tarefas GSC vivas | Pontos da etapa 3 comparados campo a campo com dono, tipo e vida útil; retirar teto genérico onde houver tipo; estragos em cada variante e sub-bloco vivo recusados | 2 a 3 dias | por fazer |
| 6 | WSL e console | Habilitar VU1 no 360 com caminho de desenho completo; portar as três famílias de shaders já usadas no PC e suas camadas, constantes, índices e recorte | Programas/camadas alcançados mapeados, saída comparada com entrada fixa; lutadores, estágio e sombras presentes; zero descarte VU1, fallback medido e sem instrução desconhecida; shaders compilados offline | 2 a 4 dias | por fazer; pergunta 4 |
| 7 | WSL e console | Profundidade persistente, byte exato de alfa e efeitos nativos de contorno, chave, tabela de profundidade e cópia de ids; DATE, FBA, névoa e brilho | Trocar alvo e apresentar preservam o conteúdo necessário; imagens com oclusão/transparência e efeitos conferidas; nenhum marcador requerido só contado e ignorado; orçamento de memória da etapa 1 revalidado | 3 a 5 dias | por fazer; pergunta 6 |
| 8 | Console, apoio no WSL | Perfil completo: lógica, gravação e decodificação em `gs_draw.c`, VU1 residual, uploads, trocas/Resolve, vértices, GPU e Present; otimizar o maior custo medido | Três janelas contínuas passam nos limites aprovados, frias e aquecidas; imagens e estado continuam corretos após cada otimização | 2 a 4 dias | por fazer; pergunta 7 se faltar margem |
| 9 | WSL e console | Fechar dívida 3D da F4: abrir `ChrView`, executar Shenlong e comparar lista/recompensa sobre a cena; regressão dos menus e save | Modelo real carregado e visível, memória viva tipada; duas composições antes recusadas agora conferidas; regressões da F4 sem ampliar tolerâncias; boca documentada como dependência da F6 | 1 a 2 dias | por fazer; pergunta 9 |
| 10 | WSL e console com Lucas | Rodada final do commit de entrega: redes completas, mídia, imagens, fps, sessão de 30 min e relatório | Todos os critérios aprovados cumpridos com builds do commit final; tabela de resultados, falhas provocadas, objetos e SHA1 x86; nenhuma pendência do escopo tratada como aprovada | 1 a 2 dias | por fazer |

Total indicativo: **17 a 32 dias úteis (102 a 192 h)**, sem filas do console ou expansão para conteúdo completo.
Maior incerteza: etapa 7, por EDRAM/profundidade/alfa, seguida da vida útil de recursos da etapa 1. Rever a previsão
depois das primeiras medições dessas etapas, em vez de prometer 30 fps só pela folga da F2.

Ordem obrigatória: 1 antes de 2, como fixado na F3. Fazer o primeiro teste da etapa 1 com corte curto e vigia;
ele prova segurança, não a luta completa. Após 2 e 3, 4 e 5 podem ter autores separados; integrar ambas antes da
conferência visual principal de 6 e 7. O perfil leve começa em 1; a otimização para aceite é 8, com o desenho
completo. Não fechar fps omitindo as passagens de 7. A etapa 9 compartilha as correções de 4 a 7; 10 integra tudo.

### Pontos de implementação que mudam a estimativa

- **Teto por bytes, não só contagem.** Em `gs_d3d.cpp`, `MEM_TEXTURAS` limita 128 MiB de pixels decodificados;
  `sAlvos` pode somar 24 texturas de 1024 x 1024 x 4, 96 MiB nominais, além de `sDate`, buffers e alinhamento.
  `tex_destroy` desconta `sMemTex` antes do `Release`, enquanto `cova()` ainda segura o recurso; a fila cheia
  chama `BlockUntilNotBusy`. O teto novo deve contar também essa memória pendente, não apenas o cache lógico.
- **O dono do cache está no frontend.** `texture_get` em `gs_draw.c` só varre por idade quando chega a 8.192
  entradas; antes disso, `texSlotsLeft() <= 0` devolve branca. Liberar o objeto D3D isoladamente deixa `GsDraw.tex`
  ou o cache apontando para ele. `GsTarget.last` só provoca substituição quando há 24 alvos, com folga de 120
  quadros. Não descartar alvo só por idade sem provar que seu conteúdo não voltará a ser lido. A proposta de
  descarte precisa invalidar o cache e preservar referências da lista gravada e comandos ainda na GPU.
- **Buffers também têm peso.** Em `GsGpu_Init`, as capacidades de `gs_draw.h` reservam 28 MiB de vértices comuns,
  24 MiB de vértices VU, 4 MiB de índices, cerca de 2,375 MiB de constantes e 8 MiB de desenhos, no layout de
  32 bits declarado. São cerca de 66,375 MiB só nesses cinco vetores, não a memória total. O comentário do backend
  cita cerca de 145 MB com intermediários; medir a composição real, inclusive reservas, temporários e pilha de
  16 MiB do jogo, antes de reduzir limites. Registrar máximos usados e falhar com diagnóstico se excedidos.
- **Pré-conversão não é recompressão.** `conv_tool --disco` valida dados extraídos, não grava uma ISO. Há núcleo
  de conversão reutilizável, mas ainda faltam leitura/escrita da imagem, validação e contrato da marca. Preservar
  BPE externo, animações comprimidas e os ganchos após descompressão; preservar SLUS/DBZP e a carga tipada das
  tabelas da decisão 13. Metadados ISO 9660 e tabelas AFS continuam no formato original; só converter os conteúdos
  definidos pelos formatos, uma vez. Conferir os hashes da origem USA e distinguir esses hashes dos da saída.
  Retirar `Conv_Arquivo` da abertura no 360 não autoriza retirar todos os conversores.
- **VU1 não começa do zero.** `gs_vu1.c` já reconhece variantes pelos tamanhos 127/124/99/101/94/104/399/451 e
  encaminha lotes para três famílias (`vu0.vert`, `vu4.vert`, `vu6.vert`) quando usa GPU. São modelos e camadas,
  geometria estática, sombras e detritos, entre outros usos. Inventariar o que o caso executa e os fallbacks;
  tamanho sozinho não dispensa confirmar o programa/entrada. No 360, `BT3_VU1` ainda vem desligado por padrão,
  e `frame_end` pula `d->vu`: só ligar a chave não desenha os modelos. O comentário sobre microcódigo cru nesse
  trecho ficou anterior à correção documentada na F4; a guarda de desligamento continua existindo.
- **Profundidade e alfa são uma entrega própria.** `sai_da_edram` só resolve cor; `entra_na_edram` limpa Z e
  restaura apenas cor. `targetAux` e `targetDepth` retornam zero no backend. Os nativos 1 a 4 são ignorados;
  DATE (5) lê alfa já saturado. O byte exato pode guardar ids acima de 0x80, perdidos na cor normalizada. Cor e
  Z a 1024 x 1024 já usam 832 + 832 dos 2.048 tiles; acrescentar saída auxiliar exige redesenhar a distribuição
  ou usar passagens extras. Não copiar a organização de três anexos por alvo do PC sem orçamento.
- **Thread separada não é ajuste gratuito.** A main cede o dispositivo à thread do jogo e só a vigia durante
  `BT3_GS=gpu`. Uma carga longa deixa de apresentar. O caminho experimental `BT3_GS_THREAD` do frontend registra
  que o jogo alterou dados ainda referidos pela lista, com câmera saltando. Qualquer separação exige posse do
  dispositivo e cópias/vida útil dos dados explícitas, além de medir sincronização e custo de memória.

## 5. Decisões para o Lucas

Os custos abaixo são incrementos relativos à linha proposta, sujeitos às medições. As recomendações não foram
aplicadas ao jogo. Sem resposta, manter a questão aberta e não implementar a arquitetura dependente.

1. **Qual luta fecha a F5 e qual resolução interna?** (a) Só a demo existente, saída 720p e escala 1: menor custo,
   reaproveita a F2, mas não demonstra o controle nem resultado do Duelo. (b) Duelo jogável fixo, demo diagnóstica
   e janela de Treino quando necessária: custa os roteiros e cerca de 1 a 2 dias da etapa 3; cobre o uso proposto.
   (c) Vários personagens/estágios e/ou escala interna maior: amplia testes e pode exigir tiling e orçamento novo,
   sem estimativa confiável antes do perfil. **Recomendação: b, escala 1 e 4:3 atuais.** Escolher personagens,
   trajes, cenário e ações obrigatórias; ampliar só por risco observado, deixando a generalização para a F7.
2. **Qual orçamento de memória e até onde mudar o cache?** (a) Só baixar `MEM_TEXTURAS`: poucas linhas, mas não
   resolve alvos, pendentes ou branca; não recomendável como entrega. (b) Contabilidade total e descarte por uso,
   com integração mínima no frontend protegida por `PORT_X360`, preservando dados ainda referidos: custo da
   etapa 1, proposta de 32 MiB livres e orçamento por classe após medir o boot. (c) Redimensionar buffers e alvos
   pela área usada, ou mudar a representação: pode poupar muito, mas aumenta os casos de limites, persistência e
   recorte e pode acrescentar vários dias. **Recomendação: b primeiro; c só com os máximos medidos.** Os 32 MiB
   são margem proposta, não exigência conhecida do sistema. O descarte já foi escolhido na F3; falta seu contrato.
3. **Marca da ISO e `mods/` da decisão 23.** Para a marca: (a) embutir identificação/versionamento em área
   comprovadamente livre da ISO, mantendo tamanho: arquivo único, mas precisa provar que a região não é usada;
   (b) manifesto lateral ligado ao conteúdo e versão da imagem: não altera região livre, porém cria segundo arquivo
   e risco de pareamento errado. Para `mods/`: (c) recusá-los no primeiro caminho do 360 com diagnóstico; (d)
   converter no PC e exigir manifesto/versão também para eles: preserva mods, mas exige ferramenta e provas extras.
   Custo de a ou b: aproximadamente 0,5 a 1 dia dentro da etapa 2, com a prova da região em a ainda incerta;
   c custa a guarda e o diagnóstico, d pode acrescentar 1 a 2 dias para ferramenta e casos negativos de mods.
   **Recomendação: a apenas depois de provar a região; c para o primeiro aceite, se o Lucas permitir.** Não
   escolher um offset nem desativar mods sem resposta. Em qualquer contrato: validar origem/versão, rejeitar
   dupla conversão, não modificar a ISO original e só publicar saída completa após conferência. BPE continua
   convertido após descompressão; não existe compressor novo neste escopo.
4. **VU1 em shaders ou interpretada?** (a) Interpretar tudo e desenhar os pacotes resultantes: caminho útil para
   diagnóstico, mas custo no Xenon ainda desconhecido. (b) Portar para HLSL as famílias já prontas no PC,
   mantendo interpretador para comparação e fallback identificado: custo da etapa 6, maior chance de cumprir fps.
   (c) Especializar novos programas/VMX antes de medir: investimento maior e nova prova numérica. A arquitetura
   base já prevê shaders para os programas quentes. **Recomendação: b; a como referência e fallback medido.**
   Programa inesperado deve aparecer no log e na cobertura; não sumir silenciosamente.
5. **Guide e apresentação durante cargas.** (a) Manter a thread do jogo dona do D3D e dividir cargas em trabalho
   limitado, com apresentação entre blocos; menor mudança, mas exige mapear esperas. (b) Thread de desenho
   independente, com lista e recursos de vida útil explícita; maior custo de sincronização e memória, sem herdar
   cegamente `BT3_GS_THREAD`. Definir também se a Guide pausa a luta: pausa pelo estado do jogo com botões
   mascarados até soltar, ou luta continua com entrada neutra. **Recomendação: tentar a e pausa**, estimando
   notificações/retomada em mais 0,5 a 1 dia dentro da folga da etapa 1; se não limitar as esperas, reestimar b
   antes de implementá-la. Nenhum dos caminhos está decidido pela simples existência do vigia.
6. **Como guardar Z e o byte de alfa sem exceder a EDRAM?** (a) Persistir por alvo os anexos necessários e
   restaurar/resolve ao trocar, usando passagens extras para o byte auxiliar; menor alteração de coordenadas,
   mais tráfego e RAM. (b) Limitar alvos à área visível/retângulos e reorganizar a EDRAM, possivelmente com tiling;
   pode poupar RAM e tráfego, mas muda recorte, endereçamento e testes. (c) Omitir sombras/contorno/névoa para caber:
   barato, reduz qualidade e muda o escopo, não fecha o critério proposto. **Recomendação: medir cedo a com um
   caso de troca de alvo, dentro do teto da etapa 1; escolher b se a não couber.** A medição e a escolha precedem
   a implementação completa da etapa 7. A faixa de 3 a 5 dias estima a; b exige reestimar após a sonda de EDRAM,
   sem incluir tiling completo no total atual. Não declarar que Z pode ser perdido sem inventário de leituras.
7. **Se a imagem completa não couber em 33,37 ms, qual troca é aceitável?** (a) Otimizar CPU/cache/uploads e
   agrupar desenhos, conservando imagem e lógica; custo da etapa 8 e possível extensão conforme o perfil. (b)
   Reduzir efeitos/qualidade ou área interna: economiza trabalho, muda imagem e precisa aprovação explícita.
   (c) Reduzir ritmo da lógica: muda a luta e conflita com a decisão 15. **Recomendação: a; apresentar o perfil
   antes de propor b.** Não adotar c nem transformar o critério em média de fps para esconder picos.
8. **Quão estritos serão os limites de fps e de imagem?** (a) Aceitar só média de 30 fps e inspeção visual:
   barato, permite engasgos e perdas de passagens. (b) Usar os percentis, máximos, três rodadas, 32 MiB e casos
   controlados da seção 3, com 98% até 8/PSNR 30 dB mais inspeção: exige instrumentação e referências da etapa 3.
   (c) Exigir todos os pixels de uma luta longa iguais ao PC: não é um critério válido com o float atual do Xenon;
   exigiria primeiro um gêmeo numérico e controle de filtro/recorte, com trabalho adicional. **Recomendação: b.**
   Aprovar ou ajustar os números antes da rodada, sem afrouxá-los olhando o resultado que se quer aprovar.
9. **Quanto da dívida 3D dos menus entra agora?** (a) Só uma abertura real de `ChrView` e dragão 1 com lista e
   recompensa: cerca de 1 a 2 dias da etapa 9, cobre o caso já exercitado e as composições reprovadas. (b) Todos
   os dragões e modelos/roupas do visualizador: execução mais ampla, custo adicional de roteiros/sorteios e
   conferência, a estimar após o primeiro. **Recomendação: a como mínimo explícito**, deixando os demais casos
   para F7 sem afirmar que rodaram. Boca/voz continua pendente na F6; não alterar a animação do jogo para forçar
   igualdade numa captura silenciosa.

## 6. Riscos e medições que os revelam cedo

| Risco ou incerteza | Medição antecipada | O que fazer com o resultado |
|---|---|---|
| Memória lógica menor que a física, fragmentação ou fila de descartes presa | Etapa 1: bytes por classe, tamanho alinhado, bytes pendentes, idade/fence, tempo em criação/Lock/Release e mínimo livre em cada alocação | Reconciliar contadores e memória física; não aumentar cache para esconder falha de liberação |
| Descarte de alvo ainda útil ou handle usado após liberação | Lista de recursos referidos no quadro, usos posteriores do conteúdo GS e conclusão da GPU; teste de orçamento pequeno | Provar geração/posse e recuperação do conteúdo, com captura antes/depois de forçar descarte |
| Paradas de 8 a 80 s não serem só falta de memória | Com teto ativo, marcas do vigia em leitura, conversão, VU1, upload, Resolve, Present e fila de liberação | Localizar a primeira espera longa; não atribuir previamente tudo a I/O ou à VU1 |
| ISO errada, incompleta ou conversão dupla | Etapa 2: casos negativos em cópias, conferência da saída reaberta e arquivos esperados/preservados, versão e vínculo da marca | Recusar antes de ler dados como structs; nenhuma reserva de conversão da original no console |
| `rgba[4]` esquecido em cópia, índice variável ou macro | Auditoria de todas as structs listadas em `gpu.md` seção 6; pacote com R/G/B/A distintos e técnicas que o emitem | Corrigir cada gravação por sua representação, com ramo original preservado e imagens exatas |
| Comparador aceitar união errada, tarefa viva ou arena como opaca | Etapa 5: snapshots dos dois `kind`, câmera ativa, alocar/resetar slots do pool; estragar número vivo e contexto/dono separadamente | Tipar o estado ativo e limitar exceções pelo contexto; não zerar campos do jogo para satisfazer comparação |
| Shader numericamente diferente, variante não reconhecida ou fallback caro | Etapa 6: matrizes, vértices, índices, camadas, programa/entrada e custo residual por quadro, em caso controlado | Separar erro de bytes, diferença de recorte e float; corrigir o caminho e publicar a cobertura |
| Z perdido, ids de alfa saturados ou auxiliares estourarem orçamento | Antes da etapa 7 completa: alternar alvos com oclusão viva e ids acima de 0x80; medir tiles, bytes e Resolve | Levar ao Lucas custo das alternativas da pergunta 6, com evidência de imagem |
| `gs_draw.c` consumir a folga da F2 ou descarte causar recodificação constante | Etapas 1 e 8: acertos/falhas por textura, pixels decodificados, hash de páginas, tempo de decodificação, upload e máximos dos vetores | Otimizar o maior custo real; não partir direto para pacote de texturas convertido ou VMX sem perfil |
| GPU assíncrona parecer rápida no cronômetro de CPU | Comparar submissão, conclusão GPU e apresentação; conferir quantos passos/imagens realmente avançam | Impedir que uma fila crescente passe por 30 fps, sem forçar sincronismo por desenho na build final |
| Janelas de rasterização perderem histórico de brilho, alvo ou sombra | Comparar uma sequência curta contínua com janelas progressivamente maiores na x86 | Fixar a janela mínima por caso antes de gastar a rodada PPC |
| FPU do console impedir casar capturas e ações por quadro | Testar introdução e caso de entradas fixas primeiro; confrontar estado e marca antes da captura | Não confundir divergência física já conhecida com endianness; se faltar referência, estimar gêmeo numérico ao Lucas |
| Nova luta chamar um dos stubs que a F2 não chamou | Contagem com nome e ponto da primeira chamada desde a etapa 3 | Falha explícita e inventário do escopo afetado; reestimar implementação, sem chamar o stub de função aprovada |
| Custo futuro do áudio ou boca divergente | Publicar margem real; catalogar as capturas afetadas por fase de voz, inclusive fala de personagem fora do quadro | F6 mede o custo e o sincronismo; a F5 não garante ambos por rodar muda |

## 7. Divisão futura e pontos que pedem o console

Este estudo foi feito por uma pessoa, sem agentes auxiliares. Para a implementação, a proposta segue o modelo da
9c: entregas pequenas com contrato, critério e integração por responsável, em vez de vários ciclos simultâneos.

| Frente possível | Arquivos/contrato principal | Pode adiantar sem console? | Integração necessária |
|---|---|---|---|
| Memória e posse dos recursos | Backend D3D e pontos protegidos do cache em `gs_draw.c` | Inventário e proposta, sim; segurança física, não | Responsável único por 1, 6 e 7, ou passagem explícita dos contratos entre autores |
| ISO convertida | Ferramenta de PC, `conv/`, leitor ISO e mensagem de recusa | Especificação, saída e casos negativos no PC, depois da etapa 1 | Lucas decide marca/mods; console prova boot e ausência da conversão de arquivo na carga |
| Cores dos efeitos | Structs de pacote e gravações de `src/battle/`, inventário estático | Sim, após contrato da etapa 3 | Integração com memória e imagens; upstream só em ramos protegidos |
| Memória da luta | Ferramentas de campos/donos, subalocações e união ativa | Sim; pode ser autor distinto do das cores | Compartilhar pontos de snapshot, sem aceitar exceções novas para encobrir falhas da outra frente |
| VU1 e efeitos HLSL | Três famílias de vértice, constantes/índices, alfa/Z e efeitos nativos | Tradução/especificação e referências, sim | Escrever em sequência se alterarem os mesmos alvos, declaração de vértices ou orçamento |
| Roteiros e regressão 3D | Roteiros, âncoras, `ChrView`, Shenlong e critérios de imagem | Preparar e conferir x86, sim | Builds integradas para PPC, revisão visual e controle no console com Lucas |

Se houver vários autores, usar branches separados para código/documentação independente. No WSL, executar
roteiros/PPC em fila única e coordenar qualquer leitura pesada; nenhum agente inicia outro emulador porque está
em outro branch. A crítica independente antes de etapas grandes pode ser só leitura, como prevê o guia; sua
adoção é para a implementação futura, não uma autorização para delegar esta tarefa.

Exigem o console: tamanho e liberação física dos recursos, limites da EDRAM, semântica do Resolve de Z/alfa,
compilação e resultado dos shaders no Xenos, tempos reais de CPU/GPU/Present, cache frio, Guide, XInput e sessão
manual. O WSL não os aprova. Build XDK, deploy e conferência no Linux ficam para as tarefas de implementação;
nenhum acesso ao console faz parte deste estudo.

Na entrega de cada etapa, registrar o que passou e o que não foi exercitado. Na final, uma tabela por roteiro e
por rede, com duração, contagem de quadros, p50/p95/p99/máximo, intervalos de apresentação, mínimo livre, tetos e
descarte pendente, capturas, memória comparada e falhas provocadas. Se uma etapa exceder o dobro do tempo previsto,
parar com estado e hipóteses documentados e apenas o trabalho provado em commit, conforme o processo do projeto.

## Respostas do Lucas (decisão 24)

1. **A luta (16/10/2026):** Goku (Early) contra Kuririn, os dois no traje padrão (0). O Vegeta (Scouter) ficou de
   fora pelo Ozaru, e o Raditz e o Nappa também, porque os dados dão a eles o Great Ape com lua no cenário
   ([inventario.md](inventario.md), seção 2). A etapa 3 repete para o Kuririn a prova da seção 2 do inventário
   (id, trajes e as tabelas de transformação e fusão) antes de congelar o roteiro; estágio e ações ficam com ela.
2. a 9. **Com as recomendações (16/10/2026):** 2b, 3a com a região provada e 3c, 4b, 5a com pausa, 6a medida cedo,
   7a, 8b e 9a. Texto em [decisoes.md](../../decisoes.md), decisão 24.

Preparo das etapas 3 e 4: [inventário das cores, ids e trajes da luta](inventario.md).

## Casos do console vindos da sessão da F4 (16/10/2026)

- **Etapa 1:** entrar na tela de versus do Torneio (o começo da carga da luta) e voltar aos menus: hoje saem
  quadrados brancos nela e, depois, o Duelo e o Treino quase todos brancos. Com o teto e o descarte, os menus seguintes
  têm de sair normais sem reiniciar (plano/F4/10.md, achado 1).
- **Depois da luta, no console:** a tela de resultado da História não avançou para a tela 9 no roteiro sem desenho
  (achado 3 do 10.md); medir com desenho e ver se é espera em tempo real antes de culpar o port.
