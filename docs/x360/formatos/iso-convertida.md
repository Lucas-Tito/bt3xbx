# ISO convertida no PC, versão 1

[Decisão 23](../decisoes.md) | [tarefa da etapa 2](../tarefas/f5-etapa2-iso.md)

O instalador `port/x360/tools/iso_convertida.py` lê a ISO original USA e usa os conversores existentes da
`conv_tool`, pelo `--saida`. Não altera o tamanho de 3.020.816.384 bytes, posições, tamanhos, tabelas ISO 9660,
UDF ou AFS. A saída é exclusiva do port. O 360 exige esta ISO: `Iso_Iniciar` (`port/x360/src/iso360.c`) procura
a assinatura da marca e, sem ela, recusa a imagem com uma caixa de mensagem do sistema; da convertida, a carga só lê
([plano/F5/02.md](../plano/F5/02.md), "Lado do 360"). Os `mods/` continuam no formato do PS2, convertidos ao abrir,
até as últimas fases.

## Origem e conversão

Só se aceita esta imagem inteira, aberta apenas para leitura:

| Identidade | Valor |
|---|---|
| Região | USA, SLUS-21678 |
| Tamanho | 3.020.816.384 bytes |
| MD5 | `350404a81de912347c8b262542fed970` |
| SHA256 | `93f1f911e9a2bbdf92e5794fc3075ddf3f87db3df4b1cb8cfbb0448721c91172` |

O SHA256 foi medido na mesma original cujo MD5 foi fornecido na tarefa. Não há aceitação por nome de arquivo,
região presumida ou somente tamanho. O MD5 e o SHA256 são novamente medidos antes da publicação.

`conv_tool --lista-conversores` consulta `Conv_FormatoDe`, a mesma escolha do `--disco`, nas três partições.
O instalador cruza a lista com todas as entradas reais da ISO, extrai só as selecionadas para um temporário,
chama `--saida` em lotes e exige conversão com cobertura integral, lista completa e tamanho idêntico. Grava o
resultado na posição original de cada entrada, numa cópia temporária da imagem. Os conversores não mudaram.
Os nomes e as faixas não são duplicados no Python.

Ficam preservados os fluxos BPE, suas caudas e as animações comprimidas. As tabelas dos pacotes e os dois
campos do cabeçalho BPE (`rawSize`, `packSize`) são trocados como no `--saida` atual; o fluxo a partir do byte 8
permanece em bytes (`Conv_Bytes`, em `menus.c`, `som.c` e `personagem.c`). Não se descomprime nem se recomprime
conteúdo para gravá-lo na ISO. O `--disco` continua conferindo também os
conteúdos após a descompressão, que permanecem a cargo dos ganchos do jogo. SLUS, DBZP, som, filmes, módulos,
nomes, datas, tabelas e preenchimento mantêm os bytes originais, exceto pela marca especificada abaixo.

## Região da marca e prova

A marca ocupa `[0x3DF400, 0x3DF600)`, 512 bytes, dentro da folga do cabeçalho de `DATA/PZS3US0.AFS`.
Todos os números abaixo vieram da imagem identificada pelos dois hashes, e são conferidos pelo instalador.

| Região do AFS0 | Deslocamento relativo | Deslocamento absoluto | Tamanho |
|---|---|---|---|
| Arquivo AFS0 inteiro | `0` | `0x3DF000` | `0x3800` (14.336) |
| Assinatura e contagem, uma entrada | `0..8` | `0x3DF000` | 8 |
| Par deslocamento/tamanho da entrada 0 | `8..16` | `0x3DF008` | 8 |
| Par opcional da tabela de nomes | `16..24` | `0x3DF010` | 8 |
| Folga original, toda zerada | `24..0x800` | `0x3DF018` | 2.024 |
| Marca reservada | `0x400..0x600` | `0x3DF400` | 512 |
| Entrada 0 | `0x800..0x29A0` | `0x3DF800` | `0x21A0` (8.608) |
| Registro de nome | `0x3000..0x3030` | `0x3E2000` | 48 |

A região é livre pelo formato e pelos leitores do port, não apenas por conter zeros:

1. O diretório ISO aponta o AFS0 para `0x3DF000`, tamanho `0x3800`. Nenhum outro arquivo ou diretório
   ISO o sobrepõe. O programa confere registros, os números nas duas ordens de bytes, limites e sobreposição.
2. O AFS0 tem uma entrada. Seu cabeçalho termina em 24, incluindo o par opcional dos nomes. A entrada e os
   nomes ficam nos intervalos da tabela. O programa confere os limites e a ausência de sobreposição da marca
   com os três intervalos ocupados. A marca não altera qualquer campo ou tabela do AFS.
3. `port/x360/src/iso360.c`: `afs()` lê exatamente 8 bytes e depois `n * 8` bytes da tabela; não lê a folga
   nem os nomes. `Iso_Abrir` para `pzs3us0/00000.bin` começa na entrada, em `0x3DF800`, e `Iso_Ler` limita
   cada leitura ao seu tamanho. `achar()` lê os setores do diretório, em outra região.
4. `port/src/plat_file.c`: `ADXF_LoadPartitionNw` só define o nome da pasta; `ADXF_OpenAfs` transforma o par
   partição/id no caminho da entrada. O jogo não pede o AFS bruto: `ADXF_Open` pede arquivos sob `disc/DATA/`,
   enquanto as partições são abertas pelo caminho anterior. `src/sys/file.c` usa esses dois caminhos, sem
   leitura física de setores da imagem. As tabelas do executável vêm do SLUS e do DBZP, fora do AFS0.
5. A ISO também contém UDF, com descritores e metadados em lacunas que uma leitura só do ISO 9660 poderia
   tomar por espaço livre. A região escolhida pertence ao interior do arquivo AFS0 e é folga pelo formato AFS;
   não é uma lacuna ISO, área do sistema, setor final ou metadado UDF. A comparação integral preserva também
   cada byte UDF, sem precisar reescrever ou reinterpretar suas tabelas.

O contrato vale para a imagem original identificada e os leitores atuais do port. Uma futura mudança que leia
o AFS inteiro ou reaproveite a folga precisa rever esta reserva. Os 512 bytes são a única alteração fora das
entradas convertidas; a região completa era zero, e a folga antes e depois dela continua obrigatoriamente zero.

## Conteúdo da marca

Inteiros em little-endian, hashes em bytes binários. A marca inteira tem 512 bytes e comparação exata.

| Posição relativa | Tamanho | Conteúdo |
|---|---|---|
| `0` | 16 | ASCII `BT3X360-ISO`, seguido por cinco zeros |
| `16` | 4 | Versão do contrato: 1 |
| `20` | 4 | Tamanho da marca: 512 |
| `24` | 8 | Tamanho da origem: 3.020.816.384 |
| `32` | 16 | MD5 da origem |
| `48` | 32 | SHA256 da origem |
| `80` | 32 | Identidade SHA256 dos fontes do instalador e dos conversores |
| `112` | 32 | SHA256 do executável `conv_tool` efetivamente usado |
| `144` | 368 | Zeros reservados |

A identidade dos fontes é o SHA256 da concatenação de `nome ASCII + NUL + conteúdo`, para todos os `.c` e `.h`
de `port/x360/conv/` em ordem de caminho, seguidos de `iso_convertida.py`. Identifica inclusive mudanças de
conversores ainda não usadas nesta imagem. O hash do executável distingue builds de compiladores diferentes.
A reabertura exige a mesma ferramenta e os mesmos fontes; conserve a `conv_tool` da instalação. A marca
identifica procedência e versão, sem substituir a conferência dos bytes da imagem.

## Publicação e recusas

Tamanho, marca de reconversão e ambos os hashes são conferidos antes de criar qualquer temporário. Saída
existente, inclusive enlace pendente, é recusada antes disso. A original nunca é aberta para escrita.

O temporário e os arquivos intermediários ficam numa pasta exclusiva ao lado da saída. A imagem temporária é
sincronizada, fechada e reaberta. Só depois da conferência completa e dos hashes finais da origem é renomeada
para o destino. No Linux/WSL usa `renameat2(RENAME_NOREPLACE)`; no Windows `os.rename` recusa destino existente.
Assim, também se recusa uma saída que apareça durante a conversão, preservando-a. A limpeza automática remove
intermediários nas falhas tratadas. Um encerramento forçado pode deixar a pasta temporária, sem publicar a ISO.

## Reabertura e critério reproduzível

Na reabertura, as tabelas ISO/AFS são relidas e comparadas às originais. Cada entrada convertida precisa ser
igual ao arquivo do `conv_tool --saida`; todas as outras entradas e todos os intervalos fora das alterações,
até o último byte da imagem, são comparados à original. Isso inclui as tabelas, as folgas dentro dos AFS, a
área do sistema, UDF e a cauda da mídia. Tamanho e marca também são conferidos.

A conferência externa usa `--dados` apontando aos dados originais extraídos, em outra execução de `--saida`,
em vez de reutilizar os arquivos extraídos pelo instalador. O inventário inclui as 68.601 entradas AFS e os
29 arquivos soltos, 68.630 no total. Abrange os 68.611 do teste anterior (entradas, SLUS, DBZP e dados soltos)
e mais 19: `SYSTEM.CNF` e 18 módulos sob `IRX/`.

Compilar e converter, na raiz do repositório (Linux/WSL, GCC e Python 3):

```sh
gcc -std=c89 -O2 -Wall -o port/build/conv_tool $(find port/x360/conv -maxdepth 1 -name '*.c' ! -name conv_jogo.c | sort)
python3 port/x360/tools/iso_convertida.py /caminho/bt3.iso ~/bt3-convertida.iso
python3 port/x360/tools/iso_convertida.py /caminho/bt3.iso ~/bt3-convertida.iso --conferir --dados gamedata
```

O critério completo recompila a ferramenta, converte, reabre contra os dados extraídos, provoca as recusas de
um byte mudado, truncamento, reconversão, saída existente e enlace pendente. Estraga numa cópia um arquivo
convertido, um preservado, uma área fora deles, uma tabela AFS, uma tabela ISO, a folga AFS e a versão da marca,
exigindo falha em cada caso. O próprio critério é visto recusando uma cópia com origem estragada, e a guarda
de cobertura recusa uma resposta adulterada do `--saida`. Confere publicação concorrente e termina com `--disco`, cobertura integral de
todos os arquivos e conteúdos descomprimidos e os hashes finais da original. Não usa emulador.

```sh
set -o pipefail
python3 port/x360/tools/teste_iso_convertida.py /srv/bt3/dados/iso/bt3.iso \
  --dados /srv/bt3/dados/gamedata --trabalho ~/f5-iso/rodada-nova 2>&1 | tee ~/f5-iso/rodada-nova.log
```

A pasta de trabalho precisa ser nova; não se apaga uma rodada anterior automaticamente. Guarda os logs,
commit, diff, hashes e uma ISO válida. As cópias estragadas são removidas. A conversão precisa de cerca de
6 GB temporários, além da original; a rodada do critério precisa de espaço para uma cópia adicional.
