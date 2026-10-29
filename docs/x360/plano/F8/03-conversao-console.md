# F8: conversão inicial da ISO no console

[Índice](../../plano.md) | [Pendência 2](pendencias.md) | [Decisões 23 e 30](../../decisoes.md)

## Contrato e igualdade

`conv/iso_conv.c` usa o mesmo `Conv_Arquivo`, a cobertura integral por byte e `Conv_Icones360` do PC. Não recompõe
BPE nem muda tabelas ou posições. São 3.327 entradas, incluindo as duas transições da versão 3. O inventário C
percorre DATA e os três AFS somente depois de autenticar todos os 3.020.816.384 bytes pela identidade SHA256 da
USA original. Essa autenticação permite limitar o leitor ao layout conhecido; ele não pretende aceitar ISOs
arbitrárias ou modificadas. O SHA256 é C89, com inteiros u32, leituras por byte e vetores conferidos com hashlib.

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
- A instalação roda na thread de hardware 2, antes de criar jogo/áudio. As threads 4 e 5 nunca recebem E/S.
  A main só apresenta, lê os dois contadores por operações interlocked e usa caixas assíncronas com buffers
  estáticos. A tela de progresso tem texto e barra, sem arquivo de fonte nem shader novo.
- A caixa pergunta se deve converter e seleciona Cancel por padrão. As frases em inglês foram **aprovadas pelo
  Lucas em 29/10/2026** e ficam num único arquivo `iso_avisos.h`. Não há nomes de chaves ou `ambiente.txt`.
- Antes da pergunta, uma thread apaga apenas `game:\bt3-x360.tmp`, o nome reservado para a instalação interrompida.
  Não há retomada parcial. Falha em removê-lo recusa a operação; cancelamento também descarta o parcial anterior.
- A origem abre somente para leitura, compartilhando somente leitura. Exige 3 GiB livres no volume de destino,
  além da original. `CREATE_NEW` cria exclusivamente o temporário, de 3.020.816.384 bytes, abaixo do limite FATX de
  4 GiB; os nomes têm menos de 42 caracteres. Não há pastas com milhares de entradas.
- Copia toda a ISO, troca as entradas e grava o selo. Confere `WriteFile`, `FlushFileBuffers`, fechamento, reabertura
  e tamanho. Na reabertura, recalcula cada entrada da original e compara; compara também **todos** os intervalos
  preservados, incluindo índices, folga AFS, UDF e cauda. Reautentica a original antes da publicação.
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

## Tempo e como medir no Xenon

Estimativa de planejamento, **sem medição de console**: 15 a 40 minutos no HD/USB saudável; mídia lenta pode
exceder. Não basta contar uma leitura e uma escrita de 3 GB: há dois hashes completos da origem, a cópia integral,
conversão, reabertura e comparação integral, além da releitura das entradas convertidas. Com T = tamanho da ISO
e S = soma das entradas convertidas, são aproximadamente `5T + S` bytes lidos e `T + S` escritos. Nesta imagem,
S = 1.395.101.552 bytes: cerca de 16,50 GB lidos e 4,42 GB escritos, 20,91 GB de tráfego acumulado; o arquivo
individual continua com 3,02 GB. A 15 a 40 MB/s, só o tráfego custa cerca de 9 a 24 minutos. O custo do SHA256
escalar e dos conversores ainda não foi medido no Xenon. Reservar 40 minutos para a primeira sessão,
sem timeout de UI.

Para medir: usar cronômetro desde Convert até a caixa de sucesso, anotando mídia, espaço livre e se foi uma
abertura fria ou repetida. Com o diário habilitado para desenvolvimento, o adaptador registra milissegundos por
fase e o total por `GetTickCount`; subtrair o tempo de inspeção das caixas. A verificação tem duas passagens
(comparação e hash final), então a porcentagem dessa fase reinicia. Não é previsão de tempo restante.
Abrir e fechar a Guide durante cópia e verificação, confirmar progresso e resposta; desligar durante cópia e
abrir de novo para confirmar descarte, preservação da original e início do zero.

## Conferências sem emulador

Os comandos devem rodar sobre o commit final, com logs em `~/`. Não há qemu, save, FTP ou execução no console:

```sh
set -o pipefail
python3 port/x360/tools/teste_iso_console.py /srv/bt3/dados/iso/bt3.iso \
  --trabalho ~/conv-console/final 2>&1 | tee ~/conv-console/final.log
python3 port/x360/tools/teste_iso_tela.py 2>&1 | tee ~/conv-console/tela-final.log
python3 port/x360/tools/teste_iso_avisos.py 2>&1 | tee ~/conv-console/avisos-final.log
port/x360/tools/teste_som360.sh 2>&1 | tee ~/conv-console/som-final.log
python3 port/x360/tools/teste_iso_convertida.py /srv/bt3/dados/iso/bt3.iso \
  --dados /srv/bt3/dados/gamedata --trabalho ~/conv-console/pc-final 2>&1 | tee ~/conv-console/pc-final.log
port/x360/tools/teste_iso.sh ~/conv-console/final/bt3-x360.iso 2>&1 | tee ~/conv-console/leitor-final.log
python3 port/x360/tools/headless.py --alvo x86 2>&1 | tee ~/conv-console/x86-final.log
```

`teste_iso_console.py` compila **iso_instalar.c real**, o mesmo núcleo C e conversores, substituindo só as APIs
FATX pelas POSIX. Compara cada byte da ISO produzida à saída de `convert_iso.py`, tamanho, MD5 e SHA256.
A original permanece somente leitura e seus dois hashes são conferidos antes/depois. O comparador é visto
falhando com um byte corrompido em cópia. Provoca identidade inválida, leitura/escrita curtas, menos espaço,
sincronização/fechamento/reabertura ruins, truncamento, destino concorrente, bytes alterados em oito classes,
saída existente, reconversão e encerramento real do processo durante a cópia. A próxima abertura limpa e recomeça.

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
