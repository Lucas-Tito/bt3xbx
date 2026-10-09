# Pesquisa 01: o que CollectionUI e hiddriver já ensinaram sobre Xbox 360

Levantamento feito em 09/10/2026 só a partir do **disco local**, para o port da decompilação do
Dragon Ball Z Budokai Tenkaichi 3 (PS2) para Xbox 360 com RGH. Nada nos projetos de origem foi
modificado. Onde algo foi **conferido na máquina** (comando rodado agora), isso está dito; onde
é **inferência minha**, também.

Fontes lidas:

| Projeto | O que foi lido |
|---|---|
| `~/Documentos/CollectionUI` | `README.md`, `docs/viabilidade.md`, `docs/decisoes.md` (143 decisões numeradas, 1.426 linhas), `docs/interface.md`, `toolchain/*` (Dockerfile, build.sh, Makefile, dockerignore, vendor-atg.sh, patches/atg.patch), `app/Makefile`, `app/main.cpp`, todos os módulos de `app/src/`, `previa/`, `prototipo/`, `ferramentas/`, `sdk/` (estrutura do XDK extraído, `xdk-docker/`, `hello/`), `git log` (82 commits) |
| `~/Documentos/CollectionUI-dados` | `LEIA-ME.md` e estrutura (cópia dos dados da FreeStyle, sem interesse direto para o BT3) |
| `~/Documentos/hiddriver` | `README.md`, `Makefile`, `hiddriver.sln`, `hiddriver/hiddriver.vcxproj`, `hiddriver/xex.xml`, `hiddriver/xkelib/`, trechos de `hiddriver/main.cpp` e `Detours.cpp`, `docs/build.md`, `docs/ftp-no-xbox.md`, `docs/threads-e-contextos.md`, `tools/*` (build.sh, testeplugin, kerneldump, diag-master.patch), `port/xbx.py`, `port/17150/info.txt`, `git log` (46 commits, 24 do Lucas) |
| `~/Documentos/gh2dxu` | README, logs de build (é mod de dados do GH2, não port de código) |
| `~/Documentos/xbx` | `CLAUDE.md`, README (só o que toca o console) |
| `~/Documentos/xbox-rgh-pipeline` | README, `instrucoes_mover_hd.md` (layout do HD e limites do FATX) |
| `~/.claude/projects/-home-lucas-tito-Documentos-{CollectionUI,hiddriver,xbx}/memory/` | as notas de memória das sessões anteriores (regras de trabalho do Lucas, dados do console) |

---

## 1. Setup de desenvolvimento disponível

### 1.1 O console alvo

Juntando `hiddriver/docs/*.md`, `port/17150/info.txt` e as memórias das sessões:

| | |
|---|---|
| Modelo | **Xbox 360 E** |
| Exploit | RGH (o dono não sabe se é RGH 1.2 ou RGH3) |
| Kernel | **2.0.17150.0** retail (lido de `XboxKrnlVersion` pelo `tools/testeplugin`). O dono **recusou** atualizar o kernel (26/09/2026) |
| Dashboard | **FreeStyle 3.0.780** (pasta `Freestyle.780`), com **DashLaunch** |
| Plugins no `launch.ini` | `hiddriver` v0.6-17150 em `Hdd:\plugins\hiddriver.xex`, `usbsecpatch` (UsbdSecPatch, para controles XInput não licenciados). Sem `xbdm.xex` instalado, até onde os registros mostram |
| Rede | **Wi-Fi interno**, IP por DHCP na faixa `192.168.0.x` (já foi `.2`, `.3`, `.4`) |
| Relógio | **Zerado**: arquivos e logs saem datados de `2005-11-22` |
| HD | Interno, com 416 a 418 itens na biblioteca da FreeStyle no HD novo |
| Memória livre medida | `GlobalMemoryStatus().dwAvailPhys` entre **~400 e ~480 MB** com o CollectionUI rodando (D3D 720p + 120 a 160 texturas DXT5 + XAudio2) |

### 1.2 O XDK

- Arquivo: `~/Documentos/CollectionUI/sdk/XBOX360 SDK 21256.3.exe`, 1.526.672.200 bytes, md5
  `b329b79db2804e2989688bffbaf49691`. As ferramentas relatam versão **2.0.21256.0**.
- Extraído **sem wine e sem instalador**, só com `cabextract` (o `.exe` é InstallShield):
  `~/Documentos/CollectionUI/sdk/XDK` (5,1 GB, 6.536 arquivos). `sdk/` inteiro tem 6,6 GB e é
  ignorado pelo git.
- Pastas úteis: `XDK/bin/win32` (262 arquivos: `cl.exe`, `link.exe`, `imagexex.exe`, `fxc.exe`,
  `xma2encode.exe`, `xbcp.exe`, `xbreboot.exe`, `xbwatson.exe`, `xbEmulate.exe`, `Bundler.exe`...),
  `XDK/lib/xbox` (133 libs), `XDK/include/xbox` (só headers do Xbox: `xtl.h`, `d3d9.h`,
  `xnamath.h`, `ppcintrinsics.h`, `vectorintrinsics.h`), `XDK/TechPreview/Jul12Compiler`
  (headers de C/C++ padrão e um **segundo compilador**, ver 1.3), `XDK/Source/Samples`
  (`Audio`, `Common` = ATG, `Graphics`, `Media`, `Online`, `System`, `ui`...), `XDK/doc`.
- Os headers C/C++ padrão (`stdio.h`, `<string>`, `stdint.h`, `cstdint`...) **não** estão em
  `include/xbox`: estão em `TechPreview/Jul12Compiler/include/xbox` (armadilha 2 do
  `toolchain/README.md`). `_CPPLIB_VER` = 520 (STL de VC10).

### 1.3 A imagem Docker `collectionui-xdk:light`

**Conferido agora** (`docker images`, `docker run ... ls`):

| | |
|---|---|
| Imagem | `collectionui-xdk:light`, ID `3b3ece9162a6`, 2,28 GB, criada em 02/10/2026 |
| Base | Ubuntu 24.04.5, wine **11.18** compilado do fonte (só dentro do container; **não há wine no host**) |
| Compilador | `Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 16.00.11886.00 for PowerPC` (geração VS2010) |
| Linker | `Incremental Linker Version 10.00.11886.00` |
| `XEDK` | `Z:/xdk` (caminho visto pelo wine) |
| `/xdk/bin/win32` | `cl.exe`, `link.exe`, `lib.exe`, `imagexex.exe`, `fxc.exe`, `xbcp.exe` + DLLs do compilador |
| `/xdk/lib/xbox` | `d3d9(d)`, `d3dx9(d)`, `dxerr9`, `libcMT(d)`, `libcpMT(d)`, `oldnames`, `xapilib(d)`, `xboxkrnl`, `xbdm`, `xgraphics(d)`, `xjson(d)`, `xnet(d)`, `xaudio2`/`xaudiod2`, `xmcore(d)`, `xav(d)` |
| `/xdk/include/xbox` | 149 headers do Xbox; `/xdk/include/crt`: 166 headers de C/C++ |

Receita: `~/Documentos/CollectionUI/toolchain/Dockerfile` (derivado do `light.Dockerfile` do
[xdk-docker](https://github.com/ClementDreptin/xdk-docker), cuja cópia está em
`CollectionUI/sdk/xdk-docker/`). Construir do zero:

```bash
cd ~/Documentos/CollectionUI/sdk
cabextract -d . "XBOX360 SDK 21256.3.exe"          # só se XDK/ não existir; ~15 s
cp ../toolchain/dockerignore .dockerignore          # corta art, Source, doc: 6,6 GB -> 2,9 GB de contexto
docker build -t collectionui-xdk:light --build-arg XDK=XDK -f ../toolchain/Dockerfile .
```

As três armadilhas registradas em `CollectionUI/toolchain/README.md`, cada uma custou uma rodada:

1. **A poda de DLLs do wine do upstream está velha.** O `keep-needed-files.sh` tem lista fixa;
   o wine atual quebra o `setupapi` sem `cfgmgr32.dll` e o `wineboot` morre com
   `unimplemented function setupapi.dll.SetupDiCreateDeviceInfoList`. O Dockerfile local **não
   poda** (imagem de 677 MB vira 1,56 GB).
2. **Headers de CRT fora de `include/xbox`.** Sem copiar `TechPreview/Jul12Compiler/include/xbox`
   para `/xdk/include/crt`, o `xtl.h` falha com `Cannot open include file: 'excpt.h'`. O `INCLUDE`
   precisa apontar para os dois.
3. **Não fixar `WINEPREFIX` na imagem nem rodar com `--user`.** O wine recusa prefixo de outro dono
   (`'/wine-prefix' is not owned by you`) e um prefixo pré-criado sai sem `Temp`, derrubando o
   `cl.exe` com `D8037: cannot create temporary il file`. Solução: rodar como root, deixar o wine
   criar o prefixo a cada execução e fazer `chown` no fim (é o que os `build.sh` fazem).

Outras lições da imagem (decisões 62 e o cabeçalho do `Dockerfile`):

- **Cada lib do XDK entra uma a uma no Dockerfile.** Lib nova = editar e reconstruir. O sufixo de
  depuração nem sempre é no fim: `xaudiod2.lib`. `xaudio2` exige `xmcore` (fila sem trava,
  `XLFQueueCreate/Add`), senão 4 externos não resolvidos. `d3d9d.lib` exige `xbdm.lib`
  (`LNK1104`). `XNotifyQueueUI` está em `xav.lib`.
- O hiddriver contornou isso de outro jeito: o `tools/build.sh` dele **monta a pasta inteira de
  libs do XDK** por cima, só leitura (`-v $XDK_DIR/lib/xbox:/xdk/lib/xbox:ro`). Para o BT3 essa é
  a melhor opção (sem reconstruir imagem a cada lib).
- O `Dockerfile` clona o wine do **master**, sem tag. Foi exatamente uma mudança no master que
  quebrou a poda (armadilha 1). A imagem atual funciona; reconstruir amanhã pode não funcionar.

O XDK tem ainda um **segundo compilador**, `TechPreview/Jul12Compiler/bin/win32/cl.exe`, versão
**16.10.16045** (o principal é 16.00.11886), com `dumpbin.exe`, `editbin.exe` e `ml.exe`. **Nunca
foi testado** em nenhum dos projetos.

### 1.4 Como se compila um `.xex`

Cadeia: `cl.exe` (cada `.cpp`/`.c` vira `.obj`) → `link.exe -XEX:NO` (gera um `.exe`/`.dll` PE
PowerPC) → `imagexex.exe` (empacota em XEX2). Tudo sob `wine` dentro do container; o `make` roda
no Linux do container.

**CollectionUI (título, app que abre como jogo):**

```bash
cd ~/Documentos/CollectionUI
./toolchain/build.sh app                     # Debug  -> app/build/Debug/bin/collectionui.xex
./toolchain/build.sh app CONFIG=Release      # Release -> app/build/Release/bin/collectionui.xex
```

`toolchain/build.sh` faz, em essência:

```bash
docker run --rm -v "$PWD":/app -w /app collectionui-xdk:light make "$@"
docker run --rm -v "$PWD":/app collectionui-xdk:light chown -R "$(id -u):$(id -g)" /app
```

Flags do `app/Makefile` (as que importam):

- C++: `-c -Zi -nologo -MP -D _XBOX -Gm- -GS -fp:fast -fp:except- -Zc:wchar_t -Zc:forScope
  -openmp- -W4 -EHsc -GR- -TP -FI"$(XEDK)/include/xbox/xbox_intellisense_platform.h"`.
- C (o sqlite): mesmas comuns com `-W3 -TC` e defines do SQLite.
- Release: `-WX -Ox -Oi -Os -D NDEBUG -MT -Gy -GF`, link `-OPT:REF`. Debug: `-Od -D _DEBUG -MTd`.
- Link: `-DEBUG -PDB:... -STACK:"262144","262144" -RELEASE -XEX:NO -MAP:"....map"` (o `.map`
  é o que resolve endereço de crash, ver 1.7).
- Libs Release: `d3d9.lib d3dx9.lib xgraphics.lib xboxkrnl.lib xaudio2.lib xmcore.lib xav.lib`;
  Debug com sufixo `d`. O `xtl.h` não puxa D3D nem XAudio2 sozinho.
- Sem `xex.xml`: o `imagexex` usa o padrão, e o `.xex` sai com **Title ID 0** (sem
  `EXECUTION_INFO`). Tamanho atual em Release: 4,27 MB (sqlite incluso); PDB 17 MB, `.xdb` 15 MB.

**hiddriver (DLL de sistema, carregada pelo DashLaunch):**

```bash
cd ~/Documentos/hiddriver
./tools/build.sh                 # uso       -> build/uso/bin/hiddriver.xex
./tools/build.sh DIAG=1          # diagnóstico (log, notificação, etapa) -> build/diag/bin/
./tools/build.sh -f tools/testeplugin/Makefile   # app de teste (título)
```

- Flags estilo "Release Retail" do `.vcxproj`: `-Ox -Ob2 -Oi -Ot -GL -GF -Gy -GS- -MT -GR- -TP
  -fp:fast`, link `-DLL -LTCG -OPT:REF -OPT:ICF -XEX:NO`, `imagexex -config:hiddriver/xex.xml`.
- `xex.xml` de DLL de sistema: `<sysdll/>`, `<baseaddr addr="0x81F00000"/>`, `<compressed/>`,
  `mediatypes` = flash, harddisk, usbmass, mu, network, allpackages.
- **Armadilha:** `<dvdxgd2/>` em `mediatypes` liga o bit `0x8` das image flags ("só disco XGD2") e
  o console **recusa** o `.xex`. O `tools/build.sh` do hiddriver tem uma guarda em Python que lê
  o cabeçalho e aborta se o bit estiver ligado.
- **Armadilha:** `imagexex -dump` falha com "unrecognized option" se vier junto de `-nologo`.
- Padrão bom do `Makefile` do hiddriver: cada variante (`DIAG`, `STAGE`, `INPUTD`) compila numa
  pasta própria, e um **arquivo-carimbo com o nome das chaves** força recompilar tudo quando elas
  mudam.

Tempo: o comentário do `app/Makefile` diz que reconstruir tudo "custa poucos minutos" (14 fontes
nossas + sqlite de 9,5 MB). Cada execução paga a criação do prefixo do wine.

### 1.5 Como se faz deploy no console

**O canal é o FTP embutido da FreeStyle** (servidor "F3 FTPD"). Tudo em
`hiddriver/docs/ftp-no-xbox.md` e decisão 73 do CollectionUI:

| Fato | Consequência |
|---|---|
| Ligar em FreeStyle → Configurações → rede → "Enable FTP server" | o IP aparece na mesma tela |
| Login `xbox` / `xbox` (o campo de senha aparece vazio, mas a senha existe) | senha vazia dá `530 Incorrect Password` |
| **IP muda por DHCP** | achar pela porta 21: `for i in $(seq 1 254); do (timeout 1 bash -c "</dev/tcp/192.168.0.$i/21" 2>/dev/null && echo 192.168.0.$i) & done; wait` e `ip neigh \| grep -i 4c:0b:be` (MAC Microsoft) |
| Console **não responde ping** | testar a porta: `bash -c "</dev/tcp/IP/21"`. Portas 80 e 9999 (HTTP da FreeStyle) também abertas |
| **Só funciona com o console parado na FreeStyle.** Com jogo/app rodando, ou travado, não há FTP | o ciclo é: sair para a FreeStyle, subir, abrir de novo |
| Uma conexão por vez; sessão pendurada bloqueia a próxima | tentar em laço com 10 s de espera, poucas vezes; se demorar, parar e perguntar ao Lucas (regra da memória `subir-no-console-sem-pedir`) |
| Não responde ao `QUIT` | `ftp.close()`, nunca `ftp.quit()` |
| `RETR` com caminho absoluto dá `550 Invalid argument` | `CWD /Hdd1` e depois `RETR nome`. `curl ftp://IP/Hdd1/arq` funciona porque faz o `CWD` sozinho |
| **Sem `REST`** (retomada) | baixar inteiro; um `STOR` interrompido deixa arquivo pela metade |
| Envio abortado **tranca** o arquivo temporário até a sessão cair | usar outro nome ou reiniciar o console |
| Ler arquivo que está sendo gravado trava o servidor | renomear (`RNFR`/`RNTO`) antes de baixar |
| `STOR` por cima de `.xex` em uso (plugin) funciona | vale no próximo boot |
| Raízes: `/Hdd1` (HD), `/Game`, `/OnBoardMU`, (pendrives quando presentes) | `launch.ini` do DashLaunch em `/Hdd1/launch.ini` |

Disciplina de upload usada em todas as rodadas do CollectionUI (reconstituída da sessão anterior;
**o script nunca foi versionado**, vivia no scratchpad):

```python
f.cwd(DEST)                                  # ex.: /Hdd1/HOMEBREW/CollectionUI
f.storbinary("STOR novo.tmp", io.BytesIO(dados))
try: f.sendcmd("DELE collectionui.xex")
except Exception as e: print("DELE:", e)
f.sendcmd("RNFR novo.tmp"); f.sendcmd("RNTO collectionui.xex")
buf = io.BytesIO(); f.retrbinary("RETR collectionui.xex", buf.write); f.close()
assert hashlib.md5(buf.getvalue()).hexdigest() == md5_local
```

Onde as coisas moram no console:

- App: `/Hdd1/HOMEBREW/CollectionUI/collectionui.xex` (+ `media/` ao lado; o `game:\` do título é
  essa pasta). Os homebrews ficam em `HOMEBREW\` na raiz do HD; a FreeStyle tem `HOMEBREW` entre
  as raízes de varredura (`xbox-rgh-pipeline/instrucoes_mover_hd.md`).
- Plugin: `Hdd:\plugins\hiddriver.xex`, referenciado em `launch.ini` (`[Plugins] pluginN = ...`).
- **Abrir o `.xex` é manual**: pela lista de aplicativos da FreeStyle, no sofá. Não há lançamento
  remoto montado (sem XBDM/JRPC2 no console; o `xbreboot`/`xbcp` do XDK exigem XBDM).
- O primeiro `.xex` (25/09/2026) foi levado por **pendrive**; depois tudo passou a ir por FTP.

Releases do CollectionUI: zip com o `.xex` + `media/`, publicado com `gh release create` (sempre
pela conta pessoal **Lucas-Tito**). O empacotamento também era montado à mão no scratchpad. O
`collectionui.ini` **não** entra no zip de propósito (decisão 97).

### 1.6 Como se depura e loga

Não há depurador. O `OutputDebugString`/`DbgPrint` só sai pelo XBDM, que não está no console
(decisão 30). O que funciona, em ordem de valor:

1. **Log em arquivo ao lado do `.xex`** (`game:\<nome>.log`), módulo
   `CollectionUI/app/src/diario.cpp`:
   - `CreateFile` + `WriteFile` + **`FlushFileBuffers` a cada linha**. Com `FILE*`+`fflush` o fim
     do log se perdia no crash, e três rodadas de diagnóstico foram construídas sobre um log
     truncado (decisões 82 e 89).
   - `CRITICAL_SECTION`, porque mais de uma thread escreve (decisão 83).
   - `_vsnprintf` devolve -1 e **não termina a string** ao truncar; o código trata isso.
   - Canal de **detalhe** separado, desligado por padrão, ligado por `logDetalhe=1` no `.ini`
     (cada linha é uma ida síncrona ao disco).
   - `Abrir` trunca; `Reabrir` volta em append (para o caminho de erro depois de tentar lançar).
2. **Filtro de exceção** `AoMorrer` (`CollectionUI/app/main.cpp`, perto da linha 2199), instalado
   como primeira coisa do `main` com `SetUnhandledExceptionFilter`:
   - registra `ExceptionCode`, `ExceptionAddress`, `ContextRecord->Iar` (PC), `->Lr` (retorno), e
     `ExceptionInformation[0]/[1]` (leitura/escrita e **endereço acessado**);
   - resolve-se o endereço com o `.map` do link (`-MAP`);
   - cala o áudio com `IXAudio2::StopEngine` (sem isso o crash **pendurava o console inteiro**);
   - devolve `EXCEPTION_CONTINUE_SEARCH` (a doc avisa que `EXECUTE_HANDLER` costuma congelar).
   - Exemplo real (decisão 131): `CRASH code=0xC0000005 addr=0x823C4E2C Iar=0x823C4E2C
     Lr=0x820A0E1C escrevendo=0x41560914` resolveu corrupção de memória por banco lido durante
     rescan, sem desassemblar.
3. **Tela como canal de "funcionou?"**: o primeiro teste pinta a tela com uma cor por fase
   (`sdk/hello`, `app` v0, `hiddriver/tools/testeplugin`: amarelo começando, azul copiando, verde
   ok, vermelho falhou). Dá para ler do sofá. `main` nunca retorna: em falha entra em laço.
4. **Chaves no `.ini` editáveis por FTP** (`config::Ligado("som")`, `"anel"`) para bisseção sem
   recompilar. Útil, mas cada rodada devolve **um bit** (decisão 90).
5. **`XNotifyQueueUI`** (balão do sistema), declarado à mão, em `xav.lib`.
6. **Métricas no log**: `[N ms]` por fase do arranque, relatório a cada 5 s com quadro, cache e
   memória livre (`GlobalMemoryStatus`).
7. No hiddriver (plugin, contexto de kernel): log em anel sem CRT nos ganchos, formatado por outra
   thread; batimento a cada 5 s; arquivo de "guarda de boot" que desliga o plugin no boot seguinte
   se o anterior travou (`docs/threads-e-contextos.md`).
8. Análise estática offline: `hiddriver/port/xbx.py` (Python + capstone, PPC 32 big-endian) lê
   dumps de memória, faz busca por assinatura com curinga e desassembla. A venv
   `hiddriver/port/.venv` tem capstone 5.0.7 e pyusb (o Python do sistema não tem capstone).

### 1.7 O que roda no Linux e o que exige Windows

| Tarefa | Onde | Situação |
|---|---|---|
| Extrair o XDK | Linux (`cabextract`) | provado |
| Compilar C/C++, linkar, gerar `.xex` | Linux, Docker + wine | provado (dois projetos) |
| `imagexex -dump` (inspecionar `.xex`) | Linux, container | provado |
| Compilar shader com `fxc.exe` | Linux, container | **binário está na imagem, nunca foi usado** (o CollectionUI usou o `SimpleShaders.fxobj` pronto do XDK) |
| Codificar áudio XMA (`xma2encode.exe`) | provavelmente wine | **não está na imagem, nunca testado** |
| Deploy por FTP | Linux (Python `ftplib`, `curl`) | provado |
| Rodar e ver | só o console | provado; não há Xenia nem PCSX2 instalados na máquina |
| Depuração ao vivo, PIX, xbWatson, `xbcp`/`xbreboot` | Windows + XBDM no console | **não disponível** |
| XuiTool, editores do XDK, Visual Studio 2010 + 2019 (caminho "oficial" do hiddriver) | Windows | não usado; o `.sln`/`.vcxproj` do hiddriver existem mas são ignorados |
| Preparar HD/FATX em massa (`xbox-rgh-pipeline`) | Windows (PowerShell, FatXplorer) | fora do escopo do port |

### 1.8 Ferramentas da máquina (conferido agora)

Presentes: `docker` 29.2.1 (usuário no grupo `docker`, roda sem sudo), `cabextract`, `python3`
3.14.4, `curl`, `ftp`, `gcc`, `make`, `git`, `gh`. Imagens úteis: `collectionui-xdk:light`,
`python:3-slim`, `python:3.11-slim`.

Ausentes: wine no host (só no container), Xenia, PCSX2, `cmake`, `ninja`, `clang`, cross-GCC MIPS
ou PowerPC, `lftp`, `nmap`.

---

## 2. Aprendizados técnicos

Os números entre parênteses são das decisões de `CollectionUI/docs/decisoes.md`.

### 2.1 Compilador e build

- **`cl.exe` 16.00 é da geração VS2010.** C++03 com o pouco de C++0x do VC10 (`auto`, lambdas,
  `nullptr`, `static_assert`, referências rvalue). O hiddriver usa `nullptr` e compila. Em modo
  C é **C89** (conferido, ver 4.1 item 1).
- **`char` é signed** no `cl.exe` do XDK: `c >= 0xC0` sem cast para `unsigned char` é sempre falso
  e o código "existe mas nunca executa" (121).
- **`DECLSPEC_NORETURN` é levado a sério**: `return` depois de `XLaunchNewImage` dá `C4702
  unreachable code` e, com `-WX` em Release, quebra o build. O otimizador apaga tudo depois da
  chamada; diagnóstico tem de vir antes (36, 85).
- **Header que muda não recompilava nada** (140): regra `%.obj: %.cpp` sozinha montou um binário
  com duas definições da mesma struct (sizeof diferente entre `.obj`) e deu crash lendo
  `0xFFFFFFF8`. Correção: todo `.obj` nosso depende de todos os headers nossos e do `Makefile`
  (grosso de propósito; `-showIncludes` atravessa o wine com caminho Windows). E as regras de
  dependência têm de ficar **depois** do `all`, senão o alvo padrão vira o primeiro `.obj`.
- O `xtl.h` não traz várias constantes do Win32 de PC: `INVALID_FILE_ATTRIBUTES`,
  `INVALID_SET_FILE_POINTER`, `MB_ERR_INVALID_CHARS` (defina à mão ou compare com `0xFFFFFFFF`).
- Funções exportadas mas **sem header**: `XNotifyQueueUI` (`xav.lib`), `XSetLaunchData`,
  `XGetLaunchData`, `XGetLaunchDataSize` (`xav.lib`/`xapilib.lib`), `ObCreateSymbolicLink`,
  `ObDeleteSymbolicLink`, `RtlInitAnsiString` (`xboxkrnl`). Declara-se à mão com `extern "C"`.
  Para muito mais do kernel e do xam, o `hiddriver/hiddriver/xkelib/` traz headers e libs de
  terceiros (`xkelib.lib`, `kernelext.lib`, `xamext.lib`, `.def` por versão de kernel).
- Ponteiro global da ATG: `namespace ATG { D3DDevice *g_pd3dDevice; }` precisa do tipo
  `ATG::D3DDevice`; com o tipo global compila e **falha no link**.
- Arquivo-fonte com **BOM UTF-8**: literais largos `L"..."` saem certos, mas literais estreitos
  `"..."` são convertidos para a página de código de execução (Latin-1), **não** UTF-8 (74).
  Quem converte texto tem de aceitar as duas codificações (`EhUtf8()` + recuo Latin-1, 75).
- **Compilar em Release para o console.** `d3d9d.lib` espera ambiente de devkit com XBDM e pode
  falhar de um jeito que confunde o diagnóstico (31). Debug serve para pegar erro de API.
- O sqlite foi compilado como **C (`-TC`)** com `SQLITE_OS_OTHER=1`, `SQLITE_THREADSAFE=0`,
  `SQLITE_DISABLE_INTRINSIC`, `SQLITE_PTRSIZE=4`, `INFINITY=HUGE_VAL` e um VFS próprio
  (`app/src/vfs_xbox.c`). Prova de que C de terceiro grande compila.

### 2.2 Modelo de execução de um título

- `void __cdecl main()`. **Sair de `main` tira o título do ar**; em falha, entra em laço.
- `game:\` é a pasta de onde o `.xex` foi lançado. **Nada além de `game:` existe** até o próprio
  app criar apelidos: `dispositivos.cpp` faz `ObCreateSymbolicLink("\??\Hdd:",
  "\Device\Harddisk0\Partition1")` e `Usb0:`..`Usb2:` → `\Device\Mass0..2` (mesmo mapa do
  X-Store). `ObCreateSymbolicLink` devolve `S_OK` mesmo para dispositivo ausente (141).
- Os apelidos da FreeStyle (`Hdd1:`) **não** são do sistema.
- **Lançar outro executável** (`XLaunchNewImage`, `XContentLaunchImageFromFile`): não pode ser
  chamado da thread dona do device D3D (dispara-se de uma thread nova) e não pode haver I/O de
  disco pendente. Antes: parar threads de I/O com espera `INFINITE`, soltar texturas com
  `BlockUntilIdle`, parar o áudio, fechar o log (41, 50, 64). `XContentLaunchImageFromFile` às
  vezes só aceita o caminho na forma `\??\Hdd:\...` (`lancador.cpp`).
- **Voltar ao dashboard**: `XLaunchNewImage(NULL, 0)` (`NULL` = `XLAUNCH_KEYWORD_DASH`,
  `xbox.h:423`), depois da mesma desmontagem. Sair pela Guide com threads, áudio e 120 texturas
  vivos dava **tela preta** (84). Para o BT3 isso define a rotina de "sair do jogo".
- Launch data entre títulos: o formato muda por emulador (struct `'AUOA'/'ROMS'` do Aurora, ou
  string crua no RetroArch); não é universal (113).

### 2.3 Threads e núcleos

- Xenon: 3 núcleos × 2 threads de hardware = **threads 0..5** (pares 0-1, 2-3, 4-5).
  `XSetThreadProcessor(handle, n)` fixa a thread (cria suspensa, fixa, `ResumeThread`).
- **O XAudio2 mora nas threads 4 e 5** (`XAUDIO2_DEFAULT_PROCESSOR = XboxThread4|XboxThread5`,
  `xaudio2.h:181`). Pôr uma thread de I/O na 4 causou crash e som estourado só quando ela
  acordava (71). Desenho na 0, carregador na 2.
- Plugins do hiddriver criam threads de sistema com `ExCreateThread(...,
  EX_CREATE_FLAG_SUSPENDED | EX_CREATE_FLAG_SYSTEM | 0x18000424)` no núcleo 4 (`MakeThread`).
  Isso é para código de sistema, não para um jogo.
- **Nunca bloquear a thread de desenho.** A Guide e o teclado do sistema são **compostos por cima
  do quadro do título**: um título parado num `WaitForSingleObject(INFINITE)` congela o console
  inteiro, até a Guide (51). Operações assíncronas do sistema (`XShowKeyboardUI` etc.) se
  consultam por quadro com `XHasOverlappedIoCompleted`, e os buffers delas **não podem estar na
  pilha** (52).
- `XGetOverlappedResult` devolve **DWORD** (código de erro, `ERROR_SUCCESS` = 0), não `BOOL`.
  Guardar em `BOOL` inverte o teste.

### 2.4 Gráficos: D3D9 do 360 / Xenos

- Inicialização provada: `Direct3DCreate9(D3D_SDK_VERSION)`, `CreateDevice(0, D3DDEVTYPE_HAL,
  NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev)` com back buffer 1280×720
  `D3DFMT_X8R8G8B8`, `SwapEffect DISCARD`, `PresentationInterval ONE`. No 360 não há indireção
  COM: `CreateDevice` é método estático.
- **Não há pipeline fixo**: tudo é shader (vertex e pixel). O CollectionUI usou os shaders
  simples da ATG (`SimpleShaders.fxobj`, efeito pré-compilado do XDK). `SimpleShaders::Initialize
  (NULL, NULL)` **não** quer dizer "sem arquivo": carrega `game:\media\effects\simpleshaders.fxobj`
  e, sem ele, `FXLCreateEffect` recebe nulo e dá fatal crash.
- **Texturas são ladrilhadas (tiled) por padrão.** Dados lineares de PC (DDS) têm de ser criados
  com os formatos `D3DFMT_LIN_*` (ex.: `D3DFMT_LIN_DXT5`). Pedir `D3DFMT_DXT5` fazia o D3DX
  reconverter cada textura e havia crash determinístico na 45ª (78). Para o BT3 (texturas de GS,
  geralmente paletizadas/swizzled no PS2) isso significa: ou converter para o layout ladrilhado
  com `XGTileTexture`/`xgraphics`, ou usar os formatos `_LIN_`.
- `D3DXCreateTextureFromFileInMemoryEx` decodifica JPG, PNG, BMP, DDS e TGA de memória (111).
  Padrão usado: 1 mip, `D3DUSAGE_CPU_CACHED_MEMORY`, `D3DPOOL_DEFAULT`, tamanho explícito.
- **Ler cabeçalho só depois de confirmar o formato** (DDS em 12..19 é little-endian; para JPG é
  lixo) (122).
- ATG `DebugDraw`: não liga `D3DRS_ALPHABLENDENABLE` sozinho (56); não usar
  `DrawScreenSpaceTexturedRectColored` com textura `NULL` (amostra sampler vazio, 55).
- **Bug da ATG corrigido e versionado como patch**: `ATG::Font::DrawText` com
  `ATGFONT_TRUNCATED` escrevia **64 bytes além** da reserva do `BeginVertices` (ring buffer de
  vértices). Custou **oito rodadas** no console (86 a 94). Correção em
  `CollectionUI/toolchain/patches/atg.patch`, aplicada pelo `vendor-atg.sh`.
- `ATG::FatalError` faz `DebugSpew` (só XBDM) + `DebugBreak` + `exit`: **crash mudo**. O patch
  também o faz escrever no log antes (81).
- Fonte `Arial_16.xpr` da ATG vai só até U+20AC; o resto vira glifo 0 (quadradinho). Acima de
  0x100 ficam glifos de botão do controle.
- `imgui-xbox360` existe (trava em Dear ImGui 1.86, `DisplaySize` fixo em 720p), avaliado mas não
  usado; o CollectionUI desenhou com a ATG. Resoluções além de 720p saem do **scaler de hardware**.
- Escrita da CPU em memória que a **GPU** lê precisa de cuidado com cache: a GPU **não** faz snoop
  da L2; o South Bridge (decodificador XMA) faz (70). Relevante para vertex buffers dinâmicos do
  BT3.

### 2.5 Áudio: XAudio2 e XMA

Módulo `CollectionUI/app/src/som.cpp`, decisões 58 a 72 e 101:

- `.xma` (RIFF/WAVE, codec XMA2 `0x166`) toca **direto** no XAudio2: o hardware decodifica. O
  `fmt ` do arquivo é o `XMA2WAVEFORMATEX` que o `CreateSourceVoice` quer.
- **Cabeçalho RIFF é little-endian; o Xenon é big-endian.** `LocalizeXma2Format()`
  (`xma2defs.h:679`) troca no lugar. Os **dados XMA nunca se trocam** de ordem (doc *Audio Data
  and Endianness*); o byte-swap do `AtgAudio.cpp` só trata PCM e EXTENSIBLE. Ou seja: **PCM
  little-endian vindo do PS2 precisa de swap**; XMA não.
- **Dados XMA em memória física alinhada**: `XPhysicalAlloc(tam, MAXULONG_PTR, 0,
  PAGE_READWRITE)`. Alinhamento **0** (= página de 4 KB), não 2048 como as amostras: a doc exige
  potência de dois ≥ página (60, 80). Os arquivos do skin são todos `N × 2048 + 92` bytes.
- Reiniciar um som tocando com `Stop`/`Flush`/`Submit`/`Start` **não** recomeça: `Stop` é
  assíncrono e o buffer entra atrás (65). Consultar `GetState().BuffersQueued`.
- Mastering voice a 48 kHz reamostra 44,1 kHz; pico entre amostras **satura**. Volume global 0,32
  e ganho por efeito (72, 101).
- Falha de áudio **não derruba** o app; som é opcional (63). `som::Iniciar` idempotente (66).
- No crash, `StopEngine` (não `DestroyVoice`, que aloca e espera) (91).
- Laço de chunk RIFF: testar tamanho por **subtração** (`tam > total - p - 8`), nunca por soma,
  senão `0xFFFFFFF8` dá a volta e vira laço infinito antes do primeiro quadro (68).
- Libs: `xaudio2.lib` + `xmcore.lib` (Release), `xaudiod2.lib` + `xmcored.lib` (Debug).
- O XDK tem `xma2encode.exe` para gerar XMA no PC, e `xmahal.lib`, `xact3.lib`, `x3daudio.lib`;
  **nada disso foi usado**.

### 2.6 Memória

- 512 MB unificados; com D3D e texturas o CollectionUI via **~400 a 480 MB livres**.
- `XPhysicalAlloc` para o que hardware lê (XMA, potencialmente buffers de GPU).
- Tetos defensivos em leituras de arquivo (8 MB para capa, 1 MB para som): caminho errado pode
  cair num arquivo de 600 MB e a morte vem na alocação, sem log (116).
- Cache de texturas com LRU por id e teto (160); vazamento de textura derrubava ao rolar rápido
  (commit `fafcffd`). Contar memória livre no log periodicamente.
- Campo não inicializado vem com lixo da pilha e o defeito sai **intermitente** (130). Inicializar
  no construtor.
- No hiddriver: um app (título) lê memória do kernel a partir de `0x80040000`; **ler abaixo disso
  trava o console**.

### 2.7 Endianness e alinhamento

- PowerPC big-endian. Formatos de PC/PS2 (RIFF, DDS, a maior parte dos arquivos do PS2) são
  little-endian. Ler sempre **byte a byte** (`LerLE32`/`LerBE32` em `fsda.cpp` e `som.cpp`), nunca
  com cast de ponteiro:
  - a ordem dos bytes está errada;
  - **acesso não alinhado no PowerPC pode falhar**, não só ficar lento (61).
- Isso é o ponto mais pesado para o BT3: toda struct que o decomp lê crua de arquivo do PS2 (AFS,
  PAK, tabelas, animações) precisa de swap campo a campo ou de conversão offline dos dados.

### 2.8 Sistema de arquivos e caminhos

- API: subconjunto do Win32 (`CreateFile`, `ReadFile`, `SetFilePointer`, `GetFileSize`,
  `FindFirstFile`, `GetFileAttributes`, `CreateDirectory`, `DeleteFile`). **Sem POSIX** (`open`,
  `stat`, `opendir` não existem). `fopen`/`fprintf` da CRT existem.
- Caminhos com volume: `game:\...`, `Hdd:\...` (depois de montar), separador `\`. Sem diretório
  corrente (o VFS do sqlite devolve o caminho como veio).
- **Case-insensitive**: a XAPI desce para `NtCreateFile` com `OBJ_CASE_INSENSITIVE`. Listar
  `Roms`, `roms` e `ROMS` abriu o mesmo diretório três vezes (119). Uma grafia por nome.
- **FATX**: nome de arquivo/pasta **até 42 caracteres** e cuidado com arquivo **acima de 4 GB**
  (`xbox-rgh-pipeline/instrucoes_mover_hd.md`).
- `GetFileAttributes` devolve `true` para diretório também; conferir atributo.
- Datas não servem: relógio zerado (2005-11-22) (30, 142). Não usar data em lógica.
- Escrita: `fopen("w")` trunca na hora; queda de energia no meio deixa cluster cheio de zeros, e um
  arquivo de config com cauda nula derrubava o app em todo boot (139). Checar o retorno de
  `fclose` (disco cheio aparece ali). Considerar gravar em temporário e renomear.
- Leitor de arquivo que alimenta uma reescrita total: **linha que não entendeu, preserva** (107).
- Arquivo que outro processo pode reescrever (ex.: banco da FreeStyle durante rescan) se lê como
  não confiável: validar tamanhos, nunca confiar em terminador (131, 132).

### 2.9 Input e HID

- No título: `XInputGetState(porta, &e)` para **as quatro portas**, somando botões por OU e
  pegando o analógico mais desviado (79). `XUSER_INDEX_ANY` (0xFF) onde se pede usuário, porque o
  console pode estar **sem perfil** logado (53).
- Repetição de direção: zona morta 14000, espera inicial 380 ms, repetição 110 ms; D-pad e
  analógico pela mesma máquina.
- Botões que confirmaram algo dentro da Guide/teclado não podem vazar como botão novo: ignorar
  entrada enquanto a UI do sistema está aberta (54).
- **hiddriver**: controles de terceiros (DualShock 4, DualSense, EasySMX X05 e 8BitDo Ultimate C
  em DInput, mais o mapeador automático para outros HID) são registrados **dentro do xam**. Para
  o jogo, aparecem como controles comuns de `XInputGetState`. O port do BT3 **não precisa** fazer
  nada específico para eles. A partir da v0.6 o input também passa pelo `XInputdReadState` do
  kernel (ordinal 486), o que cobre jogos de Xbox original.
- Sem rumble no hiddriver.
- Efeito colateral conhecido: o reinício de USB do hiddriver (etapa 3) e o ciclo do receptor do
  8BitDo **derrubam o Wi-Fi interno** (issue #2 do fork). O Wi-Fi volta uns 10 s depois de tirar o
  receptor. Isso afeta o FTP.

### 2.10 Interface do sistema

- `XShowKeyboardUI(XUSER_INDEX_ANY, VKBD_LATIN_FULL | VKBD_HIGHLIGHT_TEXT, ...)`: assíncrono,
  buffers fora da pilha, consulta por quadro (`app/src/teclado.cpp`).
- `XNotifyQueueUI(XNOTIFY_GENERIC=3, XUSER_INDEX_ANY, prioridade alta=2, texto, NULL)` para
  balões. **De dentro de um processo de sistema** (plugin) ele é descartado em silêncio sem o patch
  no xam (ordinal 1183 +48, `0x409A` → `0x4800`); de um título funciona direto.

### 2.11 Kernel e plugins (só o que vale levar)

O port do BT3 é um **título**, então quase nada disto se aplica. Fica registrado para não ser
redescoberto:

- Plugin de sistema resolve apelidos em `\System??\`; `\??\` é o namespace dos títulos (3).
- `XexLoadImage` de plugin chamado da thread de um título dá `0xC0000022` (6).
- Código escrito em tempo de execução: `dcbst; sync; icbi; isync` por linha de **128 bytes**
  (`FlushCodeRange` em `hiddriver/Detours.cpp`). Necessário para qualquer patch de código em
  runtime.
- Endereços fixos por versão de kernel (17150 ≠ 17559/17489) e conferência byte a byte antes de
  patchar (`check17150()`).
- Nada de CRT nem espera dentro de gancho chamado pela enumeração USB (spinlock pego).

### 2.12 Método (o que mais economizou rodadas no console)

1. **Num crash sem explicação, fazer o programa dizer ONDE morreu** antes de adivinhar o quê (88).
   Filtro de exceção + `.map` fechou em uma sessão o que oito bisseções não fecharam.
2. **Confirmar que o log sobrevive ao evento** antes de tirar conclusão dele (89).
3. **Conferir como a referência faz antes de escrever, não depois de quebrar** (78): amostras do
   XDK (`Source/Samples/...`) e o fonte aberto da FreeStyle 2 foram a resposta várias vezes. Para o
   BT3 a referência equivalente é a amostra do XDK de cada subsistema.
4. Registrar a intenção **antes** de cada chamada arriscada, não só o sucesso.
5. Validar lógica pura no PC: `colecoes.cpp` foi compilado no host com um `xtl.h` falso e 39 testes
   (143). O arnês **não foi versionado**.
6. Prévia no PC espelhando o desenho do C++ chamada por chamada (`previa/`), para iterar layout
   sem o ciclo compilar → FTP → sofá.
7. Agente de revisão depois de cada bloco achou defeitos reais (105, 139, 143).
8. Numerar decisões em `docs/decisoes.md` com o porquê e o que foi recusado; registrar hipóteses
   erradas (87).
9. Versão `MAIOR.MENOR.CORREÇÃO` pelo que muda para quem usa; nota de release não afirma teste
   não verificado (102, 103).

### 2.13 O que não funcionou ou foi descartado

| Tentativa | Por quê |
|---|---|
| libxenon | bare-metal via XeLL, sem kernel nem `xam`; dashboards não reconhecem (12). Para um **jogo** isso seria possível, mas perde XAudio2/D3D do XDK e o lançamento pela FreeStyle |
| OpenXeChain (LLVM + SynthXEX) | imaturo ("MANY features are missing"), parado desde out/2025 (13) |
| Poda de DLLs do wine do upstream | quebra com wine atual |
| `WINEPREFIX` fixo e `--user` no Docker | wine recusa e `cl.exe` dá `D8037` |
| Log com `FILE*` + `fflush` | perde o fim no crash |
| `D3DFMT_DXT5` para DDS de PC | reconversão e crash; usar `_LIN_` |
| Bloquear à espera da UI do sistema | trava o console inteiro |
| `XPhysicalAlloc` com alinhamento 2048 | abaixo da página; crash sem rastro |
| Thread de I/O no núcleo de hardware 4 | colide com o XAudio2 |
| Reiniciar voz XAudio2 com Stop/Flush/Submit | som atrasado e acumulado |
| `XLaunchNewImage` com tratamento de erro depois | `NORETURN`, código apagado |
| Bisseção por chave de `.ini` como método principal | um bit por rodada, estatisticamente fraco |
| Makefile sem dependência de header | binário misto, crash |
| `XamLoaderLaunchTitle` | desnecessário, `XLaunchNewImage` basta |
| Debug build (`d3d9d`) no console de varejo | espera XBDM |
| Casamento aproximado de nomes (capas) | erra em silêncio; tabela à mão (115, 129) |
| Disco/apelido numa thread nova durante o boot (plugin) | travou a FreeStyle; esperar 30 s (hiddriver, regra 8) |

### 2.14 gh2dxu (o que dá para aproveitar)

Não é port de código: é o mod de dados *Guitar Hero II Deluxe Unified* (hmxmilohax), que compila
um `.ark` com `ninja` e copia um `default.xex` já pronto de `dependencies/platform_files`. Rodou no
Linux dentro de container (`/w` nos logs). Lições pequenas:

- ferramentas .NET de terceiros em Linux pedem `libicu`, ou
  `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` (14 falhas de `superfreq png2tex` em `build.log`);
- script que chama `git describe` dentro de container precisa de `safe.directory` (`build2.log`).
- O `superfreq png2tex --platform x360` existe como conversor Linux de textura para o formato
  Milo do 360; não serve diretamente ao BT3.

### 2.15 Regras de trabalho do Lucas (das memórias e do `xbx/CLAUDE.md`)

- Tudo em **português**, inclusive descrição de comando e relatório de subagente.
- **Sem travessão** em texto que ele lê.
- Commits atômicos, mensagem em português com o porquê, **sem `Co-Authored-By` nem
  `Claude-Session`** (ele reescreveu a história do hiddriver para tirar).
- `gh` e `push` pela conta pessoal: `GH_TOKEN=$(gh auth token --user Lucas-Tito) git push ...`;
  a conta ativa da máquina é a de trabalho (`lucastito-v360`) e deve continuar sendo.
- Pode subir build no console por FTP **sem pedir**; se demorar, parar e perguntar.
- Antes de apagar algo no console, **listar e identificar o conteúdo** (5 GB de DLC quase foram
  apagados por engano).

---

## 3. O que é reaproveitável diretamente para o BT3

| Item | Origem | Como usar no `bt3xbx` |
|---|---|---|
| XDK extraído | `~/Documentos/CollectionUI/sdk/XDK` | usar no lugar, por variável `XDK_DIR` (como o hiddriver faz). **Nunca** copiar para o repositório |
| Imagem Docker | `collectionui-xdk:light` | usar já. Para robustez: `docker save collectionui-xdk:light \| zstd > ...` como backup, e uma cópia do `Dockerfile` com o wine **fixado em tag** (`wine-11.18`) |
| `Dockerfile`, `dockerignore` | `CollectionUI/toolchain/` | copiar para `bt3xbx/toolchain/` como receita versionada, com a tag do wine fixada |
| `build.sh` | `hiddriver/tools/build.sh` (melhor que o do CollectionUI) | monta `lib/xbox` inteiro, devolve a posse, e tem a guarda das image flags do `.xex` |
| `Makefile` | `CollectionUI/app/Makefile` + padrões do `hiddriver/Makefile` | separar C (`-TC`) de C++ (`-TP`), libs por configuração, `-MAP`, dependência grossa de headers, regras depois do `all`, pasta por variante e carimbo de chaves |
| `diario` (log) | `CollectionUI/app/src/diario.{h,cpp}` | copiar quase sem mudança |
| Filtro de exceção | `CollectionUI/app/main.cpp`, `AoMorrer` | copiar; acrescentar dump de mais registradores (r1/sp, r3) e, se possível, uma pilha de chamadas lida de `r1` |
| Montagem de dispositivos | `CollectionUI/app/src/dispositivos.cpp` | copiar (o BT3 pode querer ler o ISO/dados de `Hdd:` ou `Usb0:`) |
| `.ini` de chaves | `CollectionUI/app/src/config.cpp` | copiar para opções de diagnóstico (renderização, áudio, log detalhado) |
| Áudio XMA/PCM | `CollectionUI/app/src/som.cpp` | base para a camada de áudio: leitura de RIFF segura, `XPhysicalAlloc`, `LocalizeXma2Format`, ganho, `Calar` |
| Leitura LE/BE byte a byte | `fsda.cpp` (`LerLE32`, `LerBE32`) | virar um header `endian.h` do projeto |
| Saída para o dashboard | `CollectionUI/app/src/lancador.cpp` | modelo de desmontagem + `XLaunchNewImage(NULL, 0)` de outra thread |
| Teclado e notificação do sistema | `teclado.cpp`, declaração de `XNotifyQueueUI` | para nome de save, avisos de erro |
| VFS de SQLite | `app/src/vfs_xbox.c` | não é necessário ao BT3, mas é o modelo de "camada de SO" para qualquer biblioteca C portátil |
| Teste de fogo | `sdk/hello`, `CollectionUI/app` v0 (commit `c7f5fae`), `hiddriver/tools/testeplugin` | primeiro `.xex` do BT3: D3D 720p, cor por fase, log, `main` sem retorno |
| ATG + patch | `toolchain/vendor-atg.sh` + `patches/atg.patch` | se usar `AtgFont`/`AtgDebugDraw` para HUD de depuração, **levar o patch junto** |
| Shaders prontos | `SimpleShaders.fxobj`, `Source/Samples/Graphics` | ponto de partida antes de compilar shader próprio com `fxc` |
| Disassembler PPC | `hiddriver/port/xbx.py` + `port/.venv` | analisar o `.xex`/dumps, achar endereço de crash junto do `.map` |
| `xkelib` | `hiddriver/hiddriver/xkelib/` | só se o port precisar de API não documentada do kernel/xam |
| Disciplina de FTP | `hiddriver/docs/ftp-no-xbox.md`, decisão 73 | escrever **`tools/subir.py` versionado** (descobrir IP, nome temporário, renomear, md5, `close()` sem `QUIT`, timeout curto) e `tools/log.py` (renomear e baixar o log) |
| Organização do repositório | CollectionUI | `docs/decisoes.md` numerado, `toolchain/` com receita e não conteúdo, `.gitignore` para `sdk/` e tudo que for do XDK ou da Sony/Bandai |

Layout sugerido para começar (inferência, a validar com o Lucas):

```
bt3xbx/
  docs/            decisoes.md, pesquisa/, viabilidade.md
  toolchain/       Dockerfile (wine fixado), dockerignore, README com as 3 armadilhas
  tools/           build.sh, subir.py, log.py, achar-console.sh, xex-check.py
  port/            camada Xbox: main, diario, crash, dispositivos, config, áudio, input, gfx
  src/             a decomp do BT3 (submódulo ou cópia), com patches/ se precisar alterar
  Makefile         CONFIG=Release|Debug, variantes por pasta
  build/           fora do git
```

---

## 4. Lacunas, riscos e perguntas em aberto

### 4.1 Riscos técnicos

1. **C da decompilação contra o compilador do XDK. Conferido agora** (arquivos de teste de uma
   linha compilados na imagem, no scratchpad, fora dos projetos): em modo C (`-TC`) o `cl.exe`
   16.00 é **C89**.

   | Construção | `-TC` (C) | `-TP` (C++) |
   |---|---|---|
   | declaração depois de comando no bloco | `C2143` | ok |
   | `for (int i = 0; ...)` | `C2143` | ok |
   | inicializador designado `{ .b = 2 }` | `C2059` | (não testado; C++03 não tem) |
   | `static inline` | `C2054`; `__inline` funciona | ok |
   | `#include <stdbool.h>` | `C1083`, não existe | `bool` nativo |
   | `#include <stdint.h>` | ok | ok |
   | comentário `//` | ok | ok |
   | `_byteswap_ulong` | (só aviso implícito) | `C3861`, não existe |

   Para troca de bytes o XDK oferece `__loadwordbytereverse` / `__storewordbytereverse`
   (instruções `lwbrx`/`stwbrx`) em `include/xbox/ppcintrinsics.h`.

   Decomps de PS2 costumam ser C99 escrito para GCC (`__attribute__`, `asm` MIPS, extensões).
   Saídas possíveis: compilar o C como C++ (`-TP`, com os custos de `void*` implícito, palavras
   reservadas como `new`/`class` e linkagem `extern "C"`), reescrever trechos para C89, ou testar
   o compilador alternativo 16.10 do `TechPreview`. **É a primeira decisão de arquitetura do
   port**, e precisa de um teste com arquivos reais da decomp.
2. **Endianness e alinhamento de todos os dados do jogo** (2.7). O código do decomp assume
   little-endian e lê structs cruas. Também assume ponteiro de 32 bits (ok no 360) e `int`/`long`
   de 32 bits (ok).
3. **Gráficos do GS → Xenos**: nunca se escreveu shader próprio nem se usou `fxc.exe` sob wine. Não
   há experiência com render targets, EDRAM (10 MB, *predicated tiling* acima disso), texturas
   paletizadas, blending do GS. Tudo novo.
4. **Áudio do PS2 (SPU2, ADPCM/VAG) → XAudio2**: só se tocou XMA pronto. Decodificar ADPCM em
   software para PCM (com swap) ou converter offline para XMA (`xma2encode.exe`, não testado sob
   wine e fora da imagem).
5. **Sem depuração ao vivo e sem emulador.** Ciclo de teste = compilar, sair para a FreeStyle,
   FTP, abrir à mão no sofá, ler log por FTP. Xenia não está instalado. Instalar `xbdm.xex` no
   DashLaunch daria `xbreboot`/`xbcp`/lançamento remoto e capturas, mas nunca foi testado neste
   console e mexe no `launch.ini`.
6. **Wi-Fi + hiddriver**: com o reinício de USB do hiddriver ou o receptor do 8BitDo, o Wi-Fi
   interno cai e o FTP some. Um cabo de rede eliminaria o problema.
7. **Imagem Docker não reprodutível** (wine do master). Fazer backup da imagem e fixar tag.
8. **Libs fora da imagem**: resolver montando `lib/xbox` inteiro (como o hiddriver).
9. **FATX**: nome até 42 caracteres e arquivo até 4 GB. Arquivos grandes do disco do BT3 (AFS)
   podem precisar ser divididos; nomes longos de arquivo do ISO precisam caber.
10. **Title ID e `xex.xml`**: o CollectionUI saiu com Title ID 0. Um jogo provavelmente quer Title
    ID próprio (aparece certo na FreeStyle, separa saves e conteúdo), `mediatypes` sem
    `<dvdxgd2/>`, e talvez configuração de pilha/heap. Nunca foi feito.
11. **Saves**: nenhum dos projetos usou `XContentCreate`/conteúdo de save; só arquivos em `game:\`.
12. **Desempenho**: nada foi medido de CPU além de arranque. O Xenon é in-order; código de PS2
    portado sem SIMD (VMX128 via `vectorintrinsics.h`/`xnamath.h`) pode ficar no limite.
13. **Script de deploy e empacotamento não existem versionados** (viviam no scratchpad).
14. **Licença e procedência**: XDK é proprietário (nunca no git, como já é regra), e os dados do
    BT3 são da Bandai Namco/Spike; o port deve ler os dados do disco do próprio usuário, não
    redistribuí-los. Se algum código da FreeStyle (GPLv3) ou de terceiros entrar, decidir
    conscientemente (17).

### 4.2 Perguntas para o Lucas

1. O console tem **cabo de rede** disponível, ou será sempre Wi-Fi?
2. Aceita instalar **`xbdm.xex`** (e talvez JRPC2/XRPC) no DashLaunch para depuração remota e
   lançar o `.xex` do PC? Isso mudaria o ciclo de teste mais que qualquer outra coisa.
3. Onde o BT3 vai morar no HD: `Hdd1\HOMEBREW\BT3\` (como o CollectionUI) ou `PORTS\`? E os dados
   do jogo, extraídos do ISO para uma pasta ao lado do `.xex`, ou lidos do ISO inteiro?
4. Vale instalar o **Xenia Canary** (via wine/Proton ou nativo) para encurtar ciclos de
   renderização, sabendo que o veredito é sempre no console?
5. Qual região/versão do BT3 é a do ISO que ele tem (NTSC-U, PAL, NTSC-J)? A decomp mira qual?
6. O repositório `bt3xbx` vai para o GitHub pessoal (`Lucas-Tito`)? Público ou privado, dado que
   envolve decompilação?
7. Mantemos o mesmo modelo de documentação (`docs/decisoes.md` numerado, em português, sem
   travessão)?
