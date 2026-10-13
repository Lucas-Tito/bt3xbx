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

Etapa 7 (13/10/2026): o backend está em `port/x360/src/gs/` (decisão 20) e a build liga sem stub novo; falta o teste
no console (`BT3_GS=gpu`, capturas `BT3_SHOT` comparadas no WSL por `compara_quadros.py`).

Filme e som ficam para a F6; VU1, outline e o resto da luta para a F5. Risco anotado para a F5: a gravação e a
decodificação de texturas do `gs_draw.c` custam 2 a 3 ms no PC e podem passar do que sobra do quadro no Xenon.

Fora do escopo: netplay, versão PAL/japonesa, 60 fps (interpolação ou lógica a 60; decisão 15: 720p a 30).

## 6. Riscos principais

1. **Desempenho no Xenon.** No PC, softfloat e interpretador de VU1 dominam o frame. F2 existe para medir isso antes de investir em gráficos.
2. **Endianness.** Arquivos usados como structs, cerca de 210 KB de tabelas do ELF, 167 bitfields, 60 unions.
3. **Compilador.** Se nenhum candidato do F0 servir, reescrever partes para C89 é caro.
4. **Upstream instável.** O mantenedor é um só e muda o código rápido. Por isso o merge do upstream é manual e de propósito, nunca automático.
5. **Toolchain.** O wine da imagem vem do master sem tag. Fazer backup da imagem antes de qualquer rebuild.
