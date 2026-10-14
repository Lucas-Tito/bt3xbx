# Guia de port para Xbox 360: processo, ferramentas, setup e aprendizados

Escrito durante o port do BT3 para servir de ponto de partida a outros ports (outros jogos, outros
chats). O que é específico do BT3 está marcado; o resto vale para qualquer decomp de console
little-endian (PS2, PS1, GameCube não, que já é big-endian) levada ao 360.

Como usar num chat novo: peça para ler este arquivo, o `CLAUDE.md` deste repositório (modelo de
regras) e `docs/x360/decisoes.md` (o porquê de cada escolha). As pesquisas em
`docs/x360/pesquisa/01..03` têm o detalhe: 01 é o setup do 360 e as lições do CollectionUI e do
hiddriver, 02 são os ports de referência (SM64, MK64, Virtua Fighter), 03 é o estado da decomp do
BT3.

Mantido vivo: cada aprendizado que custou uma rodada entra aqui (seção 6) no mesmo commit da
correção.

---

## 1. O setup

Três máquinas, cada uma com um papel. Ninguém edita código fora da máquina de trabalho.

| Máquina | Papel | O que tem |
| --- | --- | --- |
| Linux de trabalho | edita, compila o `.xex`, sobe no console, analisa texto | repositório, Docker com o XDK, `gh`, Python |
| Windows do Lucas (192.168.0.5) | roda os testes com os dados do jogo | OpenSSH Server, WSL Ubuntu 24.04, o ISO, os builds de teste |
| Xbox 360 E RGH | o alvo | kernel 2.0.17150, FreeStyle 3.0.780, DashLaunch (hiddriver, usbsecpatch), **sem xbdm**, rede por cabo |

### 1.1 Linux de trabalho

- XDK 2.0.21256 extraído em `~/Documentos/CollectionUI/sdk/XDK`. Nunca vai para o repositório.
- Imagem Docker `collectionui-xdk:light`: wine 11.18 e o `cl.exe` 16.00.11886 (PowerPC). A receita
  está em `~/Documentos/CollectionUI/toolchain`. O wine veio do master sem tag, então faça backup
  da imagem antes de reconstruí-la.
- `port/x360/tools/build.sh`: monta `lib/xbox` inteiro só leitura, roda como root, faz `chown` no
  fim e gera o SPA no host, porque a imagem não tem Python. Saída: `build/x360/<CONFIG>/default.xex`.
- Cadeia: `cl` (`-TC`, C89) → `link -XEX:NO -MAP` → `imagexex -config:xex.xml`, com
  `/section:<TitleID>=spa.bin` para o nome e o ícone. Nunca `<dvdxgd2/>` em `mediatypes`.

### 1.2 Windows + WSL (decisão 6)

Este Linux não pode receber o ISO nem rodar o jogo. O Windows tem o OpenSSH Server (porta 22 só
para a rede local), e o comando chega daqui por SSH (`Host bt3win` em `~/.ssh/config`, chave
própria só para isso). Para cá só volta texto.

- Preparo versionado e idempotente: `port/x360/tools/preparar-wsl.sh`. Ele instala o cross
  `gcc-powerpc-linux-gnu`, o `qemu-user`, o clang/LLVM 22 do apt.llvm.org e o SDL3 compilado do
  fonte, e cria o usuário com sudo sem senha (o SSH não tem terminal).
- O clone de lá usa uma deploy key **só de leitura**. O código vai daqui por
  `port/x360/tools/wsl.sh`:
  - `enviar`: põe o clone no mesmo commit daqui (que já tem de estar no GitHub) e aplica por cima
    as alterações sem commit, por tar e base64. Grava `port/build/ENVIADO`, e toda build carimba a
    origem com ele;
  - `script < arquivo.sh`: roda um script bash lá, sem passar aspas pelo `cmd` do Windows;
  - `rodar 'cmd'`: um comando curto;
  - `puxar`: volta o clone para a `main` atualizada.
- **Processo longo:** rode-o com a sessão SSH aberta (o `Bash` em segundo plano daqui) e com
  `tee` para um log. `nohup ... &` dentro do WSL morre quando a sessão fecha.

### 1.3 Console

- FTP da FreeStyle (`xbox`/`xbox`), só com o console parado na FreeStyle, uma conexão por vez,
  sem `QUIT` e sem `REST`. `port/x360/tools/subir.py` acha o console pelo MAC (`ip neigh`, prefixo
  4c:0b:be), envia com nome temporário, renomeia e confere o md5 baixando de volta.
- Pasta: `/Hdd1/HOMEBREW/bt3xbx`. O `.xex` se abre à mão (sem xbdm não há como lançar remoto).
- O relógio do console é zerado: não use datas de arquivo para nada.

---

## 2. As ferramentas (todas em `port/x360/tools/`)

### Build e deploy do 360

| Ferramenta | O que faz |
| --- | --- |
| `build.sh` | compila o `.xex` no Docker do XDK |
| `make_spa.py` | gera o SPA (XDBF: XTHD, XSTC, nome, ícone 64x64), porque o `spac.exe` é .NET e não roda no wine |
| `make_icon.py` | ícone próprio, sem arte do jogo |
| `subir.py` | FTP para o console com conferência de md5; `--baixar` traz o log |
| `build_f2.sh` | o jogo inteiro até o `default.xex` (F2): `c89.py`, `cl.exe`, `coff.py`, link com stubs automáticos para o que falta, `dat360.py`, `imagexex`. Desde a F3 leva o backend D3D9 (`port/x360/src/gs`, shaders pelo `fxc`): sem janela por padrão, desenhando com `BT3_GS=gpu` no `ambiente.txt` |
| `c89.py` | rebaixa o C do jogo e do port para o C89 do `cl` pelo AST do clang (literal composto, declaração no meio do bloco, `typeof`, expressão-comando, case com intervalo), com os headers mínimos de `port/x360/shim` |
| `coff.py` | as tabelas do executável (esqueleto `port/data/*.s`) como objetos COFF do 360; `--absoluto NOME=VALOR` faz o papel do `--defsym` |
| `dat360.py` | `default.dat` e `default.swap` ao lado do `.xex`, pelo `.map` da Microsoft (o `esqueleto()` do `headless.py` para o 360) |
| `subir.py --baixar-pasta shots --para DIR` | baixa uma pasta inteira do console (as capturas do `BT3_SHOT`); só no WSL, porque são imagens do jogo |
| `compara_quadros.py` | capturas do console (`gpu_NNNNN.ppm`) contra as do rasterizador de software (`frame_NNNNN.ppm`), por fase e pelos piores quadros: pixels iguais, até 2 e até 8 de diferença, erro médio, PSNR, imagens pretas. Roda no WSL; só números voltam |
| `subir_f2.sh` | envia o `.xex` sempre junto com o `.dat` e o `.swap` da mesma build: os dois dependem dos endereços do `.map`, e um par de outra build copia as tabelas para o lugar errado sem erro nenhum |
| `survey.sh`, `survey_tally.py` | levantamento do código do jogo (o que depende de hardware do PS2) |

### Builds de teste da ordem de bytes (F1, rodam no WSL)

| Ferramenta | O que faz |
| --- | --- |
| `headless.py --alvo x86\|ppc [--rastreio\|--chamadas]` | builds sem janela do port de PC: x86 de 32 bits (clang `-m32`, a referência little-endian com ponteiros de 32 bits como o 360) e PowerPC 32 bits big-endian (GCC cross, roda no `qemu-ppc`). As duas usam float por software (o do PS2), então uma diferença entre elas só pode vir da ordem dos bytes. Na x86r, `SEM_RASTREIO` diz o que compila sem a instrumentação (o checksum de estado e as letras dos nomes, `namefont.c`) |
| `f1.sh` | o ciclo inteiro: builds, rastreio, mapa, luta na PPC, comparações. `F1_SEM_RASTREIO=1` reaproveita o rastreio (que leva cerca de 1 h) |
| `menus.sh` | o ciclo dos menus (F3, etapa 3): boot até as demos sem `BT3_DEMO`, PPC sem mapa com os arquivos que a x86 lê, lutas das demos e memória num quadro comparadas |
| `guarda_menu.sh` | a guarda do `Sprite_Unpack` na PPC sem mapa (F4): um controle sintético (`BT3_PAD_PLAY`: START, ESQUERDA, X num ciclo de 90 leituras) entra no menu principal pelo New Game, sem criar o save (responde "não" ao "criar o save?"); a PPC tem de parar com "Sprite_Unpack sem conversor em MainMenu_Init", conferido pelo `addr2line`. Os testes novos usam os roteiros de controle (abaixo) |
| `roteiros.sh [NOME...\|todos]` | os roteiros de controle (F4, etapa 4; formato abaixo) contra as referências e o inventário: cada roteiro duas vezes na x86 (o rastro de telas igual nas duas e igual à referência em `~/referencias` do WSL), sob o gdb (cada `Sprite_Unpack` alcançado, casado com o `unpacks.py`), na x86r com o rastreio e o `conv_tool --mapa` (o "sem conversor" por conteúdo tem de ser o das chamadas pendentes alcançadas); com `PPC=1`, também a PPC com o mapa e a guarda só anotando, cujo rastro tem de ser o começo do da x86 (a ferramenta para conferir a guarda; o mapa por posição não passa do Flash, seção 6); com `SEMMAPA=1`, o critério da etapa 5: a PPC sem mapa, só com os conversores, como o 360, com as leituras de arquivo (`BT3_LEU=1`, linhas `LEU`), conferida pelo `roteiros.py semmapa`, e a memória das duas no último vblank antes do passo `fim` (`BT3_HASH_AT`, uma rodada a mais da x86; `MEM=menu` no menu principal, o critério da 5a) e no último vblank de cada tela de menu do essencial que o roteiro visita, com os conteúdos da tela vivos (`TELAS_MEM`): o heap em uso no lugar, cada bloco vivo com um arquivo ou conteúdo lido na rodada conforme número a número (`memdiff.py --so-em-uso --marcas-vivas`) e cada palavra diferente fora deles explicada (`roteiros.py memoria`). Um por vez no WSL (memória). No fim, as chamadas de todos juntos contra o inventário. Sai com 1 se qualquer conferência falhar (a lista no fim); `REUSA=1` refaz só as conferências sobre as saídas guardadas (para ver os verificadores falharem numa cópia estragada, com `O=` e `REFS=` apontando para ela); `REUSA=x86` reaproveita as rodadas da x86 e da x86r e roda só as da PPC (depois de mudar só conversores e ganchos, com a `.text` da x86 conferida igual) |
| `roteiros.py` | as peças do `roteiros.sh`: `telas` (o rastro de uma rodada), `compara`, `chamadas` (do log do gdb), `confere` (chamadas contra o `--mapa`; toda linha do `conv_tool` que não é "convertido" nem "sem conversor" é erro, assim como "contra o mapa" com outra largura ou não trocadas, e todo "sem conversor" fora de `disc/` e das faixas de menu, menos as exceções conhecidas da tabela `CONHECIDOS`), `soma` (contra o inventário), `semmapa` (a PPC sem mapa contra a x86: ou chega ao fim com o rastro, a sequência de arquivos lidos e a luta do `BT3_HASH` iguais, ou para na guarda do `Sprite_Unpack`, que nomeia o próximo bloqueio, com o rastro e os arquivos lidos até ali iguais ao começo dos da x86; outro fim, uma queda, um erro de conversor ou um nome que não chama `Sprite_Unpack` é falha; no fim, também a memória), `memoria` (o critério da memória sobre a saída do `memdiff.py`: o memdiff terminou com `--tipos` e `--donos`, o heap no lugar, os blocos com arquivo conformes, cada trecho fora do formato ou "diferente" numa lista explicada, `DIFERENTES_CONHECIDAS`, com o porquê de cada um e, num bloco do heap, contado do começo dos dados; as classes que podem ser erro de largura, fora do que se tipou, só nas regiões de `CLASSES_CONHECIDAS`, com teto, e somando o total; a região do heap pelo tamanho pedido, não pelo índice), `vblank_fim`. A tabela `CONTEUDO` diz o que cada uma das 82 chamadas descomprime (`menus-modos.md`) |
| `save_mc.sh` | a gravação atômica do cartão (F4, etapa 3, `port/src/plat_mc.c` com `PORT_MC_ATOMIC`): o `teste_mc` (`port/x360/headless/teste_mc.c`, a camada sem o jogo, com quedas de energia simuladas por `BT3_MC_FALHA`) na x86 e na PPC; o jogo na x86 e na PPC sem mapa até o `MainMenu_Init` com um controle que cria o save (ciclo de 120 leituras), a sequência de estados do McFlow comparada entre as duas; na PPC, criação, falha de escrita, `.tmp` truncado e `.new` deixado por uma queda. `BT3_MC_FALHA=apaga` simula o apagar do arquivo antigo falhando (e a troca de nome sem substituir, como o `MoveFile` do 360): a gravação devolve erro, e o próximo uso põe o `.new` no lugar |
| `compara.py` | compara a luta quadro a quadro (vida, posição, relógio, gerador aleatório) pelo `hash.txt` do `state.c` |
| `memdiff.py` | compara a memória do jogo das duas builds num quadro: heap pareado pelos blocos do alocador do jogo, globais pelo nome, cada palavra numa classe (igual, igual sem inverter, 2xu16, u64...) |
| `memdiff.py --marcas` | o mesmo, campo a campo num bloco que guarda um arquivo, pela largura que o conversor deu a cada byte (`conv_tool --marcas`); `--marcas-tam` acha o bloco pelo tamanho pedido ao alocador, em vez do índice, que muda de um quadro e de um roteiro para outro, e cada região marcada sai numa linha "marcas ...: conforme N, fora-do-formato M" |
| `memdiff.py --tipos BASE --donos BASE` | (revisão da etapa 5) as globais e os blocos do heap campo a campo pelo tipo (`campos.py --globais` e `--donos --tipos`), com os vetores de bytes e os campos `unk*` como opacos; cada trecho fora do formato é listado, a região do heap diz o tipo, quem alocou e onde começam os dados |
| `campos.py --globais PROG BASE` / `--donos PROG VBLANK --tipos BASE` | a largura de cada byte das globais (DWARF, mapa do `elftipos.py`, declarações `extern`) e dos blocos vivos do heap (o tipo pedido na alocação ou o apontado por quem recebe o bloco), para o `memdiff.py` |
| `memdiff.py --marcas-vivas DIR --so-em-uso` | (F4, etapa 5b) cada arquivo e conteúdo de `DIR` (o `conv_tool --marcas` grava ao lado de cada `.marcas` o `.orig`, o conteúdo antes da conversão, que fica no WSL) no bloco em uso que o guarda no começo, achado pelo conteúdo: os trechos de 64 bytes não nulos iguais aos da x86 (o tamanho não basta: 21 rostos do mesmo tamanho, o arquivo do lutador num buffer fixo de 0xCE000). `--so-em-uso` tira da comparação os blocos livres, o enchimento do alocador e o `HeapBlock.unk1C`, que o alocador nunca grava. Classes novas por palavra: `misto` (u8, u8 e u16 na mesma palavra) e `float-proximo` (arredondamento, a libm do host) |
| `campos.py` | compara um bloco campo a campo pelo tipo do DWARF (via gdb); `--donos` diz quem alocou cada bloco do heap num vblank |
| `layout.py` | compara pelo DWARF o layout de toda struct do jogo entre x86 e PPC; a build PPC falha se algo divergir |
| `pacotes64.py` | todo acesso de 64 bits à memória no código do jogo (AST dos `.i`), classificado em pacote da GPU ou número; falha se uma gravação em pacote estiver sem `PS2_PKT64` (decisão 19, `formatos/gpu.md`) |
| `mem.py` | lista as variáveis globais do jogo para o dump de memória |
| `unpacks.py` | toda chamada de `Sprite_Unpack` em `src/` com o gancho de conversão logo depois (`#ifdef PORT_BIG_ENDIAN`, `Port_ConvertMenuPack(buffer, rawSize, "formato")` ou a marca `Port_UnpackSemNumeros`), com buffer, tamanho e formato conferidos contra a tabela do conversor; a tabela de chamadores da guarda igual ao código; lista as pendentes e sai com 1 enquanto houver (F4, `formatos/menus-modos.md` 7). Roda em qualquer máquina, sem build |
| `chamadores_bl.py` | (no `build_f2.sh`) cada `bl Sprite_Unpack` do `default.exe` do 360, pela função que o `.map` dá, contra a tabela `CHAMADORES` da guarda: a guarda nomeia quem chamou pela função da tabela de maior endereço antes do retorno, e um inlining do `cl` trocaria o nome sem aviso. Sai com 1 se uma chamada cai fora da tabela ou se uma função da tabela não tem chamada |

### Roteiros de controle (F4, etapa 4)

O controle do jogador 1 sai de um arquivo de texto (`BT3_ROTEIRO=arquivo`), lido por `port/x360/headless/roteiro.c`
nas três builds: x86 e PPC sem janela (pelo `Port_PadRead` do `sem_janela.c`) e 360 (pelo `pad360.c`, que com um
roteiro dá os dois controles como ligados e não lê o XInput). Sem a variável nada muda. Os roteiros ficam em
`port/x360/roteiros/`; cada um começa com `inclui boot.txt` (do boot ao menu principal com o cartão vazio, criando o
save e desbloqueando tudo).

**Por que texto ancorado na tela, e não um controle gravado.** O `BT3_PAD_PLAY` repete leituras do controle, uma por
quadro; uma diferença de tempo (uma carga mais lenta, um diálogo que só aceita entrada 40 quadros depois de abrir)
desalinha o resto em silêncio (guia, seção 6: o ciclo de 90 leituras respondia "não" ao "criar o save?"). No roteiro,
cada espera é uma condição sobre o estado do jogo, e um passo que não se cumpre para a rodada com o nome da linha.

**A tela atual**, como o rastro e as esperas a veem, sai do estado do jogo:

- `inicio`: o boot (logos e cartão, `gProgress->flags` com `MPROG_FIRST_RUN`);
- um número: o modo do gerenciador de menus (`gProgress->mode`, `Progress_Main` em `src/menu/menu_a_b.c`): 1 título,
  4 menu principal, 38 `DuelMenu`, 39 `CharSel`, 40 `TeamSel`, 44 `Train`, 45 `CharSel` do treino, 62 `Option`
  (a lista inteira está no comentário de `Progress_Main`);
- `luta`: a luta andando (`gBtlSeq->state >= 2`, fora dos menus); `carga`: entre os menus e a luta.

Ao lado, o estado do fluxo do cartão (`gMcFlow->state`, -1 sem fluxo). Com o roteiro ligado, cada troca vai ao log
("`tela 39 vblank 3266 quadro 3260`", "`mcflow 10 ...`"), e cada espera cumprida também
("`roteiro: duelo.txt:11 ok tela 39 vblank 3266 quadro 3260`"): é o rastro que se compara. Sem janela o log é a
saída padrão; no 360, o `bt3xbx.log`.

**O formato.** Uma instrução por linha; `#` começa comentário; maiúsculas não importam. O "quadro" é uma leitura do
controle do jogador 1 (não um vblank: o Shenlong, a 30 quadros por segundo, lê o controle a cada 2 vblanks).

| Instrução | O que faz |
| --- | --- |
| `espera tela N` \| `espera mcflow N` \| `espera luta` [`max Q`] | controle parado até a condição; mais de Q quadros (6000 por padrão) é falha |
| `aperta B[+B]` [`N`] | aperta e solta, N vezes (o ritmo padrão: 4 quadros apertado, 8 solto) |
| `aperta B ate COND` [`max Q`] | aperta e solta até a condição (para no quadro em que ela se cumpre) |
| `segura B N` | apertado N quadros, sem soltar no fim |
| `quadros N` | controle parado N quadros |
| `ritmo SEGURA SOLTA` | o ritmo dos `aperta` seguintes |
| `repete ate COND` [`max Q`] ... `volta` | repete o bloco até a condição, que se confere a cada quadro, em qualquer ponto do bloco; aninhados, a de cada um (a de fora encerra também os de dentro) |
| `desbloqueia` | `Save_UnlockAll` do próprio jogo (o da aba de truques da janela do PC): todo personagem, estágio, música, item e as sete esferas. Só com `BT3_DESBLOQUEIO=1` e com o cartão vazio no começo da rodada (sem o arquivo de dados do save em `BT3_SAVES`, que no 360 é `game:\saves`): fora disso o roteiro para com erro, para o desbloqueio nunca ir para um save de verdade |
| `confere som N` | o som do save em memória (`gSaveData->soundMode`: 0 estéreo, 1 mono) igual a N, ou falha com arquivo e linha |
| `marca TEXTO` | uma linha no rastro; `marca memoria NNNNN` pede ao `roteiros.sh` (`SEMMAPA=1`) a memória das duas builds naquele vblank, com o conteúdo do arquivo NNNNN da partição 1 (um contêiner BPE inteiro: retrato, estágio, página) vivo e conforme |
| `inclui ARQUIVO` | outro roteiro, relativo à pasta deste |
| `fim` | sem janela, encerra o programa (saída 0); no 360, solta o controle |

Botões: `X`, `CIRCULO`, `QUADRADO`, `TRIANGULO`, `START`, `SELECT`, `CIMA`, `BAIXO`, `ESQUERDA`, `DIREITA`, `L1`,
`R1`, `L2`, `R2`, `L3`, `R3`. Falha (espera esgotada, arquivo ruim): "`roteiro: FALHA: ...`" com o arquivo e a linha,
saída 2 sem janela, tela vermelha no 360. Exemplo (de `duelo.txt`):

```
inclui boot.txt
quadros 90                        # o menu principal aceita entrada depois do fade e da animação
aperta BAIXO 3                    # História, Ultimate Battle, Torneio, Duelo
aperta X ate tela 38              # DuelMenu
quadros 120
aperta X                          # 1P contra COM
quadros 40
aperta X                          # luta simples
espera tela 39 max 600            # CharSel
```

**O desbloqueio** (só para os roteiros): o passo `desbloqueia` do `boot.txt` roda quando o fluxo do cartão do New Game
termina (`espera mcflow 0` depois da gravação), antes do `MainMenu_Init`, que monta a lista de itens (o Shenlong só
entra com as sete esferas). É determinístico: o mesmo quadro na x86 e na PPC (o `Save_UnlockAll` sorteia os itens
"novos" pelo gerador do jogo, que anda igual nas duas). Muda só o save em memória; o do cartão continua o do New Game
até o jogo salvar de novo. Onde mexe: `roteiro.c` (o passo) e mais nada; o jogo normal nunca tem `BT3_ROTEIRO` nem
`BT3_DESBLOQUEIO`.

**Como escrever um roteiro novo** sem ver a tela: rode-o na x86 sob o gdb com um `break *Snd_PlaySe` (cada aperto que
o jogo aceita toca um efeito: 1 0 mover, 1 1 confirmar, 1 2 voltar, 1 7 recusado) e um `break *Sprite_Unpack` com
`info symbol *(int*)$esp`; para o estado interno de uma tela, imprima os campos dela no mesmo comando (`gCharSel->side[0]->state`).
Uma rodada da x86 até a luta leva uns 10 s.

### Achar o que depende da ordem dos bytes

| Ferramenta | O que faz |
| --- | --- |
| `headless/rastreio.c` | runtime de instrumentação (ASan em modo chamada): cada byte lido do disco ganha uma origem (arquivo, deslocamento), e cada leitura dele é anotada com a largura e quem leu |
| `rastreio.py mapa` | do rastreio, o mapa "arquivo, deslocamento, largura" que a build PPC usa para inverter na carga; conflitos vão para um arquivo à parte e nunca são invertidos |
| `headless/inverter.c` | aplica o mapa na PPC (na leitura do arquivo e na saída da descompressão) |
| detector de visões (`BT3_VISOES` no `rastreio.c`, `visoes.py`) | acusa memória gravada com uma largura e lida com outra (u8 de um s32, u64 sobre dois u32). A lista de pares já tratados fica em `visoes_resolvidas.tsv` |
| `tipos.py` | liga cada leitura do rastreio a struct e campo do código (coluna do DWARF + AST JSON do clang) |
| `port/x360/conv/` (`conv_tool`) | conversores por formato em C89, para o instalador do 360 e para o PC: troca in-place com limite conferido e cada byte marcado; `conv_tool --mapa` confere contra o rastreio e mede a cobertura (decisão 12); `--disco` termina com o censo dos arquivos de menu por faixa de id (quantos arquivos e contêineres BPE ainda sem conversor); `--pecas` valida cada peça comum dos menus (`formatos/menus-pecas.md`) só na seção dela, em todo pacote do disco que a usa (a tabela `sUsos`), antes de o pacote inteiro ganhar conversor, e com `--mapa` confere só as posições dessa seção. No `--mapa`, uma leitura de 2, 4 ou 8 bytes sobre texto que o conversor deixou em bytes (o laço do fim do rótulo do Flash vira `strlen` na x86r) conta à parte, "em cadeia de texto lida por inteiro". Arquivo e seção para formato numa tabela só (`sFaixas`, conv.c). A `conv_tool` se compila sem `conv_jogo.c`, a ponte do jogo, que cita funções dele. Cada conteúdo descomprimido (`ARQ@0xN#bpe`) se converte também pelo caminho do jogo (`Conv_FormatoBits`, o mapa de bits de `conv_jogo.c`): bytes, resultado e cobertura têm de ser os mesmos, e o conteúdo tem de caber em `CONV_JOGO_MAX` |
| `elfdados.py` | retipa as tabelas de dados do executável (o splat põe `.long` em tudo) pela largura lida |
| `bitfields.py` | reescreve os campos de bits do C pré-processado para o GCC big-endian ter a ordem do PS2 (no 360 é `#pragma bitfield_order(lsb_to_msb)`) |
| `headless/chamadas.c`, `chamadas.py` | sequência de funções chamadas nas duas builds, para achar onde a execução se separa |

---

## 3. O processo por fases

| Fase | O que prova | Onde roda |
| --- | --- | --- |
| F0 | toolchain, empacotamento, nome e ícone, log no console, FPU e memória do 360 (sonda) | console |
| F1 | o jogo em ordem de bytes big-endian igual ao little-endian, sem janela | WSL (qemu-ppc) |
| F2 | o mesmo jogo sem janela no 360, com o float de verdade do 360 | console |
| F3 | renderizador (D3D9 do 360, shaders) e pacotes da GPU | console |
| F6 | áudio (XAudio2, XMA) | console |

A F1 existe porque testar ordem de bytes no console custa uma rodada manual por tentativa. No qemu
roda uma luta inteira em 20 minutos, com comparação automática.

### 3.1 O ciclo de um problema de ordem de bytes

1. Rode o ciclo (`f1.sh`). Se a luta diverge, `compara.py` diz o primeiro quadro.
2. Não adivinhe: localize. Use bissecção do mapa (variantes com grupos de conflitos invertidos,
   rodando em paralelo), `chamadas.py` para ver onde a execução se separa, ou `memdiff.py` num quadro
   antes da divergência.
3. Classifique o problema (seção 5.1) e corrija na classe certa:
   - dado do disco: conversão por formato;
   - campo de bits: pragma;
   - visão com outra largura: macro ou `#ifdef` no código.
4. Rode de novo e confira que nada mais mudou: as ferramentas de comparação têm de ser rigorosas.

### 3.2 Decisões numeradas

Toda escolha que mudaria o projeto entra em `docs/x360/decisoes.md` com data e o porquê, inclusive
o que foi recusado. Hipótese que se provou errada não se apaga: ganha uma nota com o que se viu.

---

## 4. O método de trabalho com agentes

- **Estudo antes de código:** três agentes em paralelo, cada um com um escopo (setup e lições
  locais, ports de referência no GitHub, conhecimento geral do console e do jogo). Cada um
  escreve um documento em `docs/x360/pesquisa/`.
- **Crítica antes de cada etapa grande:** um agente revisor independente, com instrução para não
  editar nem recompilar, não trazer dados do jogo e preferir rodar análises leves. Peça a ele:
  - se a afirmação central se sustenta (o que exatamente foi comparado);
  - se cada correção vale em todos os usos;
  - onde as ferramentas falham em silêncio;
  - os riscos da próxima etapa.

  As três revisões da F1 acharam problemas reais. O padrão que mais se repetiu: **ferramenta de
  comparação que aceita como igual justamente o erro que ela devia achar.**
- **Todo verificador precisa ser visto falhando.** Antes de confiar num verificador (script de conferência, caso de
  teste, checagem no build), rode-o com uma entrada estragada de propósito (uma cópia num diretório temporário, nunca
  a referência real) e veja-o acusar e sair com código diferente de 0. Um teste que nunca falhou não prova nada: a
  revisão da etapa 4 da F4 achou um `roteiros.sh` que só devolvia a saída da soma (referência diferente passava), um
  `roteiros.py` que contava erro do `conv_tool` como "convertido" e pulava o "sem conversor" do que não é de menu, e
  um save que dizia "gravou ok" com o arquivo antigo no lugar. Nenhum deles tinha sido visto falhando.
- **Formatos em linha de produção:** um agente escreve a especificação de um formato a partir do código
  (com o rastreio e o disco inteiro como conferência, sem trazer bytes do jogo); outro agente, ou você,
  escreve o conversor seguindo a especificação seção a seção, com critério de aceite fechado (`conv_tool
  --disco` sem erro, `--mapa` sem divergência nos arquivos da luta, os já aprovados continuando aprovados).
  Quando a validação do disco inteiro acusa um caso que a demo não exercita, um agente amplia a
  especificação com o caso exato (a mensagem do conversor diz qual arquivo e qual entrada). Revise o
  código do agente e rode os testes você mesmo antes do commit.
- **O relatório do agente não é verdade:** confira cada achado antes de corrigir (a maioria
  procede, mas não todos).
- **Fazer o programa dizer onde morreu antes de adivinhar** (vale para o console e para a F1).

---

## 5. Aprendizados de ordem de bytes (PS2 little-endian → 360 big-endian)

### 5.1 As classes de problema

1. **Dados do disco.** Cada formato (modelo, textura, animação, colisão, som, fonte) é uma struct
   lida direto do buffer. Precisam de conversão **por formato e por tipo**, a partir das structs da
   decomp, na carga (decisão 8). O mapa por posição do rastreio é andaime de teste, não produto:
   ele só cobre o que uma luta leu.
2. **Tabelas do executável.** O ELF do jogo (do disco do usuário) tem tabelas de dados que no 360
   chegam little-endian. O splat tipa quase tudo como `.long`, então tabelas de `s16` e `u8`
   saem trocadas num assembler big-endian. É mais um formato da conversão.
3. **Campos de bits.** O código conta com o primeiro campo no bit 0 (`task->bits = ~3`). No 360:
   `#pragma bitfield_order(lsb_to_msb)` do MSVC do XDK. No GCC da F1: `bitfields.py`.
4. **Visões com outra largura** (punning, union, cast): u8 lido de um s32, u16 da metade de um
   u32, u64 sobre dois u32, cores gravadas em metades de um u64, etiqueta DMA gravada como u32 e
   lida por metades. Correção no código, de dois jeitos:
   - quando é a leitura que muda: as macros `PS2_LOW(T, x)` e `PS2_LD64(x)` de `include/types.h`
     (na build de matching expandem para a expressão original; o port as redefine em
     `port/include/port_compat.h`);
   - quando é o layout que muda: `#ifdef PORT_BIG_ENDIAN` trocando a ordem dos campos dentro da
     mesma palavra. Toda struct assim tem de constar em `layout.PERMUTADAS`, com o motivo;
   - pacotes da GPU (decisão 19): o valor de 64 bits fica como duas palavras, a baixa primeiro, com `PS2_PKT64` em
     cada gravação e `PS2_PKT64_FIELDS` nos campos de bits (`formatos/gpu.md`).
5. **Ordem de avaliação de argumentos.** Uma função que lê e preenche a mesma variável na mesma
   chamada dá resultados diferentes entre clang e GCC (e o MSVC é um terceiro). Corrige com
   `#ifdef PORT`, que vale para qualquer host.
6. **Float.** O 360 arredonda add/mul para zero e divide no mais próximo; o PS2 tem as regras dele.
   As builds da F1 usam o float do PS2 em software para isolar a ordem de bytes; o float do 360 é
   assunto da F2.

### 5.2 Achar as visões

- **Varredura estática** pega o que o jogo não executa na luta de teste. O `grep` de
  `*(uN *)&`, `((uN *)&x)[i]` e uniões, triado à mão, achou 7 casos que o detector não via.
- **Detector dinâmico:** cada byte guarda a forma da última gravação (largura e posição), o valor
  gravado e quem gravou. Uma leitura com outra forma vira suspeita. Detalhes que custaram rodadas:
  - byte cujo valor atual não é o gravado foi escrito por código sem instrumentação (struct passada
    por valor, libc, código do port): trate como neutro. Isso substitui "pilha neutra", que dependia
    do endereço da pilha e se desligava por ASLR;
  - cópia em laço (lê N, grava N do mesmo valor, mesma largura) leva a forma junto. Não tente casar
    a gravação com um pedaço de uma leitura mais larga (vetor regravado componente a componente):
    zeros coincidem, e isso espalhou origens erradas (conflitos do mapa de 266 para 3.585) e, mesmo
    levando só a forma, multiplicou os falsos do detector (de 18 para 326 combinações);
  - copiar um valor lido mais estreito que a gravação é o próprio erro; copiar mais largo (dois
    floats como 8 bytes) não muda nada;
  - o teste de "simétrico" (a leitura dá o mesmo valor nas duas ordens) compara o trecho
    `[p, p+s)` com `[gw-p-s, gw-p)` na mesma ordem, não byte a byte espelhado;
  - o detector roda na x86, onde as correções `#ifdef PORT_BIG_ENDIAN` não valem: o par
    continua aparecendo e vai para `visoes_resolvidas.tsv`.

### 5.3 Rastreio confiável

- Sem junção de leituras: `-mllvm -aggressive-instcombine-max-scan-instrs=0` (o clang junta
  bytes vizinhos num load de 4), `-fno-slp-vectorize -fno-vectorize`.
- Não use `-O0` para "ver melhor": o jogo tem comportamento indefinido e diverge.
- Instrumentar a pilha também (`-asan-stack=0` só tira as zonas vermelhas, e
  `-asan-skip-promotable-allocas=0`, `-asan-use-stack-safety=0`).
- Rode com `setarch -R` para o resultado não depender do ASLR.
- **A tabela do rastreio enche nos menus.** 2^23 posições bastavam para a luta da F1; um roteiro do boot até a luta do
  Duelo lê 7,4 milhões de posições, e a de 2^23 só aceita 7,34 (enche a 7/8): a saída diz "TABELA CHEIA" e o mapa fica
  sem o resto, em silêncio para quem não lê a última linha. `BT3_RASTREIO_BITS=24` (uns 940 MB) nos roteiros; até 24
  bits o espalhamento é o de sempre, e o `rastreio.tsv` da F1 não muda.
- Conflito (byte e número, duas larguras, leitura começando dentro de um número) **nunca**
  gera inversão. Ele vai para uma lista e se resolve pelo tipo do campo (`tipos.py`).
  Exemplo real: as flags de polígono do cenário são s32 no disco e o jogo lê um byte delas numa
  cópia dentro de `BtlCapiBPose`, struct que já foi permutada. Com o campo identificado, o conflito
  se resolve sozinho.

### 5.4 Comparação confiável

- Compare a **memória**, não só os valores que você imprime: a luta "batia" em 11 palavras
  enquanto o heap estava deslocado por uma struct de tamanho errado.
- Pareie o heap pelos blocos do alocador do jogo, não pelo endereço.
- Faixa de ponteiro do port a partir da base de ligação, não do menor símbolo (símbolos TLS valem
  0 e fariam qualquer inteiro pequeno passar por ponteiro).
- "Igual sem inverter" é uma classe à parte: é certo para bytes e errado para u32.
- Confira o layout de toda struct pelo DWARF. O clang só grava os tipos usados por variáveis:
  compile com `-fno-eliminate-unused-debug-types`.
- Apague as saídas de uma rodada antes da seguinte: um comando que nem chegou a rodar (127) deixou o
  `hash.txt` velho no lugar, e a comparação "passou" com o resultado anterior.
- Atribuição de variável no começo de um comando não pode vir de expansão (`${X:+VAR=...}`): o bash a trata
  como nome do comando. Use `env` com um vetor.
- Carimbe cada build com a origem (commit, hash da diferença incluindo arquivos novos, hash do
  rastreio usado), e só compare builds do mesmo carimbo.
- **O par de um u64 se acha pelo endereço, não pelo deslocamento na região.** Os blocos do heap do jogo começam em
  qualquer múltiplo de 4; o `memdiff` juntava a palavra `o` com a `o ^ 4` da região e, num bloco que começa em 4 mod 8,
  juntava metades de dois u64. Os dois buffers de pacotes da GPU, com o mesmo conteúdo, saíam um com 13.799 palavras
  "diferentes" e o outro com 8.682 "u64"; o `SaveData.charaBits` (u64 nativo, valores iguais) saía "diferente".
- **A classe por palavra não serve para palavra com larguras misturadas.** Dois bytes e um u16 na mesma palavra
  (`FontIconDef`, `FlashTl.state/depths`, o registro de golpe do 00001) e os u16 desalinhados do Flash dos menus saem
  "diferente" ou "2xu16" mesmo convertidos certo: 19.632 "diferentes" no quadro 3000 dos menus, todos assim ou do erro
  acima. Compare pela largura de cada campo:
  - buffer que guarda um arquivo (ou o conteúdo descomprimido): `conv_tool --marcas DIR` grava a largura que o
    conversor deu a cada byte (só larguras) e `memdiff.py --marcas heap#N=DIR/arquivo.marcas` confere número a número
    ("conforme" / "fora-do-formato");
  - struct do código: `campos.py` compara folha a folha pelo tipo do DWARF (via gdb), e `campos.py --donos` diz quem
    alocou cada bloco do heap vivo num vblank.
- **Quem gravou uma palavra:** watchpoint de hardware do gdb na x86, com o vblank do `Port_StateLog` numa variável de
  conveniência. A palavra "diferente" do buffer de pacotes não era gravada desde o vblank 197 (resto de um quadro do
  cartão de memória, além do fim do pacote atual): sem isso, a procura seria no desenho do título.
- **A macro de troca fica na gravação, não nas macros de valor.** A decisão 19 esperava que a maior parte viesse de
  `GIF_TAG_EX` e das `GS_SET_*`; a auditoria pelo AST mostrou cerca de 2.000 gravações de 64 bits em pacotes, a maioria com literal ou
  número de registrador gravado direto (`p[0] = 0x44; p[1] = GS_ALPHA_1;`). A troca dentro da macro de valor deixaria
  essas de fora e trocaria duas vezes onde o valor também passa pela gravação. O `.i` já não tem macro: a reescrita
  foi pelo texto do fonte, na linha que o AST aponta, e o que vem de macro de função (três nos efeitos) ficou à mão.
- **Campo de bits sobre um número não se permuta junto com o pacote.** `EftPrim` é uma visão de bits sobre um `u64`
  local (número) que depois se copia para o pacote; as 26 structs XYZF2, ao contrário, são o próprio pacote. Olhe
  cada uso da struct antes de inverter a ordem dos campos.
- **Revisando campo de bits em big-endian, lembre que o port já os põe do bit 0 para cima** (pragma no 360,
  `bitfields.py` no GCC). Pelo padrão da PPC (do bit mais alto para baixo), a `PS2_PKT64_FIELDS`, que só troca as
  metades, pareceu errada na revisão da etapa 4 e quase foi "corrigida" para a ordem que gravaria as palavras
  trocadas. Leia a ordem pela regra do port, não pela ABI.
- **"A x86 continua igual" não se prova pelo hash de memória inteira.** A coluna 2 do `hash.txt` muda entre duas
  rodadas da mesma x86 (13 palavras: endereços da pilha e afins). Prove pela seção `.text` (`objcopy -O binary
  --only-section=.text` e `sha1sum`: a mesma antes e depois), pelos valores da luta (`compara.py`) e pela memória
  comparada com uma segunda rodada da base.
- **Float em hardware na PPC: o GCC junta `a * b + c` num `fmadd`**, que arredonda uma vez só; a x86 com SSE2 não tem
  FMA. O rasterizador de referência saiu com 1 a 9 pixels de borda diferentes em 4 de 125 quadros do título, sem nada
  de ordem de bytes. O resto do port já compilava com `-ffp-contract=off`; o GS da PPC passou a compilar também. Antes
  de procurar troca de bytes numa diferença pequena de imagem, conte os `fmadd` no objeto
  (`objdump -d | grep -c fmadd`).
- **A última linha de uma rodada cortada pelo `timeout` sai pela metade** (o stdio grava em blocos de 4 KB): o
  `compara.py` acusava 1 quadro diferente, o último. Agora ele ignora a linha sem o fim de linha.
- **Soma de verificação sobre bytes de números nativos** (`McCard_Checksum` do cartão de memória) dá outro valor em
  big-endian, e confere em cada build: é esperado enquanto o save não sai de uma máquina para outra.

### 5.5 Conversores por formato

- Especificação antes do código (`docs/x360/formatos/`), escrita do código da decomp por um agente e
  conferida contra o rastreio por entrada. O conversor segue a especificação seção a seção.
- Contêiner com tabela de deslocamentos: valide a tabela (contagem, ordem, fim igual ao tamanho do arquivo)
  antes de confiar nela.
- Exija zero no enchimento: foi assim que apareceram bytes não nulos no alinhamento depois das malhas e
  depois do fluxo BPE. Só se libera um trecho depois de confirmar que o jogo não o lê, e com comentário.
- Dado comprimido não se converte no arquivo: converta o resultado da descompressão, no caminho de quem usa
  aquele formato (não no descompressor, que serve a vários formatos).
- Valide no disco inteiro, não só nos arquivos da luta de teste: no BT3 a especificação de personagem feita
  pela demo convertia 860 de 1.976 arquivos; os erros (costura, tipos de efeito, variantes de pacote,
  trajes vazios, a faixa do registro maior que o elenco) só apareceram assim.
- Reconheça o formato pelo jeito que o jogo escolhe o arquivo (o id: `chara*10 + 0x590...`), não por uma
  lista; e confira a estrutura (contagem da tabela) dentro do conversor.
- A validação contra o rastreio tem três números: posições iguais, com outra largura, não trocadas. Todos os
  três importam; a cobertura de bytes diz o que o rastreio não viu.
- Descompressor comum a dezenas de formatos (no BT3, o `Sprite_Unpack` dos menus, com mais de 80 chamadas): o gancho
  fica em cada chamador, com o nome do formato, porque só quem chama sabe o que descomprimiu. A ferramenta de teste
  repete essa escolha numa tabela (arquivo e seção para formato) e converte o conteúdo pelo nome que o rastreio dá
  (`arquivo@0xDESL#bpe`), no `--mapa` e no `--disco`.
- **Gancho por chamador precisa de rede.** Com dezenas de chamadores, um esquecido deixa o conteúdo cru e o jogo cai
  longe (no BT3, o menu principal no console, uma rodada inteira). Duas redes: um verificador estático
  (`unpacks.py`: chamada sem gancho, buffer ou formato errados) e uma guarda em tempo de jogo, só em big-endian, que
  anota o buffer descomprimido e quem pediu e para com o nome no primeiro ponto em que ele seria lido cru. **O ponto
  de conferência tem de ser o primeiro leitor, não o vblank:** a queda vem na linha seguinte ao `Sprite_Unpack`, no
  mesmo quadro (`Res_RelocateOffsets`), antes de qualquer vblank ou da descompressão seguinte.
- Fluxo de tags sem alinhamento (o Flash dos menus): percorra pelo mesmo caminho do leitor do jogo e exija que cada
  bloco feche exatamente no tamanho declarado. É essa conferência que pega um passo de leitura errado, porque um
  campo medido errado desalinha tudo o que vem depois.

### 5.6 Ligar leitura a tipo (`tipos.py`)

A coluna que o clang grava no DWARF para um acesso é o "local da expressão": o nome do membro em
`a->b`, o operador em `*p`, o local da base em `v[i]`. O AST JSON do clang
(`-Xclang -ast-dump=json`) só escreve nos locais o que mudou desde o último, e o `.i` tem
marcadores de linha (`presumedFile`/`presumedLine`). Por isso o AST tem de ser percorrido na ordem
em que foi escrito. Caminhos: o simbolizador devolve absoluto ou relativo; normalize.

---

## 6. Aprendizados do 360 e do ambiente (os que custaram rodadas)

Os do console estão resumidos no `CLAUDE.md` (seção "Lições do 360") e detalhados em
`pesquisa/01`. Os do ambiente de trabalho:

- **WSL por SSH:**
  - aspas passam pelo `cmd` do Windows, então use `wsl.sh script` com o script na entrada;
  - processo em segundo plano morre com a sessão.
- **IP por DHCP:** o Windows e o console mudam de IP entre um dia e outro. Ache o Windows pela porta 22 e o console
  pela porta 21 (`subir.py --achar`). No `~/.ssh/config`, o `HostKeyAlias bt3win` deixa a chave do host presa ao
  nome, e uma troca de IP só muda o `HostName`.
- **XInput com o hiddriver:** zere o `XINPUT_STATE` antes de cada `XInputGetState`. Com a Guide aberta, o hiddriver
  devolve sucesso sem preencher o estado dos controles que ele emula (decisão 21).
- **O quadrante do anel é o índice do XInput, e o título não o escolhe:** um controle único pode estar no 2. Mapeie o
  jogador ao primeiro controle ligado (decisão 21), ou um jogo que espera o controle 1 trava no boot.
- **Ubuntu 24.04:** o `gcc-multilib` remove o cross `gcc-powerpc-linux-gnu`. A x86 de 32 bits sai
  com `clang -m32` e `libc6-dev-i386 lib32gcc-13-dev --no-install-recommends`.
- **Link na PowerPC:** o linker recusa misturar objetos com float por software e por hardware. Use
  `-mno-gnu-attribute`, que é seguro se as pontes entre os dois lados só passam inteiros.
- **`char`:** no GCC de PowerPC o `char` não tem sinal; no PS2, no x86 e no MSVC tem. Use
  `-fsigned-char`.
- **MSVC do XDK:**
  - `-TC` é C89;
  - `#pragma fenv_access(on)` só depois dos includes (antes dá C2099 no xnamath);
  - não use `-fp:fast`, porque ele dobra contas que o jogo espera ver arredondadas.
- **Memória do 360:** abaixo de 0x40000000 só há páginas de 4 KB; acima, de 64 KB. 0x70000000 é
  das pilhas. Os endereços baixos saem de baixo para cima: reserve as faixas fixas do jogo antes de
  qualquer alocação grande (D3D, pilha grande, leitura de arquivos).
- **Atômicos no 360:** as `Interlocked*` sem `Acquire`/`Release` não têm barreira (`winbase.h` do
  XDK), e `MemoryBarrier` é só `lwsync`. Barreira completa é `__sync()`; não há `__isync` no
  `ppcintrinsics.h` (o `cl` o aceitou como função implícita e virou stub no link).
- **Link do XDK:** `-DEBUG` liga o ICF junto com `-OPT:REF`, e funções iguais (umas 170 vazias)
  passam a ter o mesmo endereço; o jogo compara ponteiros de função. Use `-OPT:REF,NOICF`.
- **Nunca `_IONBF` na CRT do 360:** sem buffer, o `_flsbuf` faz `_write(fh, &ch, 1)` com `ch` int
  (`XDK/Source/crt/_flsbuf.c:189`), e no PowerPC sai o byte alto, 0. Cada caractere de `printf`,
  `fprintf` e `fputc` vira um zero; o `saida.txt` voltou duas vezes com o tamanho certo (46 bytes: o
  modo texto não viu os `\n`, que com os bytes certos dariam 48) e só zeros. Não era o FATX nem a falta
  de `fclose`, como a F2 chegou a anotar. Use buffer; `fopen(..., "wc")` faz o `fflush` ir até o disco
  (`_commit`). O fonte da CRT está em `XDK/Source/crt`: leia antes de adivinhar.
- **D3D9 do Xenos, medido no console (F3, `port/x360/src/teste_d3d`):**
  - `D3DFMT_LIN_A8B8G8R8` recebe o u32 nativo `R | G<<8 | B<<16 | A<<24` do `gs_draw.c` sem troca;
  - cor de vértice `UBYTE4N` com os bytes r, g, b, a na memória chega ao shader como (a, b, g, r):
    o tipo é `x | y<<8 | z<<16 | w<<24` sobre o u32 (`MakeUBYTE4` na documentação). Inverta com `.wzyx`
    no shader; a alternativa na declaração (`MAKED3DDECLTYPE` com outro swizzle, `d3d9types.h`) não foi
    medida;
  - `D3DRS_HALFPIXELOFFSET` ligado dá centros de pixel em .5, como GL e Vulkan (xadrez de 1 pixel nítido;
    desligado sai cinza);
  - o destino do `Resolve` tem de ser textura tiled (documentação: "4 KB-aligned and tiled"); a primeira
    versão usou `LIN_`, e o resultado veio em blocos e fora do contrato. Leia com `XGAddress2DTiledOffset`
    ou `XGUntileTextureLevel`;
  - o `fxc` do XDK roda no wine e gera o microcódigo (`xvs_3_0`) em header: shaders só no build.
  - **O microcódigo do Xenos não tem `discard`:** o `fxc` do XDK recusa com "X3500: Microcode Compiler not yet
    implemented: 'discard' statement". Use `clip(x)` (descarta com x < 0). Gradiente (`tex2D`, `ddx`) fora de desvio
    dinâmico: calcule `ddx`/`ddy` antes e amostre com `tex2Dgrad`.
  - **Vários alvos com uma EDRAM só** (F3, etapa 7, decisão 20; o desenho, ainda não medido no console): um par de superfícies na EDRAM (`Base` em tiles,
    `XGSurfaceSize`) serve a todos os alvos; cada um tem uma textura tiled com o conteúdo. Trocar de alvo é `Resolve`
    do anterior (retângulo em múltiplos de 8, `GPU_RESOLVE_ALIGNMENT`) e um retângulo desenhado com a textura do novo
    sobre a EDRAM limpa. O back buffer também mora na EDRAM (base 0): depois do present, nada do alvo sobrou lá.
  - **Profundidade fora de 0..1 corta a primitiva inteira no D3D9 do 360.** O GS aceita z além do formato do buffer
    (os glifos do texto do cartão do BT3 vêm com `0x0FFFFFF0` num Z24: z / zmax perto de 16). O PC não via: o GL liga
    `GL_DEPTH_CLAMP` e a SDL GPU desliga o corte. No console a caixa aparecia e o texto não. Prenda o z no shader
    (`saturate`).
  - **Arte 2D com filtro linear no canto do pixel do GS sai macia:** o `gs.frag` toma a coordenada onde o GS a toma
    (o canto do pixel), que cai na borda do texel; com o filtro bilinear que o jogo pede, cada pixel vira a média de
    quatro texels. O rasterizador de referência lê o texel do pixel inteiro (`u >> 4`). Desenhada texel por pixel, a
    arte 2D é lida no ponto (`gs_d3d.hlsl`).
  - **`DrawPrimitiveUP` grande trava o console sem erro.** Ele copia os vértices para o ring buffer secundário, e um
    bloco só pode ter um quarto dele (512 KB no padrão de 2 MB; documentação do `BeginVertices`), não só "menos de
    65.535 vértices" como diz a do `DrawPrimitiveUP`. Com blocos de até 1,8 MB os menus passaram (nunca mais de 190 KB
    num quadro) e o console congelou no começo da luta, sem a Guide abrir (a causa provável; a confirmar). Divida em
    blocos de até 448 KB.
  - **Uma captura de tela pixel a pixel custa caro no console:** `XGAddress2DTiledOffset` por pixel e um `WriteFile`
    por linha levavam cerca de 260 ms por imagem, e três capturas a cada 60 quadros pareciam o jogo a 1,8 vez o tempo
    real. Destile de uma vez (`XGUntileSurface`), grave num `WriteFile` só, e meça os quadros com captura à parte.
  - **Sem xbdm não há como ler o contexto de outra thread** (`DmGetThreadContext` é do xbdm, `XcrashGetThreadContext`
    lê arquivo de queda): para achar onde uma thread parou, ela grava marcas e contadores, e outra thread os registra
    quando ela para de andar (o vigia da `main360.cpp`).
  - Da documentação, ainda não medido no console: recurso preso no dispositivo (`SetTexture`) fica `IsBusy` até ser
    solto, e `LockRect` ou o último `Release` esperam por ele, então solte as texturas antes de ler uma de volta e só
    libere as soltas depois de `InsertFence` e `IsBusy() == FALSE`; o destino do `Resolve` não deve estar preso como
    textura; `DrawPrimitiveUP` aceita menos de 65.535 vértices por chamada; só a thread dona usa o dispositivo
    (`ReleaseThreadOwnership` numa, `AcquireThreadOwnership` na outra).
  - Um print da tela gravado pelo próprio programa (`tela.bmp`) dispensa descrição do usuário, desde que
    tenha uma referência que não dependa do que se testa (um retângulo de `Clear` com cor pura).
- **Endereço de retorno no `cl` do 360:** `_ReturnAddress()` é intrínseca do compilador PowerPC do XDK mesmo sem
  `#pragma intrinsic` e sem header (declare `void *_ReturnAddress(void);`): numa função com prólogo vira a leitura do LR
  salvo (`lwz rX,-8(r1)` depois do prólogo), numa folha vira `mflr`. Numa função cujo único chamado é de cauda (`b x`)
  o compilador não salva o LR e mesmo assim lê `-8(r1)`: lixo. Chame-a numa função que faça uma chamada comum antes
  ou depois (a guarda do `Sprite_Unpack` chama dois ganchos e o `Bpe_Decode`). No GCC, `__builtin_return_address(0)`.
- **`docker run ... | tr` com o `cl -Zi` dentro não termina:** o `cl` sobe o `mspdbsrv.exe`, que herda a saída e fica
  vivo depois do `cl`; o pipe nunca fecha. Redirecione a saída do `cl` para arquivo (como o `build_f2.sh`) e leia depois.
- **`atof` sem declaração no C89 do `cl`:** "C4013: assuming extern returning int", e no PowerPC o valor viria do
  registrador de inteiro, não do de float. O shim (`port/x360/shim/stdlib.h`) declara o que a CRT tem; procure `C4013`
  nos `.txt` da build depois de cada fonte novo.
- **`wsl.sh enviar` recusado** (commit ainda não enviado ao GitHub) não para um `enviar | tail && ...`:
  o status do pipe é o do `tail`, e o resto roda com a árvore velha do WSL. Faça push antes e confira a
  linha "WSL em <commit>".
- **Controle sintético para os menus (`BT3_PAD_PLAY`; desde a F4, etapa 4, os roteiros de controle da seção 2 o
  substituem nos testes novos):** o diálogo do cartão começa com o cursor no "não"
  (`Dialog_SetCursor(1)`), então só apertar X volta ao título a cada tentativa; e o START só abre o menu do título
  depois do logo e da animação, então uma fase de START com tempo fixo acaba cedo demais. O que passou: um ciclo único,
  START, ESQUERDA e X espaçados, repetido do boot ao fim (o START e a ESQUERDA não fazem nada onde não cabem).
  - **Chegar ao menu principal não prova que o save foi criado** (F4, etapa 3). O ciclo de 90 leituras do
    `guarda_menu.sh` parecia criar o save e não criava: o "criar o save?" do New Game (estado 0xA do McFlow) só aceita
    entrada uns 40 quadros depois de abrir, quase um ciclo depois do X que escolheu o New Game. A ESQUERDA chegava
    antes de ele aceitar, o X respondia "não", e o "seguir sem salvar?" (estado 9) recebia a ESQUERDA e o X a tempo:
    menu principal, cartão vazio. Com um ciclo de 120 (ESQUERDA em 90, X em 110; também 150 e 180) o save sai. Confira
    pelos arquivos do cartão e pela sequência de estados do McFlow (`dprintf McFlow_Update` com `gMcFlow->state`), não
    pela tela a que se chega.
- **Roteiros de controle (F4, etapa 4), o que custou rodadas:**
  - **O modo muda na escolha, não na tela nova.** O `gProgress->mode` passa a 38 no X do menu principal, ainda com o
    menu na tela (o fade de saída e o `DuelMenu_Init` vêm depois): a espera "`tela 38`" se cumpre uns 40 quadros antes
    de a tela aceitar entrada. Depois de cada `espera tela`, uma folga (`quadros 120` a `150`) ou um `aperta ... ate`.
  - **O desbloqueio tem de vir antes do `MainMenu_Init`:** a lista de itens do menu principal se monta no Init (o
    Shenlong só entra com as sete esferas), então desbloquear já no menu muda o anel só na visita seguinte. O
    `boot.txt` desbloqueia quando o fluxo do cartão do New Game volta a 0, enquanto o título sai.
  - **Com tudo desbloqueado, o `Save_UnlockAll` esvazia a lista de registros**, e telas que dependem dela (o retrato
    da lista do Data Center) recusam o X: nem todo `Sprite_Unpack` se alcança só com o desbloqueio.
  - **A tela de replay cai nas builds sem janela** (SIGFPE no `ReplayMenu_Draw`, `menu_za_d.c:409`,
    `num / (s32)pow(10.0, i)`, divisão por zero): o `pow` de double não passa pelo `port_libm.h` (só o `powf` passa),
    e o código do jogo, em float por software, chama o da libm, de float em hardware (a causa provável do 0; não
    conferida no depurador). É do teste sem janela (o 360 compila tudo com float em hardware); o roteiro do Data
    Center não entra no replay até isso se resolver na etapa 9.
  - **Duas chamadas iguais do fonte viram uma no binário.** O clang juntou os dois `Sprite_Unpack` do pacote do
    `CharSel_Init` (o do treino e o do duelo, menu_c_e.c:400 e :409 desde os ganchos da etapa 5b) num só endereço: o endereço de retorno não diz
    qual foi, e o `roteiros.py` decide pelo modo (45 é o treino). Na PPC cada uma tem o seu gancho e não se juntam.
  - **Um `roteiros.sh` por vez no WSL.** O `rastreio.py mapa` guarda o rastreio inteiro na memória (uns 5 GB com os
    13 milhões de posições da Ultimate Battle); dois ciclos ao mesmo tempo esgotaram os 8 GB do WSL, o SSH parou de
    responder por meia hora e o mapa de um roteiro morreu (`Killed`), o que o `confere` acusou como 10 diferenças. O
    ciclo agora para o roteiro com "FALHA: rastreio.py mapa" em vez de comparar com um mapa que não existe. E mesmo
    sozinho o da Ultimate Battle não cabia (6,8 milhões de posições só no 00451). Tirar as posições lidas só como
    byte longe de números não adiantou (sobraram 93%); o que resolve é que o `rastreio.py` decide cada posição só com
    as do mesmo arquivo: `roteiros.py partes` corta o rastreio em partes de até 4 milhões de posições, cada arquivo
    inteiro numa só, e os mapas das partes, juntos e ordenados, são o mapa inteiro (conferido no Duelo, cortado em
    partes de 1 milhão: `mapa.tsv` e conflitos idênticos).
  - **Ler a estrutura do jogo fora do jogo pede os tipos do jogo.** O `confere som` lê `gSaveData->soundMode` pelo
    `include/sys/save.h`, e o `roteiro.c` compilado só com `-Iinclude` pegou o `s64` do `types.h` (um `long`, 4 bytes
    na x86 de 32 bits): o `SaveData` saiu com 16.368 bytes e o campo em 0x1694, lendo outra opção (o roteiro acusou
    "som 0, esperado 1" logo no primeiro passo). Os fontes do jogo compilam com `-include port_compat.h` (`s64` de 64
    bits); o `roteiro.c` agora também, nas três builds, e um `typedef` com tamanho negativo para a build se o campo
    sair de 0x16A4. O gdb também não ajuda aqui: `gSaveData` sai "unknown type" (leia por `*(int*)(*(char**)&gSaveData
    + 0x16A4)`).
  - **gdb: dois `break` no mesmo endereço, cada um com `commands ... continue`,** só rodam os comandos do primeiro (o
    `continue` dele retoma antes do segundo). Junte num só, com `if` dentro.
  - **A PPC com o mapa passa adiante nos menus sem conversor** (para conferir o "sem conversor" pela guarda, que só
    anota com `BT3_GUARDA=anota`), mas o conteúdo de um arquivo que já tem conversor (o menu principal, seção 2 do
    00449, que tem `menu_pacote`) não recebia o mapa: o `inverter.c` deixava o descomprimido desses arquivos para o
    gancho do chamador, que não existe. O `MPACK_AT` do `MainMenu_Init` leu a tabela crua e o ponteiro errado caiu no
    `Res_RelocateOffsets`, antes de a guarda acusar. Agora, com `anota`, o conteúdo de seção sem formato na tabela
    única (`Conv_FormatoSecao`, a mesma escolha do `conv_tool`) recebe o mapa na saída da descompressão. Mesmo assim a
    PPC para logo adiante, no `Flash_Create` do menu principal: o Flash copia registros de 8 bytes (`Flash_GetRecord`,
    `memcpy`) sobre campos de 4, e o mapa não inverte posição lida com duas larguras (três conflitos "[4, 8]" só nesse
    conteúdo). O mapa por posição não passa pelos menus; quem passa é o conversor do Flash (`menus.md` 7). A PPC com o
    mapa confere o rastro e a guarda só até o primeiro menu sem conversor.
  - **Os verificadores do ciclo só falavam, não decidiam** (revisão da etapa 4). O `roteiros.sh` imprimia
    "DIFERENTES" e seguia, e a saída dele era a da soma; o `roteiros.py confere` contava a linha de erro do
    `conv_tool` ("NOME: mensagem; cobertura ...") como convertido, não lia o "contra o mapa" e pulava o "sem
    conversor" de arquivo fora das faixas de menu (assim passaram despercebidos três da História, 00510, 00560 e
    00710, hoje na tabela `CONHECIDOS` com os dois do Shenlong). Agora cada conferência que falha entra numa lista e
    o script sai com 1. Cuidado com o `set -o pipefail` ao filtrar a saída de um comparador: `compara | grep -q`
    falha quando o compara sai com 1 mesmo que o grep ache o que procura (o "DIFERENTES" de um começo de rastro, o
    caso certo da PPC); guarde a saída numa variável e procure nela.
- **Roteiros na etapa 5a da F4, o que custou rodadas:**
  - **A PPC com o mapa (`PPC=1`) agora gasta o `TP` inteiro.** Com o menu principal convertido ela passa dele, a guarda
    anota a tela seguinte (`DuelMenu_Init`, `Train_Init`) e o jogo trava no Flash dessa tela, que o mapa por posição
    não inverte: uma hora por roteiro sem resultado novo (quatro horas na rodada do essencial). O critério é a PPC
    sem mapa (`SEMMAPA=1`), que para na guarda em minutos.
  - **`REUSA=x86` reaproveita a x86 que estiver guardada, mesmo de uma versão anterior do roteiro.** Nas Opções, a
    referência tinha sido regravada depois do `confere som` e as saídas guardadas eram de antes: a conferência contra
    a referência acusou. Depois de mudar um roteiro, rodada inteira. E a pasta do roteiro não se apaga com `REUSA=x86`:
    o cartão da PPC da rodada anterior ficava lá, o boot carregou o save, o menu abriu com o cursor em outro item e o
    roteiro foi parar na História (a PPC sem mapa caiu lá; o `semmapa` acusou "parou sem nome"). O `roteiros.sh` agora
    apaga o cartão de cada rodada da PPC antes de rodá-la.
  - **Não use `wsl.sh enviar` com um `roteiros.sh` rodando no WSL.** O `enviar` faz `reset --hard` e regrava os
    arquivos, e o bash lê o script aos poucos enquanto roda. Para testar um conversor no meio de uma rodada, mande só
    `port/x360/conv` por tar para uma pasta à parte (`~/t5a/src`) e compile a `conv_tool` de lá.
- **Roteiros na etapa 5b da F4 (a memória no passo `fim`), o que custou rodadas:**
  - **Código do port no rastreio espalha origem.** As letras dos nomes de estágio (`port/src/gs/namefont.c`, só do PC)
    copiam os pixels e a paleta das folhas do CharSel em blocos de 4 bytes: o rastreio via 495 mil "leituras de número"
    sobre bytes de imagem, e a origem delas ia junto para a pilha, onde o `FlashDraw_TransformQuad` lia um `FlashQuad`
    local que o rastreio dava como "do arquivo". Tirar as posições lidas só pelo namefont do mapa não bastou (sobrou a
    da pilha, lida por função do jogo). O que resolve é compilar o namefont sem a instrumentação na x86r
    (`SEM_RASTREIO` do `headless.py`, como o checksum de estado). Antes de discutir com o mapa, veja quem leu
    (`addr2line` nos endereços da coluna 6 do `rastreio.tsv`) e se o endereço lido é mesmo o buffer (`gdb`: o `q` do
    TransformQuad estava em 0xffffd3xx, a pilha).
  - **Bloco do heap se acha pelo conteúdo, não pelo tamanho.** Na luta há 21 rostos do mesmo tamanho, dezenas de
    animações de tamanhos iguais, e o arquivo do lutador mora num buffer fixo (0xCE000) maior que ele: pelo tamanho, o
    `memdiff` marcava animação errada e acusava "fora do formato". O `conv_tool --marcas` grava o `.orig` ao lado, e o
    bloco é o que tem metade ou mais dos trechos de 64 bytes iguais. Só os trechos não nulos contam: um retrato cheio de
    enchimento "casava" com oito blocos zerados.
  - **Bloco livre e enchimento não se comparam.** Nas Opções, 6.810 palavras "diferentes" eram resto num bloco livre de
    19 MB; na luta, o enchimento de alinhamento entre o cabeçalho e os dados (alinhamento de 0x40) e o
    `HeapBlock.unk1C`, que o alocador nunca grava. Um jogo que lesse isso se separaria da x86 no rastro ou na luta.
  - **A memória do fim acha visão com outra largura que nenhum roteiro lia.** `McCardIconSys.titleBreak`: o
    `McFlowReq` (include/sys/mcflow_a.h) guarda um s32 e o `McCardFile` (include/battle/stgm_a_b.h) lê u16 no mesmo
    lugar; no big-endian vinha 0 (corrigido com `PS2_LOW` em `PORT_BIG_ENDIAN`). E a folha do rosto do lutador
    (entrada 1 do file[0]) é lida pela HUD como `HudCTex`, cujo +0x28 é o s32 `mark` que ela grava a cada quadro: o
    conversor a trata agora como as folhas da HUD.
  - **"Diferente" que não é ordem de bytes, cada um com o porquê** (`roteiros.py`, `DIFERENTES_CONHECIDAS`): a soma do
    save, ponteiros do host e da pilha, caminhos do host (`BT3_DATA` e `BT3_SAVES` diferentes nas duas rodadas), a
    metade alta de um count u32 sob um s16 numa união do som, os campos não iniciados do pacote de sprite copiados da
    pilha para os pacotes da GPU (`GfxPostSpritePkt`: `uv0.pad`, `uv1.pad`, `nop`, que o GS ignora), floats da libm
    do host (`cosf`/`sinf` no `EftGlow_BuildTables`, umas unidades na última casa). Cada um conferido no gdb
    (watchpoint de hardware a partir de um vblank, `bt`) antes de entrar na lista.
  - **`wsl.sh enviar` levava só o que não estava no índice.** Ele listava as alterações por `git ls-files -m`, que
    compara a árvore com o índice: com os arquivos de um commit já no `git add`, o WSL rodou as ferramentas velhas e o
    `roteiros.sh` acusou falhas que a versão nova não tinha. Agora a lista é contra o HEAD (`git diff --name-only
    HEAD`). Confira a contagem "(N alterados)" da linha "WSL em" antes de rodar.
  - **gdb: `dprintf` numa função inline** dá "value has been optimized out" (o `Snd_RpcCall` tem 35 cópias). Pare na
    função de baixo que não é inline (`break *sceSifCallRpc`) e leia os argumentos pela pilha (`*(int*)($esp+8)`).
  - **Dado que o jogo grava não é dado do disco, mesmo que a camada do port o leia junto com o disco.** A camada de som
    (`port/src/gs/snd_se.c`, `sceSifCallRpc` caso 0xD) lia a fila de comandos com `le32`/`le16`, como os bancos do
    disco, mas a fila (`SndQueue`: contagem u32, handle u16, pitch s32) o jogo grava em números nativos
    (`Snd_RpcFlush`). No big-endian a contagem viraria 0x0N000000 (o laço repetiria comandos velhos), o handle trocado
    impediria o STOP e um pitch trocado travaria o `mix()`. Agora a fila se lê na ordem nativa em `PORT_BIG_ENDIAN`
    (`queue32`/`queue16`); os bancos continuam little-endian. Sem fluxo de áudio o laço não roda, então a prova foi a
    fila lida, registrada numa build de teste da PPC (não guardada) e comparada com a da x86 pelo gdb no roteiro do
    Duelo: 33 filas e 34 comandos iguais (contagem e handle; o pitch foi 0 em todos). Em cada leitura de número de uma
    camada do port, pergunte quem gravou: o disco ou o jogo.
- **Revisão da etapa 5 da F4, o que custou rodadas:**
  - **Memória da conversão em jogo: um bit por byte, reservado uma vez.** O mapa de cobertura do conversor (um byte
    por byte do conteúdo, com a largura) era alocado a cada tela: 7 MB contíguos a cada entrada na seleção, depois de
    uma luta, no console onde a F3 viu a luta esgotar a memória. O jogo só precisa de "já convertido"
    (`Conv_FormatoBits`, mapa de bits); `conv_jogo.c` o reserva na primeira conversão (o `BootCard_Init`, no boot) para
    `CONV_JOGO_MAX` (8 MB de conteúdo, 1 MB de mapa), e falha com nome se não houver memória ou se um conteúdo passar
    disso. O `conv_tool` converte cada conteúdo de jogo dos dois jeitos e exige os mesmos bytes, o mesmo resultado e a
    mesma cobertura; o `--disco` diz o maior conteúdo e falha acima da reserva. Uma linha de erro do `conv_tool` vem
    antes ou depois da de cobertura do mesmo nome: o `roteiros.py` guarda o erro (antes ficava com a última linha).
  - **A memória do fim de um roteiro não confere o que já foi liberado.** Na 5b a memória se comparava só no último
    vblank antes do `fim`: nos roteiros de luta, os pacotes de menu, os retratos, os estágios e as páginas já tinham sido
    liberados, e nenhuma conversão nova estava viva (a coluna repetia a prova da F3). Agora o `roteiros.sh` compara
    também no último vblank de cada tela de menu do essencial que o roteiro visita (38, 39, 40, 44, 45, 62:
    `roteiros.py vblanks_mem` e `TELAS_MEM`), uma rodada a mais da PPC e da x86 por tela, e a tela exige os conteúdos
    dela vivos e conformes pelas marcas (`memoria --tela N`): "sem bloco" não prova a conversão. Antes de dizer o que
    uma comparação de memória prova, liste o que estava vivo no quadro comparado.
  - **Classe por palavra que pode ser erro não pode passar calada: compare pelo tipo.** O critério da memória só
    julgava "diferente"; `igual-sem-inverter`, `2xu16`, `u64` e `float-proximo` (milhares por roteiro, fora dos blocos
    com marcas) passavam, e um u32 do disco sem troca cai justamente em `igual-sem-inverter`. Um memdiff forjado com
    90.000 delas dava "memória igual". O que resolveu foi tipar quase tudo:
    - as globais (`campos.py --globais`): as do port pelo DWARF; as do jogo, definidas nos `.s` sem DWARF, pelo mapa
      do `elftipos.py` (tabelas do executável) e, no `.bss`, pelo tipo da declaração `extern` dos `.h` e `.c`; o
      `.long Símbolo` é sempre ponteiro, e o que o port não alcança (nenhum objeto C o referencia) fica de fora;
    - os blocos do heap (`campos.py --donos ... --tipos`, no vblank do dump): o tipo que a alocação pede
      (`Heap_Alloc(sizeof(T) [* n])`) ou o tipo apontado por quem recebe o bloco (`gCharSel = Heap_Alloc(0x3A4C)`,
      pela declaração da global, com `[i]` e `->campo`); com soma no tamanho (o `Ot_Init`) o bloco não é um vetor de T;
    - o `memdiff.py --tipos --donos` compara campo a campo: número invertido, byte igual, ponteiro invertido ou do port,
      u64 também como dois u32 de pacote (decisão 19); vetor de bytes e campo `unk*`/`pad*` da decomp são opacos:
      qualquer leitura de uma largura só (u32, bytes, dois u16, u8+u8+u16, u64, ponteiro), menos "diferente";
    - o que sobra sem tipo (buffers de arquivo, os pacotes da GPU, a arena do BtlPool) só passa numa lista com o porquê
      e um teto (`CLASSES_CONHECIDAS`), e a soma das classes nas regiões listadas tem de ser a do total;
    - `float-proximo` nunca com um valor na faixa do heap (um ponteiro deslocado tem o mesmo expoente).
    Armadilhas no caminho: o cache de tipos do gdb pela string juntava duas uniões anônimas diferentes ("union
    {...}"); um vetor de u8 da decomp guarda o pacote da GPU montado em palavras (`GfxPostClut.packet`, com a CLUT em
    bytes no meio); os pools do motor de scripts (`Gsc_Init`) não zeram os elementos livres, e o resto da memória fica
    neles, igual nas duas builds. Um trecho tipado só junta bytes contíguos, e o deslocamento num bloco do heap conta do
    começo dos dados (`dados +0x..` na linha da região), para cada campo listado se ver.
  - **Aceitar uma região inteira é aceitar o erro que cair nela.** Os dois buffers de pacotes da GPU eram aceitos pelo
    índice do bloco (heap#24 e #25) com qualquer número de palavras diferentes em qualquer lugar: um forjado com 5.000
    passava, e se uma alocação do boot mudasse o índice seria outro bloco. Agora a região é a de 0x100000 bytes pedidos,
    e só passam trechos com a forma dos campos não iniciados do `GfxPostSpritePkt` (uv0.pad, uv1.pad, nop: começo em 12
    ou 8 mod 16, com 4, 8, 0x14 ou 0x24 bytes), com teto de trechos e de palavras.
  - **Global do port com caminho e números: aceite só o caminho.** `gPortAdxView` ({stat, paused, left, vol, pan,
    path}) e `sMc` ({open, mode, pos, path}) eram aceitas inteiras porque o caminho do host difere nas duas rodadas;
    uma diferença em `left` ou `pos` passaria. Agora só o `path` de cada elemento (`campo(passo, início, tamanho)` na
    `DIFERENTES_CONHECIDAS`).
  - **Conteúdo que só aparece de passagem pede um quadro próprio.** Os retratos dos marcadores (aleatório 921 e
    personagem salvo 923) só ficam vivos enquanto o cursor está na célula; o roteiro do Duelo passa por elas (CIMA do
    canto dá a volta para a célula 0xA3; ESQUERDA na penúltima linha dá a volta para a 0xA1), espera o retrato carregar
    e marca `marca memoria 00923`, e o `roteiros.sh` compara a memória ali. O travado (922) não aparece com tudo
    desbloqueado. A grade e a célula sob o cursor se acham no gdb (`gCharSel->cells[0][i].id`, `side[0]->chara`).
  - **Só compare builds do mesmo commit, e o ciclo tem de recusar as outras.** A tabela final da 5b saiu de uma x86 de
    um estado e de uma PPC e uma x86r de outro, que não estava em commit nenhum (a revisão recalculou o carimbo). O guia
    já dizia "só compare builds do mesmo carimbo", mas o `roteiros.sh` só imprimia os carimbos. Agora ele sai com 1
    antes de rodar se x86, x86r e ppc não forem do commit de agora sem diferença (`diff:da39a3ee5e`) ou se a árvore
    tiver alteração; `DESENVOLVIMENTO=1` deixa rodar e o fim diz "NÃO VALE COMO CRITÉRIO". Regra que não se confere
    na ferramenta não vale.
  - **Byte de uma palavra de pacote não é o primeiro byte da memória no big-endian.** As letras dos nomes
    (`port/src/gs/namefont.c`) conferiam o endereço do registrador do GS em `pix[0x38]`; o pacote de imagem vem
    convertido em palavras u32 (o registrador de 64 bits como duas, a baixa primeiro), então no big-endian o byte é o
    menos significativo da palavra (`PKT_LOW8`), e a PPC caía para texto simples sem aviso. O teste é o próprio
    `BT3_NAMEFONT_TEST`: sem a correção a PPC não diz nada; com ela corta as 36 e as 24 legendas como a x86. As
    imagens de teste ainda diferem em uns 3% dos bytes (causa não achada; só vale para estágio ou música de fora do
    disco, que o 360 não tem: plano, etapa 5).
- **`pkill -f padrão`** pelo `wsl.sh` mata o próprio shell, cuja linha de comando contém o padrão.
- **Rastreio limitado por tempo:** `timeout` manda SIGTERM, que não passa pelo `atexit`; o `rastreio.c`
  trata o sinal e grava (os menus rodam em ciclo, sem fim próprio).
- **Relógio:** `QueryPerformanceCounter * 1e9` passa de 64 bits uns 6 minutos depois do boot
  (50 MHz). Divida antes: segundos e resto.
- **Console:**
  - o FTP só responde na FreeStyle;
  - descubra o IP pelo MAC (timeout curto demais em varredura perde o console);
  - nome e ícone vêm do SPA embutido no `.xex`.
- **Python no build:** a imagem do XDK não tem Python, então o SPA é gerado no host.
- **Lista de convertidos tirada do rastreio:** a PPC sem mapa só converte os arquivos da lista, e a lista vinha dos
  arquivos que a rodada rastreada leu. A rodada sem rastreio vai mais longe: depois da primeira demo o título volta, a
  segunda demo sorteia outra tela de carregamento (`Load_InitScreen`, 00960 em vez de 00961), o arquivo chega cru e o
  `Bpe_Decode` derruba o qemu uns 7.000 vblanks depois do quadro comparado. Agora:
  - o `inverter.c` para na leitura, com o nome, quando um arquivo que tem conversor não está na lista nem no mapa;
  - a lista sai dos arquivos que a x86 lê na rodada inteira (`dprintf` do gdb em `Port_FileReadAt`; o WSL não tem
    `strace`).

  No 360 isso não existe (`iso360.c` converte todo arquivo que tem conversor ao abrir). Com `TP` maior que o padrão
  a PPC do `menus.sh` passa do vblank 47.000 e para nessa guarda (o 00349, som, lido depois do vblank 30.000 da
  lista): para ir mais longe, aumente `ATE` junto.
- **Queda no qemu:** `qemu-ppc -g PORTA` e `gdb-multiarch` com `target remote` dão a pilha e os registradores na hora
  da queda (o WSL tem os dois); é mais rápido que bissectar com `BT3_HASH_AT`.
- **`/tmp` do WSL some entre sessões do `wsl.sh`** (a distribuição pode parar entre uma sessão e outra): o que tem de
  passar de um comando para outro fica em `~/`.
- **Rasterizador de software no qemu** (`BT3_GS=1`): uns 100 quadros por minuto nos logos e 30 no título; a x86 faz
  15 a 27 por segundo. Do boot ao fim do título (4.244 quadros) a PPC leva perto de 1 h 30. `BT3_SHOT=N` grava
  `port/build/shots/frame_NNNNN.ppm` relativo à pasta de trabalho: rodadas ao mesmo tempo, cada uma na sua pasta, com
  `BT3_DATA` absoluto. O número é o quadro do GS, não o vblank: as telas se acham pelo gdb (`dprintf` em
  `BootCard_Init`, `Logo_Init`, `Title_Init` com `gGsFrame`). Compare as imagens lá e traga só contagens.
- **Build com falha intermitente:** uma vez 8 objetos falharam e passaram na repetição sem
  mudança. O ciclo agora mostra as linhas `FALHOU` em vez de cortá-las com `tail`.

---

## 7. Lista de passos para um port novo

1. Fork da decomp (ou do port de PC dela, se existir), `upstream` como remote, merge só de
   propósito.
2. `CLAUDE.md` com as regras do dono (idioma, commits sem atribuição, repositório privado, nada do
   jogo no repositório), o console, a toolchain e o deploy. Use o deste repositório como modelo.
3. Três agentes de estudo (seção 4) e `docs/<alvo>/pesquisa/`.
4. F0: sonda no console (toolchain, log em arquivo, filtro de exceção com `Iar`/`Lr`, FPU,
   memória, nome e ícone). Os resultados entram nas decisões.
5. Se a origem é little-endian: a F1 deste repositório quase inteira se reaproveita
   (`headless.py`, `rastreio.c`, `layout.py`, `memdiff.py`, `tipos.py`). O que muda por jogo são
   o alocador (magic dos blocos no `memdiff`), o ponto de entrada da luta ou da demo de teste e
   onde o jogo lê arquivos (`--wrap` da função de leitura e da descompressão).
6. Antes de cada etapa grande, um agente revisor (seção 4).
7. Cada aprendizado novo entra neste guia no mesmo commit da correção.
