# F8: avisos e escolha da ISO

[Pendências, itens 1, 3 e parte do 6](pendencias.md) | [Índice](../../plano.md)

## Levantamento dos textos visíveis

Busca nos fontes do backend e nos caminhos do port usados pelo 360, antes da tradução:

| Origem | O que o usuário vê | Tratamento |
|---|---|---|
| `f2/main360.cpp`, `MostrarRecusa`, única chamada a `XShowMessageBoxUI` | Antes: títulos e explicações em português para ISO original ou ausente; botão `OK` | Quatro motivos com títulos e explicações em inglês, abaixo |
| `f2/main360.cpp`, preparação e execução | Preto durante o arranque, amarelo antes do jogo, azul na execução sem desenho | Cores sem texto; o GS substitui a tela durante o jogo |
| `f2/main360.cpp`, `Port_Sair` e `AoMorrer` | Verde no sucesso, vermelho no erro ou na queda; botão volta ao dashboard | Sem texto para traduzir; falta de memória e exceção não tinham caixa própria |
| `f0/sonda.cpp` | Amarelo, azul, verde ou vermelho conforme a fase da sonda | Sem texto; diagnóstico somente no log |
| `teste_d3d/teste_d3d.cpp`, shaders e `gs/gs_d3d.cpp` | Figuras de teste e imagem do jogo | Não desenham avisos escritos pelo port |
| `port/src/gs/namefont.c` | Letras das folhas de nomes do disco, disponibilizadas ao port | Não contém avisos em português; tabelas de nomes em inglês |
| `port/src/gs/ui.cpp` e `port/src/plat_mem.c`, `MessageBoxA` | Opções, notificações e caixa do PC | `ui.cpp` não entra na build do 360; a caixa Win32 é excluída por `!PORT_X360` |

Não há outro renderizador de texto de diagnóstico no backend. Os menus e avisos do próprio jogo continuam vindo do
disco. `Diario_Escrever`, `printf` e `fprintf` do backend vão a `bt3xbx.log` e `saida.txt`; não são legendas das cores.
Os textos de diagnóstico locais ficam em português. Não se traduziu o código upstream de logs que já estava em inglês.

## Mensagens e identificação

| Motivo | Título em inglês | Orientação |
|---|---|---|
| `ISO_ORIGINAL` | `ISO conversion required` | Converter com `convert_iso.py` e copiar como `bt3-x360.iso` para a raiz do jogo |
| `ISO_NAO_SUPORTADA` | `Unsupported game version` | `Only the USA SLUS-21678 is supported.` e indicação da conversão no PC |
| `ISO_ILEGIVEL` | `Cannot read ISO` | Imagem ilegível, incompleta ou danificada: copiar novamente ou criar outra imagem do disco |
| `ISO_AUSENTE` | `ISO not found` | Indicar a raiz do jogo, o nome `bt3-x360.iso` e `default.xex` |

`iso360.h` compartilha os motivos entre leitor e interface. Antes da marca, o leitor confere:

1. PVD ISO 9660, setores de 2.048 bytes, tamanho físico igual ao volume e números nas duas ordens de bytes.
2. Raiz e registros atravessados, com limites de setor, de diretório e de volume antes de ler os campos.
3. Volume `SLUS_21678`, tamanho conhecido de 3.020.816.384 bytes e `SYSTEM.CNF` com `BOOT2` apontando a
   `SLUS_216.78;1`, `VER=1.00` e `VMODE=NTSC`. Aceita espaços, tabulações, LF e CRLF; valores repetidos são ambíguos.

Volume ou identificação incompatíveis dão versão não suportada, mesmo com a assinatura da convertida. Na USA
identificada, região da marca zerada dá original; assinatura válida dá convertida; bytes desconhecidos ou falha de
leitura dão ilegível. `SYSTEM.CNF` ausente num volume USA, nulo interno e tamanho inválido dão ilegível.

A identificação é estrutural, sem hash de 3 GB no console. Uma imagem alterada que conserve volume, tamanho e
identificação pública pode se parecer com a revisão aceita. A autenticação integral continua no instalador do PC,
pelos hashes da decisão 23. Desde [02-icones](02-icones.md), a marca precisa ter a versão 2; versões antigas recebem caixa própria.

## Busca e opções conservadoras

Sem `BT3_ISO`, prefere `game:\bt3-x360.iso` e depois tenta cada `game:\*.iso` até achar uma convertida compatível. Fecha cada imagem recusada e o enumerador;
só depois carrega as tabelas AFS e abre o handle de áudio da escolhida. Sem convertida, prefere orientar a conversão
de uma USA original encontrada; caso contrário, mantém o primeiro diagnóstico. Sem `.iso`, recusa como ausente, com o nome público esperado. `BT3_ISO` explícito é definitivo, inclusive se faltar ou apontar à original. A caixa continua assíncrona, com
buffers estáticos e apresentação da tela enquanto aguarda o sistema; o jogo não começa após recusa.

Não foi necessário perguntar: conservar caminho explícito e busca de outras imagens mantém os usos
de desenvolvimento; a decisão 30 exige rever a versão da marca e o item 9 pede a caixa sem ISO. Cores de falha permanecem cores, sem criar outra interface nesta tarefa.

## Provas sem console

Na raiz, com logs fora do repositório:

```sh
set -o pipefail
python3 port/x360/tools/teste_iso_avisos.py 2>&1 | tee ~/f8-avisos/teste-iso-final.log
port/x360/tools/teste_som360.sh 2>&1 | tee ~/f8-avisos/teste-som-final.log
python3 port/x360/tools/headless.py --alvo x86 2>&1 | tee ~/f8-avisos/x86-final.log
```

O teste novo compila o `iso360.c` real com `gnu89`, `-Wdeclaration-after-statement` e avisos como erros, usando a API
POSIX de teste já usada pelo áudio. Imagens esparsas inventadas, com diretórios e AFS mínimos, não contêm dados do
jogo. Cada processo verifica motivo, caminho escolhido, segunda chamada idempotente e leitura da entrada do AFS da
imagem escolhida. A ISO de teste do áudio foi atualizada para o contrato estrutural, sem reservar 3 GB físicos.

| Conferência da rodada final | Resultado |
|---|---|
| Identificação, recusas, limites e busca, 39 casos sintéticos, em 64 e 32 bits | `teste_iso_avisos: tudo certo` nas duas builds |
| Mesmo verificador sobre cópia com PVD estragado | Saída 1 e `FALHA`, exigidas pelo teste |
| Seis mutantes do leitor | Volume, BOOT2, revisão, tamanho, marca e parada na primeira ISO acusados em execução |
| Regressão de áudio | Núcleo e fluxo passam; seis mutantes do núcleo, cinco do fluxo e cinco das travas acusados |
| Build x86 do commit final | Sem falhas; 326 objetos sem depuração comparados com a base, só o carimbo `x360_passos.o` diferente |
| `.text` da x86 contra a base guardada antes da tarefa | Idêntica; SHA1 `43444ea4d03a6d448aa1ae74a631e35b356a7caa` |

O relatório externo registra o commit e os logs da rodada efetiva. Como só mudam o backend do console e seus testes,
não há alteração nos objetos do jogo nem necessidade de uma rodada de roteiros ou save no qemu nesta tarefa.
A build com `cl` do XDK e a aparência/abertura das caixas no 360 ficam para a revisão no Linux; não foram provadas no
WSL. As declarações novas do leitor ficam no começo dos blocos, e tamanhos físicos usam `__int64`.

A repetição em 32 bits usa `CC=~/f8-avisos/cc32.sh`, um invólucro externo que executa
`gcc -m32 -D_FILE_OFFSET_BITS=64 -isystem /usr/include/x86_64-linux-gnu "$@"`. A opção de arquivos grandes é da API
POSIX do teste; no console, os handles usam diretamente `GetFileSizeEx` e `SetFilePointerEx`.
