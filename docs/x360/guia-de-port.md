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
| `survey.sh`, `survey_tally.py` | levantamento do código do jogo (o que depende de hardware do PS2) |

### Builds de teste da ordem de bytes (F1, rodam no WSL)

| Ferramenta | O que faz |
| --- | --- |
| `headless.py --alvo x86\|ppc [--rastreio\|--chamadas]` | builds sem janela do port de PC: x86 de 32 bits (clang `-m32`, a referência little-endian com ponteiros de 32 bits como o 360) e PowerPC 32 bits big-endian (GCC cross, roda no `qemu-ppc`). As duas usam float por software (o do PS2), então uma diferença entre elas só pode vir da ordem dos bytes |
| `f1.sh` | o ciclo inteiro: builds, rastreio, mapa, luta na PPC, comparações. `F1_SEM_RASTREIO=1` reaproveita o rastreio (que leva cerca de 1 h) |
| `compara.py` | compara a luta quadro a quadro (vida, posição, relógio, gerador aleatório) pelo `hash.txt` do `state.c` |
| `memdiff.py` | compara a memória do jogo das duas builds num quadro: heap pareado pelos blocos do alocador do jogo, globais pelo nome, cada palavra numa classe (igual, igual sem inverter, 2xu16, u64...) |
| `layout.py` | compara pelo DWARF o layout de toda struct do jogo entre x86 e PPC; a build PPC falha se algo divergir |
| `mem.py` | lista as variáveis globais do jogo para o dump de memória |

### Achar o que depende da ordem dos bytes

| Ferramenta | O que faz |
| --- | --- |
| `headless/rastreio.c` | runtime de instrumentação (ASan em modo chamada): cada byte lido do disco ganha uma origem (arquivo, deslocamento), e cada leitura dele é anotada com a largura e quem leu |
| `rastreio.py mapa` | do rastreio, o mapa "arquivo, deslocamento, largura" que a build PPC usa para inverter na carga; conflitos vão para um arquivo à parte e nunca são invertidos |
| `headless/inverter.c` | aplica o mapa na PPC (na leitura do arquivo e na saída da descompressão) |
| detector de visões (`BT3_VISOES` no `rastreio.c`, `visoes.py`) | acusa memória gravada com uma largura e lida com outra (u8 de um s32, u64 sobre dois u32). A lista de pares já tratados fica em `visoes_resolvidas.tsv` |
| `tipos.py` | liga cada leitura do rastreio a struct e campo do código (coluna do DWARF + AST JSON do clang) |
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
     mesma palavra. Toda struct assim tem de constar em `layout.PERMUTADAS`, com o motivo.
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
  - cópia em laço (lê N, grava N do mesmo valor) leva a forma junto. A gravação pode ser um pedaço
    de uma leitura mais larga (vetor de 16 bytes regravado componente a componente), mas só na mesma
    posição dentro do bloco alinhado, e levando só a forma, nunca a origem do disco: casar pedaço pelo
    valor espalhou origens erradas (zeros coincidem) e multiplicou os conflitos do mapa por 7 a 13;
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
- Carimbe cada build com a origem (commit, hash da diferença incluindo arquivos novos, hash do
  rastreio usado), e só compare builds do mesmo carimbo.

### 5.5 Ligar leitura a tipo (`tipos.py`)

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
  das pilhas.
- **Console:**
  - o FTP só responde na FreeStyle;
  - descubra o IP pelo MAC (timeout curto demais em varredura perde o console);
  - nome e ícone vêm do SPA embutido no `.xex`.
- **Python no build:** a imagem do XDK não tem Python, então o SPA é gerado no host.
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
