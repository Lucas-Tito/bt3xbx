# Desmontagem após queda e retorno à FreeStyle

A sequência com teto passou no ensaio nativo do WSL; o retorno limpo à FreeStyle ainda exige XDK e console.

Base: `888072d1`, branch `codex/queda-freestyle`. Complementa a
[análise da queda no Final Flash](queda-relocate.md). Nenhuma prova deste documento depende de repetir o Broly.

## O caminho de saída

`Port_Sair` e `AoMorrer` entregam o encerramento à main. A main inicia `Saida_Threads`, uma auxiliar na
thread de hardware 1, antes de recuperar o dispositivo. Ela pede a parada das duas threads do GS e espera
os handles do jogo (3 s), da gravação (2 s) e do desenho (2 s), sempre registrando o resultado hexadecimal.
`WAIT_OBJECT_0` prova término; estado ocioso e sinal de parada não provam. Os handles do GS são emprestados,
sem outro `pthread_join` simultâneo e sem entrar nas travas que uma thread caída pode ter deixado tomadas.
A parada assíncrona da gravação não executa a callback síncrona que juntava e fechava o handle do
desenho: ambas são esperadas separadamente pela main. A parada síncrona antiga conserva sua callback.

A espera do GS no 360 consulta o encerramento fora da trava antes de esperar e a cada despertar de
até 50 ms. A publicação de trabalho e o `Port_StateLog` também consultam o pedido. Assim, uma queda
do desenho pode levar o jogo ao `Port_Sair(5)`, em vez de ele publicar trabalho para uma gravação já
encerrada. A consulta só encerra a thread do jogo, nunca as duas threads do GS. Se uma trava estiver
danificada, continuam valendo os tetos externos. No caso normal, a consulta do pedido custa um teste
e a espera costuma ser sinalizada antes dos 50 ms; seu custo no Xenon não foi medido.

Depois vêm os leitores em blocos dos filmes, os fluxos ADX, o vigia e o misturador. O cadastro de leitores
é inicializado antes da ISO e preserva a vida dos buffers. Cada leitor tem até 2 s para a trava e até 2 s
para terminar. Fluxos, vigia e misturador registram as esperas de até 2 s. `Som360_Parar` pede silêncio,
para o motor e sinaliza os produtores antes das esperas. Mesmo com um atrasado, espera o outro. Se algum
não terminar, preserva vozes e recursos que ele ainda pode usar. A queda só marca silêncio no filtro;
`StopEngine` e a desmontagem ficam na auxiliar, sem E/S nas threads de hardware 4 e 5.

A main consulta a auxiliar sem bloquear, com teto total de 15 s. Depois dos três handles gráficos
sinalizados, recupera a posse e apresenta a tela enquanto espera cargas/som. Antes disso, continua o dono
anterior, se ainda vivo; a main não disputa o dispositivo. Se o teto acabar, registra a fase, preserva
os recursos possivelmente em uso e segue direto ao painel. Também segue se uma espera retornar erro.
Não usa `TerminateThread`, não toma um dispositivo de thread viva nem libera memória sob ela.
O filtro consome também quedas das auxiliares; uma exceção na própria main tenta lançar o painel de uma
thread nova, pois já não há coordenadora para exibir a tela ou conduzir a desmontagem completa.

No caminho completo, a tela vermelha aceita um botão novo depois de soltar o anterior. A main devolve a
posse a `Saida_Video`: tira a callback de vblank, espera `BlockUntilIdle`, libera dispositivo e D3D.
A main limita essa fase a 2 s, sem executar a espera da GPU na thread que apresentava. A tela deixa de ser
apresentada na transição final de liberação. Uma GPU bloqueada também leva ao painel ao vencer o teto.
Outra auxiliar descarrega a saída de texto e fecha o diário, com teto externo de 2 s. Só então nasce a
thread de `XLaunchNewImage(NULL, 0)`. Se nem essa pilha puder ser criada, a main faz a chamada diretamente.
Uma falha de disco pode impedir a persistência das últimas linhas, mesmo com os tetos de saída.

A sequência esperada no log é:

```text
CRASH ...
saída: desmontagem das threads começou
saída: jogo: espera=00000000 teto=3000 ms
saída: GS gravação: espera=00000000 teto=2000 ms
saída: GS desenho: espera=00000000 teto=2000 ms
saída: carga blocos ...: espera=00000000 teto=2000 ms
saída: carga fluxos: espera=00000000 teto=2000 ms
saída: som vigia: espera=00000000 teto=2000 ms
saída: som misturador: espera=00000000 teto=2000 ms
som: parado (StopEngine, vozes e motor soltos)
saída: desmontagem das threads terminou; D3D liberável
voltando ao dashboard
saída: GPU: esperando ociosa (teto externo 2000 ms)
saída: GPU ociosa
saída: D3D solto
saída: solicitando XLaunchNewImage(NULL, 0) após fechar o diário
```

Leitores e som ausentes não geram todas essas linhas. `00000102` significa tempo excedido;
`FFFFFFFF`, falha de espera. O log registra a solicitação do lançamento antes do fechamento;
não afirma que o dashboard iniciou. Isso se comprova pela home respondendo e pela reabertura do jogo.
O anel do diário pode perder as últimas mensagens se a própria E/S ou sua trava estiver comprometida.

## Queda controlada, desligada por padrão

Em `ambiente.txt`, usar apenas uma variante por execução:

```text
BT3_QUEDA_TESTE=180
BT3_QUEDA_TESTE_THREAD=jogo
```

Trocar `jogo` por `desenho` para testar a dona do D3D. Aceita N de 1 a 1.000.000, decimal;
chave ausente, vazia, zero, inválida ou thread inválida desarma o teste. Thread omitida significa jogo.
A configuração é lida uma vez antes de criar as threads; desligada, só há a verificação sem efeito no
estado do jogo. A contagem começa na luta, ignora chamadas repetidas no mesmo quadro do GS e reinicia
se sair dela. No quadro N contado, lê o endereço zero por acesso `volatile`, uma única vez, sem escrita
fora da RAM. O mapa deve apontar `Port_QuedaTeste360`. O caminho do desenho requer
`BT3_GS=gpu`, `BT3_GS_THREAD=1` e `BT3_GS_DESENHO=1`, para garantir que a exceção seja nessa thread.

## Roteiro de aceite no console

1. Compilar Release no Linux, guardando `.map`, `.xex`, `.dat` e `.swap` da mesma build. Usar ISO convertida
   versão 3. Guardar cópia do save e começar com boot limpo na configuração habitual do Lucas.
2. Sem as duas chaves de teste, abrir uma luta, conferir a Guide, sair normalmente, chegar à home da
   FreeStyle e reabrir o título. Guardar o log antes de outra execução, pois o arranque o trunca.
3. Armar `BT3_QUEDA_TESTE=180` e `BT3_QUEDA_TESTE_THREAD=jogo`. Abrir Duelo, Goku contra Vegeta,
   arena inicial e trajes padrão. Não é preciso executar técnica. Depois de cerca de 6 s de quadros de
   luta, deve aparecer a tela vermelha. Soltar todos os botões, conferir que a Guide responde e apertar A
   uma vez. Exigir home da FreeStyle respondendo, sem laço de interceptação; guardar o log.
4. Reabrir com a chave retirada, entrar na luta e voltar normalmente. Esse passo prova que o ambiente
   continuou utilizável. Preservar também esse log.
5. Repetir 3 e 4 com `BT3_QUEDA_TESTE_THREAD=desenho` e a divisão do GS explicitamente ligada.
   Conferir a identidade da thread no `CRASH`, as esperas, a GPU ociosa e o D3D solto antes da solicitação
   de lançamento. Repetir cada variante três vezes, com a reabertura entre elas.
6. Depois das duas variantes e da saída normal passarem, pode repetir Final Flash contra Broly com as
   chaves retiradas. Esse confronto investiga a causa original; não substitui o aceite das duas quedas.
7. Se ficar preso na tela ou reaparecer o laço por mais de 30 s após o botão, fotografar a primeira
   mensagem e usar o botão físico para desligar. Não insistir em reabrir no laço nem cortar a alimentação
   durante gravação. Após religar, preservar o log antes de abrir o jogo. Retirar ambas as chaves ao terminar.

## Escrita fora da RAM emulada

É possível recusar a relocação antes da primeira escrita. Em `EftBurst_RelocateModel`, a leitura inicial
é `groupCount`, mas logo se escrevem `relocated`, `groups`, cada `verts`/`tris`, `tex` e `ptr0`/`ptr4`.
Checar só o destino final do TexFile chega tarde. A faixa relevante para esse pacote é o heap emulado
`[0x003BE730, 0x02000000)`, não toda memória virtual gravável do título. A base inferida `0x81B4AC80`
seria recusada antes de ler o cabeçalho; não há prova de que esse fosse o endereço real escrito no console.

Uma guarda só do 360 poderia validar cabeçalho e todos os intervalos de escrita, antes de alterar qualquer
campo: `inicio >= base`, `inicio <= fim` e `tamanho <= fim - inicio`, sem somar antes de verificar o
estouro. Contagens devem caber antes das multiplicações por 0x18 (grupo) e 0x40 (textura); deslocamentos
precisam caber antes de somar a base. Para impedir corrupção de outro objeto do próprio jogo, é necessário
conferir também o tamanho real do pacote e do bloco alocado, não apenas a faixa da RAM.

Custo estimado: O(1) para recusar uma base externa; a validação completa acrescenta uma passada
O(grupos + texturas), comparações e leituras de cabeçalhos em cada relocação. Não há alocação nem E/S no
sucesso. É um ponto de carga, mas sua frequência e seus ciclos no Xenon não foram medidos. Uma chave
lida no arranque acrescentaria um ramo por chamada quando desligada. Não se implementou guarda no
caminho da relocação sem a prova exigida pela tarefa. Proteger páginas do título inteiro não distingue
uma escrita errada numa página que também precisa continuar gravável.

## Provas no WSL e limites

`python3 port/x360/tools/teste_saida360.py --mutantes` compila `saida360.h` e `queda_teste360.h` reais
contra uma API determinística. São onze cenários: sequência completa, jogo/gravação/desenho atrasados,
espera com erro, carga atrasada, som/GPU/diário presos, falha de criação e chave de teste. Oito cópias
sabotadas precisam falhar, incluindo uma sem teto externo encerrada pelo limite do executor.
O ensaio percorre a sequência real de chamadas; não simula a AV, o SEH nem o driver.

`python3 port/x360/tools/teste_paradas360.py` compila o `blocos.c` real e recorta os corpos reais de
`Fluxo_Parar` e `Som360_Parar`. Confere dez casos, sinais, resultados das esperas, preservação de
recursos de threads atrasadas, a saída cooperativa da espera do GS fora da trava e cinco mutantes. A API substituída não mede concorrência ou latência real.
Os testes não usam dados do jogo nem emulador, portanto não tomam `~/wsl.lock`.

A build x86 é comparada com a referência preservada em `~/queda-base/x86`, já usada na entrega
`888072d1`: `f5_final_confere.py objetos` exige `objcopy --strip-debug` e `cmp`, permitindo apenas
objetos de teste. São 398 de 399 iguais, com diferença só em `obj/x360_passos.o` (carimbo).
A `.text` é idêntica também à da entrega `888072d1`, SHA1
`43444ea4d03a6d448aa1ae74a631e35b356a7caa`. A preparação gera as 72 tabelas com
`gen_data.py --asm` apontando para os dados só leitura; uma ligação sem essas tabelas não vale como prova.
A rodada final, seus carimbos e os logs ficam em `~/freestyle-provas/`; o relatório tem até 300 palavras.

Permanecem pendentes a compilação XDK e todo o aceite no console, inclusive a responsividade da Guide,
o retorno à FreeStyle, a reabertura e a hipótese de corrupção anterior. O código de recuperação não
repara memória do dashboard já corrompida antes da exceção. Não houve acesso ao console, execução de
PPC, roteiros ou alterações na worktree e no serviço `queda-relocate-888072d1`.
