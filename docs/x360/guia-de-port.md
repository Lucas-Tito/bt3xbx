# Guia de port para Xbox 360: processo, ferramentas, setup e aprendizados

Escrito durante o port do BT3 para servir de ponto de partida a outros ports (outros jogos, outros
chats). O que é específico do BT3 está marcado; o resto vale para qualquer decomp de console
little-endian (PS2, PS1, GameCube não, que já é big-endian) levada ao 360.

Como usar num chat novo: peça para ler este arquivo, o `CLAUDE.md` deste repositório (modelo de
regras) e `docs/x360/decisoes.md` (o porquê de cada escolha). As pesquisas em
`docs/x360/pesquisa/01..05` têm o detalhe: 01 é o setup do 360 e as lições do CollectionUI e do
hiddriver, 02 são os ports de referência (SM64, MK64, Virtua Fighter), 03 é o estado da decomp do
BT3, 04 é onde ficam os ícones dos botões e o custo de trocá-los, 05 é o estado do Tag Team nativo no PC.

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
  Nem um serviço do systemd (`systemd-run`) escapa: o WSL desliga a distribuição inteira uns segundos depois de fechar
  a última sessão aberta pelo Windows (o journal mostra "The system will power off now!"). Na 9c-1, o Codex disparado
  num serviço morreu em 26 s, quando o `!` que o disparou terminou. Para um processo que não fica preso a uma sessão,
  deixe uma sessão aberta só para segurar o WSL (`ssh bt3win "wsl -d Ubuntu-24.04 -u lucas -- sleep 43200"` em segundo
  plano daqui) e confira pelo `uptime -s` que o boot não mudou.
- **Agente no WSL (Codex):** roda como o usuário `codex` (sem sudo, dados só leitura em `/srv/bt3/dados`, sem ver
  `/home/lucas` nem `/mnt/c`, que o `wsl.conf` monta com `umask=077`), sem o isolamento dele, que bloqueia a x86 de
  32 bits (SIGSYS), o `ptrace` do gdb e os sockets. A troca de código é pelo repositório local `/srv/bt3/bt3xbx.git`
  (remoto `troca`); o GitHub só recebe o que passou pela revisão. Regras dele no `AGENTS.md`.
- **Cursor CLI no mesmo usuário** (`cursor-agent`, desde 17/10/2026, substituto do Codex quando a cota semanal dele
  acaba): sem interação com `-p --force --trust --sandbox disabled --model ...`. Ele põe sozinho um
  `Co-authored-by: Cursor` em cada commit, apesar do `AGENTS.md`: desligue no `~/.cursor/cli-config.json` do usuário
  (`"attribution": {"attributeCommitsToAgent": false, "attributePRsToAgent": false}`) e confira as mensagens na revisão.
  O WSL tem uns 7 GB: na investigação dos ícones um script Python do Cursor chegou a 7,6 GB lendo os dados e o OOM
  matou o serviço inteiro. Dispare com `--property=MemoryMax=5G` no `systemd-run` e peça processamento arquivo por
  arquivo. A retomada é `cursor-agent --resume <uuid>`, com o UUID da pasta em `~/.cursor/chats/<hash>/` (o hash
  sozinho é recusado).
- **O disco do WSL mora no C: do Windows e só cresce** (`ext4.vhdx`): em 17/10/2026 o C: encheu (456 MB livres), o
  WSL passou a dar erro de E/S (até o `getpwnam` falhava, "User not found") e o Cursor caiu no meio da etapa 8. As
  saídas das rodadas (`port/build/roteiros`, os dumps de memória por tela) e as pastas de trabalho dos agentes tinham
  levado o disco a 106 GB. Recuperação: liberar espaço no C:, `wsl --shutdown` (o ext4 se recupera sozinho no boot),
  apagar as saídas antigas, `fstrim /`, e compactar o `ext4.vhdx` com o `diskpart` (`select vdisk file=...`,
  `attach vdisk readonly`, `compact vdisk`, `detach vdisk`, numa sessão de administrador; o vhdx não pode estar
  esparso: `wsl --manage <distro> --set-sparse false` antes). Devolveu 77 GB. Apague as saídas de uma rodada depois
  de conferida e confira o espaço do C: antes de rodadas longas.

### 1.2b Steam Deck: a segunda fila de emulador (16/10/2026)

Dois emuladores juntos travam o WSL, então uma segunda máquina é a única forma de rodar dois roteiros ao mesmo tempo.
O Deck do Lucas (SteamOS, 8 threads, 16 GB) serve, como `Host deck` no `~/.ssh/config` daqui (chave própria):

- **Contêiner, não o sistema:** o SteamOS tem a raiz só leitura e as atualizações apagam o que se instala nela. O
  `distrobox` (já vem no SteamOS) cria `bt3`, um Ubuntu 24.04, e o mesmo `preparar-wsl.sh` o prepara (sem a linha do
  `wsl.conf`, com `USUARIO=deck`), mais `gdb` e `gdb-multiarch`. Saíram as mesmas versões do WSL (gcc 13.3.0, o
  snapshot 22.1.8 do clang, qemu 8.2.2, gdb 15.1). Tudo roda por `distrobox enter bt3 -- bash -lc "..."`.
- **Código:** um repositório nu `~/bt3xbx.git` no Deck (remoto `deck` daqui) e o clone `~/bt3xbx`; os dados em
  `~/bt3dados`, ligados ao clone como no WSL, e as referências dos roteiros em `~/referencias`, copiadas do WSL.
- **Dados copiados direto do WSL** por `tar` entre dois `ssh` (WSL → aqui → Deck), conferidos por um manifesto de
  sha1 dos dois lados (68.611 arquivos do `gamedata` iguais; o manifesto estragado foi recusado).
- **O `port/build/gen` não se copia:** os `.s` gerados pelo `port/tools/gen_data.py` guardam o caminho absoluto dos
  dados (`.incbin "/home/lucas/..."`), e a build x86 do Deck caiu no `vu1_micro.data.s`. Gere de novo lá
  (`gen_data.py --asm ~/bt3xbx/asm`); os 72 arquivos saíram iguais aos do WSL, fora o caminho.
- **A prova antes de valer:** no commit da rodada da F4, a `.text` da x86 do Deck saiu igual à do WSL
  (`e0af7e67…`), e o `SEMMAPA=1 IMG=1 roteiros.sh duelo` deu "tudo certo", com as 16 imagens idênticas byte a byte às
  do WSL. Uma máquina nova só entra depois dessas duas provas.
- Um emulador por vez também no Deck. Deixe-o na tomada, na rede e sem suspensão.
- **Processo longo no Deck:** `nohup ... &` dentro do `distrobox enter ... bash -lc "..."` morre quando o `ssh` fecha
  (log vazio e nenhum processo, sem erro). O que fica: um script no Deck e, do lado de fora do contêiner,
  `setsid nohup distrobox enter bt3 -- bash -lc ~/script.sh > ~/log 2>&1 < /dev/null & disown; sleep 15`, e
  confira o processo antes de fechar: lançado com o `ssh` fechando logo em seguida, também não subiu. Consulte o
  log com espaço (F5, etapa 4).

### 1.3 Console

- FTP da FreeStyle (`xbox`/`xbox`), só com o console parado na FreeStyle, uma conexão por vez,
  sem `QUIT` e sem `REST`. `port/x360/tools/subir.py` acha o console pelo MAC (`ip neigh`, prefixo
  4c:0b:be), envia com nome temporário, renomeia e confere o md5 baixando de volta.
- Pasta: `/Hdd1/HOMEBREW/bt3xbx`. O `.xex` se abre à mão (sem xbdm não há como lançar remoto).
- O relógio do console é zerado: não use datas de arquivo para nada.
- **O `.xex` nunca sobe sozinho:** o `default.dat` e o `default.swap` saem do `.map` da mesma build e dizem onde ficam
  as tabelas do disco. Na sessão da F4 subi só o `.xex` pelo `subir.py`, os dois do console eram de outra build, e o
  jogo travou no preto em "preparando a memória e as tabelas do disco" (o mesmo tamanho de arquivo, outro conteúdo).
  Suba sempre pelo `subir_f2.sh`, que leva os três juntos.
- **`Clear` com o preto exato não limpa** o back buffer no caminho de desenho do backend (F5, etapa 1): pedindo
  `0xFF000000` as barras do 4:3 ficavam com o azul da tela de fase anterior, pedindo `0xFF00FF00` saíam verdes e
  pedindo `0xFF010101`, pretas. A causa no D3D do 360 não foi achada; limpe com `0xFF010101`. Ponha a cor de um teste
  visual no `ambiente.txt`: três aberturas de 30 s acharam isso sem recompilar.
- **Desde 23/10/2026 os padrões do `Ambiente_Carregar` são o jogo como ele roda:** sem a demonstração, a GPU com a VU1, as
  duas threads, o som e o 16:9. Um `ambiente.txt` antigo que contava com o padrão de antes (a luta de demonstração, sem
  desenho, sem som) precisa dizer `BT3_DEMO=1`, `BT3_GS=none`, `-BT3_SOM`; a ISO continua pela busca em `game:\` ou pelo
  `BT3_ISO`. Desde a F8, `01-avisos`, a busca automática percorre as `.iso` até achar uma convertida compatível;
  `BT3_ISO` explícito continua definitivo.
- **Antes de dizer que uma variável falta no console, leia os padrões do `Ambiente_Carregar`**
  (`port/x360/src/f2/posix360.c`): o `getenv` do 360 é aquela tabela mais o `ambiente.txt`, não o da CRT nem o
  `port/x360/src/ambiente.c` da F1. O estudo da F6 deu o `BT3_SOUND_TICKS` como ausente, a primeira rodada da VU1 da
  F5 o pôs no `ambiente.txt` para provar, e ele já era padrão desde a F2: a rodada não separou nada.

---

## 2. As ferramentas (todas em `port/x360/tools/`)

### Build e deploy do 360

| Ferramenta | O que faz |
| --- | --- |
| `build.sh` | compila o `.xex` no Docker do XDK |
| `make_spa.py` | gera o SPA (XDBF: XTHD, XSTC, nome, ícone 64x64), porque o `spac.exe` é .NET e não roda no wine |
| `make_icon.py` | ícone próprio, sem arte do jogo |
| `subir.py` | FTP para o console com conferência de md5; `--baixar` traz o log, e antes renomeia o arquivo no console para `NOME.lido` (ler um arquivo que o jogo está gravando trava o servidor) |
| `build_f2.sh` | o jogo inteiro até o `default.xex` (F2): `c89.py`, `cl.exe`, `coff.py`, link com stubs automáticos para o que falta, `dat360.py`, `imagexex`. Desde a F3 leva o backend D3D9 (`port/x360/src/gs`, shaders pelo `fxc`): sem janela por padrão, desenhando com `BT3_GS=gpu` no `ambiente.txt` |
| `c89.py` | rebaixa o C do jogo e do port para o C89 do `cl` pelo AST do clang (literal composto, declaração no meio do bloco, `typeof`, expressão-comando, case com intervalo), com os headers mínimos de `port/x360/shim` |
| `coff.py` | as tabelas do executável (esqueleto `port/data/*.s`) como objetos COFF do 360; `--absoluto NOME=VALOR` faz o papel do `--defsym` |
| `dat360.py` | `default.dat` e `default.swap` ao lado do `.xex`, pelo `.map` da Microsoft (o `esqueleto()` do `headless.py` para o 360) |
| `subir.py --baixar-pasta shots --para DIR` | baixa uma pasta inteira do console (as capturas do `BT3_SHOT`); só no WSL, porque são imagens do jogo |
| `compara_quadros.py` | capturas do console (`gpu_NNNNN.ppm`) contra as do rasterizador de software (`frame_NNNNN.ppm`), por fase e pelos piores quadros: pixels iguais, até 2 e até 8 de diferença, erro médio, PSNR, imagens pretas. Roda no WSL; só números voltam |
| `ppm_stats.py` | triagem de capturas sem PIL nem numpy (o WSL só tem a biblioteca padrão): `png` (converte, recorta e amplia, para ver a imagem), `patch` (luminância de um retângulo), `stats` (luminância média, % saturado, % escuro, gradiente horizontal = nitidez da área 3D). Recusa arquivo truncado ou que não seja P6 |
| `subir_f2.sh` | envia o `.xex` sempre junto com o `.dat` e o `.swap` da mesma build: os dois dependem dos endereços do `.map`, e um par de outra build copia as tabelas para o lugar errado sem erro nenhum |
| `teste_diario.sh` | o `diario.c` (anel e thread escritora) no Linux, com uma API do Xbox de brinquedo e um disco de 40 ms por flush: ordem, integridade, que quem escreve não espera, a queda, o anel cheio; `--mutantes` estraga o `diario.c` de quatro jeitos e exige que o teste acuse cada um |
| `picos_log.py` | de um `bt3xbx.log` do console, a média, o pior quadro e a média sem o pior de cada trecho de 60 quadros, e o custo por linha gravada do relatório anterior (`F5/08.md`, "Os logs e os engasgos") |
| `som_ambiente.py` | (F6, etapa 1, rede 1) todo `getenv` e todo ramo de janela e de rede dos arquivos do som e dos filmes, com o valor no 360 (padrões do `posix360.c` e `--ambiente ambiente.txt`), nos roteiros do WSL e no PC, e o caminho que cada um dá; sai com 1 numa variável ou ramo sem classe ou num caminho que diverge entre o 360 e o WSL fora das divergências esperadas; `--mutantes` estraga cópias de seis jeitos (o sexto, F6 etapa 3: uma chave nova da camada do console) |
| `som_estado.py` | (F6, etapa 1) as linhas `som:` do `BT3_SOM_ESTADO=N` (o estado dos seis players, a boca, as bandeiras dos menus e a tela de resultado da História, por mudança e a cada N leituras do controle; no console com o tempo real): `compara` (console contra x86, vblank a vblank), `falas` (cada fala em vblanks e em ms), `parada` (a porta fechada da tela de resultado) |
| `survey.sh`, `survey_tally.py` | levantamento do código do jogo (o que depende de hardware do PS2) |

### Builds de teste da ordem de bytes (F1, rodam no WSL)

| Ferramenta | O que faz |
| --- | --- |
| `headless.py --alvo x86\|ppc [--rastreio\|--chamadas]` | builds sem janela do port de PC: x86 de 32 bits (clang `-m32`, a referência little-endian com ponteiros de 32 bits como o 360) e PowerPC 32 bits big-endian (GCC cross, roda no `qemu-ppc`). As duas usam float por software (o do PS2), então uma diferença entre elas só pode vir da ordem dos bytes. Na x86r, `SEM_RASTREIO` diz o que compila sem a instrumentação (o checksum de estado e as letras dos nomes, `namefont.c`) |
| `headless.py --alvo x86\|ppc --gpu [--vu1]` | (F5, etapa 8) o GS em modo gpu com um backend falso (`headless/gpu_falso.c`): o `gs_draw.c` de verdade e os lotes da VU1 pelo caminho dos shaders (`hle_program*`, `GsGpu_DrawVu*`), como no 360, sem desenhar; hash da lista de cada quadro por seção. `--vu1` (só x86) liga na x86 as otimizações que a PPC tem em `PORT_BIG_ENDIAN`. Pastas `x86g`, `x86gv`, `ppcg` |
| `vu1_perfil.sh` / `vu1_perfil.py` | (F5, etapa 8) o perfil da gravação da VU1 e a prova de que uma mudança nela deixou a lista igual: `x86` (tiques por família, amostras por função e linha), `lista` + `compara` (hash por quadro; `SECOES=2,4,5,6` só a VU1), `ppc` (instruções e acessos à memória de uma janela de quadros no qemu) |
| `som.sh` / `som.py` / `som_ref.py` | (F6, etapa 2) a rede de testes do som: o rastro de som (`headless/som.c`, `BT3_SOM`, em todas as builds sem janela), o dispositivo gravador (`headless.py --gravador`, `BT3_SOUND=1 BT3_GRAVADOR=arquivo`), o teste das amostras (`--teste-som`) contra a referência independente em Python; `roteiros.sh` com `SOM=1` liga as redes 2 e 4, `som.sh amostras` a 3, `som.sh controles NOME` os controles negativos (`plano/F6/02.md`) |
| `teste_som360.sh` | (F6, etapa 3) o núcleo portátil da camada de áudio do 360 (`port/x360/src/som/som_nucleo.c`: a fila de cada fluxo, o ganho, a conversão de taxa e de canais, a soma) em C89 estrito com o gcc e o teste dele (`src/som/teste/teste_nucleo.c`, contra a forma fechada da conversão), num diretório temporário; seis mutantes do núcleo têm de ser acusados. Roda aqui e no WSL (sem clone nem emulador: `RAIZ=` numa cópia) |
| `dat_compara.py` | (F6, etapa 3) o `default.dat` e o `default.swap` de duas builds do 360: mesmo conteúdo se toda palavra que difere difere pelo mesmo deslocamento (o executável cresceu antes das tabelas); sai com 1 em qualquer outra diferença |
| `f1.sh` | o ciclo inteiro: builds, rastreio, mapa, luta na PPC, comparações. `F1_SEM_RASTREIO=1` reaproveita o rastreio (que leva cerca de 1 h) |
| `menus.sh` | o ciclo dos menus (F3, etapa 3): boot até as demos sem `BT3_DEMO`, PPC sem mapa com os arquivos que a x86 lê, lutas das demos e memória num quadro comparadas |
| `guarda_menu.sh` | a guarda do `Sprite_Unpack` na PPC sem mapa (F4): um controle sintético (`BT3_PAD_PLAY`: START, ESQUERDA, X num ciclo de 90 leituras) entra no menu principal pelo New Game, sem criar o save (responde "não" ao "criar o save?"); a PPC tem de parar com "Sprite_Unpack sem conversor em MainMenu_Init", conferido pelo `addr2line`. Os testes novos usam os roteiros de controle (abaixo) |
| `roteiros.sh [NOME...\|todos]` | os roteiros de controle (F4, etapa 4; formato abaixo) contra as referências e o inventário: cada roteiro duas vezes na x86 (o rastro de telas igual nas duas e igual à referência em `~/referencias` do WSL), sob o gdb (cada `Sprite_Unpack` alcançado, casado com o `unpacks.py`), na x86r com o rastreio e o `conv_tool --mapa` (o "sem conversor" por conteúdo tem de ser o das chamadas pendentes alcançadas); com `PPC=1`, também a PPC com o mapa e a guarda só anotando, cujo rastro tem de ser o começo do da x86 (a ferramenta para conferir a guarda; o mapa por posição não passa do Flash, seção 6); com `SEMMAPA=1`, o critério da etapa 5: a PPC sem mapa, só com os conversores, como o 360, com as leituras de arquivo (`BT3_LEU=1`, linhas `LEU`), conferida pelo `roteiros.py semmapa`, e a memória das duas no último vblank antes do passo `fim` (`BT3_HASH_AT`, uma rodada a mais da x86; `MEM=menu` no menu principal, o critério da 5a) e no último vblank de cada tela de menu do essencial que o roteiro visita, com os conteúdos da tela vivos (`TELAS_MEM`): o heap em uso no lugar, cada bloco vivo com um arquivo ou conteúdo lido na rodada conforme número a número (`memdiff.py --so-em-uso --marcas-vivas`) e cada palavra diferente fora deles explicada (`roteiros.py memoria`). Um por vez no WSL (memória). No fim, as chamadas de todos juntos contra o inventário. Sai com 1 se qualquer conferência falhar (a lista no fim); `REUSA=1` refaz só as conferências sobre as saídas guardadas (para ver os verificadores falharem numa cópia estragada, com `O=` e `REFS=` apontando para ela); `REUSA=x86` reaproveita as rodadas da x86 e da x86r e roda só as da PPC (depois de mudar só conversores e ganchos, com a `.text` da x86 conferida igual); com `IMG=1`, o critério da etapa 6: as imagens das telas pelo rasterizador de software na x86 e na PPC sem mapa, no mesmo momento do roteiro (ancorado no rastro), idênticas byte a byte (`headless/imagens.c`, `roteiros.py imagens` e `compara_imagens`) |
| `roteiros.py` | as peças do `roteiros.sh`: `telas` (o rastro de uma rodada), `compara`, `chamadas` (do log do gdb), `confere` (chamadas contra o `--mapa`; toda linha do `conv_tool` que não é "convertido" nem "sem conversor" é erro, assim como "contra o mapa" com outra largura ou não trocadas, e todo "sem conversor" fora de `disc/` e das faixas de menu, menos as exceções conhecidas da tabela `CONHECIDOS`), `soma` (contra o inventário), `semmapa` (a PPC sem mapa contra a x86: ou chega ao fim com o rastro, a sequência de arquivos lidos e a luta do `BT3_HASH` iguais, ou para na guarda do `Sprite_Unpack`, que nomeia o próximo bloqueio, com o rastro e os arquivos lidos até ali iguais ao começo dos da x86; outro fim, uma queda, um erro de conversor ou um nome que não chama `Sprite_Unpack` é falha; no fim, também a memória), `imagens` (a lista de imagens de um rastro: a entrada e a saída de cada tela de menu do essencial, a luta, cada `marca`, cada `ok som`, cada pergunta do cartão, cada uma ancorada numa linha do rastro), `compara_imagens` (as imagens da x86 e da PPC: presentes, 640 x 448, capturadas no mesmo quadro e vblank, idênticas byte a byte e não de uma cor só; a diferença diz a tela, o quadro, quantos pixels, o retângulo e o maior desvio), `memoria` (o critério da memória sobre a saída do `memdiff.py`: o memdiff terminou com `--tipos` e `--donos`, o heap no lugar, os blocos com arquivo conformes, cada trecho fora do formato ou "diferente" numa lista explicada, `DIFERENTES_CONHECIDAS`, com o porquê de cada um e, num bloco do heap, contado do começo dos dados; as classes que podem ser erro de largura, fora do que se tipou, só nas regiões de `CLASSES_CONHECIDAS`, com teto, e somando o total; a região do heap pelo tamanho pedido, não pelo índice; um buffer de imagem de 0x16800 ou 0x20800 sem nenhum conteúdo vivo, `BUFFER_IMAGEM_VAZIO`, aceita essas classes até o bloco inteiro e diz quantos bytes), `vblank_fim`. A tabela `CONTEUDO` diz o que cada uma das 82 chamadas descomprime (`menus-modos.md`) |
| `headless/imagens.c` | (F4, etapa 6) as imagens das telas nas builds sem janela: `BT3_IMAGENS=lista` ("NOME N ÂNCORA": N quadros do roteiro depois da linha do rastro ÂNCORA, "#K" a K-ésima) grava `NOME.ppm` (o buffer mostrado, como o `screenshot()` do `gs_core.c`) ao lado da lista. Com `BT3_GS=1 BT3_GS_FROM=1000000000`, só se pintam os `BT3_IMAGENS_JANELA` quadros antes de cada imagem (o contador `gGsFrame` sobe para a faixa do `BT3_GS_FROM` durante a janela); o resto do GS (cargas, VU1) roda sempre. Entra por `--wrap` no `Port_GsVif1Chain` e no `Port_GsGifChannel` |
| `save_mc.sh` | a gravação atômica do cartão (F4, etapa 3, `port/src/plat_mc.c` com `PORT_MC_ATOMIC`): o `teste_mc` (`port/x360/headless/teste_mc.c`, a camada sem o jogo, com quedas de energia simuladas por `BT3_MC_FALHA`) na x86 e na PPC; o jogo na x86 e na PPC sem mapa até o `MainMenu_Init` com um controle que cria o save (ciclo de 120 leituras), a sequência de estados do McFlow comparada entre as duas; na PPC, criação, falha de escrita, `.tmp` truncado e `.new` deixado por uma queda. `BT3_MC_FALHA=apaga` simula o apagar do arquivo antigo falhando (e a troca de nome sem substituir, como o `MoveFile` do 360): a gravação devolve erro, e o próximo uso põe o `.new` no lugar |
| `cores.py` | (F5, etapa 4; roda na build da PPC, `headless.py`) as cores byte a byte dos pacotes da GPU contra o inventário (`plano/F5/inventario.md`, seção 1): todo quarteto de cor com classe (pacote por índice, pacote por layout, CPU), toda gravação de cor de cada função com classe e quantidade, a gravação de pacote por índice só no `#else` de um `#ifdef PORT_BIG_ENDIAN` cujo ramo é o mesmo texto com o índice i em 3-i, e nenhuma troca a mais em CPU, CLUT ou cópia de pacote. Busca de texto: cast por ponteiro de bytes ou campo `unkNN[4]` não aparecem. O `--apos-luta` e o `SO_NA_LUTA` do `roteiros.py` saíram com a correção |
| `roteiros.py save` | (F4, etapa 9a; no `roteiros.sh` com `SEMMAPA=1`, quando a PPC chega ao fim) o save que a rodada deixou no cartão da x86 e o da PPC sem mapa campo a campo (`save_entre.py campos`), e cada saga do passo `confere saga N` com o fim visto também no arquivo das duas |
| `save_entre.sh` | (F4, etapa 8) o save entre builds no WSL: a x86 e a PPC sem mapa criam cada uma o seu save pelo roteiro `opcoes`, comparados campo a campo; cada caso (o save da própria build, o da outra, o de PC acertado para a soma de byte baixo 0, truncado, zerado, um byte e um bit trocados) roda nas duas com o roteiro que a previsão pede (`save_mono`, `save_zerado`, `save_danificado`), e o rastro tem de cumprir a previsão sem queda; casos de mesmo resultado, mesmo rastro. `CARTAO=pasta` carrega na PPC um cartão de outra origem (o do console); `REUSA=1 O=cópia` refaz os julgamentos sobre saídas guardadas. Leva uns 15 s |
| `save_entre.py` | as peças do `save_entre.sh`: `analisa` (soma, as palavras nas duas ordens, quem gravou, o que cada build faz), `preve` (o `McCard_Checksum` do jogo com as palavras na ordem da build e a regra do cabeçalho na PPC), `estraga`, `julga` (o rastro: McFlow 19 e depois 20 ou 29, New Game, fim, código de saída), `campos` (dois saves campo a campo pela largura do `SaveData` e do `McCardIconSys`; `--mesma-ordem` para PPC contra 360) |
| `save_console.sh` | (roda aqui) os saves entre o WSL, `build/saves` e o console: `traz-wsl`, `leva-wsl`, `confere`, `lista`, `baixa`, `envia` (pelo `subir.py`, listando antes o que substitui) |
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

F7, etapa 1, sem emulador ([nota](plano/F7/01.md)): `f7_manifesto.py --dados ORIGEM --saida ~/manifesto.jsonl`
gera itens, condições originais e leitores a partir da árvore extraída ou ISO convertida. Só código/ids/campos
no git; saída e dados fora dele. `f7_cobertura.py coletar --rodada DIR --saida DIR_NOVO` preserva uma leitura
dos resultados existentes; `conferir --coleta DIR --justificativas JSON --saida ~/cobertura.json` exige a diferença
nominal de **todos** os sítios Sprite_Unpack e arquivos contra LEU, além dos resultados do `--disco`.
Sem justificativa nominal sai 1. `f7_justificativas.json` começa vazio; pendência justificada continua pendência.
`teste_f7.py --coleta DIR --dados ORIGEM --saida DIR_NOVO` testa em cópias a retirada de chamada, LEU, resultado
do disco e justificativa, e dados inválidos. Não executa jogo, não escreve na rodada alheia.

Cuidados encontrados nessa leitura, já corrigidos nas ferramentas:

- O `--disco` reconhece `sFaixas` **mais `do_lutador`**, que acrescenta 1.610 arquivos. Ler só a tabela produz
  1.715 em vez de 3.325 e deixa modelos fora da obrigação de cobertura. Exigir a igualdade nominal com o log.
- A ISO convertida troca cabeçalhos BPE, preservando o fluxo comprimido. Depois de expandir, os números ainda
  estão na ordem PS2 até o gancho do jogo. Carregar a ordem junto do bloco evita aplicar big-endian ao miolo.
- `BtlParam.transDefault` é signed: 0xFF é -1. O teste `def >= 0` em `BtlAct_CheckTransformInput` impede chamar
  17 relações de impossíveis por interpretar 255 como slot padrão. Direção explícita continua escolhendo o slot.
- `missionPages` em +0x20C começa em 5 e aumenta no SimEv17; o save tem registros de missão com passo 12.
  As 100 entradas em disco não significam que o New Game oferece todas. O estado da campanha Sim em gProgress
  é zerado pelo SimTop; distinguir isso dos recordes/prêmios persistentes antes de escrever uma receita de save.

[Lido] **Cobertura ao escopar a F7:** `roteiros.py soma` imprime quantas chamadas com gancho foram observadas,
mas sua saída 1 só exige as pendências do essencial. Com os 82 ganchos instalados, pode sair 0 com chamadas
ausentes. Para afirmar cobertura completa, compare nominalmente `unpacks.levantar()` com as chamadas dos logs,
e os arquivos das faixas do `--disco` com os `LEU`. Gancho, conversão e comportamento concluído são provas
distintas. `Roteiro_Pad` atualmente só fornece entrada ao `socket=0`; os dois controles dados como ligados pelo
teste não provam o jogador 2 nem a tela dividida. Levantamento e limites em [F7/00-escopo](plano/F7/00-escopo.md).

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
| `gira LX LY N` | (F5, etapa 9) o direcional esquerdo inclinado (LX e LY de -128 a 127, 0 o centro) por N quadros, com os botões soltos, e solto no quadro seguinte. É o que gira o modelo do visualizador (`OrbitCam_Update` lê `gPad[0].gameLeft`; zona morta de 50, então use 64 ou mais) |
| `ritmo SEGURA SOLTA` | o ritmo dos `aperta` seguintes |
| `repete ate COND` [`max Q`] ... `volta` | repete o bloco até a condição, que se confere a cada quadro, em qualquer ponto do bloco; aninhados, a de cada um (a de fora encerra também os de dentro) |
| `desbloqueia` | `Save_UnlockAll` do próprio jogo (o da aba de truques da janela do PC): todo personagem, estágio, música, item e as sete esferas. Só com `BT3_DESBLOQUEIO=1` e com o cartão vazio no começo da rodada (sem o arquivo de dados do save em `BT3_SAVES`, que no 360 é `game:\saves`): fora disso o roteiro para com erro, para o desbloqueio nunca ir para um save de verdade |
| `confere som N` | o som do save em memória (`gSaveData->soundMode`: 0 estéreo, 1 mono) igual a N, ou falha com arquivo e linha |
| `confere saga N` | (F4, etapa 9a) o fim da saga N da História visto no save em memória (`MSLOT_OUTRO_SEEN`, o valor 4, bit 2, de `gSaveData->slot[N].flags`, que o `HistOutro_Init` grava e o `HistSave` leva ao cartão). Com `SEMMAPA=1` o `roteiros.sh` confere também o arquivo do cartão das duas builds (`roteiros.py save`) |
| `termina luta` [`vence`\|`perde` [`apos N`]] [`max Q`] | (F4, etapa 9a) termina a luta que está andando pelo caminho do próprio jogo: espera o estado 3 da sequência da luta (`BtlSeqFight`) e derruba o lutador ativo do lado que perde (`vence`, o padrão: o lado 1; `perde`: o 0) com o dano do jogo (`BtlMember_Damage` com a vida inteira, que levanta os eventos de KO 0x4A e 0x48); o jogo decide o resto: no Duelo e nos outros modos o `BtlSeq_CheckBattleEnd` grava o vencedor por KO; na História (modo 1, em que o KO não se confere sozinho) o roteiro de evento do episódio dispara com o KO e grava o resultado (comando 9, `BtlFacade_SetResult`). Num time, derruba cada membro que entra; `perde apos N` derruba antes N adversários (um depois do outro, como na Sobrevivência) e então o do jogador. Termina quando a sequência sai do 3 (o anúncio do fim); mais de Q quadros (6000) é falha. O golpe sai no `BtlChars_SampleInput` (seção 6): por `--wrap` nas builds sem janela e, no 360, pela renomeação da chamada no `battle.c` (`c89.py`, `renomes`); com as mesmas proteções do `desbloqueia` |
| `confere save u8\|s32 DESL N` | (F4, etapa 9b) um campo do save em memória (`gSaveData`) pelo deslocamento igual a N: o que um modo grava no fim (a missão cumprida, os adversários vencidos da Sobrevivência, o lugar no ranking, o curso vencido) |
| `funde discos` | (F4, etapa 9b) o Disc Fusion sem a troca de disco: grava em `gProgress->discFlags` (+0x684, da sessão, não do save) os dois discos como lidos, o que o jogo faz ao achar o disco do BT1 ou do BT2 na bandeja (no port a unidade nunca abre, e o diálogo fica preso); antes de entrar no modo 24. Com as proteções do `desbloqueia` |
| `marca TEXTO` | uma linha no rastro; `marca memoria NNNNN` pede ao `roteiros.sh` (`SEMMAPA=1`) a memória das duas builds naquele vblank, com o conteúdo do arquivo NNNNN da partição 1 (um contêiner BPE inteiro: retrato, estágio, página) vivo e conforme |
| `inclui ARQUIVO` | outro roteiro, relativo à pasta deste |
| (F5, etapa 3) condições da luta, em `espera`, `aperta ... ate` e `repete ate` | `acao L A[-B]`: a ação atual do lutador do lado L (0 o jogador; `+0x948` do lutador, o número de `gBtlActHandlers`) é A ou está entre A e B, e o "ok" diz a ação que o jogo deu; `seq N`: o estado da sequência da luta (`btl_seq.h`: 3 o combate, 4 o anúncio do fim, 6 o menu do resultado); `pausa 1\|0`: o menu de pausa aberto ou fechado (`BATTLE_FLAG_PAUSE_MENU`); `dano L`: o lado L perdeu vida desde o começo do passo (o golpe acertou); `ki L N`: o ki do lado L chegou a N (100000 a barra); `janela N`: a janela contínua de combate da medida de tempo (`passos.c`) tem N passos. Lidas pelos deslocamentos do lutador (`gBtlChars`), sem chamar código do jogo |
| `confere luta C0 T0 C1 T1 ESTAGIO MODO TEMPO NIVEL` | (F5, etapa 3) a luta que o jogo montou (`BattleSetup`), com ela andando: personagem e traje do membro 0 de cada lado, estágio (o atual igual ao de início), modo, tempo (índice de `gBattleTimeLimitTbl`) e nível do COM; um membro por lado, variante 0. Diferente: falha com os valores |
| `rastreia luta` | (F5, etapa 3) a luta no rastro dali em diante: cada estado da sequência (`roteiro: seq N`), a matriz de ações dos dois lados ao sair do combate (`roteiro: matriz lado L movimento N carga N ...`, as famílias do `roteiro.c`, `kFamilias`), o resultado gravado (`roteiro: resultado estado 4 vencedor 0xW razao 0xR vida A B`) e as janelas da medida de tempo (`roteiro: janela K passos N vblanks V ... fecha MOTIVO`). Só nos roteiros que o pedem: o rastro dos outros, e as referências deles, não mudam |
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

**A vitória dada** (só para os roteiros, F4 etapa 9a): o passo `termina luta` serve a todo modo que só continua depois
de uma luta terminada (o resultado e o save da História; na 9b e na 9c, Missão, Sobrevivência, Sim Dragon, Torneio): a
luta começa e anda pelo roteiro como qualquer outra, e o passo derruba o adversário pelo dano do jogo, sem tocar no
código do jogo nem do port (`port/x360/headless/roteiro.c` e o `--wrap` do `headless.py`). Fica fora do jogo normal
como o `desbloqueia`: sem `BT3_ROTEIRO` o roteiro nem existe, sem `BT3_DESBLOQUEIO=1` o passo para o roteiro com erro,
e com um save no cartão no começo da rodada também (a vitória iria para o save de verdade). No 360, onde o link do XDK
não tem `--wrap`, o mesmo embrulho entra pela preparação: o `c89.py` compila o `src/battle/battle.c` com
`-DBtlChars_SampleInput=__wrap_BtlChars_SampleInput` (a única chamada, a do `Battle_Loop`, e a declaração do header
dentro dele), e o `roteiro.c` chama a original pelo nome (`#ifdef PORT_X360`), como já se fazia com o `main` e o
`Progress_Main`. O upstream não muda, e a `.text` da x86 também não (o `roteiro.c` da x86 sai igual depois do
pré-processador). Sem o passo, o embrulho só compara o pedido (`sDerruba`) e volta.

**Comportamento só do console no código do jogo: `PORT_X360`, não `PORT_BIG_ENDIAN`** (F4, 9c, Disc Fusion). O
`PORT_BIG_ENDIAN` vale também na PPC sem janela dos roteiros, que se compara com a x86: um comportamento novo nele faz
as duas divergirem. O que é do aparelho (a bandeja que o 360 não tem, no `DiscFusion_Input`) vai em `PORT_X360`, que só
o `c89.py` define: a x86 e a PPC seguem o caminho original, a `.text` da x86 não muda, e a prova do caminho novo fica
para o console.

**As ações do roteiro de evento da luta no rastro** (F4, etapa 9b): nas builds sem janela, cada ação do GSCF que
começa vai ao rastro, "`roteiro: gsc acao 10001 vblank V quadro Q`" (`gsc principal -1` na ação principal, `gsc roda
1000` na da carga), por `--wrap` no `Gsc_StartAction`, no `Gsc_StartMain` e no `Gsc_RunAction` (`headless.py`,
`roteiro.c`): o critério diz quais eventos da História rodaram nas duas builds, não só que a luta acabou igual. As
chamadas de dentro do `gsc.c` (uma ação que chama outra) não passam pelo `--wrap`; o 360 não escreve essas linhas.

**Como escrever um roteiro novo** sem ver a tela: rode-o na x86 sob o gdb com um `break *Snd_PlaySe` (cada aperto que
o jogo aceita toca um efeito: 1 0 mover, 1 1 confirmar, 1 2 voltar, 1 7 recusado) e um `break *Sprite_Unpack` com
`info symbol *(int*)$esp`; para o estado interno de uma tela, imprima os campos dela no mesmo comando (`gCharSel->side[0]->state`).
Uma rodada da x86 até a luta leva uns 10 s.

**Roteiros de luta (F5, etapa 3: `f5_duelo.txt`, `f5_treino.txt`, plano/F5/03.md).** O que custou rodadas:

- **A ação no estado, não o botão.** Cada ação exigida espera a ação do lutador (`aperta L2 ate acao 0 0x37`), e o "ok"
  do rastro traz a ação que o jogo deu; o `roteiros.py f5` exige a família pela faixa dele, não pela do roteiro (uma
  faixa alargada no roteiro não passa). Os botões do controle padrão (`Save_ResetOptions`, `BtlInput_GetKeyMask`):
  QUADRADO golpe, TRIANGULO esfera, CIRCULO defesa, X dash, L2 carga, L1 trava, R1 e R2 subir e descer; a técnica
  (Blast 2) é L2 + TRIANGULO com ki. As famílias de ação estão em `roteiro.c` (`kFamilias`), dos nomes dos handlers em
  `port/data/cod_1C3400.data.s` (a tabela começa no id 1: o `.space 4` é o 0).
- **O cursor do COM começa na segunda célula** do CharSel (a do jogador na primeira): `DIREITA 3` dá a quinta, o
  Kuririn. Confira com `confere luta`, que diz o que o jogo montou; a imagem do estágio não diz quem é o adversário.
- **O fim do Duelo é um menu dentro da luta** (estado 6, `BtlSeqEnd_Update`: o `PauseMenu` de resultado): a primeira
  opção refaz a luta, a segunda volta ao CharSel, a terceira ao DuelMenu, a quarta ao menu principal, e cada uma pede
  "sim" na confirmação, que começa no "não" e muda com CIMA (não ESQUERDA). Sem isso a luta fica parada no estado 6.
  No Treino, o menu de pausa: a quarta opção refaz a luta, a quinta volta ao CharSel do treino, a sexta ao menu
  principal, com a mesma confirmação.
- **A memória dos pontos da F5 é obrigatória** (`marca ponto`, etapa 5): os `f5_*` incluem carga, combate,
  técnica, pausa e retorno, conforme o roteiro. A opção `MEMLUTA` saiu. Cada dump tem o testemunho `.pool`
  produzido pela própria arquitetura (`headless/memoria.c`) e o esquema `.pool.json` do gdb (`campos.py --donos`).
  Falta de testemunho, dono, geração ou tipo vivo é erro, não uma classe genérica da arena.
- **Uma linha nova do rastro não pode ter " fim "**: o `roteiros.sh` acha o fim do roteiro por `^roteiro: .* fim `, e a
  primeira versão da janela ("... fim pausa") passava por fim do roteiro. As janelas dizem `fecha MOTIVO`.
- **Linhas novas no rastro só com um passo que as pede** (`rastreia luta`): o rastro se compara com as referências de
  cada roteiro, e uma linha nova em toda luta mudaria a referência de todos os roteiros com luta.
- **Uma marca logo antes do `fim` não tem imagem**: a imagem de uma âncora é a do quadro seguinte, e o `fim` encerra
  a x86 no mesmo quadro (a do `marca ponto retorno` faltou nas duas builds). Deixe quadros depois da última marca.
- **Um caminho novo na grade de estágios muda o resto do `stageFile`**: o buffer da imagem compactada (0x3B800 bytes)
  guarda o arquivo atual e, depois dele, bytes de uma imagem anterior maior (o `f5_duelo` passa pelo estágio 20 antes
  do 5: 8.004 palavras "igual-sem-inverter" na tela 39). É resto de buffer, como o do `faceFile`; ficou em
  `CLASSES_CONHECIDAS` com teto e só nas telas das seleções.
- **Um roteiro que passa por uma tela de `TELAS_MEM` sem carregar um dos conteúdos dela falha a memória da tela**: o
  `f5_treino` entra no Train (tela 44) e vai ao treino livre sem abrir aula, e a tela exige a `pagina_treino` viva. A
  linha "tela N sem o conteúdo X vivo e conforme" com "memdiff saiu com 0" é a expectativa do verificador, não memória
  diferente: leia a linha da DIFERENÇA antes de suspeitar de resto de buffer (a conferência final da etapa 3 da F5
  suspeitou do `stageFile`). A dispensa é por roteiro e tela (`roteiros.py`, `TELAS_MEM_SEM`) e só vale sem nenhum
  arquivo do conteúdo no `lidos.txt` da rodada.
- **Uma luta mais longa lê dados que as curtas não liam**: o `f5_duelo` (técnica, HUD dela, 3.000 passos) achou no
  `conv_tool --mapa` três posições que o conversor não troca do jeito que o jogo lê (plano/F5/03.md, seção 4).
- **Lua e troca de cenário estão nos dados do estágio**: o arquivo `0x171 + estágio` (`pzs3us1/(0x170 + estágio)`), o
  bloco na palavra 2 do cabeçalho; `+0x74` aponta as bandeiras (`STG_FLAG_MOON = 0x10`), `+0x14` o `StgInfo`, cujo
  `changeKind` 1 ou 2 troca o cenário no meio da luta (`BtlStage_GetChangeTarget`). A leitura está em plano/F5/03.md.

**A medida de tempo por passo** (`port/x360/headless/passos.c`, F5 etapa 3): a fronteira é a leitura do controle do
jogador 1 (o `Port_PadRead` da porta 0, nas três builds); a janela é o combate (estado 3) sem pausa; o trabalho é o
intervalo menos a espera de vblank (`gPortSleptNs`); o Present e as imagens vêm do `gs_d3d.cpp`; os descartes, de
`Passos_Descartes`. Nada se escreve durante a janela (nem os resumos de 60 quadros do `gs_d3d.cpp` e de 600 vblanks do
`estado360.c`); ao fechar, "passos: janela K ..." e, com `rastreia luta`, a linha determinística no rastro. A primeira
linha de todo log é "passos: origem COMMIT diff:HASH", o carimbo da build (`headless.py` e `build_f2.sh`):
`roteiros.py f5` e `passos.py` recusam o log de outra build (pelo commit e pelo hash da diferença; o sufixo
"(N alterados, M apagados)" do `wsl.sh enviar` só passa com zero e zero: `passos.py mesma_origem`). O aceite do console é `passos.py LOG... --console`.

### Achar o que depende da ordem dos bytes

| Ferramenta | O que faz |
| --- | --- |
| `headless/rastreio.c` | runtime de instrumentação (ASan em modo chamada): cada byte lido do disco ganha uma origem (arquivo, deslocamento), e cada leitura dele é anotada com a largura e quem leu |
| `rastreio.py mapa` | do rastreio, o mapa "arquivo, deslocamento, largura" que a build PPC usa para inverter na carga; conflitos vão para um arquivo à parte e nunca são invertidos |
| `headless/inverter.c` | aplica o mapa na PPC (na leitura do arquivo e na saída da descompressão) |
| detector de visões (`BT3_VISOES` no `rastreio.c`, `visoes.py`) | acusa memória gravada com uma largura e lida com outra (u8 de um s32, u64 sobre dois u32). A lista de pares já tratados fica em `visoes_resolvidas.tsv` |
| `tipos.py` | liga cada leitura do rastreio a struct e campo do código (coluna do DWARF + AST JSON do clang) |
| `port/x360/conv/` (`conv_tool`) | conversores por formato em C89, para o instalador do 360 e para o PC: troca in-place com limite conferido e cada byte marcado; `conv_tool --mapa` confere contra o rastreio e mede a cobertura (decisão 12); `--disco` termina com o censo dos arquivos de menu por faixa de id (quantos arquivos e contêineres BPE ainda sem conversor); `--pecas` valida cada peça comum dos menus (`formatos/menus-pecas.md`) só na seção dela, em todo pacote do disco que a usa (a tabela `sUsos`), antes de o pacote inteiro ganhar conversor, e com `--mapa` confere só as posições dessa seção. No `--mapa`, uma leitura de 2, 4 ou 8 bytes sobre texto que o conversor deixou em bytes (o laço do fim do rótulo do Flash vira `strlen` na x86r) conta à parte, "em cadeia de texto lida por inteiro". Arquivo e seção para formato numa tabela só (`sFaixas`, conv.c). A `conv_tool` se compila sem `conv_jogo.c`, a ponte do jogo, que cita funções dele. Cada conteúdo descomprimido (`ARQ@0xN#bpe`) se converte também pelo caminho do jogo (`Conv_FormatoBits`, o mapa de bits de `conv_jogo.c`): bytes, resultado e cobertura têm de ser os mesmos, e o conteúdo tem de caber em `CONV_JOGO_MAX` |
| `convert_iso.py` e `teste_iso_convertida.py` | instalação no PC da decisão 23: usa `conv_tool --lista-conversores` e `--saida`, publica uma ISO do mesmo tamanho só depois da reabertura integral; `--conferir --dados DIR` confere outra vez contra os dados extraídos. O teste completo provoca recusas e estragos e roda `--disco`. Contrato e comandos em `formatos/iso-convertida.md`; marca versão 2 com ícones Kenney e recusa de conversões antigas no 360 |
| `teste_iso_avisos.py` | F8: o leitor `iso360.c` real sobre a API de teste do Linux, com cabeçalhos, diretórios e AFS inventados em imagens esparsas; confere identificação, recusas e escolha da convertida, exige falha numa cópia estragada e acusa seis mutantes em execução; sem console nem emulador |
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

### Ícones na instalação e dados dos roteiros

A troca dos ícones usa `conv_tool --icones360 --saida`, nunca o `--saida` comum dos
roteiros. A PPC lê arquivos pré-convertidos por essa ferramenta: trocar a arte por
padrão faria a PPC divergir da x86 apesar de ambas partirem do `gamedata` original.
`teste_icones360.py` compara os dois caminhos: só a instalação muda 10 a 27.

O `TEX0` de `TexEntry` é um u64 trocado inteiro por `Conv_U64`: depois da conversão,
a palavra alta fica em +0x30 e a baixa em +0x34. Não usar a ordem de duas palavras
do fluxo GIF para validar esse campo. A primeira conferência da troca recusou a
folha válida por essa confusão; o teste da API cobre a folha convertida real.

O PNG `xbox_lt.png` do Kenney é indexado de **4 bits**, enquanto os outros nove são
de 8. O gerador sem dependências trata os dois e confere CRC e filtros. A primeira
execução mostrou essa diferença; não recodifique os PNG originais no repositório.

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
- **Separar rodar de analisar (16/10/2026).** O emulador não custa token; custa o agente vivo esperando por ele. A
  cada passo o agente reenvia o contexto inteiro, e uma espera de mais de 5 min expira o cache e o regrava inteiro.
  Na tarefa 9c-2 foram 36,7 milhões de tokens de leitura de cache e 2,7 milhões de escrita, contra poucos milhares de
  saída (o código). Por isso, numa rodada longa (a conferência oficial, os roteiros de uma tarefa, a rodada completa):
  1. o agente prepara o commit e dispara a rodada num serviço (`systemd-run` no WSL; `setsid` com log no Deck) e
     termina, entregando o que falta conferir;
  2. um vigia de shell em segundo plano (que não gasta token) espera o fim do serviço;
  3. só então um agente novo, com contexto pequeno, lê o resultado da rodada e age: "tudo certo" fecha a tarefa; uma
     falha vira uma tarefa nova com a linha da falha e o caminho do log.

  Rodadas curtas de desenvolvimento (uma build, um roteiro durante a investigação) continuam dentro do agente.

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
7. **Dado que o jogo grava e lê de volta (o save).** A imagem de memória vai ao cartão na ordem do host, e a soma do
   jogo (bytes somados, no BT3 o `McCard_Checksum`) dá o mesmo valor para os mesmos bytes em qualquer ordem: ela não
   recusa um save de outra ordem, só as palavras onde a soma se guarda é que saem trocadas. No BT3 o teste ainda
   aceita quando UMA das duas palavras confere, e um save de PC cuja soma tem um byte zero (um em 128) carregava na
   PPC com todos os números trocados (F4, etapa 8). Não confie que "a soma pega": calcule o que o teste do jogo faz
   com as palavras lidas na outra ordem, e prove com um save de outra build acertado para o pior caso
   (`save_entre.py estraga ... soma0`). No BT3 a recusa é pelo cabeçalho (`PORT_BIG_ENDIAN` no `McCard_Checksum`:
   palavra acima de 0xFF é de outra ordem), sem mudar o formato.

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

- **A ISO do BT3 também tem UDF (F5, etapa 2).** Não tomar a lacuna entre diretórios e arquivos, nem a cauda da
  imagem, por espaço livre só pelo ISO 9660: há metadados UDF nesses setores. A marca da ISO convertida ocupa
  512 bytes de folga interna do cabeçalho AFS0, com limites e ausência de leitores provados em
  `formatos/iso-convertida.md`. Conferir toda a imagem fora das conversões e da marca preserva também UDF.
- **Publicar sem substituir exige uma operação atômica.** Testar a existência e depois usar `rename` no Linux
  ainda pode apagar uma saída criada durante a conversão. Usar `renameat2(RENAME_NOREPLACE)` e provocar essa
  corrida com um destino preexistente; ambos os arquivos precisam permanecer intactos.
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
  - processo em segundo plano morre com a sessão;
  - a sessão morre junto com o notebook suspenso ou o Windows desligado: a rodada final da etapa 6 da F4 se perdeu
    assim no quarto roteiro. Numa rodada de horas, confira antes que nenhuma das duas máquinas vai dormir, e depois de
    uma queda refaça do começo (as builds do HEAD e todos os roteiros), não a partir do meio.
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
  - **A profundidade de cada alvo com uma EDRAM só** (F5, etapa 7, plano/F5/07.md; o desenho, ainda não medido no
    console): `Resolve(D3DRESOLVE_DEPTHSTENCIL)` para uma textura D24S8 e a volta como **cor**, numa superfície de cor
    com a mesma `Base` da profundidade e a textura relida como A8B8G8R8 por um cabeçalho do `XGSetTextureHeader`
    (bit a bit, sem tile a mais). O exemplo do XDK que ensina é o `FastDepthRestore` (`Source/Samples/Graphics` e
    `Media/Shaders`): procure nos exemplos antes de inventar. Duas pegadinhas: dentro de cada tile de 80 pixels as
    metades de 40 colunas da profundidade e da cor estão trocadas, e por isso a superfície de cor precisa da largura
    de tiles inteiros (1040 para um alvo de 1024) e as áreas, de x em múltiplos de 80. Prove o restauro no boot com
    um padrão qualquer, resolvido antes e depois, e um controle que tem de dar diferente.
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
  - **Tempo de GPU por passada sem xbdm** (F5, etapa 8, `plano/F5/08.md`; ainda não medido no console): o
    `D3DPerfCounterBatch` com `D3DPERFCOUNTERINDEX_TIMESTAMP` (o do exemplo `FastDepthRestore`) só existe no
    `d3d9i.lib`, que importa funções do xbdm; o `d3d9.lib` de Release tem só o `Lock`/`Unlock`. O que existe nele:
    `InsertCallback(D3DCALLBACK_IDLE | D3DCALLBACK_CPUn)`, que a GPU executa depois de terminar o que veio antes e
    espera voltar, com o `__mftb()` na callback (uma DPC: só guardar o relógio). Fuja da CPU 4 (o padrão, a do
    áudio). Cada trecho entre marcas inclui o custo de uma marca: meça duas marcas seguidas a cada quadro e desconte.
    E o trecho pode incluir a GPU esperando a CPU (o ring buffer vai em segmentos de 64 KB): guarde também quando a
    CPU pôs cada marca para limitar essa espera.
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
  - **A tela de replay caía nas builds sem janela** (SIGFPE no `ReplayMenu_Draw`, `menu_za_d.c:409`,
    `num / (s32)pow(10.0, i)`, divisão por zero): o `pow` de double não passa pelo `port_libm.h` (só o `powf` passa),
    e o código do jogo, em float por software, chama o da libm, de float em hardware. Provado na 9c-2: na x86 a libm
    devolve 10 no `st0` e o jogo lê `edx:eax` = 0x3800 (o `(s32)` dá 0); na PowerPC o jogo passa os argumentos em
    r3..r6 e a libm lê f1 e f2 (lá a `divw` por zero não cai: os dígitos sairiam errados em silêncio). O contorno é
    só do teste: o `headless.py` liga uma cópia do `menu_za_d.o` com o `pow` renomeado (`objcopy --redefine-sym`) para
    uma ponte de inteiros (`port/x360/headless/ponte_pow.c`). Um `--wrap=pow` não serve: o som do GS também chama
    `pow`, com float em hardware, e iria para a ponte com a convenção errada (confira com `nm -A obj/*.o` quem chama
    um nome antes de desviá-lo). O 360 compila tudo com float em hardware e não usa a ponte. A janela do PC do
    upstream, que compila o jogo do mesmo jeito, deve ter o mesmo defeito (não conferido).
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
  - **O `BT3_DATA` da PPC sem mapa nunca vai para a x86** (F4, etapa 8). A pasta dele tem os arquivos convertidos para
    o big-endian em `mods`, e a x86 os lê como little-endian: caiu no boot (SIGSEGV) e pareceu efeito do save que se
    testava. A x86 roda sem `BT3_DATA` (o `gamedata` original); os scripts passam a variável só à PPC.
  - **Um save que muda o menu muda o roteiro.** Os roteiros contam apertos a partir do cursor padrão; com o save de
    PC lido trocado (etapa 8), os desbloqueios trocados deram outro anel no menu principal, o CIMA 2 das Opções foi à
    Referência (tela 60) e a PPC parou na guarda de lá. Num roteiro que carrega um save, confira a tela a que se
    chegou (`aperta X ate tela 62 max 300`) e o estado do save (`confere som`) antes de navegar.
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
    `CONV_JOGO_MAX` (8 MB de conteúdo, 1 MB de mapa; 9 MB e 1,125 MB desde a etapa 9b, pelo tabuleiro do Sim Dragon), e falha com nome se não houver memória ou se um conteúdo passar
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
  - **O verificador de origem tem de ser testado com a origem que o ciclo real produz.** Na conferência final da
    etapa 5 da F5 o `roteiros.py f5` e o `passos.py` recusaram os próprios logs do commit certo (8 falhas, uma rodada
    de duas horas): a build veio pelo `wsl.sh enviar`, que carimba "COMMIT diff:HASH (0 alterados, 0 apagados)", e os
    dois comparavam a linha inteira com "COMMIT diff:HASH". O `roteiros.sh` cortava o `CARIMBO` nos dois primeiros
    campos e passava, e as rodadas que deram "tudo certo" antes saíram de builds carimbadas pelo git, sem o sufixo
    (o clone do agente do WSL, sem `enviar`). Agora os dois comparam commit e hash e
    aceitam o sufixo só com zero e zero, vistos recusando outro commit, outro hash, sufixo com alterações e sufixo
    estragado, sobre cópias dos logs da rodada (`REUSA=1` refaz as conferências sem emulador).
  - **Byte de uma palavra de pacote não é o primeiro byte da memória no big-endian.** As letras dos nomes
    (`port/src/gs/namefont.c`) conferiam o endereço do registrador do GS em `pix[0x38]`; o pacote de imagem vem
    convertido em palavras u32 (o registrador de 64 bits como duas, a baixa primeiro), então no big-endian o byte é o
    menos significativo da palavra (`PKT_LOW8`), e a PPC caía para texto simples sem aviso. O teste é o próprio
    `BT3_NAMEFONT_TEST`: sem a correção a PPC não diz nada; com ela corta as 36 e as 24 legendas como a x86. As
    imagens de teste ainda diferem em uns 3% dos bytes (causa não achada; só vale para estágio ou música de fora do
    disco, que o 360 não tem: `plano/F4/05.md`).
- **Coleta conjunta de memórias e imagens (28/10/2026).** Com `SEMMAPA=1`, `RAPIDO=1` é o padrão:
  a PPC normal coleta a memória final, todas as telas e as imagens numa passagem; a x86 de memória faz o mesmo.
  `BT3_MEMORIAS` lista os vblanks e destinos, e o gancho de teste grava dump, páginas e subalocações sem sair.
  Continuam as janelas de pintura, as comparações de tipos e donos e os mesmos arquivos para os verificadores.
  `RAPIDO=0` conserva o modo separado; `MEM=menu` conserva o fluxo da guarda. O gravador fica separado para provar
  que abri-lo não muda o jogo. Em `historia_novo`, os arquivos da rodada da F6 de 28/10 delimitam 4968 s no total,
  4265 s de PPC, sendo 1880 s nas imagens e 1851 s nas passagens PPC que a coleta pode retirar. O custo acrescido
  de calcular hashes com as imagens precisa entrar na medida do ganho. O inventário, o protocolo de equivalência,
  a integração da `.text` dos conferentes F5/F6 e os caminhos das medidas estão em
  [medicoes/roteiros-rapido.md](medicoes/roteiros-rapido.md). Cada execução grava sua duração em `tempos.tsv`.
  Na comparação entre modos, os caminhos absolutos são normalizados para o mesmo arquivo lógico e os dois
  ponteiros temporários `gColWalkBox`/`gColWalkCtx` usam a dispensa já existente. A primeira equivalência das Opções
  mostrou também endereços malloc de `sMapa` e do IOP de `SndMgr`, e restos livres/enchimento do heap.
  O comparador preserva NULL contra não NULL e aplica as mesmas exclusões de `memdiff --so-em-uso`;
  dados vivos, cabeçalhos e rodapés continuam byte a byte. Controles nas duas ordens de bytes provam os limites.
  Cada arquivo de páginas é recalculado sobre o dump original pela libxxhash, antes de normalizar os caminhos.
  `REUSA=1` continua julgando todas as memórias e imagens; uma cópia estragada precisa falhar também neste modo.
  Na rodada final, Opções passou de 998,93 para 829,74 s (16,9%) e Duelo de 1676,15 para 1294,51 s (22,8%),
  com 79 e 93 arquivos/provas equivalentes. A projeção dos roteiros da F6 é 13,71 para 9,79 h; da F5, 13,39 para
  9,46 h, sem contar os testes fixos. As faixas e os limites da extrapolação estão no inventário.
  Para controles com `SOM=1 REUSA=1`, colete e confira com `GUARDA_SOM=1`: os PCM são apagados por padrão,
  e a primeira restauração da cópia reprovou por falta deles. Para controles específicos de memória e imagem,
  retire `SOM` do ambiente e preserve o julgamento de áudio da rodada completa. Exija também a cópia restaurada
  com `tudo certo`. Uma mudança só de documentação não pede repetir o jogo: recompilar e comparar o código
  com a rodada aprovada conserva essa prova sem ocupar o emulador.
- **Imagens das telas na etapa 6 da F4, o que custou rodadas:**
  - **O rasterizador pintando todo quadro leva umas três horas por roteiro na PPC** (no qemu, uns 30 quadros por
    minuto nos menus; a x86, 7 minutos). O que custa é pintar: com o GS ligado e nada pintado (`BT3_GS_FROM` alto) a
    PPC faz o roteiro do Duelo em uns 7 minutos (20 com o microcódigo da VU1 cru, abaixo), e a x86 em 15 segundos. As imagens só pintam uma janela de quadros
    antes de cada uma (`headless/imagens.c`). Conferido na x86: com janela de 8, de 2 e de 0 quadros as 9 imagens do
    Duelo saíram idênticas às da rodada pintando tudo, porque cada quadro começa limpando o buffer que vai desenhar
    (`sceGsSwapDBuff`) e as texturas chegam por carga, que o GS processa sempre.
  - **Sem mexer no código do upstream**, o gancho é um `--wrap` do linker no `Port_GsVif1Chain` e no
    `Port_GsGifChannel` (chamados de `plat_gs.c` e `gs_sony.c`, outros objetos) e as funções que o `gs_core.c` já
    exporta (`Gs_VramRead`, `gGs`, `gGsFrame`, `gGsMainFbp`). A `.text` da x86 muda (é o programa de teste com um
    arquivo a mais e duas chamadas por outro caminho); a prova de que o jogo e o port não mudaram passa a ser os
    objetos: os 318 `.o` de `src/`, `port/src` e do GS idênticos byte a byte aos da build anterior.
  - **O último quadro antes da troca de tela é preto:** o modo do gerenciador muda depois do fade de saída (no
    `DuelMenu` e no `CharSel`; no menu principal muda no X, antes do fade). A imagem "de saída" é a de 100 quadros antes
    da troca, e o `compara_imagens` recusa imagem de uma cor só (idênticas, mas não provam nada).
  - **Âncora no rastro, não no vblank:** a imagem é N quadros do roteiro depois de uma linha do rastro, decidida dentro
    do programa, nas duas builds. Para uma imagem no meio de uma tela, uma `marca` no roteiro (instantânea: não muda
    quadro nenhum, só acrescenta a linha ao rastro, e a referência se regrava com `REF=1`); as páginas do Treino
    ganharam `marca pagina 1..6`.
  - **Uma marca imediatamente antes de `fim` não chega à captura.** Na 9c-3, as nove imagens presentes do Torneio
    eram idênticas, mas a décima (`marca chave atualizada`) faltava nas duas builds, e o `compara_imagens` recusou
    a rodada. `Roteiro_Pad` chama `quadro()` antes de `Imagens_Quadro`; `P_MARCA` continua na mesma leitura até
    `P_FIM`, que faz `exit(0)`. Não há quadro nem envio ao GS para gravar a imagem armada pela marca. Ponha uma
    espera depois da última marca (o Torneio usa `quadros 2`), antes de `fim`, e regrave a referência com `REF=1`.
  - **A imagem da luta achou o microcódigo da VU1 cru.** Os pacotes da VU1 das tabelas do executável
    (`vu1_micro.data`) eram `u8[]` no C, e o mapa das tabelas os deixava como vieram: a VU1 da PPC rodava instruções
    inexistentes ("unimplemented lower instruction", programas sem fim), a rodada levava 20 minutos em vez de 7 e a
    imagem da luta saía com 229 mil pixels diferentes. Tipados como fluxo VIF (`FLUXOS_VIF` do `elftipos.py`), sobram
    17.673 pixels (desvio até 7), todos de 26 primitivas com os quatro bytes da cor invertidos: os `u8 rgba[4]` dos
    efeitos da luta (`formatos/gpu.md` seção 6, F5). Por isso a luta fica fora do critério (`IMG=luta` a inclui).
  - **Uma explicação de memória que diz "como o GS lê" tem de ser vista na imagem.** A etapa 5 aceitou as palavras
    `igual-sem-inverter` dos pacotes da GPU como "cores RGBA byte a byte, o 3c3c3cNN do fade, como o GS as lê". A
    imagem da luta mostrou que eram justamente as cores invertidas dos efeitos (o dump dá `rgba 3c3c3c16` na x86 e
    `163c3c3c` na PPC): erro de conversão da F5, não cor certa. A aceitação ficou só no fim da luta (`SO_NA_LUTA` do
    `roteiros.py`); numa tela de menu, uma palavra dessas é diferença.
  - **Duas imagens no mesmo quadro do roteiro** (a `marca carregado` e o `ok som 1` das Opções saem na mesma leitura do
    controle): a primeira versão do `imagens.c` guardava uma só e a outra faltava nas duas builds; agora cada imagem
    tem a sua marca de pendente.
  - **Imagens idênticas não provam que o momento é o certo.** A primeira versão casava a âncora "tela 4" também com o
    "ok tela 4" da espera do roteiro; a "segunda visita ao menu principal" das Opções saiu da primeira, nas duas builds
    igual, e a comparação passou. A âncora agora é o texto inteiro da linha antes do " vblank " (sem o
    "roteiro: arquivo:linha "), o mesmo que o `roteiros.py imagens` conta, e o `compara_imagens` confere o quadro de
    cada imagem contra o rastro de referência.
  - **Para achar a causa de uma imagem diferente, compare as primitivas, não os pixels:** `BT3_GS_DUMP` com o quadro
    do GS da imagem (o "gs G" da linha `imagem:`) mais o `BT3_GS_FROM`, nas duas builds, e o `diff` das linhas
    `prim`/`tex` (no WSL; só as contagens voltam). Cuidado: a "clut" que o dump imprime para textura de 32 bits é a
    memória em `cbp` 0, o próprio buffer do quadro; diferente ali é consequência, não causa.
- **A etapa 9a da F4 (a História e a vitória dada nos roteiros), o que custou rodadas:**
  - **Dar o resultado da luta direto não é o caminho do jogo.** A primeira versão do `termina luta` chamava o
    `BtlFacade_SetResult(1)` (vencedor lado 0, KO), o que o roteiro de evento da História grava pelo comando 9: a x86
    caiu no `BtlSeqWinTalk_Enter` (btl_seq.c:1023), que na História (modo 1) lê o evento de roteiro em andamento, e fora
    de um evento ele é NULL. No modo 1 o KO não se confere sozinho: quem termina a luta é um evento do roteiro do
    episódio, disparado pelo KO. O passo agora derruba o adversário pelo dano do jogo (`BtlMember_Damage`, sem
    `BTL_DMG_NO_KILL`; o `BtlFacade_AddHp` dos roteiros do jogo leva `NO_KILL` e deixa 1 de vida), e o jogo faz o resto.
  - **Evento de luta levantado na leitura do controle se perde.** O `Pad_Update` (onde o `Port_PadRead` do roteiro
    roda) vem antes do `Battle_UpdateWork`, que gira os eventos ("agora" vira "anterior"), e o roteiro de evento
    (`BtlScript_Update`, no começo do quadro seguinte) só dispara com evento novo: o KO dado ali nunca disparou nada, e o
    passo esgotou os 6000 quadros com o adversário caído. O golpe sai depois do giro e antes do `Battle_Update`, onde o
    jogo dá os seus: `--wrap` no `BtlChars_SampleInput` (`headless.py`, `roteiro.c`; no 360, que não tem `--wrap`,
    a chamada renomeada só no `battle.c` pelo `c89.py`: **um `--wrap` de uma chamada que vem de outro objeto vira, no
    link da Microsoft, um `-D` só nesse objeto**, sem tocar no código). Antes de mexer no estado do jogo
    de fora, veja em que ponto do quadro o seu código roda e quem consome o que você muda (aqui, a ordem do
    `Battle_Loop`, battle.c).
  - **Palavra lida com duas larguras no próprio arquivo:** o roteiro de evento (GSCF, `gsc.c`) lê cada palavra como u32
    (o tipo, o índice do operando) e também os bytes e a metade alta dela (a contagem, a letra da opção, o id do
    comando). Nenhuma ordem no arquivo serve às duas; o conversor troca a palavra como u32 e o código lê os campos por
    deslocamento em `PORT_BIG_ENDIAN` (`GSC_BYTE1`... em `include/sys/gsc.h`, um `#ifdef` com a linha original no
    `#else` em cada um dos 24 lugares, 23 em `gsc.c` e 1 em `btl_script.c`). O rastreio já dizia: 1.779 posições do 00510 nos conflitos.
  - **Gancho em `src/` muda o `.o` da x86 mesmo dentro do `#ifdef`:** as linhas se deslocam, e a informação de
    depuração (DWARF, `-g`) muda com elas. A prova de que o código não mudou é o objeto sem ela
    (`objcopy --strip-debug` nos dois e `cmp`): na 9a, 321 de 322 iguais, só o `roteiro.c` diferente.
  - **Valor s32 lido pela metade baixa sai "com outra largura" no `--mapa`** (o rastreio viu 2 bytes, o conversor trocou
    4), e está certo **só se todo leitor estreito daquela posição lê a metade certa no big-endian** (`PS2_LOW` ou uma
    struct permutada); confira os leitores, não a posição. Num arquivo de constantes deduplicadas (o GSDT do roteiro
    de evento) a mesma posição serve a vários comandos, e um leitor novo sem a correção passaria. O `conv_tool` não tem
    classe para isso; o `roteiros.py larguras` aceita a posição só se ela estiver em `LARGURA_CONHECIDA`, uma a uma, com
    o porquê e as funções que a leem, e falha se o rastreio da rodada (os pcs do `rastreio.tsv`, pelo programa da x86r)
    mostrar outra função, ou a lista de pcs cheia. A primeira versão dava o porquê errado (dizia `SetStatus`; eram
    operandos do `SetMember`), e nada o conferia: o revisor achou pela análise do GSDT.
- **Extração de `.text` com o executável em uso (9c-1):** `objcopy --dump-section` sem arquivo de saída distinto
  também reescreve a entrada. Use uma cópia ou dê um arquivo de saída separado. Na primeira rodada de imagens da
  9c-1 houve SIGBUS interno do qemu depois dessa extração sobre o executável em uso; a repetição com a extração
  corrigida passou inteira. A prova dos objetos usa sempre arquivos de saída separados no `--strip-debug`.
- **Execução restrita não é diagnóstico do port (9c-1):** neste ambiente, compilar e ler o DWARF estático funcionou,
  mas a x86 nativa terminou com SIGSYS, o gdb recusou `ptrace` e o servidor do qemu recusou socket. A x86 emulada
  permitiu conferir rastro, rastreio, save e imagens contra a PPC. A memória completa pelo fluxo padrão ficou
  pendente, registrada assim no relatório, sem trocar falha por aprovação nem ampliar tolerâncias.
- **A etapa 9b da F4 (o Ultimate Battle e os pendentes da História), o que custou rodadas:**
  - **Os eventos de meio de luta da História pedem o R3.** Com o controle parado, a luta do primeiro episódio andou 45
    segundos e só a ação do KO rodou: o gatilho de tempo (evento 2, 20 s) põe a ação 10001 como pendente, mas ela só
    começa com o lutador do jogador livre para agir e o evento 0x50 novo, que é o R3 (`BTLB_R3`, 0x2000,
    `btl_char_mgr.c`; o aviso na HUD é o `BtlScript_UpdateHud`). O gdb mostrou o porquê (`dprintf
    BtlScript_AddEvent` com o gatilho e a ação, e na linha do `BtlScript_StartPending` o estado de cada lado). O roteiro
    `historia_novo` aperta R3 aos 20 e aos 40 segundos.
  - **Um KO no meio de uma ação de evento não termina a luta.** Derrubado durante a 10002, o adversário caiu, a ação
    seguinte pôs gatilhos novos e o KO já tinha passado: a luta seguiu sem fim (as ações 10003..10006 rodaram sozinhas,
    porque com um lado caído toda ação pendente começa) e o `termina luta` esgotou o `max`. O passo vem depois do fim da
    ação em andamento (o rastro das linhas `gsc` diz quando).
  - **O "diálogo do cartão" do Disc Fusion era a troca de disco.** O inventário da etapa 1 o chamou assim pela forma do
    arquivo (a de um diálogo do cartão); o código pede o disco do BT1 ou do BT2 na bandeja, lê o executável dele e só
    então grava `gProgress->discFlags` (da sessão, não do save; o desbloqueio não mexe nele). No port a unidade nunca
    abre a bandeja e o diálogo prende a tela para sempre, sem saída pelo controle. Os roteiros passam pelo passo `funde
    discos`, que grava o que o jogo gravaria ao achar o disco. Leia o leitor antes de confiar no nome que a forma sugere.
  - **Nem todo modo termina como a História.** O `termina luta` derruba cada membro que entra: na Missão 100 os quatro
    adversários da missão 0 e o jogo grava a missão cumprida; na Sobrevivência os 50 da fila entram um depois do outro
    (uns 14.000 vblanks de luta na x86, longo demais para o qemu), então o roteiro derruba três e depois o jogador
    (`perde apos 3`), e o jogo grava os três vencidos, porque a Sobrevivência grava também na derrota. Cada modo prova o
    fim pelo que o jogo gravou (`confere save`), não só pela tela seguinte.
  - **O resto da luta fica nos buffers de pacotes da GPU.** As cores dos efeitos gravadas byte a byte (o erro da F5,
    `SO_NA_LUTA`) eram aceitas só no fim; a Missão 100 (quatro adversários, 1.566 quadros de luta) deixou 1.050 e 1.061
    palavras nos dois buffers, e as mesmas no resultado e na seleção depois dele, que não as regravam: resto, não cor
    de menu. A memória das telas visitadas depois da primeira luta do rastro passa a aceitá-las com o teto
    (`--apos-luta`); antes da luta, nenhuma. Contagem igual em telas diferentes é o sinal de resto parado.
  - **Leitura de 4 bytes num deslocamento ímpar de um arquivo é o rastreio, não o jogo.** Os lutadores da Missão 100
    deram dez posições "não trocadas" no `--mapa` (bytes do file[0] que o rastreio diz lidos com 4 bytes, pelos
    leitores de vetor da luta), todas em 1 mod 4. No PS2 uma leitura de 4 bytes num endereço ímpar não existe (`lw` e
    `lwc1` dariam exceção, o `lq` ignora os bits baixos), e o arquivo é carregado alinhado: quem leu foi uma cópia para
    onde o rastreio levou a origem do byte. Desde a 9c-1 (resposta 5 da 9b), o `conv_tool` as conta à parte como
    "leituras de 4 bytes em deslocamento ímpar sobre byte (rastreio)": só largura 4, deslocamento ímpar e marca
    `CONV_BYTE`. Saíram as 22 entradas desse tipo de `LARGURA_CONHECIDA`; as duas metades baixas do GSDT continuam
    com a conferência de leitores. Visto falhando sobre cópias do mapa: deslocamentos pares (0 e 2 mod 4), larguras
    2 e 8, u16 convertido e continuação de número não entram na regra. 1 e 3 mod 4 sobre byte entram, com contagem.
  - **Campo opaco que começa no meio de uma palavra** (o `s16 unk29E[3]` do `SScore`, que outra visão do mesmo bloco lê
    como `PClock.ms`) era comparado byte a byte pelo `memdiff.py`, e um u16 trocado certo saía "fora do formato". Agora
    meia palavra opaca aceita o u16 invertido ou os dois bytes iguais (visto falhando com um byte estragado).
  - **A classe pelo tamanho pedido precisa da tela, e a tela do fim também conta.** O resto de 0x2A0 bytes depois de
    um retrato ou de um episódio (os dois descomprimem a 0x20560 bytes num buffer de 0x20800) aparece onde esse buffer
    vive: no `ModeMenu`, nas seleções e, depois de uma luta, com o heap já usado, com palavras de um conteúdo antigo. A
    primeira versão do aperto da 9a o aceitava só na tela 7 e nunca no fim, e os roteiros da História que terminam na
    lista de episódios (tela 7) caíram; a memória do fim agora usa a última tela do rastro.
  - **Buffer alocado e ainda vazio guarda o que havia antes.** O roteiro `ultimate` da etapa 4 termina no tabuleiro do
    Sim Dragon recém-aberto, com os buffers de retrato do anúncio já alocados e vazios: a memória do fim acusou 18.917
    palavras iguais sem inverter num deles, nada que o jogo leia antes de preenchê-los. Antes de mudar um teto, veja se
    o bloco tem algum conteúdo vivo (a linha "marcas heap#N" do memdiff); o roteiro de critério passa pelo anúncio.
    Na 9c (passo 0) virou classe, `BUFFER_IMAGEM_VAZIO` no `roteiros.py memoria`: num bloco em uso de 0x16800 ou
    0x20800 que nenhuma linha de marcas nomeia, as classes de troca valem até o bloco inteiro, e a memória diz por
    bloco quantos bytes aceitou assim. Ela é segura porque o conteúdo vivo se acha pelos bytes da x86 (o `.orig`), não
    pelos da PPC: um retrato mal convertido na PPC continua achado e conferido. O que ela não cobre, visto falhando
    numa cópia: um byte mudado num retrato vivo, um bloco com conteúdo vivo e resto acima do teto, o mesmo bloco vazio
    com outro tamanho pedido, uma palavra fora do bloco e uma palavra "diferente" dentro dele.
- **Evolution Z (9c-1): as seções sem leitor não são necessariamente vazias.** A seção 1 do EvoZ e a 23 da loja
  têm 14.016 bytes idênticos, sem a forma das peças conhecidas. Exigir vazio recusou os dois pacotes reais. Sem
  leitor, preservam-se como bytes (`Conv_RestoBytes`), como a seção 7 das Opções; não se inventa largura pela aparência
  de números. `DcSave_Init` recebe o diálogo da seção 11 do topo já convertido e chama `McFlow_Init(1)`, que passa
  pelo gancho `menu_mcflow` existente: o que faltava era o pacote do chamador.
  Na troca de pontos, a casa ativa vem de `cur[estado]` (estado 5), que começa zerada; o `cur[2] = 3` do código
  de entrada não a inicializa. Três DIREITA giram da unidade à centena: CIMA paga 100, não 1. O roteiro confere
  dinheiro e experiência, e no DcSave espera os estados 12 (salvar), 13 (substituir), 15 (gravação) e 0, sem aceitar o caminho do
  "não" como prova de save. Em ensaio manual, crie também a pasta pai de `BT3_SAVES`: faltar o pai leva ao estado
  17 de erro de gravação já no boot, não a um erro de conversão.
- **Data Center (9c-2): uma tabela de números segue até o fim da seção, não até o mínimo do leitor.** O PassChk
  indexa a primeira tabela só até 0xCA, mas ela tem 208 registros s32 e a seção acaba no último: exigir zero depois
  do mínimo recusou o pacote real. A forma é a da tabela inteira (todos os registros que cabem, com a largura que o
  leitor declara), o mínimo é só a recusa de uma tabela curta, e só o que sobra depois do último registro inteiro tem
  de ser zero. E um estrago de teste precisa cair onde a regra vale: o primeiro "enchimento estragado" do teste caiu
  num registro e passou, com razão.
- **Senha digitada pelo roteiro (9c-2).** Para alcançar um retrato que só aparece com um registro (a lista vazia
  depois do `desbloqueia`), o roteiro digita uma senha válida no teclado da tela: o texto sai do próprio codificador
  do jogo, chamado no gdb da x86 (`call ChrPass_Encode(p)` sobre um `malloc` zerado com o personagem, e
  `ChrPass_GetText()`; confira com `DcPass_Decode` antes), e os apertos saem de uma busca em largura que simula o
  `DcPass_Input` só pelas teclas de um caractere (sem teclas largas nem o anel das bordas, que leem fora da tabela
  antes do `WrapPos`). Funcionou de primeira na x86: 34 caracteres, 243 apertos.
- **O dono de uma timeline pode estar na pilha.** `EvoTop_Run` mantém `EvoTop m` local, e `FlashTl_Init` grava
  `&m.flash[0]` em `FlashTl.owner`: na raiz de 0x18 e no +0xCC de cada `FlashClip` de 0xE4 (18 no topo). Esses
  ponteiros nativos ficam fora da faixa imagem/heap reconhecida pelo memdiff. Não se troca um número do disco nem se
  muda o jogo para igualar endereços das pilhas. A explicação no `roteiros.py` exige tela 48, trecho de exatamente
  4 bytes no campo, tamanho pedido, tipo e a cadeia de alocação até `EvoTop_Init`. `campos.py --donos` preserva os
  três chamadores no TSV: só `Flash_Create` não distingue a raiz do EvoTop das raízes das janelas da mesma tela.
  No gdb, há visões diferentes chamadas `Flash`: busque o tipo no bloco de `Flash_Create`, em `gfxm_c.c`, antes de
  seguir `root` e `clips`. Compare cada `owner` com o endereço da variável local, não só com uma faixa de endereços.
  Na 9c-2 o mesmo apareceu em três telas do Data Center (o trabalho local de `DcMenu_Run`, `DcPass_Run` e
  `DcList_Run`), mais o `FlashClip.overArg` (+0xC8), que o `TextBox_AttachLine` aponta para uma caixa de texto do
  trabalho: só a conferência oficial com a memória por tela achou (o fim do roteiro passava). Antes de escrever o
  roteiro de uma tela nova, veja se o `Run` dela guarda o trabalho na pilha: se sim, a tela entra na tabela
  `OWNER_NA_PILHA` com os tamanhos exatos dos blocos, provados pelos valores dos dumps.
- **Dados gerados copiados de outro clone podem guardar caminhos absolutos.** O `.incbin` de
  `port/build/gen/data/vu1_micro.data.s` ainda apontava para o clone de outro usuário. Corrija apenas essa saída
  local para o caminho resolvido de `assets/`, sem escrever nos links de dados só leitura nem mudar o código.
  Refaça a build e confira a `.text` antes de usar os resultados.
- **`save_mc.sh` também precisa de um emulador por vez.** Os pares A/F e B1/B2 ainda eram lançados em paralelo
  na versão anterior à 9c-1. Rodam em sequência agora, como os roteiros: o `wait` interno de cada caso só espera o
  seu qemu. A simulação de falha e a recuperação do cartão não precisam de concorrência entre casos.
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
- **`subir.py --baixar NOME` tira o arquivo do lugar no console** (vira `NOME.lido`). Na etapa 7 da F4, para testar a
  conexão do WSL, baixei o `ambiente.txt`: o jogo abriu sem ele, com os padrões da F2 (a luta sem desenho), e a tela
  ficou azul e depois verde. Para testar a conexão, use `subir.py --achar` ou baixe um arquivo que não importa.
- **O `NLST` do FTP da FreeStyle devolve as linhas no formato do `LIST`** (permissões, tamanho, nome): pegue o último
  campo, como o `subir.py` faz com o `LIST`.
- **Capturas do console nos mesmos quadros da referência:** `BT3_SHOT_LISTA=G,G...` (até 32 quadros do GS) no
  `ambiente.txt` grava só esses quadros, com ou sem o `BT3_SHOT`. Os números vêm da linha "imagem: ... gs G" da build
  sem janela, e o `compara_quadros.py` mede uma contra a outra.
- **O WSL não tem numpy:** o `compara_quadros.py` tem um caminho em Python puro, mais lento. Não conte com o numpy em
  ferramenta que roda lá.
- **Fundo mais "serrilhado" na referência do que no console não é erro do console.** O jogo pede filtro bilinear
  (`TEX1`), e o fundo da seleção de personagem é uma textura de 1024 x 512 reduzida para 512 x 448. O console filtra; o
  rasterizador de software amostra no ponto e pula linhas. Isso derrubou a seleção para 82,3% de pixels iguais. Antes
  de procurar um erro numa tela com nota baixa, compare a nitidez (gradiente médio) só do fundo e só do resto. Os
  números da etapa 7 da F4: o `TEX1` pedido é 0x60 (bilinear); nitidez do fundo 35,6 no PC e 30,9 no console, o resto
  13,4 nos dois. Um protótipo do rasterizador que filtra como o GS (branch `investiga-filtro-gs`, commits `be5d764` e
  `1df9c3b`, fora da main) leva o fundo de 71,2% para 98,4% e as 36 imagens de 92,0% para 99,2% de média (mínimo
  98,6%): a diferença era toda o filtro. O Lucas decidiu não ligar esse modo nas comparações; a regra é aceitar a tela
  com textura reduzida por volta de 82% e medir a nitidez separada antes de procurar erro. A arte dos personagens vem
  sem o meio texel e pela fórmula estrita do GS sairia um pouco macia (anotado, sem decisão).
- **Teto de memória sem descarte vira textura branca para sempre.** Na sessão da F4, depois da tela de versus do
  Torneio, os menus seguintes ficaram brancos até reiniciar: o backend recusava textura acima de 128 MiB e o cache
  do frontend (`gs_draw.c`) só varre por idade com 8.192 entradas, então nada saía. Num console com memória fixa, o
  teto tem de vir com o descarte de quem é dono do cache (o frontend), contando também o que espera a GPU, e cada
  textura branca tem de dizer no log por que saiu branca (F5, etapa 1, plano/F5/01.md).
- **Triagem de defeito de imagem: leia o que o 360 NÃO faz antes de reler o shader traduzido.** Na segunda parte da
  etapa 6 da F5 os seis defeitos do cenário (lavado, borrado, preto, lutador por cima, sombra por cima e preta, HUD
  esquisito) pareciam da tradução dos shaders; a leitura lado a lado do HLSL com o `.vert` e o `gs.frag` não achou
  diferença (mesma escala de cor, sem névoa, sem mipmap dos dois lados) e todos os candidatos estavam no que o 360
  deixa de fazer em relação ao PC: profundidade zerada a cada entrada na EDRAM (42 trocas de alvo por quadro), as
  passadas nativas contadas e puladas (`nativos .../360/...` no log) e o alfa saturado. A ordem que poupou tempo:
  contadores do log, medidas das capturas (`ppm_stats.py`) contra a referência, e só então o shader. Nitidez e
  saturação medidas na área 3D (fora da caixa da pausa) dão ordem de grandeza; o viés do rasterizador de software
  (textura lida no ponto) é de uns 15% no gradiente, então 2 a 8 vezes é defeito. Quando a causa é uma passada de
  tela, `BT3_FX_OFF=8|16|24` no `ambiente.txt` (valem no 360 sem build nova; o 24 provou a triagem na rodada da
  etapa 7) a separa em uma rodada.
- **Profundidade presa por vértice não é o depth clamp: o chão perto da câmera some.** Na F5 (etapas 6 e 7) os
  programas da VU1 prendiam o z de recorte em 0..w por vértice, para imitar o depth clamp do PC (o D3D9 do Xenos não
  desliga o corte por profundidade). Com a câmera baixa, um triângulo do chão com um vértice atrás da câmera (w < 0, z
  preso em w) e outro na frente com z < w é cortado onde o z interpolado zera, muito antes de w = 0: um buraco escuro
  de borda em degraus seguindo os ladrilhos, que custou uma rodada (a abertura 2 da etapa 7 o mostrou; a persistência
  da profundidade não tinha nada com ele). A correção que reproduz o PC: o z de recorte vira `w - ε` (o único corte em
  z passa a ser o plano w = ε, o recorte pelo w) e a profundidade de verdade, z / w com z e w interpolados em
  perspectiva, sai pelo `DEPTH` do pixel shader, presa em 0..1 (`vu_pos` e `ps_gsz` em `gs_d3d.hlsl`). Custa o z
  antecipado nesses desenhos (o Hi-Z já estava desligado). Regra: prender o z por vértice só vale para primitivas
  inteiras na frente da câmera (o 2D do `vs_gs`, com w = 1).

- **Varredura sobre o `gamedata` inteiro: em fluxo, com `ulimit -v` e decodificador de BPE com limites.** Na
  investigação dos ícones (`pesquisa/04`) o primeiro script desceu em todo pacote e BPE, testou candidatos que não
  eram BPE e cresceu pilha e saída sem limite até 7,6 GB; o sistema o matou e o WSL (uns 7 GB) quase caiu. O que
  funcionou: `ulimit -v 1500000`, um arquivo por vez, o decodificador recusando `size <= 0`, exigindo a cauda
  depois do `packSize` de no máximo 128 bytes, com pilha de 254 e saída limitada ao `size`, e testes `in` de bytes
  antes de qualquer regex (20 regex por folha deixaram a varredura lenta demais; com os filtros, cerca de 5 minutos). Para achar o que o
  jogo "reserva" (um ícone, um texto), procure primeiro o consumidor no código (`FontTag_Handle` era a marca
  `<PAD=nome>`, não um código de caractere) antes de varrer os dados. E decodifique imagem do GS com as tabelas de
  `port/src/gs/gs_core.c` (T4 com a inversão `x^4` nas colunas ímpares), não de memória: a decodificação errada
  custou rodadas de folhas de contato. A volta (recodificar e comparar byte a byte) vale como verificador de
  que se pode trocar a imagem no lugar; viu-se falhar com um pixel mudado.

- **Posição de ferramenta externa: confira a unidade (bytes ou caracteres) antes de fatiar o texto.** O `clang` dá
  no AST em JSON deslocamentos em bytes do arquivo; o `c89.py` aplicava as reescritas sobre o `str` do Python, em
  caracteres. Cada acento (dois bytes em UTF-8) numa string ou num comentário antes de uma reescrita a deslocava de
  um: viu-se como "aligned sem valor" e como o `cl` recusando o `gs_draw.c`, mas o caso perigoso é o silencioso (um
  `for (int i...` depois de `"ação"` virou `for ({  = 0; i <; int in; ...`, e outro deslocamento podia formar C
  válido e errado). A correção: o `rebaixar` trabalha na vista de um caractere por byte
  (`encode("utf-8").decode("latin-1")`, e o inverso no fim), e o arquivo do AST sai nos mesmos bytes. A foto dos 317
  fontes antes e depois saiu idêntica: nenhum fonte da build tinha reescrita depois de um acento (os quatro com
  acento no corpo, `gs_core`, `hl_passos`, `hl_roteiro` e `pc_plat_mc`, não tinham). O verificador
  (fonte pequeno com `"ação"` antes de um `for` com declaração) foi visto falhando com o `c89.py` antigo.

- **Roteiro parando no boot em 1 s com "600 quadros sem mcflow 15": a pasta do `BT3_SAVES` não tem pai.** Na F5 (etapa 8,
  o `vu_plano`) as listas do `vu1_perfil.sh` no Deck pararam todas no boot (`tela 1, mcflow 17`), x86 e PPC, antes de
  qualquer código novo: o script aponta o `BT3_SAVES` para `~/perfil/sv_l`, e no Deck o `~/perfil` não existia. O jogo
  não cria a cadeia de pastas do cartão, o fluxo do cartão vai para o caminho de erro e o roteiro espera o passo que não vem.
  Custou uma rodada (o script em série seguiu para a PPC com as listas vazias). O `vu1_perfil.sh` agora faz `mkdir -p` da
  pasta; numa máquina nova, confira as pastas fora do repositório que os scripts pressupõem (`~/perfil`, `~/referencias`)
  e olhe o fim do log da primeira rodada antes de deixar a série correr.

- **Uma tela que para só no console, com o rastro igual até ela: ponha no log o que a tela espera antes de supor tempo
  real** (F6, etapa 1, `plano/F6/01.md`). A tela de resultado da História parou no console sem desenho e a F4 a deu
  como "espera em tempo real"; a F6 a deu como "estado do som ausente" porque leu o `getenv` errado (seção 1.3). As
  duas hipóteses caíram pelo código: o jogo conta tudo em vblanks e quadros, e o rastro do console era igual ao da x86
  até o vblank da entrada da tela. Quando o rastro é igual até uma tela, a diferença está dentro dela, e o que a tela
  espera se lê no código (aqui, quatro portas: o jingle no SE0 chegar a 5, o "pad true" da animação, o fim do
  `fl_out`, o `MPROG_FREEZE`). O `BT3_SOM_ESTADO=N` põe essas portas e o estado dos seis players numa linha por
  mudança, nas três builds, fora do rastro; o `som_estado.py parada` diz qual ficou fechada. Duas regras que saíram
  daí: um `getenv` do som que vira NULL no console muda o jogo em silêncio, então todo `getenv` e todo ramo de janela
  dos arquivos do som está no inventário do `som_ambiente.py` (rede 1, `--mutantes` com cinco estragos); e o início de
  um fluxo que não abre (`ADXT_StartAfs` com o `Port_FilePath` falhando) não deixa linha e deixa o player com o
  estado anterior, então um player "parado" numa tela que espera o fim pode ser um arquivo que não abriu.

### O som no WSL (F6, etapa 2, `plano/F6/02.md`)

- **Deriva de áudio não se mede por blocos enviados** (F6, etapa 8, `plano/F6/08.md`). Eles incluem até 42,7 ms
  ainda na fila, e `SamplesPlayed` também conta silêncio. Registrar a posição consumida com `GetState(..., 0)`,
  junto com vblanks e um relógio do BGM que conte saídas úteis sem voltar no laço. Exigir a mesma geração e a mesma
  luta por dez minutos; falta de dado, troca de música ou juntar lutas curtas não valem. `som_deriva.py` conserva
  a incerteza de um vblank no limite de 100 ms e recusa log sem os dados, em vez de inferir posição tocada.
- **Conferir o consumidor da Guide no código**, mesmo se o plano disser que ela pausa a luta. Em `3e1f34bd` não
  havia consumidor de `XN_SYS_UI` no port. A etapa 8 instalou o do som no vigia, consultado inclusive durante o
  tom longo; os handles BGM e MAP identificam música, e `user NULL` do sampler não pode ser confundido com um
  handle de música ainda não criado. O núcleo preserva fila e fase, mas só o console prova a pausa da luta.

- **As referências do `.dat` e do `.swap` do 360 mudaram em 22/10/2026** (main `a14c59a`, a etapa 3 da F6): de
  `6d30baaf87b1177fb2359a855f8befc0` e `a7cf723fbfaea860fcc3707ad288e755` para `fb489b2f6e00d0e4de7e394f7037540c` e
  `abbd7589c8425797c4a2639068c9bf54`. A `xaudio2.lib` entra em toda build do 360 (mesmo com o som desligado) e põe 0x90020
  bytes antes das tabelas do jogo; o `tools/dat_compara.py` confere que as 2.388 posições do `.dat` e as 7.062 do `.swap`
  andaram exatamente esse deslocamento. Uma build que der o md5 antigo não tem o som; uma que der outro precisa do
  `dat_compara.py` contra estes.
- **A referência da `.text` da x86 plana mudou em 22/10/2026** (main `a768ad2`, o merge das etapas 1 e 2 da F6): de
  `cc46fa0752f092fca305bc8bf2e2e54390763af0` para `ee54716ba7d86744152b9b71e6ac8e500f985d97`. A mudança é só de código de
  teste do `headless/` (o rastro de som `som.c` por `--wrap` e o `BT3_SOM_ESTADO` do `roteiro.c`); a etapa 2 comparou os
  objetos um a um (só o `x360_som.o` novo e o `x360_passos.o` do carimbo). Daqui em diante, a prova de que um corte não
  muda a x86 plana compara com a referência nova.
- **E mudou de novo em 22/10/2026, pela volta do laço do ADX** (decisão 25.10 revista, `plano/F6/02.md`, seção 7): de
  `ee54716ba7d86744152b9b71e6ac8e500f985d97` para `43444ea4d03a6d448aa1ae74a631e35b356a7caa`. Esta muda o jogo de
  propósito (o `snd_adx.c`); dos 326 objetos sem a depuração só diferem o `gs_snd_adx.o` e o `x360_passos.o` (carimbo).
  Uma build da x86 plana que der a `ee54716...` não tem a volta que restaura.

- **Endereço que vem de `malloc` do host não se compara entre builds.** O rastro de som gravava os endereços das tabelas
  do banco na memória do IOP; o `IopHeap_Alloc` do port é um `malloc`, e o rastro da x86 com o gravador saiu diferente do
  da x86 sem ele só por isso (custou uma rodada). Num rastro que se compara, endereço do PS2 (memória do jogo, do chip)
  fica como está; o do host vira um número pela ordem em que aparece.
- **Os stubs `static inline` não se desviam por `--wrap`.** O áudio da SDL de teste (`headless/SDL3/SDL.h`) é `static
  inline`: o compilador põe a resposta dentro do `snd_adx.o`. Para gravar o som sem mudar os objetos da build plana, o
  gravador é outra build (`--gravador`, `-DBT3_GRAVADOR` troca os stubs por declarações), e o rastro, que só olha as
  chamadas do jogo, fica na plana.
- **O `getenv` dos arquivos do `port/src/gs/` é o `Port_GetEnv`, guardado por ponto de chamada** (`gs_internal.h`): um
  teste que inclui esses arquivos precisa definir o `Port_GetEnv` e pôr a variável antes da primeira chamada, que a
  resposta não muda depois.
- **Dois agentes no mesmo clone do WSL: um worktree para cada.** `git worktree add --detach` com links para `asm`,
  `assets`, `disc`, `gamedata` e uma cópia de `port/build/gen/{data,gs}`: as builds de um não apagam as do outro (o
  `roteiros.sh` exige a build do HEAD), e o branch do clone principal não muda. A trava (`flock ~/wsl.lock`) continua
  para o emulador.
- **Num buffer de envio compartilhado, o rastro grava só o que o chamador escreveu.** O jogo manda todos os RPCs do som
  pelo mesmo buffer; o RPC 0xF escreve uma meia palavra de contagem e manda 0x18 bytes, e os RPCs sem argumento mandam 4.
  O resto é sobra do RPC anterior: ponteiros do host (outros a cada rodada) e palavras gravadas com 32 bits e lidas com 16
  (outras na PPC). Gravar o buffer inteiro deu 6 falhas de rede 2 numa rodada longa de 45 minutos que não eram nada. Pelo
  formato de cada mensagem (quem escreve o quê), nunca pelo tamanho enviado.
- **Numerar endereços do host pela ordem em que aparecem pede esquecer o endereço no `free`.** O `malloc` devolve de
  novo o endereço de um bloco liberado, e um bloco novo herdava o número do velho só numa das builds (a com mais
  alocações): `--wrap` da função que libera e a tabela marca o número como morto.
- **Toda rodada que passa pelo `boot.txt` precisa de uma pasta de saves só dela, vazia.** O New Game só existe com o
  cartão vazio; uma rodada nova que reaproveitou a pasta da rodada (a) carregou o save e ficou 4000 quadros sem o
  `mcflow 10`, em todos os roteiros, e tudo o que se comparava com ela (o mixer) falhou por tabela.
- **O limite de uma exceção se mede em todos os casos dela, não no primeiro.** A diferença da volta do laço do ADX foi
  medida numa música (10 LSB) e virou decisão; com os 72 ADX com laço na amostra deu 3951 LSB. E um verificador que
  pede uma janela fixa precisa do caso menor que a janela (laços de 36 amostras): o `som.py` parou com uma exceção do
  Python, que a rodada contou como "amostras diferentes".
- **Uma exceção no verificador esconde a variante que ela aceita.** Com a exceção do laço, o `som.py` passava o port que
  continuava o histórico, e passaria um que o fizesse por engano. Corrigido o port, a exceção sai no mesmo commit e entra um
  controle com a variante antiga (`--estraga continua`), para o verificador não voltar a aceitá-la calado. E o controle de
  um caso raro (laço) escolhe arquivos que têm o caso: os quatro primeiros da amostra podiam não ter laço nenhum.
- **O teste das amostras compila aqui, sem o WSL.** Os cinco fontes do `headless.py --teste-som` saem com o `gcc` do Linux
  (`-std=gnu99 -DBT3_GRAVADOR -Iport/x360/headless`, sem o `gen/gs`), e o `som.py amostras` roda sobre uma pasta
  `pzs3us2` de ADX sintéticos (`--n 0`): dá para provar o decodificador e o verificador, inclusive falhando com a versão
  da `main`, antes da rodada com os dados do jogo.

### A camada de áudio do 360 (F6, etapa 3, `plano/F6/03.md`)

- **A `xaudio2.lib` traz `sqrtf`, `cosf`, `floorf` e mais quatro** (as versões C++ da `crt/math.h`, `inline`: COMDAT
  "qualquer uma"), e o `math360.c` as define como função comum: 23 LNK2005 no primeiro link com o XAudio2 (custou uma
  build). Com `__inline` nas definições do `math360.c` e uma tabela global com o endereço de cada uma (sem ela o `cl` não
  emite as que ninguém chama no objeto), as dele também viram COMDAT "qualquer uma" e o link fica com a primeira que vê,
  a do objeto, antes das bibliotecas. Conferir no `.map`: cada uma com `math360.obj` (o corpo é o mesmo da `crt/math.h`,
  mas o jogo não pode depender de qual cópia o link escolheu). Mesma causa do comentário antigo sobre `fabsf`, `ceilf` e
  `powf`, que vêm da `d3d9.lib`.
- **Uma biblioteca nova desloca as tabelas: o md5 do `.dat` e do `.swap` muda sem nada ter mudado.** A `xaudio2.lib`
  pôs 0x90020 bytes antes dos dados do jogo, e toda palavra de endereço dos dois arquivos andou o mesmo tanto. O md5 de
  referência só vale entre builds com o mesmo lugar; quando o executável cresce, `dat_compara.py` confere o deslocamento
  único (e acusa um byte estragado numa cópia).
- **O retorno do XAudio2 não mistura.** A documentação do XDK ("XAudio2 Callbacks") pede que o retorno volte em menos
  de 1 ms, sem trava do programa, sem E/S e sem CPU pesada: o `OnBufferEnd` só conta (`BuffersQueued`) e dá `SetEvent`, e
  um misturador em thread própria (na 5, prioridade alta) enche o buffer e o devolve. A primeira versão misturava no
  retorno com a trava do jogo; a leitura da documentação antes do console a derrubou.
- **No C89 do `cl` o `long` tem 32 bits; no gcc do WSL, 64.** Um núcleo testado no WSL e usado no console faz as contas
  em `int` (32 nos dois), senão o teste prova outra aritmética. O `gcc -m32` não existe no WSL nem aqui (sem multilib).
- **O teste leve não precisa do clone do WSL.** O `teste_som360.sh` roda numa cópia temporária (`RAIZ=`), mandada por
  `ssh` num tar em base64: nada de `flock`, nada de build, nada de emulador, e o outro agente segue com o clone.
- **Mudar um header que a build plana do WSL também lê sem mudar as linhas dele.** O `headless/SDL3/SDL.h` ganhou o
  `BT3_SOM360` (só o `c89.py` do console o define) no mesmo número de linhas: o texto pré-processado do WSL fica igual e
  os objetos da x86 plana não mudam (nenhuma `.text` a refazer).

### A medição do decodificador dos filmes (F6, etapa 6, `plano/F6/06.md`)

- **Medir por diferença dos contadores que já existem, sem mexer no código do upstream.** O decodificador lê o `.PSS`
  dentro do `Mpeg2_Next`; os contadores de leitura da ISO do `iso360.c` (`gPicosIso*`, da F5) dão a parte da leitura
  por diferença, e a decodificação é o resto. Vale enquanto uma thread só lê a ISO (sem `BT3_SOM`).
- **Um modo de medida no 360 entra na `Jogo()`, antes do `__real_main`, e sai por `Port_Sair(0)`:** sem estado do jogo,
  na thread de hardware certa e com fim claro (tela verde). As linhas do vigia ("parada há ...") são esperadas: ninguém
  conta vblank.
- **O carimbo do `.xex` conta o que não está no commit, inclusive arquivo novo de documentação.** O `build_f2.sh` faz o
  sha1 do `git diff HEAD` mais os arquivos não rastreados: para o `.xex` da rodada sair com `diff:da39a3ee5e`, commitar o
  código, guardar o resto (`git stash`) durante a build e só depois escrever as notas (custou uma build).
- **Código novo só nos fontes do 360 não desloca as tabelas:** o `mpeg_mede.c` (e os 1,75 MiB dele vindos do `malloc`,
  não estáticos) deixou o md5 do `.dat` e do `.swap` igual, ao contrário de uma biblioteca nova.

### O filme direto à textura (F6, etapa 6, `plano/F6/06.md`, seção 5)

- **Um sinal "por quadro" do PC que nenhum backend do 360 limpa vale uma vez só.** O `gGsFbUploads` é zerado pelo `gs_gl.c`
  e pelo `gs_gpu.c` no fim do quadro; o `gs_d3d.cpp` nunca o zerava, então a marca do desenho do filme pelo bit do PC só
  sairia no primeiro quadro. Visto na leitura, antes do console: a marca vai pelo número da imagem, que só cresce.
- **O mesmo arquivo chega ao `Iso_Abrir` por dois caminhos.** O `.PSS` é aberto pelo `ADXF_Open` do jogo (pelo
  `plat_fcache.c`) e pelo `fopen` do decodificador, com o mesmo caminho. O que se liga pelo nome na abertura pega os dois
  (duas threads de leitura, o contador do filme trocado): o recurso caro se abre na primeira leitura, que o handle do jogo
  não faz com a leitura dupla cortada.
- **Conta inteira em 16.16 num shader, exata:** cada constante como 65536 vezes um inteiro mais o resto deixa as partes que
  vão ao `floor` abaixo de 2^24, inteiras num float, e a divisão por 65536 é exata. Prova sem console: refazer a conta do
  shader em float de 32 bits no teste, pixel a pixel contra a inteira, com um mutante de uma constante que tem de falhar.
- **`IsBusy` e `IsFencePending` só na dona do dispositivo.** A outra thread (a do jogo, que escreve a vaga) espera uma marca
  que a dona limpa no fim do quadro pela cerca, com teto (100 ms) e contagem: nunca trava, e uma imagem pulada (o desenho
  nunca chega) se solta quando uma mais nova é desenhada.
- **O controle do `gcc -E` tem de trocar um token fora do `#ifdef` novo.** O primeiro controle trocou o primeiro
  `STAT_READEND;` do `plat_file.c`, que agora é o da cópia nova sob `PORT_X360`, e a plana saiu "igual"; o `0x14` do
  `sceCdGetDiskTypeSafe` é de todas as builds.
- **Medir a leitura do disco por imagem com outra thread lendo:** os contadores do `iso360.c` somam todas as threads; com
  os blocos, a parte da leitura na thread do jogo é a espera dela no `Blocos_Ler` (os contadores do `blocos.c`), e só sem
  os blocos vale a diferença do `gPicosIsoTiques`.
- **Teste leve no WSL fora do `flock`.** O `teste_blocos.sh` pôs o `flock ~/wsl.lock` na frente e ficou na fila atrás de um
  `roteiros.sh` de outro agente, para uns minutos de CPU de um núcleo sem emulador. Como o `teste_som360.sh`: cópia
  temporária por tar em base64, sem trava; a trava é do emulador.

### O filme com som (F6, etapa 7, `plano/F6/07.md`)

- **O menu da História tem dois eixos de navegação.** `HistSel` usa CIMA/BAIXO no menu de duas opções, mas
  ESQUERDA/DIREITA na lista circular de sagas. O primeiro roteiro usou CIMA e terminou sem abrir o filme.
  Exigir o `start p0 fname zs3used.adx` no rastro, além do retorno ao menu, recusou essa rodada. O bit 1 do
  `gameRepeat` é `PADG_LEFT`, não o bit bruto de CIMA do controle; conferir `include/sys/pad.h`.
- **A trava não congela a árvore de fontes.** Uma build que ficou enfileirada pode começar enquanto a revisão
  ainda edita ou emenda o commit. Na primeira rodada desta etapa, x86/x86r ficaram num carimbo e PPC/gravador no
  seguinte; a conferência recusou a origem antes do roteiro. Fechar a revisão antes de enfileirar a rodada final,
  conservar a árvore e o HEAD até o fim e refazer todas as builds se a origem mudar.
- **A frequência do laço do jogo não é a frequência de origem do filme.** Os dois MPEG têm `frame_rate_code=5`
  (30 fps) e seus ADX duram exatamente N/30 segundos. O laço pede dois vblanks, nominalmente 29,97 fps. Para a
  deriva, usar o tempo de origem `imagem * 1000/30`; para o ritmo, os vblanks reais e o aceite do jogo. Usar
  29,97 nos dois lados esconderia até 140 ms no final. Não zerar o atraso na primeira amostra.
- **`SamplesPlayed` da voz única inclui o silêncio e todos os sons; a posição decodificada do ADX inclui a fila.**
  Guardar por buffer a posição do BGM que entrou na mistura, depois buscar o buffer que estava tocando no vblank
  mostrado. A identidade é o `user` do retorno, o handle adxt. Troca de fluxo ganha uma geração. Falta da voz,
  silêncio de recuperação ou histórico vencido invalidam a extrapolação para o vblank.
- **A imagem 0 do filme não é o quadro 0 do GS e pode estar sob o fade.** `BT3_FILME_SHOTS=1` escolhe o índice local,
  captura a conversão da GPU antes do fade e nomeia pelo filme e índice. A rodada de captura fixa escala 1 para a
  redução ser a identidade. Medir ritmo e deriva em outra abertura, sem capturas.
- **Esperar a thread do GS não consome um DMA da GIF ainda armado.** Na rodada de 27/10 a imagem 0 da primeira
  abertura não chegou ao desenho e a captura global 1 esperou 2 s. `MovieTag_Send` só arma CHCR; o port executa no
  próximo `Port_DmaChcr`. Sem consumi-lo no `sceGsSyncPath`, o próximo envio já tinha publicado outra imagem e
  reescrito TADR. No 360, consumir DMA 2 antes de esperar a gravação. O teste `teste_filme_envio.py` liga o DMA
  real ao corpo extraído do SyncPath e exige falha por asserção sem a correção e com a ordem invertida.
- **Uma captura faltante não deve esconder as outras nem virar aprovação parcial.** `filme.py compara` continua
  pelas quatro imagens, mostra as três presentes idênticas e sai com 1 se a 0 faltar. Dois conjuntos da abertura
  deram essa recusa; os quatro do final deram igualdade exata. Já `filme.py log` recusou boot e final por BGM sem
  geração na primeira observação, apesar da escuta normal. Não descartar o começo nem preencher com zero.
- **Pasta por abertura aceita nome, não um segundo volume.** `BT3_SHOT_PASTA` aceita também `game:\nome`, tirando
  o prefixo uma vez. Nome simples até 30 caracteres, inválido com erro e sem captura; não cair silenciosamente em
  `shots`. O mesmo caminho serve ao filme. Saves e capturas têm nomes únicos na pasta `ambientes/f6-etapa7b`.
- **Memória física livre não conta os buffers retirados de um heap já reservado.** O teto do filme soma também
  `Movie_Open`, o `sFrame` estático e o ES do decodificador; a diferença de `dwAvailPhys` sozinha subestima o custo.

### Efeitos e fluxos no 360 (F6, etapas 4 e 5, `plano/F6/04.md` e `05.md`)

- **O estado que o jogo vê pode esperar uma leitura sem deixar de ser determinístico.** A duração de um fluxo vinha do
  cabeçalho lido na thread do jogo no início. Para tirar a leitura dali sem mudar o que o jogo vê, a vista começa com um
  valor grande conhecido (`VISTA_PENDENTE`) que o tique de sempre desconta; quando o cabeçalho chega, a duração menos o
  que já foi descontado é o mesmo estado da leitura no começo. Só o caso "chegou depois do fim" difere, e ele se conta.
  Serve para qualquer dado assíncrono que o estado do jogo consulta.
- **Leitura assíncrona que se autocorrige esconde o adiantado errado.** Uma falta pede o bloco certo e o dado chega certo:
  um teste só de bytes passa com o adiantado quebrado (o do começo do laço, a sobra do bloco). O teste do `fluxo.c` conta
  as faltas numa leitura em sequência com folga e exige zero; os mutantes desses dois só foram acusados assim.
- **Uma espera fixa num teste de tempo é falha intermitente.** A folga de 1,5 ms antes de cada bloco deu uma falta em
  uma de três rodadas (o escalonador); com 5 ms, quinze seguidas sem falta. Asserção de "chegou antes" pede folga larga,
  e o teste roda três vezes no `teste_som360.sh`.
- **Uma tabela de fluxos precisa de lugar para os que estão fechando.** O fechar é da thread de leitura (o arquivo se
  fecha lá); com 20 abertos e a tabela de 24, os fechamentos pendentes a enchiam no teste. 32 lugares para 16 players.
- **Comparar avisos do `cl` entre duas builds: tirar as linhas do `wine`.** A primeira build num prefixo novo escreve
  `wine: created the configuration directory '/root/.wine'` nos `.txt` de alguns fontes (os que compilaram primeiro);
  sete "avisos diferentes" eram isso. `grep -v '^wine: '` dos dois lados.
- **Provar que a build plana do WSL não muda pelo texto pré-processado (`gcc -E -P`), sem as marcas de linha.** Código
  novo só dentro de `#ifdef` muda os números das linhas, não o texto; com `-P` o md5 do antes e do depois é o mesmo, para
  cada combinação de definições do WSL (com e sem `BT3_GRAVADOR` e `PORT_BIG_ENDIAN`). A comparação dos objetos sem a
  depuração na rodada do WSL confirma. Um `#else` que repete o original evita o `{ }` a mais que mudaria o texto.
### O 16:9 no WSL (F8, etapa 1, `plano/F8/16x9.md`, seção 9)

- **Medir o custo de uma chave de imagem na x86gv antes do console.** O mesmo roteiro com a chave desligada e ligada, sem
  limite de quadros (`BT3_UNCAPPED=1`), leva uns 30 s por rodada; cada linha do rastro do roteiro traz o vblank e o quadro, e
  o rastro igual linha a linha prova que os quadros dos dois logs se correspondem um a um (o hash do estado não serve entre
  4:3 e 16:9). `custo169.sh` é o modelo.
- **A coluna `texturas` da linha `gpu_falso: quadro` é o acumulado**, não as do quadro: por quadro é a diferença com a linha
  anterior (a medida estaria errada por milhares sem isso).
- **Um contador de teste no `gs_draw.c` atrás do `BT3_GPU_FALSO` não toca na x86 plana** (ela nem compila o `gs_draw.c`) nem
  no 360: a `.text` confere igual sem rodada a mais.
- **Uma chave de ambiente só de um lado, sem mexer no `roteiros.sh`:** o `qemu-ppc` passa o ambiente inteiro ao programa e
  lê `QEMU_UNSET_ENV` (e `QEMU_SET_ENV`) antes; `BT3_WIDE=1 QEMU_UNSET_ENV=BT3_WIDE` deixa a chave só na x86. É o controle
  negativo de uma comparação entre a x86 e a PPC numa forma.
- **Worktree no Deck:** o clone do Deck não tem `port/build/gen/gs`; o worktree leva links para `asm`, `assets`, `disc` e
  `gamedata` e cópias de `port/build/gen/data` e `port/build/gen/src`.
- **Tirar a leitura da thread do jogo não basta se a trava da leitura continua no caminho dela** (F6, `05.md`, "A thread
  do jogo sem esperar"). Com o ADX em blocos, o início de um fluxo na thread do jogo só abria e fechava a entrada da ISO,
  mas o `afs()`, o `novo()` e o `Iso_Fechar` pegavam a trava que a thread de leitura segurava durante o `ReadFile` de um
  bloco (até 98 ms no console): inícios de até 59 ms. Procure toda trava que a thread do jogo pega e pergunte quem a
  segura durante E/S. A correção deu um handle próprio à thread de leitura (cada handle com a sua trava), a tabela do AFS
  lida sem trava e a tabela dos abertos com uma trava só dela. Um handle aberto sem `FILE_FLAG_OVERLAPPED` serializa as
  leituras no próprio objeto de arquivo no Windows [lido, não medido no 360]: tirar só a nossa trava, com um handle só,
  não garantiria nada.
- **O código do console com trava se testa no Linux com um `xtl.h` de teste.** O `iso360.c` compila de verdade sobre
  `port/x360/src/som/teste/xtl/xtl.h` (as seções críticas como mutex recursivo, o `ReadFile` por `pread`), com a leitora
  lenta dentro do `ReadFile` (50 ms dormidos com a trava de quem lê pega) e uma ISO de mentira feita pelo teste. Os
  mutantes de trava põem de volta um `Enter`/`Leave` da trava comum no trecho, em vez de trocar a trava: trocar só o
  `Enter` deixa um `Leave` de trava não pega e o teste trava em vez de falhar.
- **Antes de pôr um pico na conta de uma correção, ver no log o que acontecia naquele quadro** (F6, `05.md`, "A rodada do
  console" das correções). O quadro 234 do título (98 ms de jogo, `disco 0 leituras`) entrou na lista do que A e B
  resolveriam; na rodada continuou igual (116 ms). O log anterior já dizia que não: nenhum fluxo abre antes do vblank 1750
  e o quadro cai no `mcflow -1`, a procura do save. Ler as linhas de evento logo antes do pico (`mcflow`, `tela`, `iso:`,
  `roteiro:`) separa as hipóteses sem gastar o item da rodada.
- **Dois handles no mesmo disco tiram a espera da trava, não a do disco.** Com a leitora dos fluxos no handle dela, a
  thread do jogo parou de esperar (pior início de fluxo de 59 ms para 0,4 ms), mas as cargas do jogo nos menus somaram 862
  contra 565 ms e os blocos acima de 50 ms passaram de 6 para 14 [medido numa abertura de cada; a causa estimada: os dois
  `ReadFile` disputando o disco]. O ruído do disco do 360 é grande (a mesma primeira carga, sem concorrência, levou 72 e
  25 ms): uma abertura de cada não separa. E um contador de tempo de leitura somado entre threads passa a contar tempo
  sobreposto: o `disco` dos picos pode passar do quadro.

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

- **Shenlong (9c-4): dinheiro continua livre com tudo desbloqueado.** `Shen_IsOwned` e
  `ShenCfm_IsOwned` devolvem zero para `SHEN_WISH_MONEY`; todos os três dragões têm esse desejo na primeira
  posição. Não é necessário alterar o sorteio nem os desbloqueios. O passo de teste `libera desejo` só zera
  `SaveData.money` no menu principal, com `so_teste` (permissão de desbloqueio e cartão vazio no início),
  deixando espaço para os prêmios. O dragão 1 do roteiro concede três vezes 5.000 e chega a `ShenSave_Init`.
  Como no DcSave, o McFlow abre as duas perguntas em Não: espere 12 e 13 e aperte ESQUERDA antes do X em cada
  uma. Só X recusa salvar, vai ao estado 18 e nunca chega ao 13. As esferas são os bits de um s32 em +8 do save:
  confira `s32 8 0`, não um byte desse número, que muda de posição na PPC.

- **Simbolização local dos roteiros (9c-4).** O host exportava `DEBUGINFOD_URLS=https://debuginfod.ubuntu.com`,
  e o `llvm-symbolizer` ficou parado na rede ao gerar o mapa da Referência, embora a build tivesse DWARF.
  O pai e o filho estavam em espera, sem gastar CPU, e o simbolizador tinha um socket aberto. O `roteiros.sh`
  agora exporta `DEBUGINFOD_URLS=` para os filhos: usa apenas o executável e o DWARF locais. Uma rodada
  interrompida por essa espera não conta como aprovação e precisa ser refeita.

- **Referência (9c-4): a memória exige duas provas além dos conversores.** O esqueleto reserva só quatro
  ponteiros para `gCharRefState`, mas `CharRef_Init` grava nove: os cinco seguintes ocupam nomes de globais
  do Ultimate Battle. Confirme os nove endereços de handlers e a posição dos símbolos nas duas builds antes
  de usar a visão sobreposta, sem liberar toda a faixa de ponteiros. `ChrView_Init` também aloca dois buffers
  de 0xCE000 e só zera a primeira palavra: sem `ChrView_Show`, o restante não foi inicializado. Só fica fora
  da comparação com trabalho vivo e dono provado, tela 60, estado inativo, slot size 0/id -1, buffer vivo
  apontado pelo slot e primeira palavra zero nas duas. `menus-ref.md`, seção 6, registra valores e limites.
  Não trate esses restos como conteúdo convertido nem aumente uma tolerância genérica pelo tamanho.
- **Consulta de processos ao iniciar roteiros.** Grave o script de conferência e o execute em outra chamada.
  Se o mesmo `bash -c` contém o texto `roteiros.sh` ao criar um script e depois o executa, `pgrep -af` encontra
  o próprio shell pai e a proteção recusa começar. Isso não indica outro emulador; mantenha a proteção e separe
  a gravação do script da chamada que o executa.
- **ShenWish copiado inteiro (9c-4).** A x86r instrumenta `*destino = *origem` de uma struct de oito bytes
  com `__asan_loadN_noabort(N=8)`, mesmo que gere dois `mov` de 32 bits. `ShenList_Fill` copia id e kind
  assim; a PPC tem dois `lwz` e dois `stw`, sem trocar as metades. Não mude o conversor para u64 por causa
  do mapa. Prove a cópia nas duas arquiteturas e limite a explicação às posições e ao leitor alcançados,
  com testes que recusem outro leitor e uma lista de PCs cheia. Prova e limites em `menus-ref.md`, seção 7.
- **Shenlong não avança da carga sem os modelos dos atores (9c-4).** Os ids 0xD3C/0xD3D/0xD45 e
  0xD46/0xD47 são slots file[0] de `BtlRes_Request` usados por `BtlObj_Create`, no mesmo formato de 252
  entradas dos personagens. Eram exceções antigas do rastreio, sem conversor, e a PPC ficava no vblank 3.000.
  O conversor `personagem` já cobre os cinco inteiros. Associá-los à tabela de faixas resolve a carga sem
  trocar o sorteio, simular a cena ou mudar o jogo. Prova dos leitores e cobertura em `menus-ref.md`, seção 8.
- **Shenlong: a lista também aponta para a pilha.** Além de owner e overArg do filme, o `prev` do primeiro
  nó e o `next` do último apontam para a cabeça em `Shen_Run.work.list`. O comparador prova os seis nós,
  a raiz e os 19 owners, calcula a cabeça como owner + 0x370 e as caixas como owner + 0xE4 + k*0x8C.
  Só esses valores nativos correspondentes recebem normalização. A cena também produz 48 palavras de
  cor `3C 3C 3C 80` por buffer da GPU, o erro conhecido da F5, antes de qualquer luta. A exceção é só
  na tela 70 e com teto 48, sem liberar o teto da luta nas outras telas. `menus-ref.md`, seção 9. A F5, etapa 4,
  corrigiu essas cores e retirou a exceção (`plano/F5/04.md`).

- **Imagens compostas do Shenlong (9c-4).** A lista e a recompensa são desenhadas sobre a cena 3D do
  dragão. Nas duas capturas dos mesmos quadros as builds diferiram em 40.950 e 27.650 pixels; a cena usa
  o motor da luta, com o erro de cores da F5 visto também na memória dos pacotes. Pela exceção da tarefa,
  essas duas imagens ficam para a F5. O roteiro confere saldo 0 e 5.000 no lugar das marcas, conservando
  os quadros, a cena executada e sua memória. O modo 71 continua nas imagens. Não use uma tolerância de
  pixels nem apresente a imagem composta como prova da UI sobreposta: ela também fica sem essa prova.
  **F5, etapa 4:** com as cores dos efeitos corrigidas, as duas saíram idênticas e as marcas voltaram ao roteiro
  (`plano/F5/04.md`): a diferença era a cor invertida dos efeitos, não outra causa da VU1.


### Memória da luta na F5, etapa 5

- **Um tamanho de alocação não identifica seus elementos.** `BtlPool` reserva uma arena, mas também entrega
  blocos pelo heap no slot 8. O testemunho de cada build registra slot, geração (reinício), ordem, endereço,
  tamanho e PC do dono. Compare os testemunhos e confira cursores/capacidades no dump antes de aplicar tipos.
  Resolva a PC por `addr2line`, incluindo a função interna: o clang incorpora `EftHitArena_Init` em
  `EftHit_Init`, enquanto o GCC mantém a chamada. O nome pelo símbolo externo sozinho acusa um dono falso.
- **A vista do header pode ser parcial, mesmo com sizeof conhecido.** Os trabalhos de 0x2000 dos slots dos
  lutadores usam a classe ativa; `EftEmit_LoadSet` completa as vistas parciais pelo `EftSet`. Os recursos do
  disco juntam `EftODiscRes` e a fila do `EftDiscMgr`. `EftRibbon_SetTexPair` grava dois u64 por entrada,
  embora outra vista os declare como `Vec4`. O tipo do leitor completa a vista, sem afrouxar a comparação.
- **Lista livre não é conteúdo vivo.** Corpos rígidos liberados, tarefas, partículas e formas de colisão
  conservam dados antigos. Confira nas duas memórias os links, índices, contagens e dono antes de excluir
  seus conteúdos. Um `w` sem inicialização só sai quando o consumidor usa xyz e remonta w: a aura faz
  `Vec3_Copy` no offset e força `pos.w=1`; as correntes usam só `BObj.push.xyz`. Argumentos copiados
  carregam enchimento de pilha: os pad3C de partícula/billboard e o fim do argumento de `EftAnimPart`
  não têm leitor. As coordenadas, cores e números ao lado continuam comparados.
- **Não confunda ordem de bytes com a libm já conhecida.** A concessão da F4 de até 64 ULP para as direções
  de `EftGlow_BuildTables` fica nos campos y/z produzidos por cosf/sinf, com código de tipo próprio e
  contagem `libm-host`. Sai o teto genérico da arena; nenhum outro float ganha essa concessão.
- **A fonte do menu mede também o cabeçalho de sprites.** Itens negativos chamam `Font_GetWidth` sobre
  o count u32 da folha. O ramo big-endian de `BtlMenu_GetWidth` reconstitui as duas metades como u16
  temporários; converter o cabeçalho como texto estragaria a relocação. As posições do `--mapa` têm
  leitores conferidos. Uma atribuição de `EftOColor` é cópia de quatro bytes: a nova coluna pcc do
  rastreio guarda a PC da cópia, inclusive quando nenhum byte teve uso numérico posterior.
- **Arquivo convertido pode receber números depois da carga.** A seção 8 do `00004.bin`, folha dos ícones,
  é usada pela HUD como `HudCRes`: `HudRes_UploadTex` grava mark como s32. A marcação desse trecho usa
  a variante HUD de `Conv_TexFile`; o antigo mapa de bytes falha quando mark fica 1.
- **Dump de memória não precisa repetir o fim da luta.** `BT3_MEMORIA_SAIR=1` no teste termina logo após
  gravar o dump e o testemunho no vblank pedido. Só as rodadas próprias de memória das telas/pontos usam
  essa saída; a rodada que compara rastro, arquivos e luta completa continua até o fim do roteiro.

- **O detalhe de continuação do mapa tem uma terceira forma.** Uma metade dentro de um u32 marcado
  aparece como marca 128 e "no meio de outro número", somada a "não trocadas". O `roteiros.py conv`
  precisa guardar esse detalhe antes de consultar `LARGURA_CONHECIDA`; só reconhecer "outra largura"
  e "tratou como byte" rejeitava as duas posições do HUD mesmo com o leitor correto registrado.
  A forma nova continua exigindo posição, largura e leitores da entrada específica, sem aceitar o resto.

- **Marca de carga não garante arena criada.** Em `f5_treino`, o ponto 4166 ainda tem zero subalocações
  de `BtlPool`, embora os pools GSC já existam. Combate 4560 e técnica 4722 têm 350. Preserve os pontos
  acordados e registre a vida útil observada, sem declarar uma arena viva só pelo nome da marca.
- **Existir o arquivo não significa terminar o veredito.** O redirecionamento cria `memoria.txt` antes
  de o julgador escrever. Um acompanhamento em paralelo deve aguardar a linha final `memória igual`
  ou `memória diferente`; ler o arquivo recém-criado causou uma recusa de relatório vazio, com o
  resultado oficial correto logo depois. A prova faltante foi rodada sobre o relatório fechado.
- **Hash precoce exige conferir o rastro anterior.** Repetir `historia_novo` sobre um cartão já criado
  muda o boot; um `BT3_MEMORIA_SAIR` anterior ao prazo do roteiro pode sair com 0 no fluxo errado.
  Apague apenas os cartões gerados da própria sonda e seus produtos antigos, confirme o início GSC
  e o evento esperado antes de aceitar o dump e exija os estados vivos que a prova pretende alcançar.
- **Teste reinício com reutilização do endereço.** O contrato de `x360_memoria.o`, ligado ao objeto
  real da build, conferiu geração 1 após reset e reutilização, remoção no free do slot 8, fim vazio e
  nova inicialização em geração 0 nas duas arquiteturas. Tamanho inválido aciona a falha do testemunho.
- **O pgrep pode encontrar o próprio comando inline.** Ao criar e executar no mesmo `bash -c` um
  controlador cujo texto contém `qemu` ou `roteiros`, a busca encontra esse bash. Confira os PIDs e
  os comandos completos; execute o controlador por arquivo em outra chamada, mantendo a regra de
  um emulador por vez. Não trate uma linha do shell como um segundo emulador real.

### Menus 3D na F5, etapa 9 (o `ChrView` e o Shenlong)

- **A cena de menu não é a luta.** `BtlScene_Init(8)` (no `ChrView_Init` e no `ShenScene_Init`) monta o `BtlPool`, o
  `EftHit` e a câmera de efeitos, mas não cria o gerente da aura (`EftAuraMgr_Init` é uma tarefa da luta) nem o `BtlCam`
  (o `ChrView` não cria câmera nenhuma; o Shenlong cria só a `DemoCam`, pelo `DemoCam_Init`). O `memoria_luta.py` que
  exigia dono para tudo recusava as duas telas com "gerente da aura sem dono" e "câmera demo sem gerente de câmeras":
  a recusa certa de um dado que não existe é o ponteiro nulo **nas duas builds** mais a prova de que nenhuma
  subalocação do gerente (`EftAura*`) está viva; a `DemoCam` sozinha é tipada como na luta. Visto falhando: um
  ponteiro da aura só na x86, e o teste de subalocação trocado por um dono que existe (`EftHit`).
- **O buffer de modelo do `ChrView` tem duas vidas.** `ChrView_Init` aloca dois buffers de 0xCE000 e grava só a primeira
  palavra (o resto nunca foi escrito: o `memdiff.py` o excluía pelo `contexto_charref` com o `ChrView` parado). Com o
  modelo aberto (`loading` 0, `stageOn` e `visible` 1, `cur` o slot que recebeu o arquivo, com tamanho e id de modelo
  `0x590 + chara * 10 + traje`) o slot do modelo é conteúdo vivo, conferido pelas marcas do arquivo (`01423.bin`), e só
  o **outro** slot segue como não inicializado. Qualquer outro estado do `ChrView` volta a comparar tudo.
- **`ponteiro-port` no cenário.** Com o `ChrView` desenhando, 22 palavras dos blocos do cenário de fundo (`00406.bin`:
  ponteiros para `gStgDepthTint`, `gGfxDepthFog`, `D_002BF6B0`...) valem endereços diferentes nas duas builds, ambos
  dentro da imagem: é a classe `ponteiro-port` do `memdiff.py` (não é classe suspeita, não leva teto).
- **Girar o modelo é um passo do roteiro.** `gira LX LY N` inclina o direcional esquerdo (o `OrbitCam_Update` lê
  `gPad[0].gameLeft` com zona morta de 50). A tela do visualizador é a pseudo-tela 600 (`roteiro.c`: Referência com
  `CharRef.state` na carga ou no modelo), entra em `TELAS_MEM` e `TELAS_IMG`, e o `TELAS_MEM_ARQUIVOS` exige o
  arquivo do modelo vivo e conforme (`fora-do-formato 0`). O `IMAGENS_DISTINTAS` recusa o modelo girado igual ao
  aberto (um `gira` que não girou daria duas imagens idênticas nas duas builds, e a comparação passaria).

### A gravação da VU1 na F5, etapa 8 (o perfil no WSL, `plano/F5/08.md`)

- **As builds de teste não rodavam o caminho do 360.** Sem janela o GS rasteriza por software e a VU1 roda pelo
  interpretador: `hle_program*` e `GsGpu_DrawVu0/4/6` (o que o `GsVu1_Call` faz no console) nunca executavam no WSL, e
  qualquer mudança nelas só podia ser conferida compilando. A `headless.py --gpu` liga o GS em modo gpu com um
  backend falso que só guarda a lista do quadro e faz o hash dela por seção: com isso o `gs_draw.c` de verdade é medido
  e provado. A prova de uma otimização é a lista igual quadro a quadro (`vu1_perfil.sh lista` e `compara`) na mesma
  arquitetura, antes e depois; entre x86 e PPC compare só as seções da VU1 (`SECOES=2,4,5,6`), porque 1/16 de pixel
  de um sprite de CPU difere em 11 quadros do `f5_duelo` entre as duas (já era assim).
- **Um verificador de lista só vale visto falhando**: a otimização foi estragada de propósito (a tabela sem olhar a
  época) e a lista saiu diferente em 8.358 de 8.380 quadros, com o hash do jogo mudado.
- **A contagem de instruções no `qemu-ppc` é uns 15 a 20 vezes mais lenta que a corrida normal (e o `-dfilter` só
  corta o que se imprime, não o custo de seguir cada bloco.** O filtro tem de listar só o que roda na janela:
  qualquer função que o jogo também chama antes dela (libc, `Gs_Gif`, `GsVu1_UnpackWords`, `GsGpu_FrameEnd`) enche o
  log de centenas de milhões de linhas e a rodada passa de uma hora sem chegar à janela (quatro tentativas assim). A
  saída certa: pular as chamadas da VU1 e os desenhos de CPU até o quadro A (`BT3_PERFIL_JANELA=A,B`, que sai em B),
  `-one-insn-per-tb` com `-D` numa FIFO e um `awk` que conta por endereço sem guardar o log, e o filtro gerado do
  `.map` (`vu1_perfil.py faixas`). O que fica de fora (libc, hash de página) entra na estimativa por contas.
- **A `texture_get` entra em linha na `draw_state`**: a parte que decodifica uma textura nova (rara em regime, mas a
  janela parte do zero e decodifica as 63 da luta) vale 75% das instruções da janela. O `vu1_perfil.py ppc` separa pela
  linha do `addr2line` (o fonte tem de ser o da build, `--fonte`), senão o "maior custo" é falso.
- **Cada rodada pede o save vazio**: a rodada que reaproveita o save da anterior para em "4000 quadros sem mcflow
  10". Cada rodada usa um `BT3_SAVES` novo. A `ppcg` pede a x86 normal do mesmo carimbo (o carimbo inclui o hash do
  diff): refaça a x86 antes da PPC a cada mudança.
- **Amostragem de programa**: `setitimer` e o relógio de CPU só disparam a cada uns 4 ms neste kernel (5 mil amostras
  em 20 s); `timer_create` com `CLOCK_MONOTONIC` e sinal dá 100 mil.
- **O hash de página custa mais que o pedido dele.** 107 milhões de pedidos para 1,4 milhão de recálculos, e os
  recálculos (8 KB cada, 351 por quadro de luta) são o laço quente; só 21% deles dão o hash antigo. Cortar os pedidos
  (cache por `t0` com época) rendeu 7% na x86 e uns 13% das instruções; o recálculo só cai com outro algoritmo ou
  menos páginas, e isso se decide com a medida do console (`tempo por quadro: gravação`).

### Os logs do console travavam o jogo (F5, etapa 8, `plano/F5/08.md`, "Os logs e os engasgos")

- **No FATX do HD, `WriteFile` + `FlushFileBuffers` custa uns 30 ms por linha, e o diário de antes pagava isso dentro da
  thread do jogo.** O relatório do backend (8 a 12 linhas a cada 60 quadros) era um quadro de 200 a 430 ms por segundo,
  e os "19 a 20 ms de média nos menus" eram esse quadro dividido pelos 60: tirado ele, a média dos outros quadros era
  16,6 ms. O custo ficou escondido nas rodadas de desempenho da F5 porque o medidor era o próprio log. **Antes de otimizar a
  lógica, tire o pior quadro de cada trecho da média**: se o resto está no tempo do vblank, o problema é um evento
  periódico, e o periódico mais provável é o que o próprio medidor grava (`picos_log.py`).
- **Onde o medidor mede muda o que ele vê.** O `frame_end` lê o relógio do quadro antes de escrever o relatório, então o
  custo da escrita cai no quadro seguinte e aparece como "pior quadro" do trecho seguinte: o pico de 360 ms sai
  deslocado de um quadro do relatório que o causou, e o primeiro trecho (sem relatório antes) não tem pico.
- **Diário no console: anel em memória, thread escritora de baixa prioridade em outro núcleo, `FlushFileBuffers` por
  lote, e descarga síncrona só no filtro de exceção e na saída.** A regra "numa queda o que foi escrito está no disco"
  vale para a queda com exceção (o filtro chama `Diario_Descarregar`); para um travamento sem exceção ela passa a valer
  até a idade do último flush (250 ms). Mantenha a chave que volta ao jeito síncrono (`BT3_LOG_SINCRONO`): é o que
  permite medir um contra o outro com a mesma build. O hiddriver (`referencias_log`) foi o modelo; o CollectionUI faz
  como o 360 fazia, e aguenta porque não tem lógica pesada por quadro.
- **O arquivo "c" da CRT (`saida.txt`) também é um flush.** Cada `fflush` de um arquivo aberto com "c" chama
  `FlushFileBuffers` (também vazio); o `printf` do port dava um por chamada, e o vblank um por quadro. O commit passou
  para o gancho da thread do diário.
- **Um arquivo de log que o jogo grava por quadro (`hash.txt`) tem de ser opt-in no console**: ele estende o arquivo do
  FATX na thread do jogo, e a comparação que ele servia (F2) é coberta pela F1 (decisão 17).
- **O diário vale uma bateria de teste no Linux**, mesmo sem o compilador do 360: uma API de brinquedo sobre pthreads
  pega a emenda do anel, a ordem e o anel cheio (`teste_diario.sh`). Os laços do teste precisam de limite de tempo: um mutante
  que paga o disco por linha levaria minutos (ou travaria) sem ele.

### A gravação do GS numa thread (F5, etapa 8, degrau 1, `plano/F5/08-threads.md`)

- **O detector de corrida não vê tudo; a lista quadro a quadro vê.** Sem a espera do `sceGsSyncPath` 3.316 quadros saíram diferentes e
  nenhuma linha `race:`. A prova de uma divisão em threads é a lista igual à da build sem thread (`divergencias.py`), não a falta de aviso do detector.
- **Atrasar a thread de gravação de propósito (`BT3_GS_ATRASO_US`) é o que dá chance a uma espera que falta de aparecer.** Com a gravação
  mais lenta que o jogo, o jogo chega ao ponto sem espera com a lista ainda em andamento; as provas negativas desta etapa usaram 5 ms.
  (Não se mediu o mesmo estrago com 0 ms.)
- **O jogo refaz pacotes persistentes na lógica do quadro seguinte, antes de qualquer espera do PS2.** O CLUT em tempo de execução
  (`GfxClut_InitPacket`, REF de 73 quadwords) só se cobre copiando o dado na entrega; os REF pequenos da lista (até 128 quadwords, uns 200 KB por quadro) se copiam todos.
- **A saída de uma thread com `atexit`: o resumo do backend falso rodava antes da drenagem** (o `atexit` do backend é registrado depois, no primeiro
  `GsVu1_Call`) e saía com um quadro a menos e o hash do jogo diferente. O resumo espera a thread (`Port_GsEspera`, referência fraca).
- **Nunca restaurar um arquivo de teste com `git checkout` com trabalho sem commit nele.** Uma prova negativa (estragar uma linha e
  restaurar) apagou três arquivos de trabalho não commitados; commite antes de estragar. E uma sabotagem por `sed`/`python` que não casa o texto
  passa em silêncio como "a prova não viu falhar": confira com `assert` que a troca aconteceu (a primeira da espera do `ADXF_ReadNw` não casava por uma linha em branco).
- **Uma queda do WSL no meio de uma prova negativa deixa a sabotagem no arquivo.** Depois de uma queda, `git diff` antes de tudo.
- **Gravação e dispositivo de uma thread só.** O que o frontend faz ao gravar (textura, alvo, shader) é D3D: o canal da GIF (o clear do quadro) também grava, então
  vai para a thread de gravação como trabalho que o jogo espera; no 360 isso evita criar recurso fora da thread dona.
- **O `ambiente.txt` de uma rodada de menus começa com `-BT3_DEMO`.** A build sem janela liga a luta de demonstração por padrão
  (`posix360.c`); sem a linha, o jogo abre direto na demo e sai com a tela verde. Copiar o bloco de uma nota antiga sem ela custou uma abertura.
- **Medição de tempo fina custa no Xenon o que não custa na x86.** A medição por família da gravação (umas 24 mil leituras do
  `mftb` por quadro) custou uns 3 ms da luta no console (39,3 contra 36,2 ms). Compare desempenho sempre com a medição desligada
  nos dois lados (`BT3_TEMPOS=0`) e ligue-a só para o rateio. E um corte que tira contas pode não render no Xenon se o laço espera
  a memória: o envio PSMCT32 caiu 38% na x86 e quase nada no console.
- **Medir a luta pela janela de passos, não pela média dos trechos de 60 quadros.** Na rodada do degrau 2 a média dos trechos com
  lutadores deu 49 ms e quase descartou o ganho; a mediana da janela (`passos: janela ... trabalho p50`, fechada pela pausa) deu
  38,4 ms. Os trechos misturam a pausa e as cenas paradas. E sair pela pausa com uns 3 s de espera, para a janela fechar.
- **Tela preta depois das logos, com o jogo vivo, é o filme de abertura:** o 360 ainda não desenha os filmes (contados, não
  desenhados, `gs_d3d.cpp`) e ele dura uns dois minutos. START o pula. Na rodada da etapa 2 do degrau 2 pareceu trava e custou uma
  abertura; o sinal no log é um desenho por quadro com o resto idêntico à rodada anterior.
- **Área estática que recebe dados de quadword: alinhe pelo tipo, não por `aligned`.** O `uint8_t sCopia[]` da cópia dos REF não tinha
  alinhamento garantido no `cl` (o `Gs_Gif` lê em 8 bytes; no Xenon o não alinhado pode falhar). E o `__attribute__((aligned(N)))` não passa
  no `c89.py` antigo num fonte com acentos (a posição do `clang` é em bytes, o texto em caracteres: "aligned sem valor", corrigido
  depois, seção 6); um vetor de `uint64_t` resolve.

### A divisão entre gravação e desenho (F5, etapa 8, degrau 2, etapa 3, `plano/F5/08-degrau2.md`)

- **O `c89.py` conta as posições do AST em caracteres e o `clang` em bytes: todo literal de string com acento desloca as
  reescritas que vêm depois no mesmo fonte.** Não é só o `aligned` do degrau 1: seis mensagens novas com acento no `gs_draw.c`
  puseram o `{` da declaração depois de comando do `GsGpu_FrameEnd` nove caracteres adiante (`uint64_t { t0 = ...`) e o `cl`
  recusou o fonte. Os comentários não contam (o `clang -E` os tira). Nos fontes que o `c89.py` prepara, acento em string vai por
  escape octal (`"divis\303\243o"`), ou a mensagem sai sem acento (até o `c89.py` passar a trabalhar em bytes, `72c13bc`,
  na mesma época; com ele o escape deixa de ser preciso). Um deslocamento que ainda compile seria um erro silencioso:
  depois de mexer em string de um desses fontes, olhe o `.c` gerado em `build/x360/f2/c89` perto das reescritas.
- **Na build sem janela, símbolo que não existe vira stub, e escrever nele cai.** O `gGsVu1Ms` só existe no 360 (`gs_vu1.c`,
  `#ifdef PORT_X360`); a entrega o zerava e a `x86gv` caiu no primeiro quadro, com o ligador calado. O sinal é a contagem de
  stubs subir (104 para 105): compare a linha `N stubs` do `headless.py` com a de antes a cada build.
- **A prova de uma fila de liberação precisa de liberação que a use.** Nos dois roteiros nenhuma textura solta é do quadro que o
  desenho ainda desenha (no `f5_treino`, nenhum pedido de soltar): soltar sem a fila passa limpo. Com o cache pequeno de
  propósito (`BT3_GS_TEX_MAX=256`, só nas builds `--gpu`) a varredura solta texturas do quadro anterior, e a sabotagem aparece
  como milhares de desenhos com textura já solta.
- **Na divisão, o atraso que mostra a corrida é o do desenho, não o da gravação.** A entrega sem a cópia e o `cleared` ligado pelo
  desenho saíram limpos sem atraso (o desenho do falso termina antes de a gravação chegar lá); com `BT3_GS_DESENHO_ATRASO_US`
  de 5 ou 20 ms, falharam. Cada prova negativa de uma thread nova precisa da chave que atrasa essa thread.
- **No WSL, processo deixado por `nohup ... &` dentro do `wsl -- bash -s` morre quando a sessão do ssh fecha.** Use
  `setsid nohup ... < /dev/null &` (ou um serviço). E `git fetch BUNDLE ramo:ramo` recusa o ramo que está em uso: `git fetch
  BUNDLE ramo && git reset --hard FETCH_HEAD`, só com a árvore limpa.
- **Antes de mandar commit novo ao WSL, `pgrep` pelos trabalhos do anterior.** Um `reset --hard` e uma build nova no meio de uma
  rodada põem dois emuladores juntos e podem trocar o binário de uma rodada em andamento; custou refazer a última da matriz.
- **Linha de log que um verificador lê sai numa escrita só, sobretudo quando outra thread escreve no mesmo arquivo.** A linha
  `gpu_falso: quadro N` saía em nove `fprintf` no `stderr` sem buffer; com a divisão quem a escreve é a thread de desenho, e na
  PPC a do jogo escreve ao mesmo tempo as linhas `LEU arquivo` (`BT3_LEU=1`, que a x86 não tem) no mesmo arquivo. Uma `LEU` no
  meio da linha a cortava: o `divergencias.py` via quadros faltando e quadros "com a lista diferente", e a PPC parecia ter uma
  corrida que a x86 não tinha. Antes de caçar corrida no código, compare o hash da lista (o campo do começo da linha, que sai
  inteiro) e procure texto de outra linha no meio (`grep -c '.LEU '`): aqui os dois bastaram. Monte a linha num buffer e escreva
  com um `fputs`; o verificador passou a contar as linhas misturadas e a falhar com elas.
- **A build `ppcg` (`headless.py --alvo ppc --gpu`) pede a x86 plana do mesmo commit pronta antes** (o mapa das tabelas sai dos
  `.i` dela): num script que leva commit novo ao WSL, a x86 vem primeiro, ou ele para na build com "a build x86 é de outra origem".

### A thread de desenho no 360 (F5, etapa 8, degrau 2, etapa 4, `plano/F5/08-degrau2.md`)

- **Não edite fonte com o `build_f2.sh` rodando.** O passo 1 (`c89.py`) lê os fontes e o carimbo (`commit diff:sha1`) guarda a
  diferença daquele instante; uma correção feita no meio da build ficou fora do `.xex` e a build teve de ser refeita. Para
  conferir que uma build é de um estado, compare o carimbo da linha 2 do `build_f2.log` com o do estado (ou faça o commit antes).
- **Thread ociosa que faz trabalho próprio precisa de um estado de ocupada que quem posta trabalho espera.** O desenho ocioso do
  360 repete o último quadro sozinho (a Guide); o primeiro `desenho_init` punha o pedido de init (`sDesTem = 3`) sem esperar, e
  uma repetição em andamento (`sDesTem = 2`) o apagava ao terminar (`sDesTem = 0`): o init "falhava" sem ter rodado. Achado na
  revisão, antes de rodar. Regra: todo ponto que posta trabalho espera o estado livre (0) sob a mesma trava, como a entrega já
  fazia.
- **No 360, a decisão de dividir vem antes do init do backend.** O init pega o dispositivo (`AcquireThreadOwnership`) e só a
  dona o chama: com o desenho numa terceira thread, o init tem de rodar nela, então a thread nasce antes e a gravação espera o
  init por um trabalho dela. No PC e no falso a ordem de antes não muda nada (a divisão segue no fim do `GsGpu_Init`).
- **Para dividir um backend em duas threads, um ponto de troca com o outro lado parado vale mais que travas por campo.** No
  `gs_d3d.cpp`, tudo o que passa da gravação para o desenho (as texturas das vagas por versão, a conta da VU1, os contadores do
  log) passa na entrega, com o desenho esperando; o resto tem dono único, e o que os dois mexem de verdade (os bytes pendentes)
  é atômico. O inventário campo a campo (`08-degrau2.md`, seções 1 e 2) é o que mostra quais campos sobram para esse ponto.

### O perfil da thread de gravação no WSL (F5, etapa 8, `plano/F5/08-gravacao.md`)

- **O `perf` do WSL (`/usr/lib/linux-tools/6.18.54.1-microsoft-standard-WSL2/bin/perf`) não tem unwinder** (feito sem libdw e
  sem libunwind): `--call-graph dwarf` grava dezenas de MB e não desenrola nada. E o `perf report` não resolve os símbolos da
  x86 de teste (estática, i386, ligada em `0x20000000`): mostra deslocamentos. O caminho que deu certo: `perf script -F tid,ip`
  e o `addr2line -a -f -i` sobre os endereços (somando `0x20000000` aos que vierem abaixo dele); para pilha, uma cópia da build
  com `-fno-omit-frame-pointer` (uma cópia do `headless.py` fora do git, pasta própria) e `--call-graph fp`. A primeira rodada,
  com `dwarf`, se perdeu.
- **Com frame pointer, a folha sem quadro perde o chamador**: o `memcpy` da libc não monta quadro, então a pilha pula para o
  chamador do chamador. A **linha** do endereço de retorno diz qual chamada era (`run_chain:1821` é o `GsGpu_FrameEnd`, logo a
  entrega). E o frame pointer **infla os laços quentes** no i386 (o `transfer_data` foi de 20% para 31%): a build com pilha só
  reparte; as porcentagens vêm da build normal.
- **O clang `-O2 -g` não dá ao `addr2line -i` o nome da função em linha** (`transfer_data` dentro do `Gs_Gif`, `texture_get`
  dentro do `draw_state`), só as linhas: classifique pela faixa de linhas do fonte.
- **Cortar o perfil num trecho do roteiro**: `perf record ... --control fifo:ctl,ack -D -1` e um leitor da saída do binário (com
  `stdbuf -oL`) que escreve `enable` numa linha do roteiro (`roteiro: seq 3`, o combate) e `disable` noutra (`roteiro: seq 4`);
  `/proc/PID/task/*/stat` nas duas pontas dá a CPU de cada thread na janela.
- **`nohup setsid` numa sessão `wsl` aberta pelo ssh morre quando a sessão fecha**: a rodada tem de manter o ssh aberto (rodá-la
  em segundo plano do lado de cá).
- **O log do console na janela do `passos` fica mudo** (`log_quadros` volta cedo com `Passos_Medindo()`), e a `entrega média`
  do `gs: divisão` é cumulativa desde o boot: o valor de um trecho sai da diferença das somas (média × retratos).

### Cortes e medição na gravação (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 7)

- **Não alargar uma cópia de 4 para 8 bytes em código que roda no Xenon sem provar o alinhamento da origem.** O envio PSMCT32
  em grupos saiu primeiro com quatro `memcpy` de 8 bytes; o pacote do jogo é alinhado a 16, mas o grupo começa num pixel que
  pode ser ímpar, e uma leitura de 8 bytes alinhada a 4 no Xenon pode cair numa exceção de alinhamento. Custou parar o serviço
  de provas já disparado e refazer o commit. O ganho do corte era a consulta de tabela por pixel, não a largura da cópia.
- **Medir por evento custa o relógio vezes os eventos.** A linha por família lê o relógio umas 24 mil vezes por quadro de
  luta (o `Unpack` roda uns 7 mil vezes, a VU1 uns 4 mil). Na x86 do WSL, com o `clock_gettime` do vDSO, isso já é 10% do tempo
  da rodada; na PPC do `qemu` cada leitura é uma chamada de sistema emulada e a rodada das listas da `ppcg` dobrou (121 para
  232 s). No 360 o certo é o `mftb` em linha: o `cl` aceita `uint64_t __mftb();` declarado à mão (o mesmo do
  `ppcintrinsics.h`) e emite uma instrução, sem símbolo no `default.map`; o `clang` do `c89.py` (alvo `powerpc`) aceita a
  declaração. Fora do 360 a medição fica desligada por padrão (`BT3_TEMPOS=1` liga).
- **Toda medida nova vista medindo o que se põe nela**: 5 us de espera ativa em cada envio subiram o envio da linha em 1,33 ms
  por quadro, com 274 envios por quadro (o esperado era 1,37).
- **Uma sabotagem pode passar num roteiro e falhar no outro.** A memória do `clut_texture` sem a geração das páginas deu 0
  quadros diferentes no `f5_treino` (nenhuma tabela reescrita no mesmo `cbp`) e 3.416 no `f5_duelo`: rode a prova negativa nos
  dois roteiros, não num só.
- **Uma mudança só no 360 depois das provas do WSL**: confira com `gcc -E` (nas definições da `x86gv` e da `ppcg`) que o
  pré-processado das fontes não mudou fora do 360; se não mudou, as provas valem sem refazer as rodadas.

### A entrega sem cópia (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 8)

- **Confira no código o contador que a tarefa cita antes de pô-lo no roteiro do console.** O "sem espaço" do log do `gs_d3d`
  parecia ser o desenho que não cabe nas listas do `gs_draw.c`; não é: lá o desenho que não cabe volta sem gravar e sem contar,
  e o "sem espaço" do `gs_d3d` é dos buffers da VU1 dele e do cache de texturas. O sinal de lista cheia é o `CHEIO` da linha
  `buffers, o maior quadro do trecho`.
- **Um divisor só de capacidade não enche todas as listas.** Com `BT3_GS_CAP_DIV=40` os índices enchem antes e os desenhos da
  VU param ali, então as constantes e os desenhos nunca chegam ao teto: a prova pediu mais uma rodada com 8 e 5 para as
  constantes. Leia o "maior quadro" de cada rodada para saber que lista de fato encheu.
- **A pasta de dados dos roteiros (`port/build/roteiros/ROTEIRO/dados`) fica vazia depois de uma rodada do `roteiros.sh`.** Antes
  das listas da PPC, refaça-a pelo passo que a gera (como no `~/provas-pingpong.sh`).
- **Com dois jogos de listas, a falta da troca aparece mesmo sem atraso** (1.582 quadros diferentes no `f5_treino`): a gravação
  zera e reescreve o jogo logo depois de acordar o desenho. No retrato copiado a mesma sabotagem só aparecia com a chave de
  atraso.

### O custo do `Unpack` (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 11)

- **Antes de cortar uma cópia, divida o tempo pelas vezes, não pelos bytes.** O `Unpack` parecia "2 MB a 0,6 GB/s, preso à
  banda"; dividido pelas chamadas (uns 9 mil por quadro, 264 bytes cada) dá uns 440 ns por chamada, perto de uma falta de memória
  com página de 4 KB (345 ns): o custo é a primeira linha de cada cópia, e o remédio é pedir adiante, não copiar menos.
- **O código que o `cl` gerou se lê sem console:** monte o `bin/win32` do XDK no contêiner (`-v $XDK/bin/win32:/xbin:ro`) e rode
  `wine /xbin/dumpbin.exe /disasm /range:INICIO,FIM build/x360/f2/default.exe`, com os endereços do `default.map` da mesma build
  (os de outra build não servem). Assim se viu que o `memcpy` do CRT escolhe o caminho pelo alinhamento (8 bytes, 4 bytes ou
  byte a byte), já faz `dcbt` até 8 linhas adiante só dentro da própria cópia, e que a `vu.mem` está em 4 módulo 8 (o endereço
  da estrutura sai do `lis`/`addi` do começo da função; variáveis `static` não aparecem no `.map`).

### A VU1 por dentro e a rodada pelo roteiro (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 14)

- **Para medir por dentro de algo que roda milhares de vezes por quadro, meça uma amostra e encadeie as marcas.** A VU1 tem uns 5
  mil lotes por quadro de luta; seis leituras do relógio por desenho dariam uns 60 mil `mftb` a mais por quadro, e a medida mexeria
  no que mede. Um lote em cada quatro, e cada marca fecha uma parte e abre a seguinte com a mesma leitura (nenhuma leitura de
  "início" à parte): a soma das partes é o tempo dos lotes da amostra, e cada parte vai ao total pela proporção. Sobram umas 3
  leituras por lote em média.
- **Alinhar um campo de uma estrutura `static` no 360:** o `__attribute__((aligned(N)))` no campo, num `#ifdef PORT_X360` com a
  declaração de antes no `#else` (o pré-processado de fora do 360 fica o mesmo); o `c89.py` o passa a `__declspec(align(N))`. Como a
  variável não aparece no `.map`, confira no `dumpbin` pelo `lis`/`addi` do endereço da estrutura e pelo deslocamento de um campo
  vizinho (o `ori` com o deslocamento do `tops` mostrou a `vu.mem` passando de 668 para 768).
- **A rodada de desempenho pelo roteiro no console** (`BT3_ROTEIRO`): o `boot.txt` faz o New Game e o `desbloqueia`, que só aceita o
  cartão vazio, então **cada abertura precisa de uma pasta de saves nova** (`BT3_SAVES=game:\saves_rsem`; a segunda abertura com a
  mesma pasta para em vermelho no boot: troque o nome, não apague no console), mais `BT3_DESBLOQUEIO=1`, `-BT3_DEMO` e
  `BT3_NOMOVIE=1` (sem janela os filmes não tocam, e o roteiro foi feito assim). O `subir_f2.sh` põe os extras na raiz, então o
  roteiro e o `boot.txt` vão juntos para `game:\` (o `inclui` é relativo à pasta). **Os passos por janela e o resultado iguais aos
  da referência do WSL** (`roteiro: janela K passos N`, `roteiro: resultado`) provam que o console jogou a mesma luta; os vblanks
  não precisam bater.
- **Um bundle para o WSL vai dentro do script, em base64** (`base64 -d > ~/x.bundle <<'FIM'` ... `FIM`, e o script pelo stdin do
  `wsl -- bash -s`): no `ssh bt3win "wsl ... bash -c 'cat > ~/x.bundle'"` o `>` é do `cmd` do Windows, que responde "The system cannot
  find the path specified".

### O estado e o fecho da VU1 (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 15)

- **Antes de desenhar uma memória de resultado, conte por qual campo a chave falha.** A memória de uma vaga do `draw_state` acertava
  49% das chamadas; a contagem por campo mostrou que 88% dos erros eram só o contexto do GS (as duas camadas dos programas 0 e 1
  alternam entre o 0 e o 1 a cada chamada). Uma vaga por contexto passou a 86%. Sem a contagem, o caminho óbvio era alargar a chave
  ou desistir.
- **Uma memória de resultado se prova com uma chave que roda o caminho completo em todo acerto e compara** (o
  `BT3_VU_ESTADO_CONFERE`): a lista igual só diz que nada mudou nos roteiros de prova, a conferência diz que todo acerto daria o
  mesmo. Sabote tirando um campo da chave (o TEX0 tirou 1,3 milhão de acertos certos para errados).
- **No Deck, o `pkill -f PADRÃO` dentro do mesmo `ssh` cujo comando contém o PADRÃO mata a própria sessão** (sai com 255): use a
  classe de um caractere (`pkill -f "provas-estad[o].sh"`) ou os PIDs.
- **O `git fetch REPO RAMO:RAMO` recusa o ramo que está em uso** ("refusing to fetch into branch ... checked out"): o script faz
  `git checkout -q --detach` antes. E envie os commits ao remoto `deck` antes de disparar, ou o script roda o HEAD velho.
- **A `ppcg` pede a x86 plana do mesmo commit** (o mapa das tabelas sai dos `.i` dela; sem isso o `headless.py --alvo ppc --gpu` sai
  com 1 dizendo "a build x86 é de outra origem"): nas provas de antes e depois, `--alvo x86` antes do `--alvo ppc --gpu` de cada lado.
- **A pasta de dados dos roteiros para as listas da PPC sai do `roteiros.sh f5_treino f5_duelo` sem `PPC`, `SEMMAPA` nem `IMG`** (as
  duas x86, o `gdb`, o rastreio, o mapa e o `conv_tool`), com as builds `x86` e `x86r` do HEAD: uns 10 minutos no Deck.

### Os índices da VU1 (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 16)

- **No `cl` do 360, um laço que escreve por um `uint32_t *` e conta num global do mesmo tipo relê e regrava o global a cada
  escrita** (o ponteiro pode apontar para ele): o `gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k` tinha 30 instruções por
  triângulo, nove leituras de globais e três escritas do contador, cada escrita seguida da leitura do mesmo endereço (no Xenon, a
  leitura espera a escrita). Com o ponteiro, a base e a posição em locais e o contador gravado uma vez no fim, 12 instruções e
  nenhum acesso a global. Procure o padrão em todo laço quente que escreve numa lista global e conta num global.
- **O `dumpbin` não está na imagem do XDK: `wine /xdk/bin/win32/link.exe /dump /nologo /disasm OBJ`** é o mesmo. No `.obj` os
  endereços dos globais aparecem como `lis rN,0` (a relocação não é mostrada): ache o global pela constante com que o código o
  compara (o `lis r9,8` do teste de capacidade dos índices, 0x80000, marcou o registrador do `gsVuIdxCount`).
- **Ao tirar uma variável do laço novo, tire-a da declaração dentro do mesmo `#ifdef`** (a linha de antes no `#else`): uma local sem
  uso é o C4101 do `cl`, e os avisos por fonte deixariam de bater com os da base.

### O tamanho dos alvos em variáveis (F5, etapa 8, 720p, etapa 2, `plano/F5/08-720p.md`, seção 7)

- **O controle de um `gcc -E` tem de mudar um token que sobrevive à expansão.** O primeiro controle do pré-processado trocou `GS_W`
  por `GS_H` numa cópia do `gs_draw.c` e saiu "igual": as duas valem 1024. Troque um número escrito (`0.5f` por `0.25f`). E o
  `gs_draw.c` só pré-processa com `-Iport/x360/headless` (o `SDL3/SDL.h` de mentira); sem ele o `gcc` para no include e, com o erro
  jogado fora, as três configurações dão o mesmo arquivo truncado.
- **Uma constante de tamanho que vira registrador não muda a conta se o recíproco for potência de 2:** `x / 512.0f` e
  `x * c14.x` com `c14.x = 2 / 1024` dão o mesmo `float`. Ao trocar um literal de shader por constante, escolha a forma em que o
  padrão é exato.
- **Um alvo de tamanho variável com o `gs_draw.c` normalizando por 1024:** o backend leva o uv à textura de verdade no shader (um
  fator por textura, com os gradientes do `tex2Dgrad`), e o que passa da área sai pela borda preta do amostrador (o Xenos aceita
  `D3DTADDRESS_BORDER` com a cor 0), que é o que a parte limpa do alvo grande dava. Nada portável muda.
- **`Clear(0, NULL, ...)` limpa a viewport:** com a superfície da EDRAM do tamanho da área, a limpeza de toda troca de alvo encolhe
  sem mexer no código dela.
- **Para trocar um `#define` usado em umas 50 contas, um script com a contagem esperada de cada troca** (o `assert` do número de
  ocorrências antes de substituir) achou na hora as duas linhas que pareciam iguais e não eram; o `grep` do nome velho no fim
  confere que nada ficou.

### A escala no backend (F5, etapa 8, 720p, etapa 3, `plano/F5/08-720p.md`, seção 8)

- **A borda de um retângulo escalado segue o rasterizador, não o arredondamento comum.** Com os centros dos pixels em .5
  (`HALFPIXELOFFSET`), o pixel x do GS cobre de `floor(x * s)` a `floor((x + 1) * s) - 1`; o `floor(x * s + 0,5)` do plano também
  não deixa fresta entre tesouras, mas na escala 1,5 põe a tesoura um pixel fora da borda da primitiva. Uma regra só (`esc_px`) para
  tesouras, retângulos de passada e redução de captura, e o teste confere cada pixel de saída contra `(2k + 1) / meios`.
- **Levar o desenho à escala sem tocar no lado portável:** a posição pelo `c14` em pixels do GS com a viewport do alvo inteiro, e os
  retângulos convertidos uma vez, onde a tesoura do GS entra no backend (`prepara`, `nat_ret`). Tudo que já era guardado em pixels
  do alvo (sujo, conteúdo, Resolves, restauros, DATE) fica certo sem mudar: procure as entradas de coordenada do GS, não as saídas.
- **Um shader que usa a mesma constante para "pixel para uv" e "passo ao vizinho" quebra na escala.** O contorno lia os dois do
  `natEsc.xy`; separados, o passo vai em texels e cada leitura cai no centro de um texel (`floor`), senão um passo de 1,5 texel lê a
  borda entre dois no ponto e o resultado depende do arredondamento da GPU. O autoteste continua com passo 1.
- **Capturas comparáveis entre escalas:** reduzir para o tamanho da escala 1 pela média dos pixels de saída de cada pixel do GS, e
  pedir os buffers maiores só quando a escala os usa: um buffer estático novo mudaria a memória livre da build sem a chave.

### A retomada do 720p (F5, etapa 8, `plano/F5/08-720p.md`, seção 9)

- **A borda `floor(x * s)` só serve enquanto a fração de `x * s` não passa de 0,5.** A regra do rasterizador com os centros em .5 é
  `ceil(x * s - 0,5)`, em inteiros `(2 x num + den - 1) / (2 den)` com a escala em fração; nas escalas 1, 1,5 e 2 as duas dão os
  mesmos números, e na 1,6 a `floor` põe o pixel de saída 1 no pixel 1 do GS, quando o centro dele é do 0. Ao trocar a conta, um
  programa que imprime tudo o que o header antigo calcula e o mesmo pelo novo (`diff` das duas saídas) prova que as escalas de antes
  não mudaram; uma cópia do header com a regra velha tem de falhar só nas escalas novas.
- **O `fxc` do XDK estima o custo de cada shader:** a listagem `/Fc` termina com os ciclos por vetor de 64 pixels, as instruções,
  os registradores e as threads. Compare as variantes de um shader por ela antes de gastar uma rodada (o `ps_gs` 74 a 80 ciclos, o
  especializado 16, limitado pelo interpolador). A mesma listagem mostra a ordem das multiplicações: duas variantes que têm de dar o
  mesmo byte precisam da mesma ordem, porque o `fxc` reassocia (o `t * cor * k` sai `(t * k) * cor`).
- **O `tex2Dgrad` custa no Xenos:** sai `getGradients`, `serialize` e `setGradientH`/`V` antes da leitura. Com um nível de textura,
  sem filtro de mip e o mesmo filtro de perto e de longe, o gradiente não escolhe nada, e o `tex2D` lê o mesmo texel; o que só o
  console prova fica dito como tal no plano.
- **Variante de shader por desenho sem mexer no resto:** o `prepara` só trocava o pixel shader quando mudava a família do desenho;
  o shader escolhido fica guardado ao lado (valendo só enquanto a família não muda) e, com a chave, é comparado a cada desenho.
- **O Resolve exige retângulos em múltiplos de 8 também nas cópias "inteiras":** a cópia dos ids de 640 x 448 na escala 1,6 tem
  717 linhas e vai a 720.
- **Imagem do jogo não vem para cá, mas a comparação pode:** com o FNV-1a de cada captura no log do console, duas aberturas que
  têm de dar as mesmas imagens se conferem pelo `grep` do log; o `compara_quadros.py` no WSL só entra se algum número diferir.
- **O `git stash` é um só para todos os worktrees do repositório:** apareceu na lista um stash de outro agente (`f5-envio-rapido`).
  Um `stash pop` no momento errado aplicaria o trabalho dele no seu worktree. Para tirar um pedaço de um commit, guarde a cópia do
  arquivo no scratchpad e volte com ela.
- **A estimativa de ciclos do `fxc` não prevê o ganho no console** (a rodada de 23/10): o especializado de 16 ciclos contra 74 a 80
  tirou 2,3 a 3,5 ms na 1,5, um quinto do que o shader trivial tira (11 a 16 ms), e nada no `vu4`, onde o trivial tira dois terços.
  O que pesa é a leitura da textura, que o trivial não faz. Rode o diagnóstico do teto (o trivial) antes de investir numa variante, e
  uma variante "só a leitura" separa a leitura da conta.
- **Na luta, a GPU por classe só existe em três trechos:** o relatório de 60 quadros do `gs_d3d` não sai dentro das janelas do
  `passos` (o começo, a pausa e o fim do combate, quadros 4.619, 5.399 e 7.799 no `f5_duelo`). Compare as aberturas trecho a trecho
  (os desenhos por quadro iguais confirmam o mesmo conteúdo), não pela mediana de muitos trechos.
- **O número de capturas do `BT3_SHOT` depende de quanto o console ficou ligado depois do `fim` do roteiro:** uma a cada 600
  quadros até o jogo sair. Compare só os quadros que as duas aberturas têm.
- **Logs de rodada não moram no scratchpad:** um reinício da máquina o limpa (os logs da 1 e da 1,5 de 22/10 se perderam). Guarde
  em `build/sessao_<data>/<nome>` e escreva na nota os números que a próxima comparação vai usar.
- **Adiar o Resolve "para quando a textura for lida" exige olhar a ordem das chamadas** (`08-720p.md`, seção 10, a rodada de
  23/10): a conferência da leitura rodava antes da troca de alvo, então o alvo que ainda estava posto, e que o desenho seguinte lia,
  passava como "o posto" e ficava sem Resolve. Três sintomas de uma vez (imagem duplicada, partes pretas, a silhueta de uma página
  de trabalho somada à tela) e um contador "esperado 0" em 31 por quadro, que o desenho tinha dado como impossível. Antes da rodada,
  para cada Resolve que sai do caminho, escreva quem lê aquele alvo e em que linha do laço, e ponha um contador que confere a
  leitura no ponto em que ela acontece (depois da troca, antes do desenho), com uma chave de sabotagem para vê-lo acusar.

### O bilinear afiado (F5, etapa 8, `plano/F5/08-720p.md`, seção 10)

- **O header do `fxc` traz a linha de comando no comentário:** dois headers do mesmo shader compilados com caminhos de saída
  diferentes (`/Fh`, `/Fc`) diferem só ali. Para provar "o mesmo microcódigo", compile o `.hlsl` da main com o mesmo caminho de saída
  do build (numa pasta à parte que reproduz `build/x360/f2/shaders`) e compare com `cmp`; ou compare sem as linhas `//`.
- **Uma variante de shader sem duplicar o corpo:** um parâmetro `uniform bool` na função comum, `false` nos pontos de entrada de antes
  e `true` no novo. O `fxc` resolve o desvio na compilação: os 21 headers de antes saíram byte a byte iguais. Dentro do ramo novo, a
  leitura com `tex2Dlod` (sem gradiente), que o `fxc` aceita em desvio.
- **Leituras nos centros dos texels com o sampler no ponto deixam a mistura exata e independente do filtro do jogo:** com o
  bilinear do hardware, um ponto calculado para cair no centro pode sair um tico antes e levar o vizinho com o peso mínimo do filtro.
  Quatro leituras no ponto e a mistura no shader custam pouco quando só a arte 2D passa por ali.
- **No modelo de um filtro em float, "pixel misturado" é peso acima de meio passo de byte (1/512):** o centro de um pixel a
  exatamente meio pixel da borda dá peso 0 na conta exata e uns 1e-4 em float, e um critério de 1e-6 acusava "2 misturados por borda"
  que nenhum byte mostra.
- **Ler a arte 2D por pixel de saída exige a grade que o GS mostra, não o uv contínuo.** O GS lê no canto do pixel dele e fica com
  o texel inteiro; o HUD manda o uv no centro do texel (`(u << 4) + 8`), e essa fração some no console. O afiado lido no centro
  do pixel com o uv do vértice ficou meio texel deslocado: a última linha da peça puxava a linha de baixo da folha e, com um
  texel por pixel e a borda em pixel inteiro, a leitura caía na fronteira entre dois texels (uma rodada do console, 23/10). O
  modelo sintético passou porque punha o uv inteiro na borda: **o modelo usa as convenções de coordenada do jogo** (procure o
  `<< 4) + 8` e o `+ 8` no desenho) e uma folha com vizinhos de outra cor.
- **Um dado por peça quando a lista junta peças num desenho:** uma constante por desenho não serve (o `same_state` junta texto e
  HUD num desenho só). Um atributo a mais por vértice (a caixa da primitiva no TEXCOORD1), montado no backend num vetor próprio
  para o `DrawPrimitiveUP`, com uma declaração e um vertex shader à parte e uma chave a mais no cache do estado, não toca o
  portável.
- **Duas aberturas com as capturas de mesmo `fnv` têm o mesmo alvo:** o que a tela mostra de diferente vem só da cópia para a
  tela. Foi o que separou a gauge de ki do bit 2 (a suspeita) do bit 1 (a causa).


### As texturas novas (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 17)

- **Antes de montar um cache para não redecodificar, classifique as redecodificações pelo conteúdo decodificado.** Um hash dos
  pixels por textura, com a chave do cache ao lado, separa a primeira vez, o descarte, a página que mudou sem mudar a textura e a
  mesma imagem noutro endereço. A hipótese de partida (o descarte por uso do 360) deu zero; quatro quintos dos pixels da luta eram
  imagens já vistas com outra chave (a página de 8 KB mudou por causa de outra textura, ou o jogo reenviou o conjunto a outro
  endereço). O cache certo é pela origem, não um que sobreviva ao descarte.
- **Um acessor genérico por pixel custa no Xenon umas 9 vezes o que custa na x86** (110 contra 12 ns por pixel na decodificação das
  texturas): duas chamadas, um `switch` e leituras de globais por pixel. Um laço por linha com as tabelas por página e a paleta
  expandida uma vez fez 10 vezes menos na x86, com 12 instruções por pixel no `cl`. Procure o mesmo padrão onde o custo é por pixel
  ou por vértice.
- **Depois de tirar uma causa de pico, olhe o que sobrou antes de atacar a seguinte** (F5, etapa 8, 22/10): com as texturas a
  10 ns por pixel, os piores passos da luta que restaram não tinham textura nenhuma e tinham uma leitura de disco de 40 a 98 ms
  na thread do jogo, que outra frente (o ADX em blocos da F6) já resolvia. Medir as duas juntas numa abertura só poupou uma
  rodada.
- **Um teste de unidade da ordem dos bytes tem de rodar na PPC de verdade** (`powerpc-linux-gnu-gcc -static` no `qemu-ppc`, um
  arquivo de teste que inclui o `.c` e liga com `-Wl,--unresolved-symbols=ignore-all`). Na x86 com `-DPORT_BIG_ENDIAN` o `GS_LE32`
  troca os bytes mas a memória segue little-endian: o acesso por byte à VRAM saiu "errado" em 1.179 de 1.500 casos que na PPC
  estavam certos.
- **As listas do `gpu_falso` veem o conteúdo das texturas** (a seção 0: o `fake_tex_upload` hasheia os pixels enviados): uma
  decodificação errada nos 4 bits mudou 141 quadros. Mesmo assim, uma conferência por textura (a decodificação nova refeita noutro
  buffer contra a de antes, `BT3_TEX_CONTA`) diz qual textura e qual formato, e vale para os quadros em que a textura errada não
  chegaria a ser desenhada.
- **A chave de um cache pelo conteúdo tem de ter exatamente o que a decodificação lê, nem mais** (seção 26). A primeira versão do
  cache pela origem hasheava 1 KB a partir da paleta e o `TEXA` sempre: reusou 1.549 texturas no `f5_duelo` e deixou de fora
  justo o alvo, o Blast 2 (65 texturas que voltam com outro `TEXA`, que as cores de 32 bits não leem) e os quadros de página (as
  16 cores dividem o bloco com paletas que o jogo reescreve). Confira o alvo na contagem por quadro (`tex_conta.py --quadro`)
  antes da rodada longa; uma chave a mais não erra a imagem, só não pega, e as provas de igualdade passam do mesmo jeito.
- **Um reuso que funde recursos muda a contagem de desenhos sem mudar a imagem** (seção 26): o `same_state` junta dois desenhos
  pelo número da textura, e com o recurso partilhado 21 quadros do duelo saíram com desenhos a menos. A lista normal só via a
  diferença na seção das contagens (o `drawCount`), que ela inclui. O `BT3_GPU_NORMAL=2` junta pelo conteúdo antes de contar.
  Ao trocar a identidade de qualquer recurso, pergunte quem compara a identidade (junções, memórias de estado) antes de rodar
  as listas: custou duas largadas da rodada do WSL.

### O envio de imagens para a VRAM (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 18)

- **Conte o que passa por um caminho antes de otimizá-lo.** O pedido partiu da leitura do `transfer_data`: os envios de 8 e de 4
  bits iam pixel a pixel. Feito, provado na x86 e na PPC, e a contagem por formato (`BT3_ENVIO_CONTA`, `envio_conta.py`) deu 0 bytes
  nesses formatos: o jogo envia tudo em PSMCT32, com as texturas indexadas já arrumadas em imagens de 32 bits. Uma rodada de
  implementação perdida; a contagem leva um minuto na `x86gv`.
- **Uma sabotagem que as listas não veem pode ser um caminho que não roda.** Os meios bytes trocados no envio de 4 bits deram
  `divergencias.py` 0 nos dois roteiros: nenhum pixel passava por ali. Ponha na contagem quantos itens foram pelo caminho novo
  (a última coluna do `BT3_ENVIO_CONTA`) e confira que não é zero antes de ler a prova negativa.
- **A razão console sobre x86 diz se o custo é conta ou memória.** A decodificação das texturas, presa às instruções, custava 9
  vezes a x86 no Xenon; o envio custa 20 vezes (2,6 contra 0,13 ns por byte) com 4 instruções por pixel no `dumpbin`. Acima da
  razão das contas, o remédio é a memória (`dcbt` na origem, `dcbz128` no destino escrito inteiro), não menos instruções.
- **O `cl` faz um `memcpy` de 4 bytes de um `uint8_t *` byte a byte** (quatro `lbz` e quatro `stb`): ele não sabe o alinhamento. Onde
  a origem é garantidamente alinhada a 4 (os dados de um pacote da GIF, em quadwords), leia e grave como `uint32_t`. Confira no
  `dumpbin` toda cópia pequena de um laço quente.
- **`dcbz128` só numa linha escrita inteira e numa memória alinhada a 128**: a variável com `__attribute__((aligned(128)))` no
  `#ifdef PORT_X360`, o endereço conferido no `dumpbin` (o `lis`/`addi`), uma guarda em tempo de execução, e no teste de unidade o
  `dcbz128` trocado por um `memset` da linha, para uma linha zerada e não escrita aparecer como diferença.
- **Compare um corte da gravação entre aberturas por pares de quadros, não pela mediana de cada uma** (seção 19). O `BT3_PICOS`
  só registra os quadros acima do limiar, e as linhas de 60 quadros do `gs_d3d` não saem dentro das janelas do roteiro: os quadros
  que aparecem mudam de uma abertura para a outra (14 contra 30 no combate), e a mediana de cada conjunto deu 3,6 ms de ganho
  onde o par dava 2,2. Como o roteiro é determinístico, junte o mesmo quadro nas duas, com os mesmos KB de envios e sem texturas
  nem disco: 103 pares deram 1,8 ms, de 1,3 a 3,1. Para medir o quadro típico do combate, que fica abaixo do limiar, a abertura
  precisa de um limiar menor ou do `BT3_TEMPOS=1`.
- **Antes de estimar o ganho na apresentação, classifique os quadros lentos pelo que esperava** (seção 19). O envio tirou 1,8 ms
  da gravação e a apresentação do combate não mudou: os quadros acima de 36 ms já eram o jogo esperando o vblank com a gravação
  parada (80 de 89), não a gravação ocupada (4). A conta "menos trabalho, menos picos" só vale para os quadros presos ao trabalho.

### A espera de vblank e a tela (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 18)

- **Antes de ler uma global como acumulado, procure todos os que a escrevem.** O `passos.c` tirava a espera de vblank do passo
  pela diferença do `gPortSleptNs` entre duas leituras do controle, mas o `gs_core.c` zera essa global a cada entrega da lista:
  a espera saía 0 e o "trabalho" de todas as rodadas da F5 era o intervalo inteiro (média 33,37 ms por construção). Ninguém viu
  porque o número parecia plausível. Um medidor que lê por diferença precisa de um contador dele, que só cresce.
- **Meça a apresentação onde a tela troca, não na CPU depois do `Present`.** Com `D3DPRESENT_INTERVAL_ONE` a imagem entra no
  vblank de hardware; o instante em que o `Present` volta na CPU tem a fase do relógio do jogo, não a do vídeo. A callback de
  vblank do D3D (`SetVerticalBlankCallback`) diz por `D3DVBLANKDATA.Swap` qual troca está na tela: quantos vblanks cada imagem
  ficou, contados ali, são o que a pessoa vê.
- **Callback de vblank (e de troca) roda numa DPC**: só `Interlocked*`, `SetEvent` e afins com handle válido, sem float, sem
  espera, sem o dispositivo. Guarde o `mftb`, some inteiros, sinalize um evento que nunca se fecha (assim ele segue válido numa
  queda) e tire a callback antes de soltar o dispositivo (`SetVerticalBlankCallback(NULL)`, pela dona).
- **Marcar o tempo do jogo pelo vblank de hardware, não pelo `Sleep`.** O `Sleep` do 360 é em milissegundos inteiros e não tem
  fase com o vídeo: o jogo acordava até 2,5 ms tarde e a imagem às vezes perdia o vblank. A espera por um evento sinalizado pela
  callback de vblank, com a mesma conta de antes (um alvo por chamada; o vblank que já passou não se espera; muito atrás,
  recomeça) não soma espera nenhuma e deixa a fase fixa. Deixe o `Sleep` de reserva para antes do dispositivo, para a Guide (um
  evento que não vem em 50 ms) e para a saída, e uma chave que volta a ele para medir um contra o outro na mesma build.
- **Uma espera que deixa passar o alvo atrasado guarda a dívida, e outra fila pode travá-la para sempre.** Na rodada do console
  (seção 19), o alvo da espera pelo vblank de hardware ficou de 1 a 5 vblanks atrás numa cena lenta, abaixo dos 6 que o
  recomeçam; depois, a fila da gravação e do `Present` segurou cada passo em exatamente 2 vblanks, o alvo andou junto com o vídeo
  e a janela inteira teve 0 esperas (a espera de vblank 0,0 ms e o "trabalho" igual ao intervalo de novo). Com o `Sleep` isso se
  desfazia sozinho porque a grade (16,683 ms) é mais lenta que o vblank do console (~16,64 ms). Um contador "chamadas com o alvo já
  passado" na linha da janela teria mostrado na hora; ponha um em toda espera com recuperação de atraso.
- **A apresentação medida na CPU exagera o tranco que a tela mostra.** No mesmo log, 10,33% dos intervalos acima de 34,37 ms na
  CPU depois do `Present` contra 0,86% de imagens com 3 vblanks na tela. Aceite pela tela (a callback de vblank), a CPU só como
  diagnóstico.
- **Para destravar uma espera presa atrás de outra fila, recomeçar o alvo não basta: espere já.** Na trava da seção 19 o jogo
  fica um passo à frente da tela, parado na entrega à gravação; um alvo novo sem espera nesta chamada devolve o jogo à mesma
  fila, e ele trava de novo (a simulação: 52 a 61 recomeços, 7 esperas em 684 passos). Esperando o próximo vblank na hora, a fila
  esvazia um passo sem mudar a tela, e o controle passa a ser lido um vblank mais perto dela (seção 20).
- **Um modelo de fila do 360 tem de segurar o desenho no `Present` até a imagem entrar na tela.** Sem isso o desenho adianta a
  imagem seguinte, sai uma de 1 vblank que paga a dívida, e a simulação nunca reproduz o que o console mostrou (685 imagens de
  exatamente 2 vblanks com o desenho de ~17 ms). Antes de provar uma correção numa simulação, ela tem de reproduzir o defeito com
  a conta de antes (a cena A do `teste_vblank.c` confere isso: a conta de antes sem espera em pelo menos metade das fases).
- **A troca que a callback de vblank conta não é a imagem que a pessoa vê.** O `GsD3d_Repete` apresenta a mesma imagem de novo
  depois de 50 ms sem quadro; a callback vê uma troca nova e conta duas imagens de 2 vblanks onde a tela ficou parada 4. Assim o
  engasgo de 64 a 75 ms do Blast 2 passou como "nenhuma imagem de 4" em 9 de 10 aberturas (seção 25). Some as repetições à imagem
  que repetem antes de aceitar pela tela.
- **Para chegar a zero, conte por imagem e ligue a imagem ao quadro.** A porcentagem de imagens longas não diz qual quadro nem
  que lado atrasou, e o filtro do `picos` (o intervalo na CPU) não é o da tela. O `QuerySwapStatus` logo depois do `Present` dá
  o número da troca, o `D3DVBLANKDATA.Swap` na callback diz quando ela entrou: com os `mftb` dos três lados, cada imagem longa
  sai com o quadro e a distância de cada lado ao prazo (`BT3_PICOS_TELA`, seção 25).
- **Imagens de 3 e de 1 aos pares, com os vblanks da janela exatamente 2 por passo, são quadros isolados que perderam o vblank**,
  não um passo lento: o seguinte entrou no tempo. Procure o quadro, não o passo.
- **Uma repetição pode estar somada na duração, mas ainda reiniciar o prazo do diagnóstico** (F5, etapa 8, seção 27).
  O `tela_troca` soma os 4 vblanks do Blast 2, mas calcula as folgas a partir da última troca, a repetição: GPU 5,5 ms
  antes desse prazo corresponde a cerca de 27,8 ms depois do prazo da primeira entrada da imagem. Confira a âncora do prazo
  antes de concluir que uma imagem longa tinha folga; na janela sem repetições, a leitura direta vale.
- **O quadro que deixa uma imagem longa na tela é o seguinte ao número inicial da linha da tela.** Em 23/10, a imagem 6959
  ficou 3 vblanks porque o 6960 terminou tarde. As imagens curtas também precisam ser listadas: as duas de 6911/6912 vieram
  antes das longas, não como recuperação imediata de cada uma. Histogramas iguais não provam pares consecutivos.
- **O filtro dos picos não mede o que deixou de imprimir.** Depois do cache pela origem, 5338 e 7017 somem tanto das linhas
  da tela quanto das linhas `picos`; isso prova a apresentação, mas não mede a gravação ocupada depois nem lhe dá teto de
  36 ms, pois o filtro consulta períodos de apresentação e do jogo. Não apresentar a subtração dos custos antigos como ganho
  medido. E a partida livre tinha `BT3_TEMPOS=1` por padrão: comparar seu p99 com uma rodada sem marcas mistura duas mudanças.

### A correção da colisão das tabelas (F5, etapa 9, `plano/F5/09.md`, "A correção da colisão das tabelas")

- **Uma correção em duas camadas pede uma sabotagem por camada.** A correção confere o conteúdo e também melhora o hash. A
  sabotagem óbvia (a busca de volta só pelo hash) não foi acusada: o hash novo sozinho já evitava as colisões do roteiro.
  Valeram três: as duas camadas tiradas juntas (acusada, e igual à main), e a conferência sozinha com o hash antigo e com um
  hash de 4 bits (iguais ao branch: é ela que corrige).
- **Desfazer um patch de diagnóstico pelo que ele toca, não pelo que se lembra.** O `diag_tabela.patch` mexe no `gs_core.c`
  além do `gs_draw.c`, e o `git checkout` só do segundo deixou o primeiro alterado e dois `.orig`: a ppcg da main recusou a
  x86 "de outra origem" e as listas dela foram refeitas num segundo serviço. Use `git checkout -- .` e apague os `.orig` (ou
  `git stash -u`), e confira `git status --porcelain` vazio antes de cada build de referência.
- **Uma correção que cria texturas onde a main reaproveitava muda a lista crua desde a primeira.** Os números das texturas
  andam: no `shenlong`, 2.337 quadros diferentes na crua contra os 271 da colisão na normal (`--normal`). Para dizer onde
  está a diferença, um hash normal com a textura suspeita mascarada (`clut_mascara.py`, 0 quadros diferentes) e a lista dos
  quadros diferentes igual à dos quadros com colisão do diagnóstico.

### A VU1 gravada direto nos buffers da GPU (F5, etapa 8, `plano/F5/08-gravacao.md`, seção 18)

- **Escrever direto onde a GPU lê pede um jogo de buffers a mais que a troca entre as threads.** Os dois jogos de listas bastam
  entre a gravação e o desenho (a entrega espera o desenho), mas a GPU lê o quadro depois do `frame_end` dele, e a gravação começa o
  quadro N+2 no jogo de N logo que o `frame_end` N acaba. Com três em rodízio, o jogo regravado é o de três quadros atrás.
- **Uma thread que não é dona do dispositivo sabe que a GPU largou um recurso por uma marca, não por `IsBusy`/`IsFencePending`:**
  o desenho põe `InsertCallback(D3DCALLBACK_IDLE | D3DCALLBACK_CPU1)` com o número do quadro antes do `Present`, o callback o grava
  num volátil, e a outra thread compara. A espera fica com quem vai escrever, nunca com o desenho.
- **Antes de mudar uma lista para escrita combinada, procure quem a relê.** A gravação lia dois índices por desenho de volta (a
  faixa do `vu_plano`), uns 10 mil acessos por quadro que iriam à memória; o valor saiu de quem gera os índices. E a escrita
  combinada de uma thread só chega à GPU depois de um `__sync` no núcleo dela (o `lwsync` das travas não a ordena).
- **Uma GPU falsa com atraso prova o rodízio no WSL:** o falso relê cada jogo uns quadros depois de desenhá-lo e compara com o
  hash do fim do quadro. A lista quadro a quadro (`divergencias.py`) só vê o que foi regravado antes de o fim do quadro lê-la;
  um jogo regravado enquanto a GPU ainda o lê só a releitura atrasada vê.
- **Um corte no desenho só encurta o intervalo se a gravação esperava o desenho.** A cópia da VU1 (3,8 ms por quadro nas cenas
  pesadas) saiu inteira e o `frame_end` caiu 4,8 ms, mas o `quadro inteiro` ficou igual ao centésimo e os intervalos da luta não
  mudaram: nos picos, a gravação esperou o desenho em 3 de 72 quadros; o que estoura é o jogo mais a gravação. Antes de gastar uma
  rodada num corte de uma thread, contar nos `picos:` da rodada anterior quantas vezes a thread seguinte esperou por ela
  (`esperando o desenho`, `espera da gravação`); perto de zero, o corte só dá folga. Num trecho limitado pela GPU, tirar trabalho
  da CPU do desenho só vira espera no anel de comandos (o trecho da pausa: 24 ms com e sem a cópia).
- **A prova que roda noutra máquina tem de voltar para a principal no fim.** A rodada do Deck deixou o resumo só lá, e a análise
  do console encontrou o Deck fora da rede: a prova ficou sem leitura. O script da rodada copia o resumo e as saídas que ele cita
  para a máquina principal (ou o resumo vai no relatório de quem disparou) antes de o branch ir à análise.

### As 16 cores no clut_hash (F5, seção 26 de `08-gravacao.md`)

- **Um diagnóstico não pode copiar a coordenada suspeita do hash.** O `diag_clut.patch` lia as paletas de 16 cores como
  16 x 1, repetindo o defeito do `clut_hash`, e não via a segunda linha. O `clut16.py` usa a leitura da decodificação,
  8 x 2, compara palavras exatas e roda antes de qualquer retorno do cache. Sua prova negativa muda isoladamente cada
  uma das 16 cores e restaura a chave antiga numa cópia: as oito da segunda linha precisam ser acusadas, inclusive na PPC.

### O preparo da rodada final da F5 (`plano/F5/10.md`)

- **Código 0 do `save_mc.sh` não é julgamento de todos os casos.** O script imprime estados diferentes e testes da
  camada, mas não agrega cada falha. A rodada final usa `f5_final_confere.py save-mc`: término dos dois testes,
  estados A/B1/B2 contra x86, erro de escrita F, chegada ao menu e arquivos recuperados. Esse julgamento também
  precisa recusar cópias estragadas, mesmo se o comando anterior saiu com 0.
- **A captura interna não vê o FXAA.** No `frame_end`, `captura()` vem antes de `fxaa_passa()`. `BT3_SHOT_CHEIA=1`
  muda a resolução capturada, não essa ordem. As PPM provam o alvo anterior ao filtro; imagem final com FXAA exige
  captura da saída ou instrumentação específica depois dele, com medição de fps em outra abertura.
- **Não aninhar a trava do CLUT16.** `teste_clut16.sh` abre `~/wsl.lock` e toma `flock` sozinho. Rodá-lo dentro de
  outro `flock` sobre o mesmo arquivo bloqueia a própria fila. O `f5_final.sh` usa trava por etapa, deixa a do
  CLUT16 com ele e recusa outro qemu/roteiro antes de iniciar. `FIM` só diz que o serviço terminou; verificar o
  código de saída e a linha de sucesso antes de aprovar a rede do WSL.
- **Cópia negativa de save sem arrastar os dados.** `port/build/save_mc/dados` contém links para os dados do jogo.
  Copiar só estados, logs e cartões necessários, ou preservar os links e excluir `dados`; um `copytree` que segue
  links duplica gigabytes sem acrescentar prova. Usar diretório próprio em `~/` e conferir o tamanho antes.
- **Ambiente de um filho não volta para o executor.** O `roteiros.sh` exporta os padrões do teste só para seus
  próprios filhos. Antes do `som.sh controles`, o executor precisa definir também `BT3_SOUND_TICKS=1`,
  `BT3_DESBLOQUEIO=1`, `BT3_NOMOVIE=1`, `BT3_MENU_ITEM4=0`, `BT3_GS=none` e `BT3_SETTINGS=`; sem isso a rodada
  nova do gravador pode ter outro estado. As listas GPU sobrescrevem o desenho apenas no seu subshell.

### O rateio pontual da gravação e da GPU (F5, cortes)

- O perfil de GPU antigo fica mudo em `Passos_Medindo()`. `BT3_CORTES_GPU=A,B` seleciona quadros nessa janela,
  guarda o número do quadro emitido junto das marcas e drena os dois buffers mesmo depois da faixa selecionada.
  Sem o dreno, selecionar só o 6960 nunca publica a sua GPU. Não chamar o quadro leitor de quadro medido.
- O rateio de `BT3_CORTES=A,B` usa uma pilha de etapas exclusivas. A VU1 também chama o GIF; subtrair de tempos
  inclusivos sem considerar esse aninhamento conta envio duas vezes. A soma das etapas deve conservar o total.
  Os tempos incluem o relógio e a instrumentação por chamada; comparar ganhos com o perfil desligado também.
- A comparação binária dos uniformes admite agrupar quatro palavras com XOR/OR sem mudar a igualdade. Não usar
  comparação numérica de floats: sinais de zero e NaNs precisam manter os bits. Os uniformes só garantem quatro
  bytes de alinhamento, portanto uma leitura de 64 bits seria uma hipótese adicional sobre o Xenon.
- Nos programas 4 e 6, o bloco uniforme tem 304 bytes, mas 76 e 152 bytes recebem valores logo depois da limpeza.
  Uma limpeza parcial precisa provar também os campos que o shader não lê: eles entram no hash binário da lista.
  Preencher a vaga com bytes não nulos antes do teste encontra os campos esquecidos que uma área estática zerada esconderia.
- `BT3_GPU_LISTA` e `BT3_GPU_NORMAL` no backend falso são chaves por presença: `=0` ainda grava as listas e calcula
  o hash normal. Para uma medida sem esse trabalho, retire as variáveis do ambiente. Registre separadamente o tempo
  total do roteiro e o das parcelas instrumentadas, sem chamar a entrega do falso de tempo de desenho do Xenos.
- O `save_mc.sh` preserva sua saída antiga, em que estados diferentes só são impressos. Ao integrá-lo numa rodada
  automática, não basta aceitar RC 0: o serviço dos cortes confere os estados A/B1/B2 contra a x86, exige dados não
  vazios e compara o save recuperado de B2. A comparação adicional foi vista recusando estados adulterados.
- A limpeza parcial e a comparação em grupos cortam centésimos de ms na x86gv, mas não demonstram ganho de ritmo.
  Para o cenário, o uniforme só depende de `consts[0..15]`, XYOFFSET e ZBUF.PSM. Uma memória desses valores evita
  montar o bloco e converter inteiros para float em cada lote. Sua vaga é da gravação: o fecho precisa copiá-la
  para a lista antes do próximo lote quando abre um desenho. Conferir cada acerto contra a montagem completa.
- Uma vaga estática não é identidade suficiente para pular a igualdade de uniformes: seu endereço não muda quando
  a matriz muda. A cópia gravada precisa do endereço da lista, do quadro e da versão da entrada; o teste troca cada
  um separadamente. A versão só avança numa falha da chave completa, e a lista recebe os 304 bytes antes da marca.
- O diagnóstico de gravação do 360 calibra o mftb contra o relógio do sistema mesmo com `BT3_TEMPOS=0`.
  Não depender da calibração de `Gs_FamTrecho`: ela volta cedo quando as famílias estão desligadas e deixa o valor
  inicial de 49,875 MHz. As duas leituras do sistema ficam fora do intervalo exclusivo, não em cada lote.

- **As marcas podem deslocar a imagem longa sem mudar sua contagem** (F5, gravação, seção 29). Marcando o
  antecessor e o alvo juntos no console, a imagem longa passou para o antecessor: duas longas continuaram duas,
  mas o prazo do alvo mudou de fase. Perfil pontual não certifica apresentação, mesmo descontando o trecho vazio.
  Para localizar custo, medir o alvo sozinho em outra abertura e manter uma referência sem marcas; para certificar
  zero, tirar as marcas. O rateio da gravação também acrescentou uma terceira longa nesta rodada.
- **Medianas só se comparam com a faixa explícita e o backend identificado.** O rateio x86gv do combate inteiro
  tinha mediana 4,635 ms; nos mesmos 126 quadros medidos no Xenon, 2,633. Entrega no falso inclui leitura/hash da
  lista, não desenho do Xenos. `cortes.py` exige o rodapé do falso: a extração de console precisa conferir o fim
  real e conservar as parcelas sem fabricar esse rodapé. O contador da parcela hash inclui acertos, enquanto o
  dos picos conta recálculos de página. `Gs_PageHash` já guarda o resultado por geração: medir invalidações e
  escritas repetidas antes de propor outro cache por geração.

### Identificação e avisos da ISO (F8, `plano/F8/01-avisos.md`)

- **Identificar antes da marca evita orientar a conversão de outra região.** O volume e o `SYSTEM.CNF` distinguem a
  USA conhecida; tamanho físico diferente do volume é truncamento. A marca só é consultada depois. Os hashes
  continuam no instalador do PC: metadados iguais não autenticam a imagem inteira.
- **Um registro ISO inválido precisa estar no caminho que o teste percorre.** A primeira tentativa de testar um
  registro cruzando o setor ainda deixava o `SYSTEM.CNF` antes dele, então a busca já tinha terminado. A imagem
  sintética agora coloca oito registros inventados de 254 bytes antes da quebra, que deixa só 16 bytes no setor.
  Não ler comprimento do nome nem os outros campos antes de conferir o tamanho mínimo e o intervalo do registro.
- **Ao endurecer a identificação, atualizar também a ISO de mentira do áudio.** O `teste_fluxo.c` compila o leitor
  real e precisa de PVD, volume, ordens de bytes e `SYSTEM.CNF` coerentes. Completar o arquivo de teste com um byte
  no fim o torna esparso, com o tamanho esperado de 3 GB sem gravar 3 GB. O `teste_som360.sh` segue passando com os
  mutantes das travas acusados; falhar só na identificação não provaria as travas.
- **GCC com `-m32` pode não achar `asm/errno.h` neste WSL.** Na repetição do teste da ISO em 32 bits,
  `-isystem /usr/include/x86_64-linux-gnu` fornece os headers de sistema que faltavam na busca. O invólucro externo
  também usa `-D_FILE_OFFSET_BITS=64` para o `stat` e o `pread` da imagem esparsa de 3 GB; os `long` continuam com
  32 bits, como no `cl`. Não instalar `gcc-multilib`, que remove o cross da PPC (armadilha acima).

### Abrir a classe comum da GPU (F5, gravação, seção 30)

- A marca de `desenha_comum` vem depois de `prepara`. O intervalo atribuído ao desenho inclui o preparo do
  seguinte, até a próxima mudança de classe. As subclasses de `BT3_COMUM_GPU` preservam essa fronteira para
  substituir só `comum`; o líquido continua uma estimativa com o custo do trecho vazio descontado. Não chamar
  o resultado de tempo isolado do shader, nem somar de novo as linhas detalhadas às classes agregadas.
- O backend falso começa em `gsScale=2`, enquanto o 360 força 1. Para analisar tesouras e `misc.w` do console,
  usar **`BT3_SCALE=1`** na x86gv/PPCg. Não dividir as posições por dois: elas já estão em pixels lógicos do GS.
  `gpu_comum.py` recusa uma lista de outra escala. A escala de saída 1,33 é a fração **4/3**, não 133/100.
- `BT3_GPU_LISTA` contém hashes, não os desenhos completos. `BT3_GPU_COMUM=pasta` e
  `BT3_GPU_COMUM_QUADROS=6929,6930,...` exportam os estados e as posições comuns em JSONL, somente no falso.
  Dados fora do git. Cobertura dos triângulos não é contagem exata de escritas: alfa, DATE, Z e máscara podem
  recusá-las. O censo separa fragmentos, união por alvo e intervalo de escritas; não soma a segunda passada FBA,
  a VU1, as nativas, limpezas ou Resolves à área comum.
- Esperar a build terminar antes de lançar o roteiro: o executável anterior continua no caminho enquanto se
  compila. Uma rodada iniciada antes do fim pode terminar normalmente sem o exportador novo. Juntar build e
  execução num script sequencial sob a mesma trava; conferir carimbo e arquivos selecionados, além do fim.
- Os quadros próximos de uma troca de cena podem mudar de fase entre arquiteturas. Na x86gv desta análise,
  a segunda cadeia de brilho já entra no 6959; o perfil do console só muda no 6960. Comparar a estrutura
  e os quadros vizinhos, sem transportar o tempo do 6959 do console para a lista 6959 da x86.
- No censo de escritas comuns, FBA em duas passadas retira o alfa da primeira máscara. Um desenho que só
  escreve alfa pode ter cobertura positiva e nenhuma escrita de cor na primeira chamada, ficando todo em
  `T_FBA`. O teste deve incluir esse caso com área não nula: no Duelo analisado o único caso sem testes tem
  área zero, o que escondia um mínimo incorreto em entradas futuras. O máximo do censo permanece um limite
  conservador; não se acrescenta a área da segunda chamada à família comum.

### Julgar o perfil fino do console (F5, gravação, seção 31)

- Começar pela variação entre aberturas iguais. Nos três pares alternados de 28/10, os alvos da base variam até
  0,1 ms de folga GPU e os de ambos 0,0, na precisão do log. O efeito de 0,3 a 0,4 ms da combinação se repete,
  mas as duas imagens longas permanecem. Melhorar trabalho mediano ou uma parcela não certifica o prazo da tela.
- `comum_console.py` seleciona **só um quadro por abertura**. Os quatro perfis finos de 28/10 não contêm subclasses
  dos vizinhos 6929/6931/6959/6961; o perfil agregado desses vizinhos é da rodada anterior. Publicar a ausência
  e a origem dos números antigos, em vez de interpolar custos ou tratar repetições como medição dos vizinhos.
- Marcar só o alvo ainda altera os quadros seguintes. As duas aberturas do 6930 acrescentaram uma longa em
  6931 → 6932, e as do 6960 pioraram seu atraso para 14,7 ms. O desconto vazio de cerca de 3,1/3,7 ms é menor
  que a piora de prazo observada, cerca de 3,8 a 3,9/4,8 a 4,9 ms. Calibração não desfaz fase e esvaziamento
  residual; o zero se julga exclusivamente sem essas marcas.
- No perfil fino, a classe comum antiga fica zerada e as oito subclasses a substituem. Conferir os detalhes
  contra cada subclasse, e depois as subclasses junto das outras famílias contra o total. No 6930/6960 são
  69/90 trechos comuns; as 70/91 linhas incluem uma linha API. Não somar detalhes e agregados de novo.
- Contar chamadas efetivas resolve uma ambiguidade, não demonstra redundância: 36/53 chamadas Resolve na rodada
  fina não são os 21/36 segmentos da classe de cor. O custo de uma classe inclui preparo e não é ganho de um
  shader novo. As duas cadeias de brilho somam cerca de 2,35 ms comuns no 6960, com conteúdo diferente entre elas;
  o comum inteiro tem 12,3 ms, dominado por sem textura geral (5,76) e leitura de alvos (3,45).

### Projetar a cadeia inteira antes de acrescentar threads (F5, gravação, seção 32)

- O orçamento é a distância entre entrega do jogo e fim GPU contra o prazo da imagem anterior. Em 28/10,
  42,7 ms para 32,8 disponíveis deixam 9,9 de atraso, ou 10,9 de corte com margem. A cauda pós-Present
  já exclui o trabalho GPU sobreposto ao envio CPU: não somar GPU total novamente. Identificar A, a imagem
  retida, e B, o próximo quadro atrasado, antes de atribuir a causa.
- `BT3_CADEIA_CPU=Q,Q` abre CPU por família e etapa sem comandos GPU novos, independente de `BT3_TEMPOS`.
  `cadeia_cpu.py LOG --quadros Q` confere total, parcelas, soma e fim do Duelo. O teste nativo
  `teste_cadeia_cpu.py --saida ~/prova --base bbc07118` compila o código C++03 real, conserva aninhamento e
  retornos, prova ausência de leituras do relógio desligado e exige os controles negativos. Isso não compila
  o backend XDK inteiro. Não ativar CPU fina junto da GPU fina para atribuir ganho ou aprovar apresentação.
- A classe antiga `outros` contém a varredura estatística `alfa_alto`, antes do primeiro envio GPU; sua
  contribuição foi medida em ~5,4 ms no 6930 da rodada rp32a2810 (seção 33). A VU0 do perfil de desenho
  é a família dos lutadores produzida pela VU1, não evidência de outro interpretador. Tempo de API pode incluir bloqueio/preempção;
  não chamar a diferença CPU menos GPU de espera medida. GetLevelDesc fica em preparo no diagnóstico novo.
- Envio à VRAM GS é trabalho CPU; envio de comandos D3D é outra coisa. Hash por geração, cache pela origem,
  listas alternadas, VU1 direto e shaders compilados no build já existem. Qualquer cache novo precisa mostrar
  trabalho ainda repetido. Nos alvos não houve textura nova/XG; não creditar criação evitada a eles.
- Para consumir prefixos antes do fim da gravação, não basta acordar o desenho cedo: `ed_plano`, `usa_z`,
  `date_area`, `vu_plano` e a entrega de versões dependem do quadro integral. Publicar só prefixos fechados,
  com vida de recursos e última escrita comprovadas, adaptar as consultas ao futuro e conservar uma única
  ordem D3D e um Present. A callback de rodízio VU1 e as liberações têm de ficar depois do último consumidor.
- `reprojeto_console.py DIR --rodada ID` prepara 34 aberturas separadas: controles, CPU, rateio de gravação e
  complemento gf29b. Saves distintos localmente não provam pastas vazias no console. Conferir inexistência
  antes de usar e trocar o identificador inteiro se qualquer pasta já existir. Começar pelos controles e
  CPU dos alvos; analisar antes de gastar os vizinhos. Todas as saídas ficam fora do git.


### Perfil CPU rp32a2810: desligado também precisa de controle (F5, seção 33)

- Zero leituras de relógio com a chave 0 prova esse contrato, não desempenho igual. Os escopos inativos
  ainda podem custar testes por chamada e mudar o código gerado. Nesta rodada as três bases ganharam
  `7169 → 7170` de três vblanks, ausente nos controles antigos, e `frame_end` do 7170 subiu de 15,9 a
  16,1 para 17,8 a 17,9 ms. Não certificar ausência de regressão pela melhora dos dois alvos conhecidos
  ou pelo teste nativo. Alternar as duas builds na mesma sessão, com saves novos; a causa continua por isolar.
- `alfa_alto` confirmou ~5,4 ms no 6930. É estatística periódica por trecho de 60 quadros, não compilação
  de shader de primeira vez. Em VU1 direto ela pode reler índices/vértices de escrita combinada; produzir
  metadados exatos na passagem que já gera os dados precisa de oráculo por desenho e medida sem marcas.
  Mover a leitura para depois de Present pode apenas atrasar o quadro seguinte.
- As 22.303 entradas de escopo no 6960 elevam a amostra CPU a ~8,5 ms, contra 6,5 a 6,6 no controle,
  mas U perde só 0,1 ms. Não descontar a diferença inteira de uma família nem comparar o total com
  os ~4,2 ms de outra instrumentação como regressão do produto. Preparo exclui seus filhos; estado
  contém as APIs de pipe/sampler. CPU menos GPU não isola espera.
- Aquecimento só recebe crédito depois de comparação controlada: zero criação/decodificação/XG nos
  alvos dá zero ms de criação a retirar. O perfil histórico conserva VU0 em 6961 (3,822 contra 3,859 ms),
  sem isolar cache/driver. Aberturas _1/_2 não são a mesma carga antes/depois de aquecer dentro da abertura.
- Guardar SHA256 dos bytes originais, mas recortar no primeiro fim para aplicar o aceite. Continuação
  longa depois do roteiro não é janela adicional. Uma longa marginal, até `-0,0` ms arredondado, continua
  longa se a permanência foi três vblanks. Não trocar permanência na tela por intervalo CPU de apresentação.

### Vizinhos e rateio da gravação rp32a2810 (F5, seção 34)

- Os perfis CPU finos de 6960 e 6961 mantêm 1.220 desenhos VU0 e cerca de 5,96 a 5,98 ms nessa família,
  contra 48 desenhos e 0,17 ms no 6959. O custo persiste no vizinho seguinte, sem ganho de aquecimento
  demonstrado. Não confundir primeira submissão de uma carga maior com um custo transitório removível.
- Um total estável não torna estável sua atribuição a APIs: no 6959, comum/UP passa de 1,275 a 0,260 ms
  entre aberturas e quadro/resto de 0,216 a 1,258, com total 3,131/3,167. Não chamar essa diferença de
  shader frio ou espera localizada sem outra marca que identifique a causa.
- Um par grav b/a por alvo localiza parcelas, não certifica ganho isolado nem ganho de tela. Fecho cai
  ~0,42 ms, constantes crescem ~0,08, saldo ~0,34; total cai 0,327/0,411 no 6930/6960. No 6930, U fica
  -3,4 ms nos dois, apesar de a gravação diminuir. Não recalcular o déficit do produto com marcas ativas.
- No console, uma abertura com `BT3_CORTES=Q,Q` tem só um quadro e não tem rodapé `gpu_falso`.
  Conferir o formato das onze parcelas de `cortes.py`, soma e alvo, sem fabricar rodapé para fazer o leitor
  aceitar. Chamadas de hash incluem acertos; não são os recálculos de página dos picos.
- Nos dois grav*_a, j2 de picos conta 2.335 trocas, contra 2.334 em passos, apenas uma imagem de dois
  vblanks a mais na fronteira. Publicar os dois denominadores; as duas longas são as mesmas. A ausência
  de longa marginal no 7170 desses dois a não restaura F antigo nem dispensa controles entre builds.

### Builds alternadas no 7170: regressão do diagnóstico inativo (F5, seção 35)

- A rp33a2810 alternou três pares c57e807/8c1ec1d na mesma sessão, com saves novos. A longa
  7169 → 7170 apareceu em 3/3 novas e 0/3 antigas; F foi 16,0 a 16,1 contra 17,8 ms, e a folga
  mediana perdeu 0,6 ms. Isso fecha a dúvida entre build e sessão das seções 33 e 34. Não continuar
  atribuindo a ocorrência a variação de sessão nem aprovar um corte pela melhora de outros alvos.
- As únicas fontes de execução alteradas nesse diff são o diagnóstico CPU e seus escopos no backend.
  Retirar as marcas numa cópia recupera byte a byte o backend antigo, com controle negativo de chamada.
  Zero relógios desligado continua insuficiente: construtores/destrutores inativos podem mudar
  instruções, inlining, registradores e pilha. Restaurar produção sem essas marcas e selecionar perfil
  na compilação. Antes de integrar alfa/prefixos, provar código, dados e relocações com o XDK contra o
  controle antigo, fora do console; a prova nativa e a igualdade dos fontes não substituem essa comparação.
- O perfil exato do 7170 mede alfa em 4,723/4,781 ms e VU0 em 14,037/13,980, num total de ~22,3,
  com 37.499 escopos. Alfa já existia na antiga; não confundir seu custo com os ~1,8 ms de regressão,
  nem usar o total instrumentado para ratear o acréscimo desligado. Continua sem prova de shader frio,
  espera eliminável ou troca de cena. O controle novo sem marcas é ~17,8 ms.
- Passos e imagens têm denominadores distintos: nesta rodada as antigas registram 2.335 imagens para
  2.334 passos, tanto em passos quanto em picos, com histograma conservado. Publicar ambos e conferir
  as associações. Não corrigir automaticamente a contagem nem fabricar uma imagem longa adicional.
- **Como provar que a build normal voltou ao código antigo (feito em 28/10):** compilar o `gs_d3d.cpp` de cada
  versão no XDK com as opções da `build_f2.sh` (a cópia numa pasta `gs/` com `som` ligado ao do port, por causa do
  `#include "../som/..."`), desmontar com `link.exe /dump /disasm` (a imagem `collectionui-xdk:light` não tem
  `dumpbin.exe`) e comparar com os rótulos `$M`/`$LN` normalizados, que mudam de número sem mudar o código. Funções
  `inline` vazias ainda saem como COMDATs de um `blr`; conferir no `default.map` que o `-OPT:REF` as descartou.
  Diagnóstico de tempo no caminho do desenho fica atrás de uma chave de **compilação**, nunca só de ambiente.

### A preparação da rodada final da F6 (`plano/F6/09.md`)

- **Serviço de usuário precisa do barramento da sessão.** Neste WSL o systemd está ativo, mas a sessão do agente
  pode chegar sem `XDG_RUNTIME_DIR`. `systemctl --user` então diz `No medium found`; usar
  `/run/user/$(id -u)`, sem elevar usuário nem criar outro serviço root. `f6_final.sh` define essa variável e
  mantém uma única chamada bloqueante de `flock` para a fila inteira, com logs em `~/`.
- **A rede do filme não se reduz às quatro capturas.** O escopo pede hash RGBA por imagem nas duas arquiteturas:
  `f6_filme_hash.c` decodifica os 7010 índices, `f6_final_confere.py filmes` recusa corte, índice repetido e hash
  diferente. O modelo inclui o decodificador real, sem mudar upstream. O `mpeg2test` citado no escopo não está
  neste clone; a referência independente fica explicitamente reprovada, sem renomear o mesmo código como oráculo.
- **Finalizar a fila não significa aprovar a rodada.** O conferente F6 exige todos os casos, códigos, logs íntegros
  por SHA256, carimbos, SHA1 da `.text` e filmes completos. Continua as redes independentes para diagnosticar,
  mas sai 1 em qualquer falha. `FIM` sozinho é término. As propostas antigas de p99 de trabalho não substituem
  o zero de imagens longas no combate da decisão 27; a soma de custo do som ainda tem limite próprio.
- **No WSL, espaço livre no ext4 não mede o espaço do Windows.** Em 28/10 a rodada F6 encheu o disco do
  Windows e morreu após quatro roteiros, apesar de `df` mostrar centenas de GiB livres no volume virtual.
  Guardar todos os rastreios, partes, convertidos, dumps e imagens acumulou cerca de 16 GiB só nesta rodada.
  Após julgamento, conservar logs textuais, gdb, rastro de som para negativos, carimbos e hashes; apagar os
  intermediários de cada caso imediatamente. `f6_final_arquivo.py` não segue links e registra o descarte em
  `retencao.json`. Isso impede repetir `REUSA=1` sobre o caso compactado; os logs conservam o julgamento,
  não os insumos de uma nova comparação. Reservar provisoriamente 16 GiB adicionais no disco físico para
  um caso ativo e a ISO, sem contar os dados existentes; `espaco.json` mede o pico amostrado a cada 20 s.
  Os 40 GB disponíveis nesta retomada bastam para essa reserva, sem acumular saídas entre casos.
- **Retomar conserva reprovações e proveniência.** `f6_final.sh COMMIT BASE DESTINO --retomar` valida a
  origem, os SHA256 dos logs, worktrees limpos e os cinco carimbos. Pula todo caso já registrado, inclusive
  falha; uma interrupção sem registro refaz só esse caso. Metadados são publicados por troca atômica.
  O executor pode mudar apenas ferramentas/documentos/ambientes, mantendo os fontes ensaiados. Sua revisão
  fica em `executores.txt`; não chamar builds antigas de builds do executor novo. Novas durações usam
  `$SECONDS`: o relógio civil recuou 1 s na rodada original e deixou `filme-build` com duração negativa.
  Esse registro é preservado e recusado pelo conferente. Um `.sh` versionado sem bit executável deve ser
  invocado por `bash`; a tentativa original recusada permanece na tabela, com novo caso explícito para o teste.
- **Medida final da retenção da F6:** 21 roteiros, 47,6 GiB de intermediários descartados ao longo da
  fila; pico amostrado da pasta 4.940.568 KiB (4,712 GiB), 2.035 amostras de 20 s, final cerca de 0,67 GiB.
  Essa medida da retomada não inclui temporários externos nem um pico entre amostras. Manter reserva de
  16 GiB físicos adicionais para uma rodada nova, ISO, temporários e margem; não confiar no `df` do ext4.
  Apagar permite reutilizar blocos do volume virtual, sem implicar compactação do VHDX no Windows.
- **Falha de imagem pode ser de identidade ou ausência.** Em `discfusion`, duas lutas usam o mesmo nome
  `luta`: a captura da segunda sobrescreve a primeira e o conferente recusa o quadro, mesmo com 26/26
  PPM iguais. Em `f6_final`/`f6_final_start`, a marca de retorno e `fim` coincidem; falta o desenho seguinte
  para capturar a marca nas duas arquiteturas. Registrar como captura/âncora reprovada, sem afirmar
  diferença de pixels nem retirar a imagem exigida. A referência antiga do Data Center, anterior à senha
  e ao Replay, também precisa de revisão explícita; não regravar automaticamente só para obter código zero.

### Alfa ap35a2810: medir o que saiu do filtro e conferir os descartes dos picos (F5, seção 37)

- Com `BT3_PICOS=36`, a melhora pode retirar o quadro inteiro do log: `picos_escreve` só publica se o intervalo
  de apresentação ou o período do jogo ultrapassar o limiar. `picos: tela` só publica permanências diferentes de
  dois vblanks. Associações completas e histogramas conservados provam a ausência da longa, mas não fornecem
  J/G/P/U ou F individuais, nem margem de 1 ms. Não transformar os 5,4 ms do perfil histórico ou um teto de
  modelo em ganho líquido medido. U do quadro A, retido na tela, é diferente de U do seguinte B atrasado; a
  linha de B pode ainda revelar a folga anterior de A, quando existe.
- Uma mutação sem âncora de `descarte: 0 pressões` pode atingir primeiro `picos:   quadro` no boot, em vez de
  `gs_d3d:   descarte:`. O primeiro negativo da ap35 foi aceito pelo leitor que só conferia o resumo. Conferir
  os dois formatos e ver os dois negativos saírem com 1; também exigir que a mutação alterou o texto e que a
  recusa veio da regra pretendida. Zero em `passos` não substitui os recursos individuais e os resumos.
- A ap35 testou o alfa selecionado por ambiente, antes da chave de compilação: base perdeu 0,3 a 0,4 ms no
  6960 contra main, alfa perdeu mais 0,2 a 0,3. Não transferir esse custo à versão com os blocos ausentes da
  compilação, nem usar o ganho repetido de uma versão para certificar o código gerado ou a imagem da outra.

### Pesquisa de mods nativos: branches, forks e licenças (29/10/2026)

- A pesquisa do Tag Team encontrou um fork público do próprio autor pela lista de forks, embora a busca por
  nome ainda não o mostrasse. Consultar a API paginada e `git log --all`, além do README da branch padrão.
  `tagteam-exp` tinha quatro vistas fora da `main`; no projeto independente, a branch atual era `decomp-native`,
  enquanto a padrão ainda mostrava a implementação antiga por recompilação estática. Fixar commit e data das
  fontes, conferir código e release, e distinguir nota histórica de correções posteriores.
- A licença também depende da revisão: o upstream passou de MIT para AGPL em 14/10, antes das quatro vistas.
  Não aplicar o rótulo MIT antigo ao código novo. As fontes, os limites e a opção conservadora de somente
  referência estão em [pesquisa/05-tag-team.md](pesquisa/05-tag-team.md).
