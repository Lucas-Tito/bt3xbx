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
- **Fluxo no console:**
  1. O usuário copia a própria ISO para `disc/` ao lado do `default.xex`.
  2. Um instalador dentro do XEX confere os hashes e converte os dados para `gamedata/`: troca de bytes, texturas no formato Xenos, poucos arquivos grandes por causa do limite de entradas por pasta do FATX.
  3. Tudo isso com barra de progresso e mensagem de erro clara na tela.
- **Alternativa:** uma ferramenta opcional de PC que faz a mesma conversão.

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
| 7 | console | Caminho essencial no console, pelos roteiros (`BT3_PAD_PLAY`) | mesmas telas da x86; capturas no patamar da F3; nenhuma queda |
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

Critério da F4: no console, com o controle, do boot ao começo da luta pelo Duelo e pelo Treino, Opções com salvar e
carregar, e os outros modos navegáveis; a sequência de telas igual à x86. Antes da etapa 5, um revisor independente
(guia, seção 4).

## 6. Riscos principais

1. **Desempenho no Xenon.** No PC, softfloat e interpretador de VU1 dominam o frame. F2 existe para medir isso antes de investir em gráficos.
2. **Endianness.** Arquivos usados como structs, cerca de 210 KB de tabelas do ELF, 167 bitfields, 60 unions.
3. **Compilador.** Se nenhum candidato do F0 servir, reescrever partes para C89 é caro.
4. **Upstream instável.** O mantenedor é um só e muda o código rápido. Por isso o merge do upstream é manual e de propósito, nunca automático.
5. **Toolchain.** O wine da imagem vem do master sem tag. Fazer backup da imagem antes de qualquer rebuild.
