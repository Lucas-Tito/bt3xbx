# 02 — Ports de referência (SM64, MK64, Virtua Fighter) e o que se aplica ao BT3 → Xbox 360

> Pesquisa feita em 2026-10-09. Os repositórios citados foram clonados (shallow) e lidos como **dados**: nenhum
> script deles foi executado. Os links de arquivo apontam para o commit lido (permalink), para que continuem
> válidos se o repositório mudar.
>
> O que é **verificado** (lido no código ou medido no binário) está marcado assim. O que é **inferência**
> também está marcado e precisa ser confirmado antes de virar decisão.

---

## Índice

0. [Resumo executivo](#0-resumo-executivo)
1. [Qual é o "port do Virtua Fighter" (identificação)](#1-qual-é-o-port-do-virtua-fighter)
2. [Virtua Fighter para Xbox 360 (wanszai): análise](#2-virtua-fighter-para-xbox-360-wanszai)
3. [Super Mario 64: sm64-port, sm64ex, Render96 e os ports para Xbox/360](#3-super-mario-64)
4. [Mario Kart 64: SpaghettiKart/libultraship/Torch e o MKart360](#4-mario-kart-64)
5. [Ferramentas para Xbox 360: XDK, RXDK-360, libxenon, OpenXeChain](#5-ferramentas-para-xbox-360)
6. [Tabela comparativa](#6-tabela-comparativa)
7. [O que separa um port "polido" de um "esquisito" no 360](#7-polido-vs-esquisito)
8. [Transposição para o BT3 (PS2 → Xbox 360)](#8-transposição-para-o-bt3)
9. [Recomendações concretas](#9-recomendações-concretas)
10. [Riscos e perguntas em aberto](#10-riscos-e-perguntas-em-aberto)
11. [Fontes](#11-fontes)

---

## 0. Resumo executivo

- **O "Virtua Fighter do 360" não é um port de decompilação.** É o
  [Virtua Fighter — Xbox 360 de *wanszai*](https://github.com/wanszai/Virtua-Fighter---Xbox360)
  (v1.0.0 em 2026-06-28, v1.0.2 em 2026-07-08): um **emulador de um jogo só** (Sega Model 1), com binário fechado.
  O cabeçalho do `default.xex` diz que o executável original se chamava `mametiny.exe` (portanto, provavelmente
  derivado do MAME) e que foi linkado com o **XDK oficial 2.0.21256** (D3D9, XAudio2, XMedia, XFFB, XNet).
  O que vale copiar dele é **a experiência do jogador**: a forma de empacotar, o fluxo de primeira execução e o
  menu. A arquitetura interna não serve de modelo para o BT3.
- **Existem ports de decompilação reais para o 360, todos de 2026:**
  - [MKart360 (sirdankz)](https://github.com/sirdankz/MKart360) — Mario Kart 64, **código aberto**, feito com o
    XDK no Visual Studio. Distribui o código sem ROM: os assets são gerados localmente a partir da ROM do usuário.
  - **SM64 para 360 de ConfusionRS** (junho de 2026) — binário fechado, com a **ROM embutida no XEX**. É o
  contraexemplo jurídico.
- **Nos ports para PC, o padrão é uma camada de plataforma com interfaces trocáveis:**
  - sm64-port e sm64ex: `GfxRenderingAPI`, `GfxWindowManagerAPI`, `AudioAPI` e `ControllerAPI`.
  - libultraship (SpaghettiKart): a mesma ideia como biblioteca, mais extração na primeira execução (Torch → `.o2r`),
    configuração em CVars e menu ImGui.
- **Para o BT3 a base certa já existe:** o [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp), port
  nativo para PC (licença MIT) do próprio BT3-Decompiled, do mesmo autor. Ele já resolveu:
  - memória em endereços do PS2;
  - float no modelo do PS2;
  - frontend do GS e VU1 (interpretador e shader);
  - ADX, sons e MPEG2;
  - memory card;
  - instalador que verifica o disco por SHA-1;
  - executável distribuível sem dados do jogo (tabelas "só com o formato", preenchidas a partir do disco do usuário).

  O port para 360 deve ser **um terceiro backend dessa camada `port/`**, não um port do zero.
- **A grande diferença entre PS2 e N64 no 360 é a endianness.**
  - O N64 é big-endian como o 360, então MK64 e SM64 rodam quase sem trocar bytes. O MKart360 lê MIO0 e
    a ROM do jeito que estão.
  - O PS2 é **little-endian**. Tudo o que vem do disco (AFS, modelos, texturas, animações, cadeias DMA/VIF/GIF
    pré-montadas) precisa de byte-swap, na carga ou numa conversão no momento da instalação.
- **A segunda grande diferença é a CPU.**
  - O port de PC emula o float do PS2 por software e interpreta microcódigo VU1 numa CPU moderna.
  - O Xenon (PowerPC in-order) não tem folga para isso. No 360 será preciso float de hardware com
    arredondamento ajustado (FPSCR/VMX), **todos** os programas VU1 quentes como vertex shaders HLSL e um caminho
    de GS mais "nativo" (efeitos reimplementados, texturas convertidas uma única vez).
- **Toolchain:** XDK, ou o RXDK-360 por cima de um XDK do próprio usuário. É o único caminho que entrega
  Guide, perfis, saves via XContent, metadados para Aurora/FSD e retorno ao dashboard, ou seja, o mínimo para
  "não ficar esquisito". libxenon e OpenXeChain servem como curiosidade, não como base.

---

## 1. Qual é o "port do Virtua Fighter"

Busca feita no GitHub (API de busca de repositórios e de código), na web e em fóruns (1Emulation, LaunchBox).
Candidatos:

| # | Projeto | O que é | Plataforma | Código | Relevância para "igual ao do VF" |
|---|---|---|---|---|---|
| **1** | [wanszai/Virtua-Fighter---Xbox360](https://github.com/wanszai/Virtua-Fighter---Xbox360) ([releases](https://github.com/wanszai/Virtua-Fighter---Xbox360/releases)) | Emulador de um jogo só (Model 1), "60fps HD Widescreen, 1 or 2 Players" | **Xbox 360 (RGH/JTAG, roda também no Xenia)** | Fechado (o repositório tem só o README) | **É quase certamente este.** Lançado em 28/06/2026, alguns dias depois do SM64 360, com fio no 1Emulation ([tópico 37933](https://www.1emulation.com/forums/topic/37933-virtua-fighter-for-xbox-360-new/)) e ficha no [LaunchBox](https://gamesdb.launchbox-app.com/developers/games/68818-wans-zai) |
| 1b | [wanszai/Virtua-Fighter---XboxOne-Windows](https://github.com/wanszai/Virtua-Fighter---XboxOne-Windows) | O mesmo "wrapper" para PC e Xbox Series (Dev Mode, UWP) | PC/XSX | Fechado | Mesmo produto; os issues mostram o menu (EEPROM, 4:3/16:9, filtros, remapeamento) |
| 2 | [celsowm/vf2-decomp](https://github.com/celsowm/vf2-decomp) (fork [biggestsonicfan/vf2-decomp](https://github.com/biggestsonicfan/vf2-decomp)) | Decompilação clean-room, *non-matching*, do VF2 (Model 2A, i960) | Pesquisa, sem host jogável | Aberto, CMake | Bom modelo de **verificação de ROM** (`tools/vf2rom`) e de testes diferenciais; ainda não é um port (partida jogável ~20%, segundo o próprio README) |
| 3 | [Thorhax/VF2-NX-Modern](https://github.com/Thorhax/VF2-NX-Modern) | "Native Nintendo Switch port of VF2 (Model 2A static recompilation)" | Switch | Aberto | Recompilação estática, não decomp; outro console |
| 4 | [sp00nznet/vf5](https://github.com/sp00nznet/vf5) + [sp00nznet/model2recomp](https://github.com/sp00nznet/model2recomp) | Recompilação estática do VF5 (PS3, via ps3recomp) para Windows; runtime de Model 2 | PC | Aberto | Recomp, não é para o 360 |
| 5 | [wacastel/virtua-fighter-engine](https://github.com/wacastel/virtua-fighter-engine) (e VF2/VF3) | Tradução offline de CPU para Apple Silicon | macOS | Aberto | Não é para o 360 |

**Conclusão:** quando o usuário pensa em um "port do VF para Xbox 360", trata-se do **item 1, de wanszai**. Ele
**não é** decompilação nem recompilação: é um emulador dedicado. Ou seja, "igual ao do VF" quer dizer **igual em
empacotamento, UX e conjunto de recursos**, e não em arquitetura. Os outros candidatos aparecem para completar a
lista e nenhum deles roda no 360.

---

## 2. Virtua Fighter para Xbox 360 (wanszai)

### 2.1 O que foi verificado no binário (release 1.0.2, `VirtuaFighter.zip`)

Conteúdo do zip:

```
VirtuaFighter/
  default.xex          2.6 MB   (XEX2, criptografado + compressão LZX)
  menu_assets.xpr     10.5 MB   (XPR2 = "Xbox Packed Resource" do XDK: texturas TX2D do menu)
  intro.wmv            0.35 MB  (vídeo de abertura, ASF/WMV, tocado via XMedia)
  menumusic.mp3        2.2 MB   (música do menu)
  cover.png            1600x1082 (capa para o usuário/frontend)
  ReadMe.txt
  roms/                (vazio: o usuário coloca vf.zip aqui)
```

Cabeçalhos opcionais do XEX2, lidos com um parser próprio de ~40 linhas:

| Campo | Valor | Leitura |
|---|---|---|
| Nome original do PE | `mametiny.exe` | **Inferência forte:** build "tiny" do MAME, reduzido a um driver |
| Title ID | `0xFFFF5361` (VR: `0xFFFF5562`, Sega Rally: `0xFFFF5668`) | ID "homebrew" próprio por jogo (prefixo `FFFF`) |
| Recurso embutido | seção `FFFF5361` (≈11 KB) | XDBF/SPA com nome e ícone do título: é o que o **Aurora/FSD** exibem |
| Bibliotecas (todas com versão **2.0.21256**, o XDK final) | `XAPILIB, XBOXKRNL, D3D9, D3DX9, XAUDIO2, XAPOBA, XMCORE, XFFB, XGRAPHC, XNET`, CRT `LIBCMT`, `C1/C2/LINK` do VS2010 | **XDK oficial**, não libxenon. XMCORE = XMedia (WMV/MP3), XFFB = force feedback, XNET = rede |
| Importa | `xam.xex`, `xboxkrnl.exe` | Integra-se com o dashboard (perfis, Guide, device selector) |
| Base / entry | `0x82000000` / `0x826D92E0` | Padrão de título XDK |

Os outros títulos do mesmo autor (Virtua Racing, Sega Rally, Ridge Racer Collection, Ace Driver) seguem o **mesmo
molde**: mesmo `mametiny.exe`, mesmo conjunto de arquivos (`menu_assets.xpr`, `intro.wmv`, `menumusic.mp3`, `roms/`).

### 2.2 Fluxo do jogador (ReadMe e notas de release)

1. Extrair a pasta para o HDD do 360.
2. Colocar **o seu** `vf.zip` (romset MAME) em `roms/`, ao lado do `default.xex`.
3. Abrir pelo Aurora/FSD/XeXMenu.
4. **Na primeira execução, escolher o HDD no device selector** para criar o save. Sem isso, as configurações
   (por exemplo, free play) não persistem. Isso é XContent do XDK: save no lugar certo, por perfil, gerenciável
   pelo dashboard.
5. No Xenia é obrigatório ter um perfil logado capaz de salvar. Sem ele a ROM não inicia e a tela fica preta.

Recursos (VF 1.0.x, VR 1.0.3, Sega Rally 1.0.0c):

- 60 fps, 720p, 16:9 com **pillarbox automático** na seleção de personagem (v1.0.1);
- sombras transparentes (correção da sombra "sólida" original);
- remapeamento de botões e "combo buttons" (v1.0.2);
- configuração de EEPROM antes de carregar (dificuldade, região, rounds);
- vídeo: 4:3/16:9, filtros CRT, brilho, escala de resolução;
- nos jogos de corrida: rumble, volante com force feedback, System Link/online com crossplay PC↔360↔XSX
  (relays próprios na UE e nos EUA), trilha sonora customizada (`soundtrack/*.mp3` com nomes fixos), placar
  persistente, "Smooth/Smooth+" (60 fps interpolado sobre um renderer de polígonos a 30).

### 2.3 Arquitetura (inferida dos binários e das notas)

- **CPU do jogo:** emulada (Model 1: V60 + DSP TGP). Notas como "fixed a precision issue in TGP that could make
  the camera wobble" confirmam que o coprocessador geométrico é emulado e ajustado à mão.
- **Gráficos:** D3D9 do 360 com um renderer de polígonos de alto nível (o Model 1 desenha polígonos, não pixels),
  o que permite 720p/16:9 "de verdade" e efeitos como as sombras transparentes. O widescreen não é um esticamento:
  o pillarbox na seleção de personagem mostra que há **correções por tela**.
- **Áudio:** XAudio2 (+ XAPOBA para efeitos), com o som do Model 1 emulado.
- **Frontend:** menu próprio desenhado com as texturas do `menu_assets.xpr`, vídeo de introdução e música.
  **Não usa XUI.**
- **Persistência:** XContent (device selector), com fallback incerto: o Xenia sem perfil mostra tela preta.
- **Licença:** nenhuma é declarada. Se o binário de fato deriva do MAME (GPL-2.0+ no conjunto), a distribuição
  sem código-fonte é juridicamente questionável. É um ponto a **não** copiar.

### 2.4 Por que ele parece "polido"

- **Uma pasta, um `default.xex`, um `ReadMe.txt` curto e uma pasta `roms/` óbvia.** Não há passo no PC, script ou
  conversão.
- **Abre direto num menu com cara de produto** (vídeo, música, arte), e não num terminal ou numa tela de debug.
- **Usa os serviços do console** (device selector, perfis, XNet, force feedback). O jogo se comporta como um título
  de verdade.
- **Title ID e XDBF próprios**: no Aurora aparece com nome e ícone, não como "default.xex". Vem com `cover.png`.
- **Opções que os jogadores esperam** (16:9/4:3, filtros, remapeamento, rumble) e **notas de versão curtas e
  concretas**.

### 2.5 O que é "esquisito" e deve ser evitado

- Tela preta quando a ROM falta ou não há perfil, em vez de uma mensagem.
- Binário fechado, sem licença.
- Dependência de um nome de arquivo exato (`vf.zip`) sem verificação de hash visível.
- Releases distintas por plataforma com recursos divergentes ("video options do not apply to the 360 build").

---

## 3. Super Mario 64

### 3.1 sm64-port (o port de PC "canônico")

Repositório: [sm64-port/sm64-port](https://github.com/sm64-port/sm64-port) (lido em `2b17d08`).

**Arquitetura (verificada):** uma camada `src/pc/` com **quatro interfaces de tabela de funções**, escolhidas na
compilação:

- [`gfx_rendering_api.h`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/gfx/gfx_rendering_api.h)
  define `struct GfxRenderingAPI` (shaders, texturas, samplers, depth, viewport/scissor, `draw_triangles`). As
  implementações são `gfx_opengl.c`, `gfx_direct3d11.cpp`, `gfx_direct3d12.cpp` e `gfx_dummy.c`.
- [`gfx_window_manager_api.h`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/gfx/gfx_window_manager_api.h)
  define a janela, o loop principal e o swap (SDL2, DXGI, GLX).
- [`audio_api.h`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/audio/audio_api.h)
  define `init/buffered/get_desired_buffered/play`. Implementações: SDL, WASAPI, Pulse, ALSA, null.
- `controller_api.h` cobre SDL, **XInput**, teclado e TAS gravado.

**Como o hardware original é substituído:**

- **RSP gráfico (Fast3D):** [`gfx_pc.c`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/gfx/gfx_pc.c)
  interpreta as display lists F3D/F3DEX. É "HLE de microcódigo": transforma, ilumina e recorta na CPU e entrega
  triângulos prontos ao backend. O color combiner do RDP vira um shader gerado por combinação
  ([`gfx_cc.c`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/gfx/gfx_cc.c)).
- **RSP de áudio:** [`mixer.c`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/mixer.c)
  reimplementa os comandos ABI (ADPCM, resample, envelope, mix).
- **libultra:** [`ultra_reimplementation.c`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/ultra_reimplementation.c)
  troca filas de mensagens, EEPROM por arquivo e timers.
- **Timing:** [`pc_main.c` → `produce_one_frame()`](https://github.com/sm64-port/sm64-port/blob/2b17d081c9798b31b91dc71f37994b0da28cffc9/src/pc/pc_main.c#L81)
  roda uma iteração do jogo, gera 528 ou 544 amostras de áudio conforme o buffer e faz o swap. Tudo fica numa
  thread, com o ritmo dado pelo vsync e pelo nível do buffer de áudio.

**Assets:** extraídos **no build** a partir de `baserom.<versão>.z64` (`extract_assets.py`) e compilados no
executável. Isso funciona para quem compila, mas **o binário resultante contém os assets e não pode ser
distribuído**.

**Endianness:** o N64 é big-endian e o PC é little-endian. O build converte os dados em C/`.inc.c` e os macros do
GBI cuidam dos campos. Num host big-endian (360, Wii U) esse problema praticamente desaparece.

**Licença:** nenhuma. O projeto upstream (n64decomp/sm64) também não tem.

### 3.2 sm64ex (o fork "de usuário final")

Repositório: [sm64pc/sm64ex](https://github.com/sm64pc/sm64ex) (lido em `d7ca2c0`). Acrescenta ao sm64-port o que
o deixa "produto":

- **Sistema de arquivos virtual com pacotes** ([`src/pc/fs/`](https://github.com/sm64pc/sm64ex/tree/d7ca2c04364a6dd0dac58b47151e04e26887e6f0/src/pc/fs)
  com `fs_packtype_zip.c` e `fs_packtype_dir.c`). Com `EXTERNAL_DATA=1`
  ([Makefile](https://github.com/sm64pc/sm64ex/blob/d7ca2c04364a6dd0dac58b47151e04e26887e6f0/Makefile#L51)),
  texturas e sons vão para `res/base.zip` fora do executável. Isso permite **texture packs** (Render96) e separa o
  código dos dados.
- **Configuração persistente** ([`configfile.c`](https://github.com/sm64pc/sm64ex/blob/d7ca2c04364a6dd0dac58b47151e04e26887e6f0/src/pc/configfile.c),
  `sm64config.txt`) com binds múltiplos por ação.
- **Menu de opções dentro do jogo** ([`src/game/options_menu.c`](https://github.com/sm64pc/sm64ex/blob/d7ca2c04364a6dd0dac58b47151e04e26887e6f0/src/game/options_menu.c)),
  que usa a fonte e o estilo do próprio jogo. Esse ponto é muito importante para o "não esquisito".
- **Diretório do usuário por plataforma** (`platform.c`), opções de linha de comando (`cliopts.c`) e flags de build
  para melhorias opcionais (`BETTERCAMERA`, `NODRAWINGDISTANCE`, `TEXTURE_FIX`), **todas desligadas por padrão**.

### 3.3 Render96ex

Fork do sm64ex ([Render96/Render96ex](https://github.com/Render96/Render96ex)) com **packs de modelos e de
texturas** carregados do diretório externo. Ferramentas como [R96ex-restool](https://github.com/cdeletre/R96ex-restool)
automatizam "ROM → pacote de recursos". **Lição:** separar código, dados extraídos e dados de mod, cada um em sua
camada, é o que permite melhorias visuais sem tocar no jogo.

### 3.4 SM64 no Xbox original (nxdk) — referência de "console com GPU fixa"

- [fgsfdsfgs/sm64-port, branch `xbox`](https://github.com/fgsfdsfgs/sm64-port/tree/xbox), que continua em
  [jtryba/sm64-hybrid-xbox](https://github.com/jtryba/sm64-hybrid-xbox), usa **nxdk + XGU** (SDK livre) e gera
  `default.xbe` e ISO.
- Ele se inspirou no renderer de [mborgerson/xsm64](https://github.com/mborgerson/xsm64).
- É útil como prova de que a separação `GfxRenderingAPI` entra num console com GPU de função fixa.

### 3.5 SM64 para Xbox 360 (ConfusionRS)

- Divulgado em 23–27/06/2026 ([r/360hacks](https://www.reddit.com/r/360hacks/comments/1ud68at/ported_sm64_to_the_xbox_360/),
  [ElOtroLado](https://www.elotrolado.net/hilo_super-mario-64-port_2534038),
  [1Emulation tópico 37931](https://www.1emulation.com/forums/topic/37931-mario-64-port-for-xbox-360-new/),
  [vídeo](https://www.youtube.com/watch?v=uEr3umV7AD4)). Roda em RGH, pelo Aurora.
- **Não há código público.** O "you do not need the rom… it's all baked into the executable" mostra que **o
  binário redistribui os assets da Nintendo**. É o modelo a **evitar**.
- Bug relatado: o console trava ao entrar nos canhões.
- O MKart360 ([`xbox360_input.cpp`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_input.cpp))
  comenta que "the SM64 Xbox binary we mapped used `XamInputGetState(0,1,…)`". Isso indica um build com o XDK
  (Xam) e, provavelmente, derivado do sm64-port com backend D3D9 (**inferência**).
- Derivado: [PedroMarioaBros/SM64toX360](https://github.com/PedroMarioaBros/SM64toX360) aplica **patches no XEX
  binário** de outra pessoa (tradução PT-BR) e tenta montar uma toolchain aberta
  ([OpenXeChain-X360-Builder](https://github.com/PedroMarioaBros/OpenXeChain-X360-Builder)). Não serve de
  referência de engenharia: o próprio README diz "faz patches e reconstrução de executável existente, não um build
  integral de fonte".

---

## 4. Mario Kart 64

### 4.1 SpaghettiKart + libultraship + Torch (HarbourMasters)

Repositórios: [HarbourMasters/SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) (`2b3ddb4`),
[Kenix3/libultraship](https://github.com/Kenix3/libultraship) (`759b134`, MIT),
[HarbourMasters/Torch](https://github.com/HarbourMasters/Torch) (`72960ca`, MIT).

**Arquitetura (verificada):** a camada de plataforma vira **uma biblioteca reutilizável** (a mesma de Ship of
Harkinian, 2 Ship 2 Harkinian e Starship):

- `Ship::Context` é o núcleo: logging, console, CVars, `ResourceManager`/`ArchiveManager` (O2R/OTR/pastas), janela
  e controles.
- Gráficos:
  - [`src/fast/interpreter.cpp`](https://github.com/Kenix3/libultraship/blob/759b13401cd6903daa67a00a753066f791622e78/src/fast/interpreter.cpp)
    é o descendente do `gfx_pc.c`.
  - [`src/fast/backends/`](https://github.com/Kenix3/libultraship/tree/759b13401cd6903daa67a00a753066f791622e78/src/fast/backends)
    traz D3D11, OpenGL e Metal.
  - [`Fast3dWindow.cpp`](https://github.com/Kenix3/libultraship/blob/759b13401cd6903daa67a00a753066f791622e78/src/fast/Fast3dWindow.cpp)
    cuida de refresh, vsync e latência máxima.
- Áudio: [`src/ship/audio/`](https://github.com/Kenix3/libultraship/tree/759b13401cd6903daa67a00a753066f791622e78/src/ship/audio)
  com SDL, WASAPI, CoreAudio, null e decodificador de matriz.
- Input: `ControlDeck` com mapeamentos padrão por jogo e remapeamento pela UI
  ([`Engine.cpp`](https://github.com/HarbourMasters/SpaghettiKart/blob/2b3ddb4a579887d7ab41c56445691b9411996399/src/port/Engine.cpp)).
- Menus em Dear ImGui (`SpaghettiGui`), console de desenvolvedor, CVars persistidas em JSON.
- Interpolação de quadros ([`src/port/interpolation/FrameInterpolation.cpp`](https://github.com/HarbourMasters/SpaghettiKart/blob/2b3ddb4a579887d7ab41c56445691b9411996399/src/port/interpolation/FrameInterpolation.cpp))
  para 60/120 fps com a lógica a 30. A simulação continua no ritmo original.

**Assets e primeira execução (verificado), o fluxo mais polido entre os estudados:**

1. O jogo procura `mk64.o2r`. Se não existe, [`ModManager.cpp`](https://github.com/HarbourMasters/SpaghettiKart/blob/2b3ddb4a579887d7ab41c56445691b9411996399/src/engine/mods/ModManager.cpp#L60)
   pergunta: *"No O2R files found. Generate one now?"*.
2. [`GameExtractor::SelectGameFromUI`](https://github.com/HarbourMasters/SpaghettiKart/blob/2b3ddb4a579887d7ab41c56445691b9411996399/src/port/GameExtractor.cpp#L29)
   **varre os arquivos candidatos e identifica a ROM pelo SHA-1** (`mGameList`). Só se não achar abre um diálogo de
   arquivo. Em consoles (`__SWITCH__`) cai em `baserom.us.z64` ao lado do executável.
3. Se o hash é desconhecido, mostra *"Unsupported ROM"* com o motivo. Se conhece, mostra *"Found Mario Kart 64 (US)
   — the extraction will now begin"*.
4. O Torch ([`Companion.cpp`](https://github.com/HarbourMasters/Torch/blob/72960ca16f4723f96aaf4037b76072c230af8c11/src/Companion.cpp))
   lê a ROM, aplica as receitas YAML (`yamls/`) e grava `mk64.o2r`, um zip com recursos binários em formato
   próprio e endianness declarada (`SetEndianness(Torch::Endianness::Big)`). Os leitores do libultraship
   (`BinaryReader`) convertem para a endianness do host.
5. Os mods são `.o2r`/`.zip` numa pasta `mods/` e sobrepõem caminhos.

**Build:** CMake + vcpkg, com Torch e libultraship como submódulos. Existe build para Switch, o que mostra que a
pilha roda em console (com SDL portado). Há também um port UWP para Xbox Series ([dhakaaas1/spaghettikart-xbox](https://github.com/dhakaaas1/spaghettikart-xbox)).

**Lição para o 360:** libultraship depende de SDL2, C++17/20 e ImGui, e o compilador do XDK (VS2010) não suporta
C++ moderno. **Não dá para levar a lib inteira para o 360**, mas dá para copiar o *desenho*:

- extrator que identifica a mídia por hash;
- recursos versionados num pacote;
- CVars;
- menu de opções;
- interpolação opcional.

### 4.2 MKart360 (sirdankz) — o port de decomp para 360 de código aberto

Repositório: [sirdankz/MKart360](https://github.com/sirdankz/MKart360) (`a36c9be`, 2026-09-28). É o exemplo mais
direto do que queremos fazer, na mesma plataforma.

**Estrutura (verificada):**

```
MK64.sln, MK64/MK64.vcxproj          ← projeto Visual Studio, plataforma "Xbox 360" (XDK)
PUBLIC_BUILD_XBOX360.ps1             ← prepara assets + MSBuild Release|Xbox 360
mk64-master/                         ← árvore n64decomp/mk64
  PUBLIC_PREPARE_MK64_ASSETS.py      ← verifica a ROM (tamanho 0xC00000, CRC32 0x434389C1) e gera os .inc.c
  PUBLIC_ASSET_RECIPES.json          ← receita por arquivo: rom_offset, tamanho, representação, sha256 do resultado
  PUBLIC_SOURCE_GOLD_HASHES.json     ← sha256 dos fontes do port (detectar árvore alterada)
  include/xbox360/, src/xbox360/     ← camada de plataforma (≈11,5 mil linhas, 70% netplay)
  xbox360/PATCH_SUMMARY.txt          ← lista do que mudou no código do jogo ("seams")
```

**Camada de plataforma** ([`include/xbox360/platform.h`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/include/xbox360/platform.h)):
mantém os tipos do N64 na fronteira, para que o jogo mude o mínimo. As "costuras" estão documentadas em
[`SOURCE_AUDIT.txt`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/xbox360/SOURCE_AUDIT.txt):
controle (`osContStartReadData…`), gráficos (`exec_display_list(SPTask*)`), VI (`osViSwapBuffer` + vblanks) e
áudio (comandos ABI do RSP).

| Subsistema | Como fizeram | Avaliação |
|---|---|---|
| **Entrada do programa** | [`xbox360_entry.cpp` `main()`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_entry.cpp#L247): carrega a ROM, inicia D3D, Live/Party, menu de rede, áudio, `gfx_init` e `main_func()` | Se a ROM falta, **`return 1` sem mensagem** (volta ao dashboard): esquisito |
| **ROM/assets** | Build: script Python gera os `.inc.c` a partir da ROM (verifica CRC32 e o sha256 de cada saída). Runtime: lê `game:\baserom.us.z64` para um array estático de 12 MB, confere o CRC32 e emula a DMA do PI com `memcpy` ([`xbox360_asset_loader.cpp`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_asset_loader.cpp)) | Repositório limpo (sem ROM e sem XEX), mas **o XEX compilado contém assets** gerados da ROM. O usuário precisa compilar, porque não há release binária "limpa" |
| **Endianness** | Nenhum swap: o N64 e o 360 são BE. [`xbox360_memory_codec.c`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_memory_codec.c) decodifica MIO0 "do jeito que está" | **Não se aplica ao PS2** (ver §8) |
| **Ponteiros** | 32 bits no 360 (como no N64). `x360_resolve_address` traduz segmentos e endereços `0x8000xxxx` da ROM | Análogo ao que o BT3 precisa para os endereços fixos do PS2 |
| **Gráficos** | `gfx_pc.c` (Fast3D) com backend D3D9 próprio ([`xbox360_renderer.cpp`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_renderer.cpp)), shaders do combiner compilados **em tempo de execução** com `D3DXCompileShader` ([L66](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_renderer.cpp#L66)), texturas `D3DFMT_LIN_A8R8G8B8` ([L102](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_renderer.cpp#L102)), tela lógica de 1280x720 reescalada para SD | Funciona, mas: compilar shaders no console custa tempo de boot (o log mostra "compiling startup shaders"); texturas lineares são mais lentas que as *tiled* no Xenos |
| **Vídeo/timing** | [`xbox360_video.cpp`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_video.cpp): `XGetVideoMode` (respeita 480i/480p e widescreen), `PresentationInterval ONE` + laço de `Sleep(1)` até o deadline (30 ou 60) | Simples. `Sleep(1)` dá jitter. Melhor esperar só no vblank |
| **Áudio** | `mixer.c` do sm64-port adaptado ao ABI do MK64 (HLE de RSP) → **uma** source voice XAudio2 com buffer circular, alvo de 100 ms e recuperação de underrun ([`xbox360_audio.cpp`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_audio.cpp)) | Padrão correto para um jogo que já mixa por software |
| **Input** | `XInputGetState` mapeado para `OSContPad`, com remapeamento, sensibilidade e "config" por arquivo | Ok |
| **Saves** | EEPROM de 512 B em **dois slots com journaling**: magic, geração e checksum FNV, gravação com `WRITE_THROUGH` e releitura para verificar ([`xbox360_save_input.cpp`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/mk64-master/src/xbox360/xbox360_save_input.cpp)) | Robusto, mas grava em `game:\` (pasta do XEX) e **não usa XContent**: sem save por perfil, sem gerenciamento pelo dashboard |
| **Rede** | System Link/Internet UDP 6464, Xbox LIVE Party para encontrar o host (`XPartySetCustomData`), crossplay com o port do Xbox original | Muito além do necessário; mostra que a integração XAM funciona em RGH |
| **Empacotamento** | Saída `MK64.xex`. **Sem Title ID, sem SPA/XDBF** (nenhuma configuração de XEX no `.vcxproj`). Volta ao dashboard com `XLaunchNewImage(0,0)` | No Aurora aparece sem nome nem ícone. Esquisito |
| **Licença** | Código do port: MIT, e MPL-2.0 adicional ([`SIRDANKZ-CODE-LICENSE.md`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/SIRDANKZ-CODE-LICENSE.md)), que exclui explicitamente o código decompilado e os dados da Nintendo | Bom modelo de redação |

**Configurações do projeto** ([`MK64.vcxproj`](https://github.com/sirdankz/MKart360/blob/a36c9bedba2ee7d91fccc7441bc1044974bb56f6/MK64/MK64.vcxproj)):

- `XBOX360_PORT=1; NON_MATCHING=1; AVOID_UB=1`;
- as seis configurações padrão do XDK (Debug, Profile, Profile_FastCap, Release, Release_LTCG, CodeAnalysis);
- libs: `xapilib, d3d9, d3dx9, xgraphics, xboxkrnl, xnet, xaudio2, xact3, x3daudio, xmcore, xbdm, vcomp, xonline,
  xparty`;
- deploy: `CopyToHardDrive`.

---

## 5. Ferramentas para Xbox 360

| Opção | O que é | Prós | Contras | Uso nos ports estudados |
|---|---|---|---|---|
| **XDK oficial (2.0.21256, VS2010)** | SDK da Microsoft. D3D9 do Xenos, XAudio2, XInput, XAM (perfis, Guide, XContent, device selector), XMedia (WMV/XMV), XUI, `fxc`, `xextool`/`imagexex`, SPA (`spac`) | Integração completa com o dashboard; ferramentas de perfil (PIX, xbWatson); é o que **todos** os ports bons usam | Proprietário, sem distribuição legal. VS2010 = C++03 com algumas partes de C++11 e C89/C90 no C, ou seja, nada de C99 completo em C. É preciso compilar os `.c` do jogo como C++ ou adaptar | VF/VR/Sega Rally (wanszai), MKart360, SM64 360 (inferido) |
| **RXDK-360** ([Team-Resurgent/RXDK360](https://github.com/Team-Resurgent/RXDK360), GPLv3) | Integração do VS2022/2026 com **o XDK do próprio usuário**, com um [clang/lld para 360](https://github.com/Team-Resurgent/llvm-project) e [XexTool](https://github.com/Team-Resurgent/XexTool). Há [samples do XDK portados](https://github.com/Team-Resurgent/RXDK360-Samples) | IDE e compilador modernos (C99/C11, C++17) com as libs do XDK. Não redistribui conteúdo da Microsoft | Muito novo (setembro de 2026). [Notas de hardware](https://github.com/Stu1dd3m0n9/xbox-360-port-enablers/blob/main/docs/HW-NOTES.md) de um usuário relatam `fopen` travando o console, `D3DXCompileShader` travando, varargs > 5 perdidos e helpers de runtime ausentes (**problemas desse caminho clang/picolibc, não do XDK com MSVC**) | Experimentos ([xbox-360-port-enablers](https://github.com/Stu1dd3m0n9/xbox-360-port-enablers)) |
| **libxenon** ([Free60](https://free60.org/Development/LibXenon/)) | Biblioteca livre, bare-metal. Executa como ELF via XeLL | 100% livre | Sem XAM: sem Guide, sem perfis/saves do dashboard, sem voltar ao Aurora de forma limpa. GPU programada "na mão", sem D3D. Ecossistema parado | Emuladores antigos (Mupen64-360 etc.). **Nenhum** port de decomp recente |
| **OpenXeChain / FreeChainXenon** ([SynthXEX](https://git.aidenisik.scot/FreeChainXenon/SynthXEX), AGPL) | LLVM + newlib + gerador de XEX2 livres | Totalmente livre | "Early development, MANY features missing". Só produz um `hello.xex` | Tentativa do SM64toX360 |

**Regras práticas do Xenos/D3D9 do 360** (somando o código do MKart360, as notas de hardware do Stu1dd3m0n9 e
documentação do XDK; itens marcados com † vêm só do caminho RXDK/clang e devem ser confirmados no XDK com MSVC):

- **Compile os shaders offline** (`fxc /Fh` → arrays de DWORD) e evite `D3DXCompileShader` no console (†: trava
  no RXDK; no XDK com MSVC funciona, mas custa boot).
- **Texturas são *tiled*:** suba com `XGTileTextureLevel`, ou use formatos `LIN_` sabendo que são mais lentos.
  Não existem `D3DUSAGE_DYNAMIC`, `StretchRect` nem `GetRenderTargetData` como no PC. Use `Resolve` da EDRAM.
- **EDRAM de 10 MB:** 1280x720 com cor e depth de 32 bits ≈ 7,0 MB cabe sem *tiling*. Com MSAA 2x/4x é preciso
  *predicated tiling*.
- **Nunca retorne de `main()`:** desliga ou reinicia o console (†). Saia com `XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0)`
  ou volte ao launcher.

---

## 6. Tabela comparativa

| | sm64-port | sm64ex / Render96 | SpaghettiKart | MKart360 | SM64 360 (ConfusionRS) | VF 360 (wanszai) | **Tenkaichi3Decomp (BT3 PC)** |
|---|---|---|---|---|---|---|---|
| Natureza | Decomp + HLE | Decomp + HLE | Decomp + libultraship | Decomp + HLE | Decomp (inferido) | **Emulador dedicado** | Decomp *matching* + HLE do GS/VU1 |
| Plataforma | PC | PC | PC/Switch | **360 (XDK)** | 360 | **360 (XDK)** | PC (Linux/Windows) |
| Código | Aberto | Aberto | Aberto | Aberto (MIT/MPL) | Fechado | Fechado | Aberto (MIT na camada port) |
| Assets | No build, embutidos | No build → `res/base.zip` | **1ª execução → `.o2r`** (hash SHA-1) | No build (`.inc.c`) + ROM lida no boot (CRC32) | **ROM embutida no XEX** | ROM do usuário lida no boot | **Instalador:** ISO → `gamedata/`, SHA-1 dos executáveis, tabelas restauradas do disco |
| Distribui binário limpo? | Não | Não (EXTERNAL_DATA ajuda) | **Sim** | Não (o usuário compila) | Não (ilegal) | Sim | **Sim** (`bt3` + `bt3.dat`) |
| Menu de opções | Não | Sim (no jogo) | Sim (ImGui) | Sim (próprio) | ? | **Sim (frontend próprio)** | Sim (F1, ImGui) |
| Saves | Arquivo | Arquivo | Arquivo | `game:\` com journaling | ? | **XContent** | Imagem de memory card |
| Metadados no dashboard | — | — | — | **Não** | ? | **Title ID + XDBF + capa** | — |

---

## 7. Polido vs. esquisito

Lista de checagem extraída dos ports acima, aplicada ao contexto de um homebrew de 360.

**Polido:**

1. **Uma pasta autocontida:** `default.xex` + `ReadMe.txt` + **um** lugar óbvio para o dado do usuário (VF:
   `roms/`).
2. **Identifica a mídia por hash e explica o erro na tela**: "Unsupported ROM" (SpaghettiKart) ou "disc image is
   not the unmodified USA release" (Tenkaichi3Decomp). Nunca tela preta.
3. **Primeira execução guiada**, com barra de progresso e tempo estimado. Nas seguintes vai direto ao jogo
   (marcador `.installed` do Tenkaichi3Decomp).
4. **Integra-se ao console**:
   - Title ID e XDBF com nome e ícone;
   - save em XContent, com device selector na primeira vez;
   - Guide funcionando;
   - pausa automática quando o Guide abre ou o controle desconecta;
   - saída limpa para o dashboard ou launcher;
   - respeito a 480p/720p/1080i e 4:3/16:9 do `XGetVideoMode`.
5. **Abre num menu com cara de produto** (vídeo e música do próprio jogo, sem ImGui de debug para o jogador).
6. **Opções que o jogador espera**: 16:9 sem distorção (com pillarbox onde a tela é 4:3, como no VF), filtro,
   remapeamento, vibração, volume. **Os padrões ficam fiéis ao original**; as melhorias são opt-in.
7. **Timing estável:** a lógica no ritmo original (30 Hz no BT3) e a apresentação no vblank, sem `Sleep(1)`.
8. **Notas de versão curtas** e um release por versão.
9. **Licença clara** para o código do port e exclusão explícita dos dados do jogo.

**Esquisito:**

- ROM ou assets embutidos no binário (SM64 360).
- `return` silencioso para o dashboard (MKart360), ou tela preta sem perfil (VF).
- Saves e logs espalhados na pasta do XEX.
- XEX sem nome e sem ícone no Aurora.
- Compilação de shader no boot.
- Exigir um passo no PC sem explicar.
- Builds separados com recursos divergentes.
- Logs de depuração ligados por padrão.

---

## 8. Transposição para o BT3

### 8.1 Ponto de partida: Tenkaichi3Decomp (não o BT3-Decompiled cru)

O [BT3-Decompiled](https://github.com/z3xox/BT3-Decompiled) (CC0) é **matching**: 99,92% do `SLUS_216.78` e
99,97% do `DBZP.BIN` (programa de menu) em C. Ficam em assembly 4 rotinas de VU0 e os 9 programas de VU1 como
listagens anotadas. O build exige `ee-gcc 2.96`, splat e a ISO do usuário.

O mesmo autor mantém o [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp) (MIT na camada `port/`,
lido em `7043a3e`). É **o equivalente do sm64-port/SpaghettiKart para o BT3**. As decisões dele que
transferimos para o 360:

| Tema | Como o port de PC fez | Arquivo |
|---|---|---|
| Regra de layout | `src/` e `include/` são do decomp. Mudanças só sob `#ifdef PORT`, feitas **no decomp** e verificadas para continuarem *byte-identical*. Tudo do host fica em `port/` | [docs/port/README.md](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/docs/port/README.md) |
| Tipos | `port_compat.h` forçado em todo fonte: `long` do PS2 tem 64 bits (`s64/u64` = `long long`) e os 128 bits ficam em typedef | idem |
| Memória | Heap do jogo **no endereço do PS2** (`0x003BE730..0x02000000`), registradores de HW (`0x10000000`, `0x12000000`) como memória comum, scratchpad em `0x70000000`. No Windows: heap "em qualquer lugar abaixo de 2 GB" | [`plat_mem.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/plat_mem.c) |
| Ponteiros | O jogo guarda ponteiros em campos de 32 bits. O primeiro build foi `-m32`; o build de 64 bits reescreve ponteiros (`ptr32.py`) e linka em endereço baixo | `port/tools/ptr32.py`, `link.py` |
| Float | **Todo o código do jogo compilado com `-msoft-float`**, com cada operação no modelo do PS2 (truncamento, sem denormais, sem inf/NaN) | [`softfloat_ps2.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/softfloat_ps2.c), `softfloat_ps2_inl.h` |
| Arquivos | CRI ADXF sobre pastas extraídas (`gamedata/pzs3us0/00000.bin`…), `gamedata/mods/` sobrepõe arquivos | [`plat_file.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/plat_file.c), [`extract_disc.py`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/tools/extract_disc.py) |
| GS | Frontend que interpreta a cadeia DMA → VIF1 → GIF → registradores do GS. **Memória do GS emulada com o layout real (4 MB, swizzle)**, porque o jogo sobe texturas de 4 bits como imagens de 32 bits e lê framebuffers como textura. Backend Vulkan/OpenGL. Passes "truques de memória do PS2" (shuffle de canais, depth como textura) **descartados e reimplementados como efeitos nativos** (outline, glow, depth tint, blur) | [`gs/gs_core.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/gs/gs_core.c), [`gs_draw.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/gs/gs_draw.c), [`gs_gpu.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/gs/gs_gpu.c) |
| VU1 | Interpretador dos 9 microprogramas originais (referência e fallback) **e** versões em shader dos programas quentes (o programa 0, skinning de dois ossos + cel shading por rampa, virou `vu0.vert`) | [`gs/gs_vu1.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/gs/gs_vu1.c) |
| Som | ADX (streams) e efeitos sonoros reimplementados | [`gs/snd_adx.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/gs/snd_adx.c), [`gs/snd_se.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/gs/snd_se.c) |
| Vídeos | Decodificador MPEG-2 próprio, "sem ffmpeg" | [`plat_mpeg2.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/plat_mpeg2.c) |
| Memory card | Emulado (imagem) | [`plat_mc.c`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/plat_mc.c) |
| **Distribuição sem dados** | `port/data/` tem as tabelas de dados do jogo **só com o formato** (rótulos, tamanhos, slots de ponteiro). No primeiro boot o programa copia os valores do `SLUS_216.78`/`DBZP.BIN` do usuário e confere um checksum (`Port_LoadGameData`) | [`plat_mem.c#L272`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/src/plat_mem.c#L272) |
| **Instalador** | Lê a ISO (ISO 9660) sem 7z, **confere o SHA-1 só dos dois executáveis** (`kRomSha1`, `kDbzpSha1`, rápido, sem hashear 4 GB), divide os AFS em arquivos soltos, grava o marcador `gamedata/.installed` e roda uma luta de demonstração como autoteste | [`port/setup/native.cpp`](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/port/setup/native.cpp#L25) |
| Validação | Replays gravados no PCSX2 e *save states* comparados byte a byte (heap e globais) para provar que a luta é idêntica à do console | `compare_state.py`, `BT3_REPLAY` |

> Existe também o [BT3-Recomp](https://github.com/z3xox/BT3-Recomp) (recompilação estática via PS2Recomp, OpenGL e
> ImGui). Para o 360, **decomp > recomp**: o código recompilado de MIPS para C++ é maior, mais lento e sem tipos.
> Isso pesa muito numa CPU in-order.

### 8.2 PS2 vs. N64: as diferenças que mudam o projeto

| Aspecto | N64 → 360 (SM64/MK64) | PS2 → 360 (BT3) | Consequência |
|---|---|---|---|
| **Endianness** | BE → BE: **nada a fazer**. O MKart360 lê ROM e MIO0 diretamente | **LE → BE.** O código do jogo lê formatos do disco com *casts* de struct (tabela AFS `<4sI`, modelos `.unk`, animações, parâmetros, cadeias DMA/VIF/GIF pré-montadas, texturas, ADX) | O maior trabalho específico do 360. Opções em §8.3 |
| **Ponteiros** | 32 bits | 32 bits (EE) | O 360 com XDK também usa ponteiros de 32 bits. Equivale ao build `-m32` do port de PC, **sem** precisar do `ptr32.py` |
| **Inteiros largos** | Raros | `long` de 64 bits, registradores e `lq/sq` de **128 bits**, `u128` em DMA | MSVC do XDK não tem `__int128`: usar `__vector4`/struct de dois `u64`. `port_compat.h` já isola isso |
| **Float** | IEEE (MIPS VR4300) | **Não IEEE** (sem denormais, inf ou NaN; truncamento) | O PC usa softfloat. No Xenon isso é caro demais (ver §8.4) |
| **GPU** | RSP+RDP: display list de alto nível (F3D) → HLE limpo, triângulos prontos (`gfx_pc.c`) | EE monta pacotes GIF/VIF; **VU1 roda microcódigo próprio do jogo**; o GS é um rasterizador "burro" com memória local de 4 MB e truques de formato | Sem um "Fast3D" pronto. Frontend de GS + HLE por programa VU1 + efeitos nativos |
| **Coprocessador vetorial** | — | VU0 em modo macro (biblioteca vetorial) + 4 rotinas em asm | Mapeia bem para **VMX128** do Xenon (128 registradores vetoriais) |
| **Áudio** | RSP de áudio (ABI) → `mixer.c` | **IOP** (MIPS R3000) + SPU2 + middleware CRI ADX; RPC via SIF | HLE da interface RPC (`sceSif*`), não do IOP. ADX e efeitos já reimplementados no port de PC |
| **Mídia** | Cartucho de 8–64 MB, **cabe todo em RAM** (MKart360 carrega 12 MB) | **DVD com GBs**, streaming de AFS | Leitura assíncrona do HDD/USB. Atenção ao limite de 4 GiB por arquivo em FATX/FAT32 |
| **Overlays** | Segmentos | `DBZP.BIN` carregado em endereço fixo | Linkado estaticamente (como no PC) |
| **Saves** | EEPROM de 512 B | Memory card (sistema de arquivos próprio) | XContent + emulação do mc (`plat_mc.c`) |
| **Vídeos** | — | PSS/MPEG-2 | Decodificar no 360 (MPEG-2 SD é viável numa thread de HW) ou converter |
| **Threads** | 1 CPU | EE + VU1/GS assíncronos + IOP | 360: 3 núcleos e 6 threads. Jogo, render e áudio/IO em núcleos separados |
| **Resolução nativa** | 320x240 | 512x448 entrelaçado | 720p no 360 já é ~4x de pixels. O widescreen sem distorção já existe no port de PC |

### 8.3 Endianness: três caminhos

1. **Swap na carga, por formato.** Cada loader do jogo ganha, sob `#ifdef PORT_BE`, uma função que inverte os
   campos do formato. Exige conhecer todos os formatos (modelos, animações, parâmetros, efeitos, cadeias DMA dos
   modelos). É trabalhoso e um campo esquecido vira bug silencioso, mas mantém o dado do usuário intacto e o
   instalador simples.
2. **Conversão na instalação (recomendado como meta).** O instalador (no console ou numa ferramenta de PC opcional)
   grava `gamedata/` **já em BE e em formatos do 360**: texturas destiladas, *tiled*, com paleta resolvida ou em
   formato de índice+paleta para shader, e vídeos possivelmente transcodificados. O runtime fica simples e rápido.
   Exige o mesmo conhecimento de formatos, mas o custo vai para a instalação.
3. **Leitores endian-aware no runtime** (`__loadwordbytereverse`/`lwbrx` são baratos no PPC). Útil para os campos
   "quentes" lidos pelo frontend do GS e pelo HLE do VU1 (pacotes GIF montados no disco), combinado com 1 ou 2.

**Ponto sutil:** os pacotes GIF/VIF **montados pela CPU** em tempo de execução (via `u64`/`u128`) ficam nativos BE
no 360. Os pacotes **lidos prontos do disco** estão em LE. O frontend do GS precisa saber a origem de cada bloco,
ou o loader precisa normalizar para o mesmo formato. Isso deve ser projetado no início, não descoberto depurando.

### 8.4 CPU: o que não sobrevive à troca de x86 por Xenon

Medições do próprio port de PC ([docs/port/README.md, "Frame time", "Speed", "Where a fight's frame goes"](https://github.com/z3xox/Tenkaichi3Decomp/blob/7043a3e42fb4e73de10ec64257537dd38357fa1a/docs/port/README.md)):

- Num Ryzen 9800X3D, o código do jogo leva ~1,5 ms por frame de luta e **mais da metade disso é
  `Port_FastAdd/FastMul/FastMtxApply` (softfloat)**. O total, com desenho, fica em ~4,3 ms.
- Num i5 de 12ª geração foram 8–10 ms por frame (orçamento de 33,4 ms a 30 Hz).
- Antes do HLE do VU1, o interpretador de VU1 consumia ~45% do frame, mais 9% de UNPACK.
- O hash de página da memória do GS e a escrita de texturas pixel a pixel na memória do GS também aparecem.

O Xenon é PowerPC **in-order**, sensível a *load-hit-store* e a desvios mal previstos. Em código de inteiros
cheio de ramificações (como o softfloat) é várias vezes mais lento que um i5 moderno por thread.
**Inferência, a medir:** com a arquitetura do PC sem mudanças, o 360 não sustenta 30 fps em luta. Por isso:

- **Float de hardware com semântica próxima à do PS2.** A FPU do PPC tem modo de arredondamento para zero
  (`FPSCR[RN]=01`). A VMX tem modo *non-Java* que zera denormais (`VSCR[NJ]=1`). Faltam o *clamp* de inf/NaN
  (como os modos de *clamping* do PCSX2) e as diferenças de `madd`.

  O determinismo bit a bit só é obrigatório para replays e netplay. Para jogar sozinho, a aproximação basta. O
  softfloat pode ficar como build de validação, usando os replays do port de PC como teste de regressão.
- **Todos os programas VU1 relevantes como vertex shaders HLSL** (vs_3_0, compilados com `fxc` offline). O
  interpretador fica só como referência no PC.
- **Texturas decodificadas e *tiled* uma única vez**, cache por conteúdo (o port de PC já usa hash de conteúdo).
  O Xenos não tem texturas paletizadas: a saída é índice L8 + paleta 256x1 resolvida no pixel shader, ou expandir
  para ARGB na conversão.
- **Efeitos de tela em versões nativas.** Xenos tem `Resolve` da EDRAM para textura, inclusive depth. Isso cobre
  outline, glow, depth tint e blur, que o port de PC já tratou como "efeitos nativos".
- **Render thread com cópia própria da lista** (o problema de corrida que o port de PC documentou) num núcleo
  separado, com áudio e IO em outro.

### 8.5 Fluxo de assets e primeira execução no 360 (proposta, "igual ao VF")

Estrutura entregue (zip do release, sem nenhum dado do jogo):

```
BT3/                       ← o usuário copia para Hdd1:\Games\ (ou USB)
  default.xex              ← Title ID próprio 0xFFFFxxxx, XDBF com nome + ícone
  bt3.dat                  ← mapa das tabelas "só formato" (como no Tenkaichi3Decomp)
  media/                   ← arte do frontend feita por nós (não do jogo), fontes
  cover.png, ReadMe.txt, LICENSE*
  disc/                    ← o usuário coloca aqui a ISO OU o conteúdo extraído do DVD
```

Primeira execução, toda dentro do XEX com UI própria:

1. Se `gamedata/.installed` existe, vai direto ao jogo.
2. Senão, procura `disc/*.iso` ou `disc/SLUS_216.78`. Se não encontra, mostra uma tela explicando **exatamente**
   onde colocar o arquivo, com o caminho completo e o botão B para sair.
3. Confere o **SHA-1 do `SLUS_216.78` e do `DBZP.BIN`** (as mesmas constantes do `native.cpp`). Se não bate:
   "Esta não é a versão USA SLUS-21678 sem modificações".
4. Extrai e converte para `gamedata/` (AFS → arquivos, swap e conversão de formatos se adotado o caminho 2), com
   barra de progresso e opção de apagar a ISO no final.
5. Restaura as tabelas de dados do executável do usuário (`Port_LoadGameData`).
6. Device selector para o save (XContent) e então o jogo.

**A verificar:** o tamanho da ISO do BT3 USA. FATX e FAT32 limitam arquivos a 4 GiB−1. Se a ISO passar disso,
o fluxo precisa aceitar o conteúdo extraído do DVD (pastas `DATA/`, `BIN/`) ou uma ferramenta de PC.

Ferramenta de PC **opcional** (mesmo código do instalador, compilado para Windows/Linux) para quem quiser preparar
`gamedata/` no PC e copiar por FTP, mais rápido que converter no 360.

### 8.6 Empacotamento e integração com o dashboard

- **Title ID próprio** (prefixo `0xFFFF`, como wanszai) definido no `xex.xml`/ImageXex.
- **SPA/XDBF** via `.gameconfig` + `spac`, com o nome "Dragon Ball Z Budokai Tenkaichi 3" e um ícone 64x64.
  **Inferência:** um ícone próprio, e não a arte oficial, reduz o risco de marca.
- `XShowDeviceSelectorUI` + `XContentCreate` para saves e configurações. Logs desligados por padrão; quando ligados,
  vão para `game:\logs\`.
- Tratar Guide (`XN_SYS_UI`), desconexão de controle, troca de perfil e remoção de mídia (pausar e mostrar aviso).
- Saída: `XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0)` ou de volta ao launcher. Nunca `return` de `main()`.
- Vídeo: ler `XGetVideoMode`, desenhar em 1280x720 (ou na resolução nativa em SD) e adotar 16:9 sem distorção, com
  pillarbox nas telas 2D 4:3 (como o VF faz na seleção de personagem).

### 8.7 Licença

- BT3-Decompiled: CC0 para os nomes, docs e scripts. **O código do jogo não é licenciável.**
- Tenkaichi3Decomp: MIT na camada `port/`.
- A camada 360 deve ser **MIT** (compatível), em `port/x360/`, com um aviso no estilo do MKart360 excluindo código
  e dados do jogo.
- **Nunca** versionar headers ou libs do XDK. O build referencia a instalação do desenvolvedor, como RXDK-360 e
  MKart360 fazem.

---

## 9. Recomendações concretas

1. **Base:** fork, ou upstream como remote, do **Tenkaichi3Decomp**, e não do BT3-Decompiled cru. Manter a mesma
   disciplina: mudanças no código do jogo só sob `#ifdef PORT` (e `PORT_X360`/`PORT_BE` quando específico), feitas
   no decomp e verificadas como *byte-identical*.
2. **Toolchain:** XDK 2.0.21256 + projeto MSBuild com plataforma "Xbox 360" (como o MKart360), com CMake gerando o
   `.vcxproj` se for útil. Avaliar o RXDK-360 só depois do primeiro boot, porque ele dá C99/C++ moderno, mas é
   novo e tem problemas relatados.

   **Atenção:** o C do decomp é C99/GNU (compilado com gcc no PC) e o MSVC 2010 não aceita C99 completo em `.c`.
   Duas saídas: compilar como C++ (com cuidado com *casts*) ou usar o clang do RXDK-360. Decidir cedo.
3. **Ordem de trabalho**, espelhando o que deu certo no PC:
   1. build *headless* que bota e roda a luta de demonstração no 360, com saída por log;
   2. replay gravado comparado com o PC (estado da luta por tick);
   3. GS com backend D3D9;
   4. HLE dos programas VU1 0, 3/2 e 4 (os que somam ~95%);
   5. áudio XAudio2;
   6. input XInput;
   7. saves em XContent;
   8. instalador no console;
   9. menu de opções;
   10. empacotamento.
4. **Endianness:** abrir um inventário de formatos (já começado nos `docs/` do decomp) e decidir formato por formato
   entre "swap na carga" e "conversão na instalação". Projetar o frontend do GS para distinguir pacotes montados
   pela CPU dos pacotes lidos do disco.
5. **Float:** começar com softfloat (correção primeiro, validada pelo replay), medir no hardware e migrar para FPU
   com `RN=toward zero` + `NJ` + clamps, mantendo o softfloat como build de validação.
6. **UX "igual ao VF":** pasta única, `ReadMe` curto, pasta óbvia para a mídia do usuário, verificação por hash
   com mensagens claras, frontend com cara de produto, Title ID e ícone, XContent, opções de 16:9 e filtro,
   remapeamento, vibração. Padrões fiéis ao original.
7. **Evitar** os erros dos outros ports:
   - assets no XEX;
   - `return` silencioso;
   - saves em `game:\`;
   - `D3DXCompileShader` no boot;
   - XEX sem Title ID;
   - log ligado por padrão;
   - release sem notas.

## 10. Riscos e perguntas em aberto

- **Desempenho no Xenon** é o maior risco técnico. Medir cedo, com o build *headless* rodando o replay, antes de
  investir em UI.
- **Tamanho da ISO** em relação ao limite de 4 GiB do FATX/FAT32: define se o instalador aceita a ISO ou só o
  conteúdo extraído.
- **C99 no MSVC 2010** versus clang do RXDK-360.
- **Memória:** o heap do PS2 (~28 MB) cabe com folga em 512 MB. Mas os endereços fixos (heap em `0x003BE730`,
  registradores em `0x10000000`, scratchpad em `0x70000000`) **provavelmente não podem ser reservados** no espaço
  de endereços de um título do 360 (imagem em `0x82000000`, alocações virtuais em `0x4xxxxxxx`/`0x7xxxxxxx`).
  **Inferência**, a confirmar com `VirtualAlloc` no hardware. O caminho é o do build de Windows do port de PC:
  heap em qualquer endereço e acessos a registradores de HW via macros.
- **Netplay** do port de PC: fora do escopo inicial. Se vier, System Link via XNet, como wanszai e MKart360.
- **Vídeos MPEG-2**: custo de decodificação no Xenon, a medir.
- **Determinismo** entre PC e 360 (crossplay): só com o mesmo modelo de float. Adiar.

---

## 11. Fontes

**Virtua Fighter / wanszai**
- https://github.com/wanszai/Virtua-Fighter---Xbox360 (releases [1.0.0](https://github.com/wanszai/Virtua-Fighter---Xbox360/releases/tag/1.0.0), [1.0.1](https://github.com/wanszai/Virtua-Fighter---Xbox360/releases/tag/1.0.1), [1.0.2](https://github.com/wanszai/Virtua-Fighter---Xbox360/releases/tag/1.0.2))
- https://github.com/wanszai/Virtua-Fighter---XboxOne-Windows (issues #1–#8)
- https://github.com/wanszai/Virtua-Racing---Xbox360 · https://github.com/wanszai/Sega-Rally-Championship-PC-Xbox360-Series-X- · https://github.com/wanszai/Ridge-Racer-Collection · https://github.com/wanszai/Ace-Driver-Collection-X360-Releases
- https://www.1emulation.com/forums/topic/37933-virtua-fighter-for-xbox-360-new/
- https://gamesdb.launchbox-app.com/developers/games/68818-wans-zai
- Outros candidatos: https://github.com/celsowm/vf2-decomp · https://github.com/biggestsonicfan/vf2-decomp · https://github.com/Thorhax/VF2-NX-Modern · https://github.com/sp00nznet/vf5 · https://github.com/sp00nznet/model2recomp · https://github.com/wacastel/virtua-fighter-engine

**Super Mario 64**
- https://github.com/sm64-port/sm64-port · https://github.com/sm64pc/sm64ex · https://github.com/Render96/Render96ex · https://github.com/cdeletre/R96ex-restool
- https://github.com/fgsfdsfgs/sm64-port/tree/xbox · https://github.com/jtryba/sm64-hybrid-xbox · https://github.com/mborgerson/xsm64
- SM64 360 (ConfusionRS): https://www.reddit.com/r/360hacks/comments/1ud68at/ported_sm64_to_the_xbox_360/ · https://www.1emulation.com/forums/topic/37931-mario-64-port-for-xbox-360-new/ · https://www.elotrolado.net/hilo_super-mario-64-port_2534038 · https://www.youtube.com/watch?v=uEr3umV7AD4
- https://github.com/PedroMarioaBros/SM64toX360 · https://github.com/PedroMarioaBros/OpenXeChain-X360-Builder

**Mario Kart 64**
- https://github.com/HarbourMasters/SpaghettiKart · https://github.com/Kenix3/libultraship · https://github.com/HarbourMasters/Torch · https://github.com/dhakaaas1/spaghettikart-xbox
- https://github.com/sirdankz/MKart360 · https://github.com/sirdankz/Mario-Kart-64-X-Netplay

**Ferramentas para 360**
- https://github.com/Team-Resurgent/RXDK360 · https://github.com/Team-Resurgent/RXDK360-Samples · https://github.com/Stu1dd3m0n9/xbox-360-port-enablers (docs/HW-NOTES.md)
- https://free60.org/Development/LibXenon/ · https://git.aidenisik.scot/FreeChainXenon/SynthXEX
- https://github.com/frankischilling/libSDLx360

**BT3**
- https://github.com/z3xox/BT3-Decompiled · https://github.com/z3xox/Tenkaichi3Decomp · https://github.com/z3xox/BT3-Recomp
