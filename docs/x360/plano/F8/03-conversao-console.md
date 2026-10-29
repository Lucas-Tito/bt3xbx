# F8: conversão inicial da ISO no console

[Índice](../../plano.md) | [Pendência 2](pendencias.md) | [Decisões 23 e 30](../../decisoes.md)

## Contrato e igualdade

`conv/iso_conv.c` usa o mesmo `Conv_Arquivo`, a cobertura integral por byte e `Conv_Icones360` do PC. Não recompõe
BPE nem muda tabelas ou posições. São 3.327 entradas, incluindo as duas transições da versão 3. O inventário C
percorre DATA e os três AFS com limites conferidos antes da autenticação. A identidade SHA256 dos
3.020.816.384 bytes da USA original é calculada na mesma passagem da cópia/conversão e precisa bater antes
da publicação. O leitor não aceita ISOs arbitrárias ou modificadas. O SHA256 é C89, com inteiros u32, leituras por byte e vetores conferidos com hashlib.

A marca versão 3 contém também o hash do executável **PC**, que depende do compilador. Trocar esse campo pelo
hash do `.xex` impediria igualdade literal e alteraria o contrato. Por isso `preparar_iso_console.py` compila a
ferramenta PC no host e chama a função `marca` do instalador existente. Gera, sem dados do jogo,
`build/x360/iso/iso_selo.h`, `selo.bin` e `conv_tool`. `build_f2.sh` faz isso antes de chamar o XDK e embute o selo
no console. Os arquivos gerados ficam fora do git. Não se mudou a versão nem a interpretação de qualquer campo.

Para reproduzir **todos os bytes**, inclusive a procedência na marca, conservar a ferramenta dessa build:

```sh
python3 port/x360/tools/preparar_iso_console.py
python3 port/x360/tools/convert_iso.py /caminho/bt3.iso ~/bt3-x360.iso \
  --conv-tool build/x360/iso/conv_tool
```

Usar outra ferramenta compilada de fontes ou compilador diferentes pode produzir a mesma conversão com os
64 bytes de procedência distintos, como já acontecia entre duas instalações PC. Não se promete igualdade entre
instaladores diferentes. O ensaio nativo usa o mesmo selo e a mesma ferramenta da build em ambos os caminhos.

## Segurança e fluxo

- `iso360.c` mantém a busca preferindo uma convertida compatível. Sem ela, a original identificada leva ao novo
  fluxo. Uma imagem de outra região ou ilegível mantém o aviso existente. Uma convertida antiga não serve como
  origem: a autenticação integral recusa e pede a original intacta.
- A instalação e o SHA256 da original rodam na thread de hardware 2, antes de criar jogo/áudio.
  A leitora usa a 3; o SHA256 da saída e a escritora usam a 1. As threads 4 e 5 nunca recebem E/S.
  A main só apresenta, lê os dois contadores por operações interlocked e usa caixas assíncronas com buffers
  estáticos. A tela de progresso tem texto e barra, sem arquivo de fonte nem shader novo.
- A caixa pergunta se deve converter e seleciona Cancel por padrão. As frases em inglês foram **aprovadas pelo
  Lucas em 29/10/2026** e ficam num único arquivo `iso_avisos.h`. Não há nomes de chaves ou `ambiente.txt`.
- Antes da pergunta, uma thread apaga apenas `game:\bt3-x360.tmp`, o nome reservado para a instalação interrompida.
  Não há retomada parcial. Falha em removê-lo recusa a operação; cancelamento também descarta o parcial anterior.
- A origem abre somente para leitura, compartilhando somente leitura. Exige 3 GiB livres no volume de destino,
  além da original. `CREATE_NEW` cria exclusivamente o temporário, de 3.020.816.384 bytes, abaixo do limite FATX de
  4 GiB; os nomes têm menos de 42 caracteres. Não há pastas com milhares de entradas.
- Lê cada byte da original uma vez, converte entradas e insere o selo antes de escrever em ordem. O hash da
  original é atualizado antes de alterar o buffer, e o da saída antes de cada escrita. Identidade incorreta
  no fim descarta o temporário. `WriteFile`, sincronização, fechamento, reabertura e tamanho continuam conferidos.
- A reabertura compara literalmente todos os índices que foram antecipados, incluindo a tabela inteira do AFS2.
  Relê também todas as entradas convertidas, primeiro MiB, selo e amostras das áreas preservadas, conforme abaixo.
- Só então fecha o temporário e usa `MoveFile`, sem substituição, para `bt3-x360.iso`. Um destino preexistente ou
  que apareça durante a conversão é preservado. Falhas tratadas removem o temporário criado pela operação.
- Se a própria original estiver nomeada `bt3-x360.iso`, recusa sobrescrevê-la e orienta usar `bt3.iso` para a
  original. Uma convertida antiga que ocupe o destino também exige liberar esse nome. Não renomeia nem apaga
  arquivos do usuário por conta própria. Essa é a opção conservadora para a colisão entre origem e destino.
- Depois do sucesso, fecha a interface, libera D3D e relança `game:\default.xex` pela thread de saída existente.
  Isso preserva a ordem do arranque: o heap fixo do PS2 precisa ser reservado antes das alocações do D3D.
  Cancelamento ou falha levam ao dashboard. A abertura seguinte prefere a convertida publicada.

## Textos aprovados pelo Lucas (29/10/2026)

| Uso | Texto literal |
|---|---|
| Título da pergunta | `Convert ISO` |
| Pergunta | `Convert your original USA ISO for Xbox 360? This takes up to 40 minutes and needs 3 GB of free space. Your original will be kept.` |
| Botões da pergunta | `Convert` / `Cancel` (padrão) |
| Progresso | `Checking ISO...`, `Copying ISO...`, `Converting ISO...`, `Verifying ISO...`, `Finishing...` |
| Orientação do progresso | `Please wait. Do not turn off the console.` |
| Título de sucesso | `ISO ready` |
| Sucesso | `Conversion complete. The game will restart.` |
| Título sem espaço | `Not enough space` |
| Sem espaço | `Free at least 3 GB in the game drive and try again.` |
| Título de falha e nome ocupado | `Conversion failed` |
| Falha | `The ISO could not be converted. Check your original USA disc image and storage, then try again.` |
| Nome ocupado | `Rename your original ISO to bt3.iso and try again. The name bt3-x360.iso is reserved for the converted ISO.` |
| Botão dos avisos | `OK` |

Só mudaram o título e o corpo da pergunta, o corpo da falha e a orientação para nome ocupado. Os demais textos
e Cancel como padrão foram mantidos. O prazo na pergunta é o texto aprovado; a estimativa continua sem medição
no console, conforme abaixo.

## Otimização pedida pelo Lucas em 29/10/2026

O caminho anterior lia `5T + S` e escrevia `T + S`, com T = 3.020.816.384 e S = 1.395.101.552 bytes.
O novo caminho lê T da original e escreve T da saída, sem remendar entradas após a cópia. O inventário antecipa
552.004 bytes de metadados e os conserva em cache imutável; esses bytes entram no SHA256 na ordem da ISO, mas
não são relidos da origem. O ensaio conta intervalos efetivos de `ReadFile` e exige T bytes e nenhuma sobreposição.

Há três buffers de 8 MiB + 1, com início alinhado a 128 bytes, e uma cobertura reutilizada de 8 MiB + 1.
As lacunas são divididas em peças de até 4 MiB; peças consecutivas são agrupadas em lotes normalmente
de 4 a 8 MiB para evitar uma chamada FATX por arquivo pequeno. Uma entrada com conversor cabe inteira num buffer
(a maior da USA tem 8.374.016 bytes). A thread 3 adianta a próxima leitura enquanto a 2 autentica/converte o
lote atual e a 1 calcula o hash e grava o anterior. Assim os hashes usam núcleos físicos diferentes:
a 1 divide o núcleo com a main (0), e a 3 divide o outro com a conversão (2). Cada pedido possui eventos de início e término;
um buffer só volta à leitora depois de terminar a escrita que o usou. Falhas drenam ambos os pedidos antes de
soltar buffers, fechar ou apagar o temporário. Falta de eventos, threads ou memória recusa sem publicar.
O total de trabalho reservado fica perto de 34 MiB, antes da memória do jogo.

A sobreposição usa `ReadFile` e `WriteFile` síncronos em threads e handles distintos, com E/S simultaneamente
pendente. Não depende de `FILE_FLAG_OVERLAPPED` nem de APIs desktop novas. Não usa E/S sem cache: início dos
buffers é alinhado, mas posições e comprimentos seguem exatamente as entradas do jogo. Não se presume que um
HD físico execute duas transferências em paralelo ou dobre a taxa por causa das threads. A main continua apenas
apresentando; o teste com pthreads registra pedidos de leitura e escrita sobrepostos e recusa afinidade 4.

### O que a reabertura confere

1. Sincronização, fechamento, reabertura e tamanho exato, como antes.
2. Comparação literal dos 552.004 bytes de índices antecipados, com seu cache autenticado da original.
3. CRC32 IEEE de cada uma das 3.327 entradas convertidas completas e da peça do selo.
4. Primeiro MiB completo, incluindo os descritores ISO e UDF iniciais.
5. Lacunas preservadas de até 128 KiB completas; nas maiores, primeiros e últimos 64 KiB de cada peça de até 4 MiB.
   A última amostra inclui o último byte da ISO. CRC32 esperado é calculado no buffer que será enviado à escritora.
6. Comparação literal adicional dos 512 bytes do selo.

São 1.451.394.452 bytes relidos, contando repetições entre índices, amostras e selo. Tráfego total:
**7.493.027.220 bytes**, contra 20.915.101.408 anteriores, redução de **64,17%**.
O SHA256 completo da saída é calculado durante a escrita e registrado no diário para comparação externa;
não depende da amostragem nem substitui a releitura. O ensaio o compara à hashlib sobre a saída inteira.

**Limite deliberado:** o miolo das lacunas preservadas maiores que 128 KiB não é relido. Uma corrupção silenciosa
ali, depois do hash e com `WriteFile`/flush bem sucedidos, pode passar. CRC32 também não é proteção contra
adulteração intencional nem oferece a resistência a colisões do SHA256. A identidade integral da origem, a
conversão com cobertura de cada byte, os índices completos, a marca, a original somente leitura e a publicação
exclusiva continuam exigidos. Isso conserva a segurança de instalação e recusa de origem, mas reduz a cobertura
da comprovação de persistência. Não se promete detectar todo erro físico de mídia. O teste demonstra o limite
com corrupção no deslocamento 2 MiB, aceita pelo console simulado e recusada pela conferência integral Python.
A conferência integral do PC permanece disponível para diagnóstico e para a prova byte a byte.

### Tempo e como medir no Xenon

Modelo de planejamento, **sem medição no console**, em MB decimais: leitura FATX a 15 a 40 MB/s e escrita a
10 a 30 MB/s. Para HD compartilhado, somar `4472,214 / R + 3020,816 / W` segundos de E/S, cerca de 213 a 600 s;
a sobreposição esconde CPU/latências, sem somar artificialmente as taxas do mesmo disco. Supor SHA256 escalar
a **6 a 20 ms/MB por fluxo**, de 18 a 60 s para cada ISO, na thread 2 para a origem e na 1 para a saída.
Para os conversores com cobertura, reservar 30 a 80 ms/MB de S, de 42 a 112 s, mais CRC32, pequenas transferências,
movimento de cabeças e flush. As faixas são hipóteses, não benchmark Xenon; o custo de 264 ms para converter
6,7 MB medido na decisão 18 serve apenas de ordem de grandeza. Estimativa nova: **5 a 15 minutos** em HD/USB
saudável, com possibilidade de exceder em mídia lenta. O texto aprovado continua dizendo até 40 minutos;
não se muda uma frase aprovada para acomodar uma previsão ainda não medida.

Medir do botão Convert à caixa de sucesso, com mídia, espaço livre, abertura fria/repetida e Guide anotados.
O diário agora registra total, fases, leitura, gravação, SHA256 da origem, SHA256 da saída e conversores em ms,
além dos bytes relidos e do digest da saída. Dividir os tempos de hash por 3020,816 e o de conversores por
1395,102 para obter ms/MB. Leitura/gravação são tempos acumulados dos pedidos, não parcelas que possam ser
somadas ao total por estarem sobrepostas; incluem espera do SO. Obter MB/s efetivos a partir de bytes/tempo e
medir também a fase de verificação e o flush. A barra usa Converting ISO durante a passagem conjunta e
Verifying ISO durante a releitura, sem reiniciar a mesma fase. Não é previsão do tempo restante.
Confirmar Guide responsiva, reinício e desligamento durante cópia; próxima abertura deve limpar e começar do zero.

## Conferências sem emulador

Os comandos devem rodar sobre o commit final, com logs em `~/`. Não há qemu, save, FTP ou execução no console:

```sh
set -o pipefail
python3 port/x360/tools/teste_iso_console.py /srv/bt3/dados/iso/bt3.iso \
  --trabalho ~/conv-console/otim-final 2>&1 | tee ~/conv-console/final.log
python3 port/x360/tools/teste_iso_tela.py 2>&1 | tee ~/conv-console/tela-final.log
python3 port/x360/tools/teste_iso_avisos.py 2>&1 | tee ~/conv-console/avisos-final.log
port/x360/tools/teste_som360.sh 2>&1 | tee ~/conv-console/som-final.log
port/x360/tools/teste_iso.sh ~/conv-console/otim-final/bt3-x360.iso 2>&1 | tee ~/conv-console/leitor-final.log
python3 port/x360/tools/headless.py --alvo x86 2>&1 | tee ~/conv-console/x86-final.log
```

`teste_iso_console.py` compila **iso_instalar.c real**, o mesmo núcleo C e conversores, substituindo só as APIs
FATX pelas POSIX/pthreads. Exige uma leitura de cada intervalo da origem, uma escrita por byte, releitura menor
que 1,5 GB, pedidos de E/S sobrepostos e SHA256 de saída em fluxo igual à hashlib. Verificadores de métricas e
digest rejeitam cópias adulteradas de um registro real; cópias compiladas com afinidade 4 ou buffer desalinhado devem falhar em execução.
Compara cada byte C contra a receita Python por `convert_iso.py --conferir --dados`, incluindo todos os intervalos
preservados e a arte. Em seguida apaga a C, executa a produção real Python e compara tamanho, MD5 e SHA256.
**Só uma ISO de teste é mantida por vez**; não cria `pc.iso` ao lado da C. Temporários de entradas do Python são
limpos pelo próprio contexto e as imagens inválidas/parciais são removidas após cada caso.

A original permanece somente leitura e seus hashes são conferidos antes/depois. A conferência Python integral
é vista recusando um byte alterado fora da amostra. Provoca identidade inválida descoberta no fim, índices ruins,
leitura/escrita curtas, espaço insuficiente, falta de alocação/eventos/threads, sincronização/fechamento/reabertura
ruins, truncamento, destino concorrente, bytes alterados em nove classes, saída existente, reconversão e
encerramento real durante a cópia. Inclui o meio da tabela do AFS2, fora da amostra normal. A próxima abertura
limpa o parcial e recomeça. Não se roda um segundo produtor de ISO ao mesmo tempo.

`teste_iso_tela.py` compila o fluxo C++ real sobre uma UI e fila de threads simuladas. Verifica cancelamento,
falhas, sucesso, espera pela Guide e apresentação contínua. Mutantes compilados têm de falhar em execução:
afinidade 4, espera bloqueante, confirmação como padrão e limpeza omitida. Não prova a aparência ou a Guide real.

`teste_iso_avisos.py` também compara literalmente todas as frases aprovadas da instalação, incluindo os botões
e as cinco fases em ordem. Compila cópias do cabeçalho adulteradas nos quatro textos alterados e exige saída 1
em execução de cada uma. Para o ajuste de textos, a rodada final repete esse teste e `teste_iso_tela.py` sobre
o commit final; as provas de conversão da implementação anterior permanecem registradas no relatório externo.

A regressão do áudio achou a imagem sintética ainda com marca versão 2. Agora ela usa `ISO_MARCA_VERSAO` do leitor;
o script inclui esse cabeçalho também na compilação do teste. Nenhum comportamento do som mudou.

### Correção após a recusa do XDK (29/10/2026)

O XDK recusou `INVALID_FILE_ATTRIBUTES`, que a simulação oferecia indevidamente. O adaptador agora compara
`GetFileAttributes` com `(DWORD)-1`, seguindo `f2/posix360.c`; a simulação retorna esse valor e deixa de definir
a constante. Antes da correção, compilar o adaptador real com esse cabeçalho reduzido reproduziu o erro.

A revisão das adições encontrou `GetFileAttributes` e `GetDiskFreeSpaceEx`, ambos presentes na camada de teste;
não há chamada nova a `GetFileAttributesEx` ou `MoveFileEx`, nem uso de flags `MOVEFILE_*`. A publicação usa
`MoveFile`, já usado no port, sem substituir o destino. As demais flags e constantes de arquivo usadas estão
declaradas na simulação. Os headers de `~/Documentos/CollectionUI/sdk/XDK` não estão acessíveis neste WSL:
essa revisão não comprova disponibilidade ou contrato das APIs no XDK. A compilação nativa e os testes do
adaptador usam agora o cabeçalho sem a constante; logs e escopo da rodada ficam no relatório externo.

A tabela da rodada final, os hashes, commits e o SHA1 da `.text` ficam no relatório externo
`~/entrega-conv-console.md`. Build XDK, contratos efetivos das APIs FATX, aparência das caixas/barra, reinício,
interrupção elétrica, desempenho Xenon e Guide real continuam para a revisão do Lucas. As simulações não os aprovam.
