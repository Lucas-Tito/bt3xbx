# bt3xbx: port do BT3 para Xbox 360

Fork do [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp) (port de PC sobre a decomp
BT3-Decompiled) com um backend de Xbox 360 (RGH, `.xex` de título). Plano em `docs/x360/plano.md`,
decisões numeradas em `docs/x360/decisoes.md`, estudos em `docs/x360/pesquisa/01..04`.
**`docs/x360/guia-de-port.md`**: setup, ferramentas, processo, método com agentes e aprendizados, para
acelerar outros ports. Todo aprendizado que custou uma rodada entra lá no mesmo commit da correção.
O que vem do upstream (`src/`, `include/`, `port/` fora de `port/x360/`, `docs/` fora de `docs/x360/`)
segue as regras dele (`docs/agent_rules.md`, `docs/port/`).

## Regras do Lucas

- Tudo em português, sem travessão em texto que ele lê.
- Commits atômicos, mensagem em português com o porquê, **sem nenhuma atribuição ao Claude**
  (nada de `Co-Authored-By` nem `Claude-Session`). Identidade local do repo: conta pessoal Lucas-Tito.
- Repositório **privado**. `gh`/push só pela conta pessoal:
  `GH_TOKEN=$(gh auth token --user Lucas-Tito) git push ...`, sem trocar a conta ativa do `gh`.
- **Nenhum byte do jogo no repositório nem no release** (ISO, ELF, AFS, dados extraídos). O usuário
  fornece a própria ISO USA SLUS-21678.
- Pode subir build no console por FTP sem pedir; se demorar, parar e perguntar. Antes de apagar algo
  no console, listar e identificar o conteúdo.
- Merge do upstream só de propósito (`git merge upstream/main`), nunca automático.

## Console

Xbox 360 E, RGH, kernel 2.0.17150, FreeStyle 3.0.780 + DashLaunch (plugins hiddriver e usbsecpatch,
**sem xbdm**). Rede por **cabo** (IP por DHCP, achar pela porta 21). Relógio zerado: não usar datas.

## Toolchain (Linux, sem Windows)

- XDK 2.0.21256 extraído em `~/Documentos/CollectionUI/sdk/XDK` (nunca copiar para o repo).
- Imagem Docker `collectionui-xdk:light` (wine 11.18, `cl.exe` 16.00.11886 PPC). Modelo de build:
  `~/Documentos/hiddriver/tools/build.sh` (monta `lib/xbox` inteiro só leitura, roda como root,
  `chown` no fim). Não fixar `WINEPREFIX` nem usar `--user`. O wine da imagem veio do master sem tag:
  fazer backup antes de reconstruir.
- Cadeia: `cl` → `link -XEX:NO -MAP` → `imagexex -config:xex.xml`. Nunca `<dvdxgd2/>` em
  `mediatypes` (o console recusa o `.xex`). `imagexex -dump` não aceita `-nologo` junto.
- `cl.exe` em modo C (`-TC`) é **C89**; `char` é signed; `inline` não existe em C (`__inline`).
  O código do jogo é `gnu89` para GCC. Troca de bytes: `__loadwordbytereverse` (`ppcintrinsics.h`).
- Compilar Release para o console (`d3d9d.lib` espera XBDM).

## Deploy (FTP da FreeStyle)

`xbox`/`xbox`, só com o console parado na FreeStyle, uma conexão por vez, sem `QUIT` (usar
`close()`), sem `REST`, `RETR` só depois de `CWD`. Envio: nome temporário → `DELE` → `RNFR`/`RNTO`
→ conferir md5. Abrir o `.xex` é manual (sem xbdm).

## Lições do 360 que custaram rodadas

- `main` nunca retorna; em falha, laço com cor de tela por fase.
- Log em arquivo com `WriteFile` + `FlushFileBuffers` por linha, com trava. `SetUnhandledExceptionFilter`
  registrando `Iar`, `Lr` e endereço acessado, resolvidos pelo `.map`. **Fazer o programa dizer onde
  morreu antes de adivinhar.**
- Sem pipeline fixo: tudo é shader. Texturas lineares precisam de `D3DFMT_LIN_*`.
- XAudio2 mora nas threads de hardware 4 e 5: nada de I/O lá. Dados XMA em `XPhysicalAlloc` com
  alinhamento 0. No crash, `StopEngine`.
- Nunca bloquear a thread de desenho (a Guide congela o console). Buffers de UI do sistema fora da pilha.
- Dados little-endian se leem byte a byte; acesso não alinhado no PowerPC pode falhar.
- `game:\` é a pasta do `.xex`; outros volumes só existem depois de `ObCreateSymbolicLink`.
  FATX: nome até 42 caracteres, arquivo até 4 GB, poucas centenas de entradas por pasta.
- Sair para o dashboard: desmontar threads, áudio e texturas, depois `XLaunchNewImage(NULL, 0)` de
  outra thread (é `NORETURN`).
- Todo `.obj` depende de todos os headers e do Makefile (já saiu binário misto).

## Lições das ferramentas do port que custaram horas

- **Roteiros na PPC: `SEMMAPA=1`, nunca `PPC=1`.** A PPC com o mapa passa do menu principal e trava no Flash da tela
  seguinte, que o mapa por posição não inverte: uma hora por roteiro sem resultado (a etapa 5a da F4 perdeu quatro).
  A PPC sem mapa para na guarda em minutos. Detalhes no guia, "Roteiros na etapa 5a da F4".
- **Um ciclo de emulador por vez no WSL** (dois `roteiros.sh` ou dois qemu juntos travam o WSL), e nada de
  `wsl.sh enviar` com um `roteiros.sh` rodando.
- **Todo verificador tem de ser visto falhando de propósito** (entrada estragada numa cópia) antes de valer: na F4
  o `roteiros.sh` dava "ok" com erro de conversor e nunca saía com 1.
