# Plano do port BT3 para Xbox 360

Consolidação dos três estudos em `docs/x360/pesquisa/`:
- `01-collectionui-hiddriver.md`: o que já sabemos fazer no 360 (toolchain, deploy, depuração).
- `02-ports-referencia.md`: SM64, MK64 (MKart360), Virtua Fighter (wanszai) e ports de PC.
- `03-bt3-ps2-x360.md`: decompilação do BT3, port de PC existente, PS2 vs 360.

## 1. Ponto de partida

- **Base de código:** `z3xox/Tenkaichi3Decomp`, o port nativo para PC do mesmo autor da decomp.
  - Já roda o jogo inteiro: boot, menus, lutas, áudio, filmes, saves.
  - Tem teste de replay que reproduz bit a bit uma luta gravada no PCSX2.
  - A camada `port/` é MIT.
- **O que o 360 vira:** um terceiro backend dessa camada de plataforma. Não é um port do zero.
- **"Igual ao Virtua Fighter":** o VF do wanszai é um emulador MAME dedicado, binário fechado. O que copiamos dele é a experiência do jogador, não a arquitetura:
  - uma pasta com `default.xex`, Title ID e ícone próprios, aparecendo bonito no Aurora/FreeStyle;
  - o usuário coloca a mídia dele numa pasta óbvia;
  - save pelo sistema do console, 720p, 16:9 opcional, remapeamento de botões.
- **Referência de código na plataforma:** MKart360 (MIT/MPL, XDK com D3D9 + XAudio2 + XInput). Copiar o que ele faz bem e evitar o que ele faz esquisito:
  - sai em silêncio sem a ROM;
  - compila shaders no boot;
  - não tem Title ID;
  - grava save na pasta do XEX.

## 2. Arquitetura

| Camada | Decisão |
|---|---|
| Toolchain | XDK 2.0.21256 na imagem `collectionui-xdk:light` (wine, Linux). Cadeia `cl` → `link -XEX:NO` → `imagexex`, no modelo do `tools/build.sh` do hiddriver. |
| Linguagem | **Decisão aberta (spike F0):** o `cl.exe` do XDK é C89 em modo C e a decomp é C99/GCC. Candidatos: (a) compilar como C++ `-TP`, (b) compilador 16.10 do TechPreview, (c) clang do RXDK-360. |
| Endianness | Código nativo big-endian. Troca de bytes na fronteira dos dados, com swappers gerados por ferramenta a partir das structs da decomp. Validação diferencial: build headless em qemu-ppc comparada tick a tick com a build x86. |
| Gráficos | Backend D3D9/Xenos do frontend GS do port de PC. VU1 quentes viram vertex shaders HLSL compilados offline (`fxc`), nunca no boot. Texturas no formato do Xenos. Resolução interna 2x cabe na EDRAM. |
| Áudio | Mixer do port de PC alimentando XAudio2. Nada de I/O nas threads de hardware 4/5. |
| Input | XInput, com remapeamento nas opções. |
| Float | A decidir com os números da F0 (decisão 4). Candidato: FPU de hardware com soma e multiplicação truncadas e divisão ao mais próximo, como o PS2 sob o PCSX2 (`softfloat_ps2.c`). Validar com `port/tests/ps2float`. O softfloat do PC pode ser caro demais para o Xenon. |
| Saves | Sistema de saves do console (XContent, por perfil, seletor de dispositivo). |
| Depuração | Diário em arquivo, `SetUnhandledExceptionFilter` com `-MAP`, cor de tela por fase (reaproveitados do CollectionUI). |

## 3. Assets sem ROM

- **O repositório e o release não levam nenhum byte do jogo.**
- **Versão aceita:** só a americana SLUS-21678. O SHA-1 do `SLUS_216.78` e do `DBZP.BIN` é conferido no primeiro boot.
- **Hoje (decisão 18):** o usuário copia a própria ISO para a pasta do jogo, e o jogo a lê direto, convertendo cada
  arquivo na carga. Sem instalador.
- **Passo futuro (decisão 23, F5 depois do teto de memória):** um script de PC converte a ISO do usuário numa ISO
  do mesmo tamanho, e o jogo exige essa ISO convertida (a original é recusada com mensagem clara). Os conteúdos
  comprimidos dos menus continuam convertidos no console até uma etapa posterior (precisa de um compressor BPE).

## 4. Layout do repositório

O `bt3xbx` é um **fork** do Tenkaichi3Decomp (remote `upstream`), não um submódulo. O código do jogo vai precisar de `#ifdef` para o 360 (endianness), e com fork isso é só mais um commit; trazer novidades do upstream é um `git merge upstream/main` feito de propósito (decisão 1 em `decisoes.md`).

```
src/, include/       código do jogo (upstream); mudanças para host big-endian em #ifdef PORT_BIG_ENDIAN ou nas macros PS2_LOW/PS2_LD64 (include/types.h)
port/                camada de plataforma de PC (upstream)
port/x360/           nosso backend: main, gfx, áudio, input, fs, save, instalador
port/x360/tools/     build.sh, subir.py (FTP), gerador de swappers
port/x360/toolchain/ Dockerfile (wine fixado) e notas da imagem
port/x360/package/   xex.xml, ícone, metadados do Aurora
docs/x360/           decisoes.md numerado, pesquisa/, este plano
```

Mudanças que servem ao upstream (correções de endianness, ganchos de plataforma) podem virar PR para lá.

## 5. Roadmap

| Fase | Entrega verificável |
|---|---|
| F0 | Spike do compilador com arquivos reais da decomp + hello `.xex` com Title ID e ícone aparecendo no Aurora + medição de memória e reserva de endereços fixos do PS2 num título XDK. |
| F1 | Build headless big-endian no qemu-ppc reproduzindo o replay igual à build x86. |
| F2 | O mesmo replay headless rodando no 360, com tempo de frame medido. |
| F3 | Dados direto da ISO (decisão 18) + boot até a tela de título, com imagem. |
| F4 | Menus e save do console. |
| F5 | Uma luta a 30 fps. |
| F6 | Áudio (ADX e efeitos) e filmes. |
| F7 | Conteúdo completo e split-screen. |
| F8 | Acabamento (opções, 16:9, remapeamento) e release. |
| F9 | Depois do jogo base rodando bem: tela dividida para 4 jogadores, a partir do mod Tag Team (decisão 16; sem o online). |

### F3 em etapas (12/10/2026)

Do estudo do caminho do boot: `main`, logos (arquivo de menu 0, `pzs3us1/00473`), checagem do cartão (`00477`),
filme de abertura (pulável com `BT3_NOMOVIE`), arquivo de menu 1 (`00449`) e `Title_Run`, que vai sozinho para a
demo depois de 1.800 quadros parado. Os três arquivos de menu não têm conversor: pacote de seções em BPE, e depois
da descompressão texturas (`TexFile`, já especificado), Flash ("FOD\x11LIT") e diálogo com textos.

| # | Onde | O que | Critério |
|---|---|---|---|
| 1 | console | Dados direto da ISO (`iso360.c`) | a luta da F2 roda sem `gamedata` |
| 2 | WSL | Menus sem janela em x86 e PPC, sem `BT3_DEMO`, mais o rastreio desse caminho | execução igual até a demo começar pelo título; catálogo dos arquivos e larguras lidos |
| 3 | WSL | `formatos/menus.md` e os conversores dos menus, com o gancho depois do `Sprite_Unpack` | `conv_tool --disco` sem erro, `--mapa` sem divergência, PPC sem mapa igual à x86 |
| 4 | WSL | Frontend do GS em big-endian: a convenção de 64 bits nos pacotes da GPU (decisão a registrar), a memória do GS, o rasterizador de software de referência | imagens (PPM) idênticas entre x86 e PPC nos logos, no cartão e no título |
| 5 | console | A build sem janela pelos menus até a demo | log com a sequência de telas e o tempo por quadro nos menus (alvo 16,7 ms) |
| 6 | console | Esqueleto D3D9: device 720p, um shader do `fxc` sob wine, padrão de teste | cores, geometria e meio pixel certos; a Guide abre e fecha sem congelar |
| 7 | console | `GsBackend` D3D9 mínimo (desenho 2D, texturas, um alvo, present) | logos e título na tela, batendo com o PPM da etapa 4 |
| 8 | console | Entrada por XInput | START abre o menu do título; desconectar mostra o aviso |

Etapa 8 concluída em 13/10/2026 (decisão 21): o controle pelo XInput levou o jogo do título até o New Game e a
criação do save. O menu principal cai em seguida (`Res_RelocateOffsets` de `MainMenu_Init`): é um pacote de recursos
sem o conversor, uma das chamadas de `Sprite_Unpack` fora do caminho do título, que ficaram para a F4. **A F3 está
concluída.**

Etapa 7 concluída em 13/10/2026 (decisão 20): cartão, logos e título no console batendo com o rasterizador, e os
menus em 16,2 ms por quadro. Os testes no console param antes da luta (`BT3_ATE=4400`): ela esgota a memória
(primeiro item da F5, abaixo).

Filme e som ficam para a F6; VU1, outline e o resto da luta para a F5. Riscos anotados para a F5:

- **A luta esgota a memória com o D3D9** (13/10/2026): as texturas chegam ao teto de 128 MB e os alvos a 24 (cerca de
  96 MB); sobram 3 MB, a criação de alvos falha, a thread do jogo para de 8 a 80 s seguidos, e a Guide não abre (o
  console trava). É o primeiro item da F5: um teto de memória no backend, com descarte do que está há mais tempo sem
  uso, e só depois a causa das paradas longas.
- **ISO convertida no PC** (decisão 23): logo depois do teto de memória, o script de PC que gera a ISO convertida, e
  o jogo passa a exigi-la.
- A gravação e a decodificação de texturas do `gs_draw.c` custam 2 a 3 ms no PC e podem passar do que sobra do quadro
  no Xenon.

Fora do escopo: netplay, versão PAL/japonesa, 60 fps (interpolação ou lógica a 60; decisão 15: 720p a 30).

### F4 em etapas (13/10/2026)

Escopo e decisões na decisão 22. A queda do menu principal (`MainMenu_Init`, `src/menu/menu_a.c`) é a primeira das
77 chamadas de `Sprite_Unpack` sem conversor (`formatos/menus.md` seção 11; a contagem antiga dizia 76). Para não ir caindo uma por uma no console,
quatro redes de cobertura:

- o código: verificador que falha em `Sprite_Unpack` sem gancho;
- o disco inteiro: o "sem conversor" do `conv_tool --disco` por faixa de id;
- roteiros de controle no WSL, com rastreio;
- uma guarda no jogo, que nomeia o chamador sem conversor em vez de cair num endereço inválido.

| # | Onde | O que | Critério |
|---|---|---|---|
| 1 | WSL | Inventário das 77 chamadas (id, seção, formato, leitores), censo do `--disco` por faixa, verificador `unpacks.py`, uma tabela de formatos só em `conv.c` | toda chamada com formato proposto; `unpacks.py` lista as pendentes |
| 2 | WSL | Guarda em `sprite.c` (`PORT_BIG_ENDIAN`); Dragon Net Battle escondido no 360 e nos roteiros | a PPC sem mapa para com "sem conversor em MainMenu_Init"; a `.text` da x86 inalterada |
| 3 | console | Save em `game:\saves` robusto: gravação em temporário e troca de nome, `fclose` conferido | salvar, sair e abrir de novo: o título tem Continue e a opção volta como estava |
| 4 | WSL | Roteiros de controle em texto, ancorados na troca de tela; desbloqueio de tudo no headless; um roteiro por modo | cada roteiro repete na x86 as telas de referência; o "sem conversor" do `--mapa` é igual ao inventário |
| 5 | WSL | Especificações e conversores do caminho essencial: menu principal, submenu do Duelo (`DuelMenu`), Duelo (personagem, time, estágio), Treino, Opções com o save delas | `--disco` 100% nessas faixas; `--mapa` sem divergência; PPC sem mapa igual à x86 em cada roteiro |
| 6 | WSL | Rasterizador x86 e PPC nas telas essenciais | PPM idênticos |
| 7 | console | Caminho essencial no console, pelos roteiros (`BT3_ROTEIRO`) | mesmas telas da x86; capturas no patamar da F3; nenhuma queda |
| 8 | WSL e console | Save entre builds (PPC e 360) | os dois sentidos carregam; save de PC cai no fluxo de "dados danificados" do jogo, sem queda |
| 9 | WSL | Os outros modos pela mesma linha de produção | `unpacks.py` sem pendência; `--disco` 100% em todas as faixas de menu |
| 10 | console | Sessão de 30 min com o Lucas pelos modos | o critério da F4 sem queda |

Etapas 1 e 2 concluídas em 13/10/2026. O inventário está em `formatos/menus-modos.md`: 82 chamadas, 5 com gancho, 77
pendentes (16 no caminho essencial), 33 formatos novos (6 no essencial: `menu_texfile`, `menu_principal`,
`menu_duelo`, `menu_selecao`, `menu_treino`, `menu_opcoes`). Arquivo e seção para formato estão numa tabela só
(`sFaixas`, `port/x360/conv/conv.c`), e o `--disco` termina com o censo dos menus por faixa (seção 6 de lá). O
`unpacks.py` lista as pendentes; a guarda faz a PPC sem mapa parar com "Sprite_Unpack sem conversor em
MainMenu_Init" (`port/x360/tools/guarda_menu.sh`), com a `.text` da x86 inalterada. O Dragon Net Battle sai do menu
no 360 (padrão do `ambiente.txt`) e nos roteiros (`BT3_MENU_ITEM4=0`). O Lucas respondeu às duas perguntas do
inventário (`menus-modos.md` 5, decisão 22): o "submenu dos modos" do essencial é o `DuelMenu` (o `ModeMenu` só serve
à História e vai para a etapa 9), e o save essencial é o das Opções, pelo `McFlow` (o DcSave, do Data Center e do
Evolution Z, vai para a etapa 9).

Etapa 3 concluída em 13/10/2026 (a metade do critério que o console alcança; a outra metade depois da etapa 7). A camada do cartão (`port/src/plat_mc.c`) gravava
direto no arquivo do save, a cada `sceMcWrite`, sem conferir o `fclose`. Nas builds big-endian (`PORT_MC_ATOMIC`)
o arquivo aberto para gravação agora fica em memória e, no fechamento, vai inteiro para `NOME.tmp`, que vira
`NOME.new` (o ponto de compromisso: só existe completo); então `NOME` é apagado e `NOME.new` toma o lugar dele
(decisão 22). Numa queda de energia:

- durante o `.tmp`, ou antes de ele virar `.new`: o próximo uso apaga o `.tmp` e vale o save anterior (numa primeira
  gravação, nenhum, como no PS2);
- com o `.new` pronto, antes ou depois de apagar o anterior: o próximo uso põe o `.new` no lugar, e vale o save novo.

Falha de gravação vira o erro do cartão que o jogo já mostra, e cada gravação e leitura deixa uma linha `mc:` no
`bt3xbx.log`. No WSL (`port/x360/tools/save_mc.sh`): os 24 casos do `teste_mc` na x86 e na PPC; o jogo na PPC sem mapa
cria o save pelo New Game e o carrega no boot seguinte com a mesma sequência de estados do McFlow da x86, também com
um `.tmp` truncado ao lado ou só com o `.new` de uma queda; com a escrita falhando, mostra o erro e segue sem queda.
A `.text` da x86 não mudou; o 360 compila com os mesmos 103 stubs.

No console (build `668b2e18`), com o Lucas: o save da F3, em `saves`, carregou no boot (linha "mc: leu ... 16384 de
16384 bytes") e o título ofereceu Continue. Com `BT3_SAVES=game:\saves2` (cartão vazio), o New Game criou o save
(as três linhas "mc: gravou ... ok"), o menu principal parou na guarda ("sem conversor em MainMenu_Init", tela
vermelha), e no boot seguinte o save carregou e o título ofereceu Continue. Na pasta ficaram só os três arquivos,
sem `.tmp` nem `.new`: o `DeleteFile` e o `MoveFile` da CRT funcionam no FATX.

No console, enquanto o menu principal cair (etapa 5), o critério possível é a metade dele: o New Game cria o save
(três linhas "mc: gravou ... ok": `icon.sys` 964 bytes, `dbzsm.ico` 40.090, o arquivo de dados 16.384), e depois de
sair e abrir de novo o boot o carrega (linha "mc: leu ... 16384 de 16384 bytes", sem o aviso de dados danificados) e
o título oferece Continue. "A opção volta como estava" fica para depois da etapa 7, quando as Opções forem alcançáveis
no console.

Etapa 4 concluída em 13/10/2026 (WSL). O controle sai de roteiros em texto (`BT3_ROTEIRO`, `port/x360/roteiros`),
lidos por `port/x360/headless/roteiro.c` nas três builds (x86 e PPC pelo `sem_janela.c`, 360 pelo `pad360.c`, que
compila com os mesmos 103 stubs). Cada espera é uma condição sobre o estado do jogo: a tela (`gProgress->mode`, ou
`inicio`, `carga` e `luta`), o estado do McFlow, a luta; o log guarda cada troca com o vblank, e um passo que não se
cumpre para a rodada com o arquivo e a linha. O formato está no guia (seção 2, "Roteiros de controle"). O desbloqueio
é o passo `desbloqueia` (o `Save_UnlockAll` do próprio jogo), só com `BT3_DESBLOQUEIO=1`, no `boot.txt` quando o fluxo
do cartão do New Game termina, antes do `MainMenu_Init`; o Dragon Net Battle segue escondido (`BT3_MENU_ITEM4=0`).

Roteiros (todos do boot com o cartão vazio: `boot.txt` cria o save e desbloqueia):

| Roteiro | Alcança |
| --- | --- |
| `duelo` | `DuelMenu` (38), `CharSel` (39) com retratos e estágios, luta simples |
| `duelo_time` | `DuelMenu`, `TeamSel` (40) com dois membros e o COM, luta em time |
| `treino` | `Train` (44) com seis páginas de uma aula, treino livre, `CharSel` do treino (45), luta |
| `opcoes` | `Option` (62): som em mono, salvar, estéreo, carregar (o mono volta), sair |
| `historia` | `HistSel` (6), `ModeMenu` (7) com a imagem do episódio, luta do primeiro episódio |
| `ultimate` | `UbMenu` (13), Missão (14, 15 `UbTeamSel`), Sobrevivência (17), Disc Fusion (24), Sim Dragon (20, 21 `SoloSel`, 22 tabuleiro) |
| `torneio` | `TourMenu` (33), `EntrySel` (34), chave (35), primeira luta |
| `evolution` | `EvoTop` (48), `EvoZ` (49), loja (50) |
| `datacenter` | `DcMenu` (53), senha (54), lista (55) |
| `referencia` | `CharRef` (60) com os detalhes de um personagem |
| `shenlong` | `Shen` (70) até a lista de desejos |

O ciclo é `port/x360/tools/roteiros.sh` (e `roteiros.py`). Resultado do critério:

- **Repetição na x86:** os 11 roteiros repetem o rastro (trocas de tela, esperas, vblanks) entre duas rodadas e contra a
  referência guardada no WSL (`~/referencias`, fora do repositório); a x86r do rastreio faz o mesmo rastro.
- **"Sem conversor" do `--mapa` contra o inventário:** em cada roteiro, o conteúdo que o `conv_tool --mapa` dá como sem
  conversor (por arquivo e seção, sobre o rastreio da mesma rodada) é exatamente o das chamadas pendentes que a rodada
  fez, e o das 5 com gancho sai convertido. Juntos, os 4 roteiros do essencial alcançam as 16 pendentes dele e as 5
  com gancho; os 7 dos outros modos alcançam 39 das 61 pendentes deles. Uma nota explicada: no `datacenter` o
  `DcList_Init` (menu_y_b.c:1008) descomprime um retrato que ninguém lê como número (o `DcList_UpdateFace` descomprime
  de novo antes de ler, `menus-modos.md` 3.2), então ele não aparece no rastreio; nada a converter ali.
- **PPC com o mapa** (`PPC=1`, guarda só anotando): repete o rastro da x86 do boot até o menu principal e nomeia
  `MainMenu_Init`, e para no `Flash_Create` dele: o mapa por posição não inverte os registros do Flash (conflitos
  "[4, 8]"). Até o conversor do Flash (etapa 5), a PPC confere só até o primeiro menu sem conversor.

As 22 pendentes que nenhum roteiro alcança (todas da etapa 9), cada uma por uma condição do jogo:

- pedem uma luta terminada: `HistOutro_Init` (2), `HistResult_Init` (3), `HistSave_Init`, `MisResult_Init`,
  `SurvResult_Init`, `SimResult_Init`, `UbResult_Init`, `Bracket_LoadImages` (2, a chave depois de uma luta) e
  `SimDay_UpdateFaceLoad` (2, o adversário de um dia do Sim Dragon);
- pedem os ramos do Disc Fusion, cujo X abre um diálogo do cartão: `UbzSel_Init`, `UbRank_Init`,
  `UbRank_UpdateFaceLoad` (2);
- pedem algo que o desbloqueio não dá ou tira: `ShenSave_Init` (um desejo concedido; com tudo desbloqueado o dragão
  recusa todos), `DcList_UpdateFace` (um registro; o `Save_UnlockAll` esvazia a lista), `DcPass_UpdateFace` (uma senha
  válida);
- `ReplayMenu_Init`: a tela de replay cai nas builds sem janela (abaixo).

Riscos para a etapa 5: o mapa por posição não passa dos menus, então a conferência "PPC sem mapa igual à x86 em cada
roteiro" só existe com os conversores (um conversor que falte para a PPC no primeiro menu dele, e a guarda o nomeia);
esperas por quadros dentro de uma tela (o `CharSel`, o diálogo do cartão que só aceita entrada 40 quadros depois)
valem enquanto o jogo andar igual por quadro, o que no console depende de as cargas e as falas não o atrasarem (o
rastro mostra onde); e a tela de replay cai nas builds sem janela (divisão por zero em `menu_za_d.c:409`; causa provável, o `pow` de
double da libm de float em hardware chamado do código de float em software), o que a etapa 9 tem de resolver antes de
usar o `ReplayMenu`.

Critério da F4: no console, com o controle, do boot ao começo da luta pelo Duelo e pelo Treino, Opções com salvar e
carregar, e os outros modos navegáveis; a sequência de telas igual à x86. Antes da etapa 5, um revisor independente
(guia, seção 4).

Revisão da etapa 4 (13/10/2026), corrigida antes da etapa 5. Os verificadores agora decidem e cada um foi visto
falhando com uma entrada estragada de propósito (guia, seção 4): o `roteiros.sh` sai com 1 em qualquer conferência que
falhe (antes saía com o código da soma); o `roteiros.py confere` trata como erro a linha de erro do `conv_tool` e o
"contra o mapa" com outra largura ou não trocadas, e acusa todo "sem conversor" fora de `disc/` e das faixas de menu,
menos a lista explícita `CONHECIDOS`; o `conv_tool` confere que as faixas da tabela única não se sobrepõem; o
`build_f2.sh` confere que toda chamada de `Sprite_Unpack` no `.exe` cai numa função da tabela da guarda
(`chamadores_bl.py`); a guarda também percorre os blocos do contêiner antes de descomprimir. O roteiro das Opções
confere o som do save (`confere som N`: mono escolhido, estéreo de volta, mono depois do carregar); o `desbloqueia`
recusa um cartão que já tinha save no começo da rodada (proteção do save real para a etapa 7); o `repete` aninhado
confere a condição de cada nível. O save atômico devolve erro quando o arquivo antigo não pôde ser apagado e o `.new`
não tomou o lugar dele (`BT3_MC_FALHA=apaga` no `teste_mc`).

Pendências da etapa 9 que o `confere` corrigido revelou (tabela `CONHECIDOS` do `roteiros.py`): arquivos fora das
faixas de menu lidos como número sem conversor, os dois atores da cena do Shenlong (03388 e 03397, ids 0xD3D e
0xD46, `ShenScene`) e três da luta do primeiro episódio da História (00510, 00560 e 00710, ids 0x1FF, 0x231 e 0x2C7;
quem os lê e o formato, a levantar).

**Etapa 5: o critério "PPC sem mapa igual à x86 em cada roteiro"**, mais forte do que telas e tempo. Em cada roteiro do
essencial, a PPC sem mapa (só os conversores) e a x86 têm de dar:

- o mesmo rastro (telas, estados do McFlow, esperas cumpridas e conferências, com o vblank);
- a mesma sequência de arquivos lidos (as linhas `LEU` do `dprintf` em `Port_FileReadAt`, na ordem);
- o mesmo `BT3_HASH` da luta (o roteiro que chega à luta);
- o `memdiff --marcas` no passo `fim` de cada roteiro sem diferença fora do que as marcas explicam.

Antes do critério, o `roteiros.sh` precisa de um modo "PPC sem mapa" (os arquivos convertidos pelo `conv_tool --saida`,
sem `BT3_MAPA`, com a guarda fatal) que faça essas quatro comparações e entre na lista de falhas; ele ainda não existe
(hoje só há a PPC com o mapa e a guarda anotando, que para no Flash).

Ordem de trabalho da etapa 5:

1. as peças comuns, especificadas primeiro (são usadas por quase todas as telas): `JanelaMsg`, `JanelaIcone`,
   `LipPack`, `ListaIds`, `Células`, `SubpacoteTex`, `ItemPanel`, `ItemHelp`;
2. o `menu_principal` com o Flash (o primeiro bloqueio da PPC: os registros de 8 bytes lidos sobre campos de 4);
3. as entradas do `formato_de` que o essencial pede: 448, 453 e 461 como `menu_pacote`; retratos, estágios e as
   páginas do Treino como `menu_bpe`; com o `menu_texfile`;
4. duelo, seleção, treino e opções, um por vez, cada um com o critério acima antes do seguinte.

As faixas por hipótese (retratos 921..923, estágios 0x24 e 0x25, o fim das páginas do Treino) só entram no
`formato_de` depois de conferir pelos `lidos.txt` de todos os roteiros que nenhum outro código lê esses arquivos.

**Etapa 5, parte 5a (13/10/2026, WSL): o modo PPC sem mapa, as peças comuns e o menu principal.** Feito:

- **Modo PPC sem mapa** (`roteiros.sh` com `SEMMAPA=1`, `roteiros.py semmapa`): a PPC só com os conversores, como o
  360 (os arquivos pelo `conv_tool --saida`, o conteúdo pelos ganchos, sem `BT3_MAPA`, guarda fatal), com as leituras
  de arquivo (`BT3_LEU=1`). Ela tem de chegar ao fim do roteiro com o rastro, as linhas `LEU` e a luta (`BT3_HASH`)
  iguais às da x86, ou parar na guarda, que nomeia o próximo bloqueio, com o rastro e as leituras até ali iguais ao
  começo dos da x86; outro fim é falha. A memória das duas builds no último vblank da primeira visita ao menu principal
  (`memdiff.py --marcas-tam`): o heap no lugar, e o arquivo 00449 e o pacote do menu conformes número a número. O
  quarto item do critério (a memória no passo `fim`) ainda não existe: um roteiro que chegue ao fim falha com
  "FALTA: memdiff no passo fim" até a 5b ligá-lo. Visto falhando com saídas estragadas numa cópia (vblank, leitura,
  guarda apagada, rastro mais longo, função fora da lista, erro de conversor, luta diferente, número do pacote na
  ordem errada no dump).
- **As peças comuns** especificadas do código em `formatos/menus-pecas.md` e implementadas em `port/x360/conv/
  menus_pecas.c`: JanelaMsg, JanelaIcone, LipPack (legendas com a boca, contagem s32), ItemPanel, ItemHelp, ListaIds
  (com os limites de estágios, músicas e Opções), Células e SubpacoteTex (fichas de 165, ícones de 38); o Flash, os
  textos, o diálogo e a tabela de seções de `menus.c` exportados. `conv_tool --pecas` aplica cada peça só à seção
  dela em todo pacote do disco que a usa: 75 usos sem erro (também os dos outros modos), e contra o mapa dos onze
  roteiros nenhuma posição divergente. O disco decidiu as hipóteses (N exatos; a seção 5 do ItemPanel é filme; a 4 do
  ItemHelp não é peça conhecida e fica em bytes, sem leitor).
- **O menu principal** (`menu_principal`, 449 seção 2, as 28 seções) com o gancho no `MainMenu_Init`; 448, 453 e 461
  como `menu_pacote` no `sFaixas`, para a guarda nomear a tela seguinte. O Flash dele, que parava a PPC com o mapa,
  passa pelo conversor do Flash (os registros de 8 bytes lidos sobre campos de 4 são o u32 do cabeçalho de registro;
  `menus-pecas.md` 6.1).

Números: `unpacks.py` com 76 pendentes (eram 77; 15 do essencial); `--disco` com 2.386 arquivos e 8 conteúdos sem
erro, censo dos menus com 6 de 30 arquivos e 5 de 49 contêineres com conversor; `--mapa` do menu principal sem
divergência nos onze roteiros (12.099 a 16.251 posições por roteiro); a `.text` da x86 com o mesmo sha1 antes e
depois (`02fd5df74264e3f467da6d4b1f735d3bdbc7b47f`).

Resultado por roteiro (rodada inteira em `a11fbc2`: x86 duas vezes, gdb, x86r com o rastreio, `--mapa`, inventário;
e a PPC sem mapa de novo com as builds x86 e PPC de `a6ed060`, `REUSA=x86`): a x86 repete a referência em todos, o
"sem conversor" do `--mapa` é o do inventário, e a PPC sem mapa passa do menu principal em todos:

| Roteiro | Próximo bloqueio (guarda) | Rastro igual ao da x86 | Leituras (`LEU`) iguais | Memória no menu principal |
| --- | --- | --- | --- | --- |
| `duelo` | `DuelMenu_Init` (448 s1), na tela 38, vblank 3000 | 31 de 37 linhas | 15 de 40 | 133 blocos no lugar; 00449 e o pacote conformes |
| `duelo_time` | `DuelMenu_Init`, tela 38, vblank 3000 | 31 de 37 | 15 de 45 | idem |
| `treino` | `Train_Init` (461 s1), tela 44, vblank 3024 | 31 de 37 | 15 de 45 | idem |
| `opcoes` | `Option_Init` (453 s1), tela 62, vblank 2988 | 31 de 63 | 15 de 17 | idem |

O que a 5b precisa saber:

- A ordem continua a do item 4 da lista acima: `menu_duelo` (448 s1), `menu_selecao` (448 s2 e 461 s2), `menu_treino`
  (461 s1), `menu_opcoes` (453 s1), mais `menu_texfile` para retratos, estágios e páginas do Treino (as faixas no
  `formato_de` só depois de conferir os `lidos.txt`). As peças já estão prontas e validadas no disco: cada conversor
  de tela é a tabela de seções chamando-as (`menus-pecas.md` 1 e 5.6 dizem qual seção é qual peça).
- Rode `SEMMAPA=1` sem `PPC=1`: a PPC com o mapa trava no Flash da tela seguinte ao menu e gasta o `TP` inteiro (uma
  hora por roteiro). `REUSA=x86` serve depois de mudar só conversores e ganchos, com a `.text` da x86 conferida.
- Quando um roteiro chegar ao fim na PPC, falta ligar a memória no passo `fim` (o `semmapa` falha até lá): o mesmo
  `BT3_HASH_AT` com o vblank da linha `fim`, e as marcas de cada arquivo e conteúdo vivo nesse quadro.
- O `--mapa` do conteúdo dessas telas tem posições de rótulo do Flash que o rastreio dá como número de 4 bytes; a
  `conv_tool` as conta à parte ("cadeia de texto lida por inteiro").
- O jogo grava nas listas de ids depois da conversão (desbloqueios): a comparação de memória dessas telas tem de
  contar com isso.

**Etapa 5, parte 5b (13/10/2026, WSL): o resto do caminho essencial e a memória no passo `fim`. Etapa 5 concluída.**
Feito:

- **Especificação das telas** (`formatos/menus-telas.md`) e os conversores em `port/x360/conv/menus.c`, cada um a tabela
  de seções chamando as peças: `menu_duelo` (448 s1, N = 27), `menu_selecao` (448 s2 e 461 s2, N = 56, o CharSel e o
  TeamSel), `menu_treino` (461 s1, N = 26, com as aulas `TrainLesson`, peça nova `Conv_LicoesTreino`), `menu_opcoes`
  (453 s1, N = 38) e as imagens únicas: `menu_texfile` (retratos 760..923, páginas do Treino 1025..1422) e
  `menu_fundo` (estágios 924..959, 8 registros, que o `Sprite_DrawPicture` lê). Cada `TexFile` de tela confere o mínimo
  de registros que o leitor usa. As faixas por hipótese decididas: retratos inteiros (todo leitor soma 0x2F9 a um
  personagem; nenhum roteiro lê outro formato ali), páginas até 397 (o dado das aulas), estágios 0x24 e 0x25 nunca
  carregados pela grade.
- **Os 15 ganchos** do essencial em `src/menu` (`DuelMenu_Init`, `CharSel_*`, `TeamSel_*`, `Train_Init`,
  `Train_UpdateImage`, `Option_Init`), em `#ifdef PORT_BIG_ENDIAN`; os do retrato e do estágio dentro dos laços.
- **A memória no passo `fim`** (`roteiros.sh` com `SEMMAPA=1`; `MEM=menu` guarda o critério da 5a): as duas builds no
  último vblank antes do `fim`; o heap em uso no lugar; cada bloco vivo que guarda um arquivo ou um conteúdo lido na
  rodada, achado pelo conteúdo (`memdiff.py --marcas-vivas`, o `.orig` ao lado das marcas), conforme número a número;
  blocos livres, enchimento do alocador e `HeapBlock.unk1C` fora (`--so-em-uso`); e cada palavra diferente fora deles
  explicada na lista `DIFERENTES_CONHECIDAS` do `roteiros.py` (`menus-telas.md` 8.4).
- **Duas visões de outra largura que a memória achou**, corrigidas: o `McCardIconSys.titleBreak` (s32 gravado, u16
  lido; `PS2_LOW` em `PORT_BIG_ENDIAN`) e o `mark` que a HUD grava na folha do rosto do lutador (o conversor do
  personagem trata a entrada 1 como as folhas da HUD).
- **O namefont fora do rastreio** (`SEM_RASTREIO` do `headless.py`): as letras dos nomes do port copiavam pixels em
  blocos de 4 bytes e a origem ia para a pilha; o `--mapa` da seleção acusava 484 mil posições.

Os verificadores novos vistos falhando (guia, seção 4): o `roteiros.sh` (`REUSA=1`) numa cópia das saídas do Duelo com
um u32 do 00001 sem troca e uma palavra do `BtlObjWork` estragada no dump da PPC sai com 1; o `roteiros.py memoria`
acusa saídas forjadas (marca fora do formato, bloco fora do lugar, memdiff com erro ou cortado, deslocamento ou global
fora da lista, trecho não listado, pacote da GPU com `u64`); o `vblank_fim` sem a linha do fim sai com 1; o
`unpacks.py` numa cópia de `src/` acusa formato fora da tabela e gancho sobre o buffer errado; os conversores das
telas, sobre cópias estragadas dos conteúdos, saem com erro e o nome da seção (`menus-telas.md` 8.4).

**O "hash da luta igual".** As duas builds do WSL usam o float do PS2 por software, então a luta da PPC sem mapa é
comparável quadro a quadro com a da x86, e saiu idêntica (vida, posição, relógio e gerador em todos os vblanks de luta
até o `fim`). A separação pelo float é só no console (decisão 17): lá o critério é o rastro, não o hash.

Números: `unpacks.py` com 21 chamadas com gancho e 61 pendentes, nenhuma do essencial (eram 76, 15 do essencial); a
soma dos roteiros, "pendentes do essencial alcançadas: 0 de 0" e as 21 com gancho alcançadas; `--disco` com 2.984
arquivos e 611 conteúdos sem erro, todos com 100% de cobertura (censo: 6 de 30 arquivos de menu, 10 de 49 contêineres,
os retratos, estágios e páginas inteiros); `--mapa` sem nenhuma posição divergente nos quatro roteiros (as posições por
conteúdo em `menus-telas.md` 8.2); a `.text` da x86 com o mesmo sha1 (`02fd5df74264e3f467da6d4b1f735d3bdbc7b47f`); o 360
compila com os mesmos 103 stubs, e o `chamadores_bl` passa (81 `bl Sprite_Unpack` em 62 funções, todas da tabela).

Resultado por roteiro (rodada inteira, `SEMMAPA=1`, sem `REUSA`: x86 duas vezes, gdb, x86r com o rastreio, `--mapa`,
inventário, PPC sem mapa até o `fim`, memória; "tudo certo"):

| Roteiro | PPC sem mapa | Rastro | Leituras (`LEU`) | Luta (`BT3_HASH`) | Memória no `fim` |
| --- | --- | --- | --- | --- | --- |
| `duelo` | chega ao `fim` (luta, vblank 5.208) | 37 de 37 linhas | 40 de 40 | 122 vblanks de luta, 0 diferentes | 180 blocos no lugar; 16 com arquivo, 8.481.591 conformes; 13 trechos explicados |
| `duelo_time` | chega ao `fim` (vblank 5.712) | 37 de 37 | 45 de 45 | 122, 0 diferentes | 180 no lugar; 17 com arquivo; 56 trechos explicados (43 nos pacotes da GPU) |
| `treino` | chega ao `fim` (vblank 5.186) | 37 de 37 | 45 de 45 | 120, 0 diferentes | 180 no lugar; 16 com arquivo; 11 trechos explicados |
| `opcoes` | chega ao `fim` (menu principal, vblank 5.334) | 63 de 63, com os três `confere som` (o mono volta depois do carregar) | 17 de 17 | não chega à luta | 133 no lugar; 6 com arquivo, 5.501.132 conformes; 7 trechos explicados |

Próximo bloqueio de cada roteiro: nenhum no WSL. A etapa 6 (rasterizador nas telas essenciais) e a 7 (o caminho no
console) vêm a seguir; a guarda continua nomeando qualquer `Sprite_Unpack` sem conversor dos outros modos (etapa 9).

Perguntas para o Lucas (etapa 5) e as respostas dele (14/10/2026):

1. **Som no big-endian:** corrigir já, em `PORT_BIG_ENDIAN`. Feito (abaixo): a fila do RPC 0xD se lê na ordem nativa.
2. **Lixo nos pacotes da GPU aceito na comparação:** apertar já. Feito pela forma dos campos não iniciados e com teto
   (abaixo, item 4).
3. **Seções sem leitor:** ficam como estão (as com forma conhecida convertidas pela forma, as outras em bytes).
4. **Retratos 921..923 no `formato_de`:** ficam, e o roteiro do Duelo passa a carregá-los e conferi-los (abaixo).

**Revisão da etapa 5 (14/10/2026) e as correções.** O revisor achou oito pontos; o Lucas decidiu corrigir todos. Cada
verificador corrigido foi visto passando com a entrada boa e falhando (saída 1) com uma estragada numa cópia:

1. **Memória por tela** (achado 1: a memória no `fim` não conferia nenhuma conversão da 5b, já liberada). O
   `roteiros.sh` compara também no último vblank de cada tela de menu do essencial que o roteiro visita (38, 39, 40,
   44, 45, 62) e cada tela exige os conteúdos dela vivos e conformes (`roteiros.py TELAS_MEM`): 448 s1; 448 s2 com
   retratos e estágios; 461 s1 com a página; 461 s2 com retratos e estágios; 453 s1. Falha: a marca do 448 s2 tirada
   do memdiff da tela 39.
2. **Classes que denunciam erro de largura** (achado 2). As globais e os blocos do heap se comparam campo a campo pelo
   tipo (`campos.py --globais` e `--donos --tipos`, `memdiff.py --tipos --donos`): as globais do port pelo DWARF, as do
   jogo pelo mapa do `elftipos.py` e pelas declarações `extern`, os blocos do heap pelo tipo que a alocação pede ou que
   quem recebe o bloco aponta. Vetores de bytes e campos `unk*` da decomp são opacos (qualquer leitura de uma largura
   só, menos "diferente"). O que sobra sem tipo só passa em `CLASSES_CONHECIDAS`, com o porquê e um teto, e a soma das
   classes nas regiões tem de dar o total; `float-proximo` nunca com valor na faixa do heap. Falhas: o forjado do
   revisor (90.000 `igual-sem-inverter` e 20.000 `2xu16` só no total), uma classe numa região sem lista, um s32 do
   `CharSel` sem trocar no dump da PPC, e `float_proximo(0x0187a570, 0x0187a5a0)`, que agora é falso.
3. **Pacotes da GPU** (achado 3, pergunta 2): a região é a de 0x100000 bytes pedidos (não o índice), só com trechos
   da forma de uv0.pad, uv1.pad e nop do `GfxPostSpritePkt`, até 64 trechos e 160 palavras por buffer (o maior visto:
   22 e 88). Falha: o forjado do revisor (5.000 diferentes em dois trechos).
4. **`gPortAdxView` e `sMc`** (achado 4) aceitos só no `path` de cada elemento. Falha: um trecho no `left`.
5. **Builds de origens diferentes** (achado 5): o `roteiros.sh` sai com 1 se x86, x86r e ppc não forem do commit de
   agora sem diferença, ou se a árvore tiver alteração; `DESENVOLVIMENTO=1` roda assim mesmo e o fim diz "NÃO VALE
   COMO CRITÉRIO". Falha: as builds de 7db2bad com a árvore alterada.
6. **Memória da conversão em jogo** (achado 6): um bit por byte do conteúdo (`Conv_FormatoBits`), reservado uma vez na
   primeira conversão (o `BootCard_Init`, no boot) para `CONV_JOGO_MAX` (8 MB de conteúdo, 1 MB de mapa), sem
   alocação por tela; falta de memória ou conteúdo maior é falha com nome no log. O `conv_tool` converte cada conteúdo
   de jogo dos dois jeitos: no `--disco`, 611 conteúdos com os mesmos bytes, o mesmo resultado e a mesma cobertura; o
   maior tem 0x6BF440 bytes (448 s2). Falhas: o mapa de bits estragado e a reserva de 1 MB.
7. **Páginas do Treino** (achado 7): a seção 24 do 461 s1 com o teto 397. Falha: a última página trocada por 398.
8. **Letras dos nomes no big-endian** (achado 8): o `sheet_decode` conferia o byte do registrador do GS como o primeiro
   da palavra; no big-endian é o último (`PKT_LOW8`). A PPC agora corta os 36 nomes de estágio e os 24 de música com as
   mesmas letras, contorno e espaço da x86 (`BT3_NAMEFONT_TEST`). **Pendente:** as imagens de teste da PPC diferem das
   da x86 em uns 3% dos bytes (até 123 de diferença), causa não achada; só vale para estágio ou música de fora do
   disco, que o 360 não tem.

E as decisões do Lucas:

- **Som** (`port/src/gs/snd_se.c`): a fila (contagem u32, handle u16, pitch s32, gravada pelo jogo em `Snd_RpcFlush`)
  se lê na ordem nativa em `PORT_BIG_ENDIAN` (`queue32`/`queue16`); os bancos do disco seguem little-endian. Prova
  numa build de teste da PPC (não guardada), a fila lida contra a da x86 pelo gdb no Duelo: 33 filas e 34 comandos
  iguais, com o handle; o pitch foi 0 em todos os comandos do roteiro.
- **Retratos especiais**: o roteiro do Duelo passa o cursor pela célula do personagem salvo (0xA3, arquivo 923) e pela
  do aleatório (0xA1, 921), espera cada retrato carregar e volta ao canto (o resto e a luta iguais); o passo
  `marca memoria NNNNN` faz o `roteiros.sh` comparar a memória com aquele retrato vivo. O travado (922) não aparece com
  tudo desbloqueado: fica sem roteiro. Referência do Duelo regravada.

O que a tipagem achou e ficou explicado (`roteiros.py`, cada um conferido no código): as somas do cartão (`SaveData`,
`McFlow.saveBuf` e o cabeçalho do replay), o `SndMgr.iopBuf`, os elementos livres dos pools do motor de scripts
(`Gsc_Init` não os zera), o buffer de envio dos RPC de som, e sem tipo os buffers de arquivo (folha de carregamento,
retrato cru), as cores RGBA byte a byte nos pacotes da GPU e na tabela de ordenação (a conferir na etapa 6, pelo
rasterizador) e a arena do BtlPool. **Riscos que ficam:** a arena do BtlPool (0x1A0800 bytes, o alocador próprio da
cena da luta) passa só por teto de classe, sem tipo por estrutura; os campos opacos da decomp aceitam qualquer
largura única; um campo de uma tarefa do motor de scripts em uso, fora do link, cairia na explicação dos elementos
livres. Tipar a arena por sub-bloco fica para quando a luta for olhada de novo (F5).

Resultado da rodada final (14/10/2026, builds x86, x86r e ppc refeitas no WSL do HEAD `012b1f5`, carimbo
`012b1f5 diff:da39a3ee5e (0 alterados, 0 apagados)` nas três; `SEMMAPA=1`, sem `REUSA`, um roteiro por vez; "tudo
certo" nos quatro; a `.text` da x86 com o mesmo sha1, `02fd5df74264e3f467da6d4b1f735d3bdbc7b47f`):

| Roteiro | Rastro | Leituras (`LEU`) | Luta (`BT3_HASH`) | Memória no `fim` | Memória por tela (conteúdos vivos e conformes) |
| --- | --- | --- | --- | --- | --- |
| `duelo` | 39 de 39 linhas | 46 de 46 | 122 vblanks, 0 diferentes | 180 blocos no lugar; 16 com arquivo, 8.481.591 conformes; 2.354 globais e 70 blocos pelo tipo | 38: 448 s1; 39: 448 s2, retratos 782 e 785, estágios 947 e 948; e os retratos 923 e 921 nas marcas |
| `duelo_time` | 37 de 37 | 45 de 45 | 122, 0 diferentes | 180 no lugar; 17 com arquivo, 8.484.759 conformes; 43 trechos dos pacotes da GPU na forma | 38: 448 s1; 40: 448 s2, retratos 782 e 785, estágios 947 e 948 |
| `treino` | 37 de 37 | 45 de 45 | 120, 0 diferentes | 180 no lugar; 16 com arquivo, 8.481.591 conformes | 44: 461 s1 e a página 1030; 45: 461 s2, retratos 782 e 785, estágios 947 e 948 |
| `opcoes` | 63 de 63, com os três `confere som` | 17 de 17 | não chega à luta | 133 no lugar (menu principal); 6 com arquivo, 5.501.132 conformes | 62: 453 s1, e as somas do `McFlow` explicadas |

O 360 compila com os mesmos 103 stubs, e o `chamadores_bl` passa (81 `bl Sprite_Unpack` em 62 funções, todas da
tabela).

**Etapa 6 (14/10/2026, WSL): o rasterizador x86 e PPC nas telas essenciais. Concluída.** Critério: as imagens das
telas do essencial idênticas byte a byte entre a x86 e a PPC sem mapa.

Como as imagens se capturam (`port/x360/headless/imagens.c`, nas duas builds sem janela; o 360 não o compila):

- **O momento:** cada imagem é N quadros do roteiro depois de uma linha do rastro (a âncora: "tela 39", "marca
  memoria 00923", "ok som 1", com "#K" a K-ésima vez), decidido dentro do programa, nas duas builds, e não por vblank
  fixo. A lista sai do rastro de referência (`roteiros.py imagens`): a entrada de cada tela de menu do essencial (4,
  38, 39, 40, 44, 45, 62; 100 quadros depois da troca, com o fade de entrada acabado), a saída (100 quadros antes da
  troca seguinte: os últimos quadros são o fade, até o preto), a tela de carregamento antes da luta, cada `marca`
  (os retratos 923 e 921 vivos; as páginas do Treino, que ganharam `marca pagina 1..6`; salvo e carregado), cada
  `ok som` e cada pergunta do cartão (`ok mcflow 12` e `ok mcflow 2`, 40 quadros depois). A imagem é o buffer que o
  jogo mostra (`gGsMainFbp`, o FRAME dele em `gGs`, lido por `Gs_VramRead`), 640 x 448, como o `screenshot()` do
  `gs_core.c`.
- **O custo:** pintando todo quadro, o rasterizador na PPC (qemu) levaria umas três horas por roteiro. Com o GS
  ligado e o desenho só a partir de `BT3_GS_FROM=1000000000`, só se pintam os 8 quadros antes de cada imagem (o
  `gGsFrame` sobe para essa faixa durante a janela); cargas de textura, registradores e VU1 rodam sempre. Na x86, as
  9 imagens do Duelo com janela de 8, de 2 e de 0 quadros saíram idênticas às da rodada pintando tudo (cada quadro
  limpa o buffer que vai desenhar). A PPC faz as imagens de um roteiro em 3 a 5 minutos (161 a 258 s).
- **Sem tocar no upstream:** o gancho é `--wrap` no `Port_GsVif1Chain` e no `Port_GsGifChannel` (`headless.py`),
  com o que o `gs_core.c` já exporta. A `.text` da x86 mudou (o programa de teste ganhou um arquivo e duas chamadas
  por outro caminho): de `02fd5df74264e3f467da6d4b1f735d3bdbc7b47f` para
  `0c516517b6d124fb1ac868c9c43877082b3bb0ed`; os 318 objetos de `src/`, `port/src` e do GS saíram idênticos byte a
  byte aos da build de `012b1f5`.

Como se comparam (`roteiros.sh` com `IMG=1`, depois do passo da PPC sem mapa; `roteiros.py compara_imagens`): a x86 e
a PPC sem mapa com os mesmos arquivos convertidos; o rastro de cada rodada com imagens igual ao da x86 de referência;
cada imagem presente nas duas, P6 de 640 x 448, do mesmo quadro e vblank nas duas, do quadro que a lista pede pelo
rastro de referência, não de uma cor só, e idêntica byte a byte. Uma diferença sai com a tela (a âncora), o quadro, o
vblank, quantos pixels diferem, o retângulo e o maior desvio, e o `roteiros.sh` sai com 1. As imagens ficam no WSL
(`port/build/roteiros/NOME/img/x86` e `img/ppc`), nunca aqui. `IMG=luta` acrescenta a luta (abaixo).

O verificador visto falhando (cópias das saídas do Treino e das Opções com `REUSA=1`, cada uma saindo com 1 e a
linha da diferença): um pixel da PPC com um bit trocado ("1 de 286720 pixels diferentes, em (300,200)"), uma imagem
apagada ("imagem faltando (ppc)"), o quadro de uma imagem trocado na saída da PPC ("capturada em outro momento"), o
rastro da PPC com outro vblank, as duas imagens pretas ("de uma cor só"), um PPM cortado ("imagem ruim") e, nas saídas
da primeira rodada final, a imagem da segunda visita ao menu principal das Opções tirada da primeira ("a lista pede o
quadro 5327"; abaixo). Intacta, a cópia passa.

Diferenças achadas e o que se fez:

1. **A luta (fora do critério até a F5).** Na primeira comparação, a imagem da luta do Duelo saiu com 229 mil pixels
   diferentes, e a PPC levava 20 minutos por roteiro: os pacotes da VU1 das tabelas do executável (`vu1_micro.data`:
   códigos VIF, microcódigo e constantes) são `u8[]` no C e o mapa das tabelas os deixava como vieram, então a VU1 da
   PPC rodava instruções inexistentes. Corrigido: o `elftipos.py` os trata como fluxo VIF (`FLUXOS_VIF`, 3.892
   palavras u32 a mais no mapa da PPC e no `default.swap` do 360, onde a VU1 segue desligada; `gpu.md` seção 6).
   Sobram 17.673 pixels (desvio até 7, no céu e numa faixa à esquerda): o `BT3_GS_DUMP` do quadro nas duas builds dá
   58.163 linhas de primitivas, e as únicas diferentes são 26 primitivas (24 triângulos com textura em 0x2A20 e 2 em
   0x2B00) com os quatro bytes da cor invertidos (`rgba 3c3c3c16` na x86, `163c3c3c` na PPC): os `u8 rgba[4]` dos
   vértices dos efeitos da luta, erro de conversão já marcado para a F5 (`gpu.md` seção 6). Não é aceito: a luta fica
   fora da lista do critério, e `IMG=luta` a compara (falha até a F5). Imagem lado a lado (x86, PPC, pixels
   diferentes em vermelho): `build/capturas/etapa6/luta_x86_ppc_diferencas.png` (fora do git) e
   `~/m6/capturas/luta_x86_ppc_diferencas.png` no WSL.
2. **A pendência "cores RGBA dos pacotes da GPU" da etapa 5.** A memória aceitava as palavras `igual-sem-inverter`
   dos buffers de pacotes e da tabela de ordenação como "cores RGBA byte a byte, o 3c3c3cNN do fade, como o GS as
   lê". A imagem da luta mostrou que são essas mesmas cores invertidas dos efeitos (o 3c3c3cNN): erro da F5, não cor
   certa. Nas telas de menu a memória não tem nenhuma dessas palavras e as imagens saíram idênticas; a aceitação
   ficou só fora das telas de menu (`SO_NA_LUTA` do `roteiros.py`), e uma palavra dessas numa tela de menu é
   diferença (visto falhando: 5 palavras injetadas no buffer de pacotes do memdiff da tela 39).
3. **Erros do próprio verificador, achados nas rodadas:** o último quadro antes de cada troca é preto (o fade de
   saída), e a imagem de saída passou a ser a de 100 quadros antes; duas imagens no mesmo quadro do roteiro (a `marca
   carregado` e o `ok som 1` das Opções) e o `imagens.c` gravava só uma; a âncora "tela 4" casava também com o "ok
   tela 4" da espera, e a imagem da segunda visita ao menu principal das Opções saiu da primeira, igual nas duas
   builds (a primeira rodada final passou com ela). Corrigidos, com o quadro de cada imagem conferido contra o rastro
   de referência, e a rodada final refeita.
4. **O namefont** (as imagens de teste da PPC diferem das da x86 em uns 3%, pendência da etapa 5) não apareceu: as
   telas de seleção saíram idênticas. Continua pendente e só vale para nomes de fora do disco.

Resultado da rodada final (14/10/2026, builds x86, x86r e ppc refeitas no WSL do HEAD `ada53c7`, carimbo
`ada53c7 diff:da39a3ee5e` nas três, clone do WSL na main sem alteração; `SEMMAPA=1 IMG=1`, sem `REUSA`, um roteiro por vez;
"tudo certo" nos quatro, com todas as conferências da etapa 5 também):

| Roteiro | Imagens (âncora e quadros depois) | Idênticas | Rastro das rodadas com imagens | Resto da rodada (etapa 5) |
| --- | --- | --- | --- | --- |
| `duelo` | 8: menu principal (entrada), DuelMenu (entrada, saída), CharSel (entrada, saída com personagens e estágio escolhidos), retratos 923 e 921, tela de carregamento; quadros 2.967 a 4.374 | 8 de 8 | 39 de 39 linhas nas duas | rastro 39/39, leituras 46/46, luta 122 vblanks iguais, memória igual nas telas 38, 39 e nos retratos |
| `duelo_time` | 6: menu principal, DuelMenu (entrada, saída), TeamSel (entrada, saída), carregamento; quadros 2.967 a 4.598 | 6 de 6 | 37 de 37 | rastro 37/37, leituras 45/45, luta 122 iguais, memória igual nas telas 38 e 40 |
| `treino` | 12: menu principal, Train (entrada, saída), as seis páginas da aula, CharSel do treino (entrada, saída), carregamento; quadros 2.967 a 4.936 | 12 de 12 | 43 de 43 | rastro 43/43, leituras 45/45, luta 120 iguais, memória igual nas telas 44 e 45 |
| `opcoes` | 11: menu principal, Option (entrada, saída), som em mono, "salvar?", salvo, estéreo, "carregar?", carregado, mono de volta, menu principal na volta; quadros 2.967 a 5.327 | 11 de 11 | 63 de 63 | rastro 63/63 com os três `confere som`, leituras 17/17, memória igual na tela 62 |

O 360 compila com os mesmos 103 stubs, o `chamadores_bl` passa (81 `bl Sprite_Unpack` em 62 funções) e o
`default.swap` tem 7.062 posições (3.170 antes do microcódigo da VU1).

Perguntas para o Lucas (etapa 6), com a escolha conservadora feita:

1. **A `.text` da x86 mudou** por código só do teste (`port/x360/headless/imagens.c` e o `--wrap`), não do jogo nem
   do port: a prova de que o upstream não mudou passou a ser os 318 objetos idênticos, e o sha1 novo (`0c516517...`)
   é a referência daqui em diante. Se preferir manter o sha1 antigo como prova, o caminho é uma build à parte só para
   imagens.
2. **A luta fica fora do critério da etapa 6** até a F5 corrigir os `u8 rgba[4]` dos efeitos (uns 150 pontos em
   `src/battle`). Se quiser a luta já na etapa 7, a correção vem antes.

## 6. Riscos principais

1. **Desempenho no Xenon.** No PC, softfloat e interpretador de VU1 dominam o frame. F2 existe para medir isso antes de investir em gráficos.
2. **Endianness.** Arquivos usados como structs, cerca de 210 KB de tabelas do ELF, 167 bitfields, 60 unions.
3. **Compilador.** Se nenhum candidato do F0 servir, reescrever partes para C89 é caro.
4. **Upstream instável.** O mantenedor é um só e muda o código rápido. Por isso o merge do upstream é manual e de propósito, nunca automático.
5. **Toolchain.** O wine da imagem vem do master sem tag. Fazer backup da imagem antes de qualquer rebuild.
