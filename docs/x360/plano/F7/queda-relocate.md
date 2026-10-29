# Queda no Final Flash e saída após exceção

[Índice](../../plano.md) | [Cobertura da F7](01.md)

## Evidência recebida

Log de 28/10/2026, main `c3dcc4d2`, mapa da mesma árvore recompilada. O mapa situa
`Res_RelocateOffsets` em `822520F0`: Iar `8225211C` está em +0x2C, LR `82252114` em +0x24.
Sem o código de máquina dessa build, o mapa não identifica qual instrução de carga leu `02AEAE84`.
Não basta esse endereço para concluir que o deslocamento, em vez da própria base, estava errado.
A função preserva `entOfs`, `pixOfs` e `clutOfs`, escrevendo os ponteiros em campos separados.
Repeti-la sobre o mesmo recurso, base e cabeçalho não soma os deslocamentos duas vezes.

As pressões 4..11 foram no quadro 29877; a 13, no 29878. Todas disseram `cabe`, com 88 MB livres,
0 esperas da GPU. O log chegou ao quadro 32040 junto da queda, mais de 2100 quadros depois.
Isso enfraquece a hipótese de falta imediata de textura; não exclui corrupção anterior ou uma corrida.

## Caminho até a FreeStyle, lido no código

`AoMorrer` grava o contexto, descarrega o diário e chama `Som360_Queda` (`StopEngine`, salvo quando a própria
thread do XAudio2 caiu). Na queda da thread do jogo, pede `Port_GsPararJa` e `GsDraw_DesenhoPararJa`, solta o
D3D se for o dono e encerra só essa thread. Os pedidos ao GS são assíncronos. O desenho pode estar trabalhando;
um sinal de parada não é prova de término. A thread principal já vê `gTerminou` antes desses pedidos.

A principal espera até 2 s pela posse do D3D e pode tomá-la à força, cancela a callback de vblank e apresenta
a tela vermelha. Após soltar os botões e apertar um de novo, espera até 3 s pela thread do jogo quando há
gravação, mas não confere o resultado da espera. Não há `join` explícito das duas threads do GS nesse caminho.
Chama `Som360_Parar` (leitura, vigia, misturador, vozes e motor), `BlockUntilIdle`, libera D3D, fecha o diário e
cria a thread que chama `XLaunchNewImage(NULL, 0)`. A saída normal por `Port_Sair` usa a parada síncrona do GS.

O log recebido contém `som: StopEngine na queda`, `voltando ao dashboard`, `som: parado` e `diario: fim`,
sem aviso de thread de áudio excedendo 2 s ou tomada forçada do dispositivo. Isso prova passagem por essas
etapas, não o sucesso do lançamento seguinte. Não há instrumentação após o fechamento do diário.
O GS ainda vivo durante a liberação do dispositivo é uma hipótese concreta de segunda exceção no título.
Não há prova de thread do título sobrevivendo a um lançamento concluído. Os buffers físicos de áudio e texturas,
a GPU e o tratador/plugin do ambiente continuam hipóteses para o laço; não há log da FreeStyle para distingui-las.
Não se alterou o encerramento com cancelamento forçado de threads ou liberação especulativa de memória.

Há uma hipótese adicional anterior à tela vermelha: na tabela crua das transições, o deslocamento 2 vale
0x80020000 quando lido como big-endian, e o 3 vale 0x00FC0200. O primeiro entra em
`EftBurst_RelocateModel`, que também **escreve** no cabeçalho e nos grupos antes de chegar ao TexFile.
Se o console mapear esse endereço fora da RAM emulada como memória gravável, pode haver corrupção antes da
leitura que finalmente lançou a exceção. Isso também é compatível com um ambiente residente só recuperado
após desligar. Não foi medido o mapeamento físico desse endereço no console.
Se a instrução em Iar estiver lendo `hdr+4`, o endereço registrado admite a base de pacote 0x01B2AC80:
base + 0x00FC0200 + 4 = 0x02AEAE84. Nessa hipótese o modelo apontaria para 0x81B4AC80.
É uma inferência testável pelos novos registradores e pelo código de máquina, não uma prova fornecida pelo mapa.


## Prova segura no console, pendente

1. Compilar este branch no XDK e guardar `.map`, `.xex`, `.dat` e `.swap` correspondentes. Manter cópia do save.
2. Após boot limpo, conferir uma saída normal pela Guide e reabrir. Registrar o ambiente e os plugins ativos.
3. Repetir o confronto e a técnica. Se cair, não insistir em abrir o título no laço. Fotografar a tela vermelha,
   apertar um botão uma vez e observar a primeira mensagem do dashboard. Se entrar no laço, usar o botão físico
   de desligar, sem cortar a alimentação durante gravação. Após religar, preservar o log antes de abrir o jogo.
4. Resolver Iar, LR e candidatos `CRASH pilha` pelo mapa da build. Os registradores são os valores no ponto da
   exceção, não necessariamente os argumentos originais. Confirmar os retornos pelo código de máquina.
5. Só com uma forma de restaurar a configuração conhecida, comparar boot limpo com a configuração habitual e
   sem o plugin de interceptação de exceções. Uma mudança por vez, preservando os logs de título e dashboard.
   Não instalar plugins novos nem provocar falha artificial no console como primeiro teste.

## Limites atuais

O percurso sintético da pilha passa no WSL e rejeita o mutante sem guarda de progressão.
SEH, a compilação XDK, os retornos reais e a saída para a FreeStyle exigem console.
Os resultados do roteiro e a atribuição do recurso serão registrados após a execução.


## Conversão e instalação, já conferidas no WSL

Os ids 0x1BF e 0x1C0 não estavam em `sFaixas`: o conversor anterior responde `sem conversor` para
`00446.bin` e `00447.bin`. O leitor direto não passa pelos 82 ganchos que `unpacks.py` confere.
O formato [transicao](../../formatos/transicao.md) cobre a câmera, o modelo e o TexFile das partículas.
O censo das três partições examinou 68.601 arquivos e achou exatamente esses dois pacotes com a mesma estrutura.
As 4.037 entradas do censo anterior (3.325 arquivos e 712 conteúdos) mantiveram o resultado idêntico.
Acrescentaram-se apenas as duas transições. A ISO versão 3 passou na conversão, reabertura integral,
recusas e estragos do teste oficial. O leitor recusa a versão 2. É obrigatório reconverter a ISO original.
A saída válida está fora do git, em `~/queda-provas/iso-v3/bt3-x360.iso`.
O vínculo da lacuna com a queda do console ainda depende da reprodução; não é conclusão baseada só no mapa.


## Fechamento desta tentativa no WSL

A reprodução não foi executada. A única espera desta tarefa no `flock` ficou mais de 70 minutos
em `locks_lock_inode_wait`, acima do dobro do teto de 30 minutos preparado para a reprodução inicial.
A inspeção confirmou um detentor externo e outras esperas; nenhum deles foi sinalizado.
Foi encerrado somente o PID 635045, que ainda aguardava a trava, conforme a regra de interrupção do `AGENTS.md`.
Nenhum qemu ou roteiro desta tarefa começou. O confronto `queda_relocate` é uma receita preparada,
não um roteiro aprovado. Os passos de dano e o segundo Final Flash ainda precisam de ajuste por execução.

Provas realizadas: os dois pacotes convertidos com cobertura integral e 24 corrupções recusadas;
os números dos vértices, triângulos, texturas e pacotes conferidos independentemente, imagens preservadas;
as 4.037 entradas antigas do censo com o mesmo resultado; ISO versão 3 no teste oficial completo;
leitor com as versões 1 e 2 recusadas e oito mutantes acusados; caminhada sintética da pilha e mutante;
os leitores reais na x86, duas relocações seguidas em cada pacote, e um caso com contagem estragada recusado.
O verificador de conversão também falhou sobre a cópia da saída com um byte trocado e sobre o cadastro antigo.
As builds PPC compilaram, mas seus executáveis não foram rodados.

Faltam: reproduzir a queda na PPC sem mapa com a build anterior e dados do conversor anterior,
identificar a chamada e o LEU no confronto, executar o caso isolado PPC com dados crus e convertidos,
aprovar `SEMMAPA=1 IMG=1 roteiros.sh duelo queda_relocate`, `save_mc.sh` e seu julgamento
`f5_final_confere.py save-mc`, além da compilação XDK e do console. Não há tabela final de execução aprovada.
O relatório externo é `~/entrega-queda-relocate.md`. A pergunta sobre arena e trajes não recebeu resposta;
a receita usa arena inicial, traje 0 e um membro sem itens por lado.

Para retomar, os scripts de execução estão em `~/queda-sonda.sh`, `~/queda-baseline-ppc.sh`,
`~/queda-casos.sh` e `~/queda-oficial.sh`. Reservar a trava uma única vez e encadear os passos necessários.
A hipótese prioritária é a transição crua; a correspondência com o endereço do console permanece inferência.
