# F8, etapa 5: o pacote da release, o README do usuário e as regras das issues

[Escopo da F8](00-escopo.md), seções 1.1 e 4 | [Decisão 33](../../decisoes.md) | [Pendências](pendencias.md)

[Lido] Trabalho de 29/10/2026 no branch `claude/f8-publicacao`, sobre a main `e005ac94`. Só Linux: nada compilado
no XDK, nada enviado ao console, nada criado no GitHub (sem `gh`, sem push). A legenda é a do escopo: **[medido]**
é saída de comando desta rodada, **[lido]** é o que está no código e nos documentos, **[estimado]** é escolha ou
proposta ainda sem prova.

## 1. O que foi feito

| Entrega | Arquivo | O que é |
|---|---|---|
| Versão | `port/x360/package/versao.env` | `VERSION=1.0.0`, o único lugar da versão: nome do zip, pasta raiz e README |
| Pacote | `port/x360/tools/pacote.py` | Monta `bt3-x360-<versão>.zip` por lista branca, reprodutível, e só deixa o zip se o verificador aprovar |
| Verificador | `port/x360/tools/pacote_confere.py` | Sai com 1 em qualquer coisa fora da regra (seção 3); aceita o zip ou a pasta aberta |
| Prova do verificador | `port/x360/tools/teste_pacote.py` | Planta um estrago de cada tipo numa cópia e exige saída 1 com o motivo; `--mutantes` estraga o próprio verificador |
| README do usuário | `port/x360/package/README.txt` | Inglês, texto puro, os oito pontos do escopo (4.2) e a decisão 33; vai como `README.txt` no pacote |
| Licenças | `port/x360/package/THIRD-PARTY.txt` | Código do jogo, Tenkaichi3Decomp (MIT, base de 10/10), newlib/fdlibm, Kenney (CC0, crédito), XDK |
| Issues | `.github/ISSUE_TEMPLATE/crash_report.yml`, `bug_report.yml`, `config.yml` | Formulários com campos obrigatórios; issue em branco desligada |
| Regras | `.github/CONTRIBUTING.md`, `.github/labels.yml` | O que não se aceita, o que acontece com quem descumpre, os 13 rótulos (só em arquivo) |

[Lido] O README da raiz (do upstream) não foi tocado: a abertura do repositório ainda não foi planejada (escopo,
seção 5).

### 1.1 O que vai no zip

```
bt3-x360-1.0.0/
  README.txt                              port/x360/package/README.txt, com {{VERSION}} trocado pela versão
  LICENSE.txt                             LICENSE da raiz (MIT, z3xox and contributors)
  THIRD-PARTY.txt                         port/x360/package/THIRD-PARTY.txt
  licenses/COPYING.NEWLIB.txt             port/third_party/newlib_libm/COPYING.NEWLIB, sem mudança
  licenses/Kenney-Input-Prompts-License.txt   port/x360/arte/kenney/License.txt, sem mudança
  bt3-x360/default.xex                    da pasta da build (build_f2.sh)
  bt3-x360/default.dat                    da mesma pasta
  bt3-x360/default.swap                   da mesma pasta
  pc-tools/port/x360/tools/convert_iso.py da árvore dos fontes
  pc-tools/port/x360/conv/*.c, *.h        todos os fontes do conversor (o conv_tool), da mesma árvore
  SHA256SUMS.txt                          de todos os outros, no formato do sha256sum
```

[Lido] Por que o `pc-tools/port/x360/...`: o `convert_iso.py` acha os fontes do conversor por
`parents[3] / "port/x360/conv"` e procura o `conv_tool` em `port/build/conv_tool`, e guarda na marca o SHA256 de
`port/x360/conv/*.[ch]` mais o dele próprio. Mantendo o leiaute do repositório, o script vai **sem nenhuma mudança**
e a identidade dos fontes na marca sai igual à do commit. Todos os fontes vão (inclusive `conv_jogo.c`, que não
entra no `conv_tool`) pelo mesmo motivo. O console não confere essa identidade (só a magia, a versão, o tamanho e a
origem, `tem_marca` em `iso360.c`), então um `conv_tool` compilado pelo usuário serve.

[Lido] O `convert_iso.py` só importa a biblioteca padrão do Python; o `pacote.py` confere isso pela árvore sintática
e recusa se um dia ele importar outra coisa (teria de entrar no pacote).

[Lido] Fica fora: qualquer ISO, AFS ou dado extraído, `environment.txt`, `saves`, logs, `crash.txt`, capturas,
`.map`, `.pdb`, `.exe`, o `conv_tool` compilado (o usuário compila; a decisão 33 não pediu binário pronto). O `.map`
e o `.pdb` da build da release ficam com o Lucas para resolver os `crash.txt` (escopo, 4.1).

## 2. Como gerar e verificar

Na raiz da árvore, depois do `build_f2.sh` (o zip vai para `build/x360/pacote/`, fora do git):

```sh
python3 port/x360/tools/pacote.py --estrito
python3 port/x360/tools/pacote_confere.py build/x360/pacote/bt3-x360-1.0.0.zip --fontes .
python3 port/x360/tools/teste_pacote.py build/x360/pacote/bt3-x360-1.0.0.zip --trabalho ~/pacote-estragos --mutantes
```

- `--estrito` (a release) exige a build, os fontes do conversor e os textos da mesma árvore, sem diferença para o
  commit. Sem ele (ensaio) o script aceita uma build de outra árvore e avisa que os commits diferem.
- `--fontes .` confere, além das regras, que os fontes do conversor, o `convert_iso.py`, as licenças e o README do
  zip são iguais aos da árvore (é o que o `--estrito` também roda depois de gravar).
- Reprodutível: entradas em ordem, data fixa (01/01/2026), permissões fixas, compressão 9. Dois empacotamentos do
  mesmo commit dão o mesmo zip byte a byte (seção 4).
- Para conferir a pasta já aberta (por exemplo a que o Lucas vai copiar ao console), o verificador aceita a pasta
  `bt3-x360-1.0.0` no lugar do zip.

## 3. As regras do verificador

| Regra | O que recusa |
|---|---|
| Caminho | Fora da pasta raiz `bt3-x360-<versão>/`, absoluto, com `..`, link simbólico, pasta explícita, nome repetido |
| Nomes proibidos | `.iso` e outras imagens, `.bin`/`.afs`, ELF e `SLUS_216.78`, `.pss`/`.adx` e outros sons, `environment.txt`, `ambiente*.txt`, `.log`, `output.txt`/`saida.txt`, `crash.txt`, capturas e imagens, `saves/`, `BASLUS-*`, `icon.sys`, `.map`/`.pdb`/`.exe`, temporários, arquivos compactados |
| Lista branca | Qualquer arquivo fora da lista fixa e do padrão `pc-tools/port/x360/conv/<nome>.[ch]` |
| Teto | `default.xex` até 16 MiB, `.dat` e `.swap` até 1 MiB, textos de 16 a 512 KiB, total de 32 MiB (o pacote de hoje tem 2 MiB aberto) |
| Assinaturas | ELF, AFS, MPEG (PSS), PNG, JPEG, BMP, PPM, GIF, ZIP, 7z, RAR, gzip e RIFF no início; ISO 9660 (`CD001`), AFS, ELF e MPEG em todo início de setor de 2.048 bytes; o `(c)CRI` do ADX em qualquer ponto |
| Formato | `default.xex` é XEX2 com o Title ID de `titulo.env` (`0xFFFF4254`) e as mídias com o HD e sem nenhuma de disco; `default.dat` e `.swap` com a magia (`BT3D`, `BT3S`) e o tamanho que o cabeçalho manda; textos UTF-8 sem NUL |
| Textos do usuário | README e THIRD-PARTY sem nome de desenvolvimento (`environment.txt`, `ambiente`, chaves `BT3_`, `bt3xbx.log`, `output.txt`, `/home/`) e sem travessão; o README com a versão |
| Somas | `SHA256SUMS.txt` lista exatamente os outros arquivos, cada hash confere |
| Obrigatórios | Os dez arquivos fixos e os fontes mínimos do conversor (`conv.c`, `conv.h`, `conv_tool.c`, `icones360.c`, `icones360_arte.h`) |

[Estimado] As mídias: o `.xex` desta build tem `0xFF000301` (HD, USB, rede e todos os pacotes, o `xex.xml`). O
verificador exige o bit do HD e nenhum dos bits 0x2 a 0x10, que pela tabela usada pelo Xenia são as mídias de
DVD; não se provou que o `<dvdxgd2/>` liga exatamente o 0x2. A prova definitiva continua o `imagexex -dump` da
checagem final (escopo 4.4, passo 1).

## 4. A prova (29/10/2026)

[Medido] Build de ensaio: `/home/lucas-tito/Documentos/bt3xbx-teste/build/x360/f2` (commit `968bc501`, o
`teste/queda-console` com a conversão no console e a ISO versão 3), com o conversor da mesma árvore e os textos
deste branch. Nada da ISO entrou: só o `.xex`, o `.dat` e o `.swap` da build.

```sh
S=/tmp/claude-1000/-home-lucas-tito-Documentos-bt3xbx/8701d9b1-1a29-4a30-a44b-7a3d2a1a9551/scratchpad
python3 port/x360/tools/pacote.py --build ~/Documentos/bt3xbx-teste/build/x360/f2 --saida $S/pacote1
python3 port/x360/tools/pacote.py --build ~/Documentos/bt3xbx-teste/build/x360/f2 --saida $S/pacote2
cmp $S/pacote1/bt3-x360-1.0.0.zip $S/pacote2/bt3-x360-1.0.0.zip
python3 port/x360/tools/teste_pacote.py $S/pacote1/bt3-x360-1.0.0.zip --trabalho $S/estragos --mutantes
```

| Conferência | Resultado [medido] |
|---|---|
| Zip | `bt3-x360-1.0.0.zip`, 1.663.870 bytes, 34 arquivos, 2.100.390 bytes abertos, SHA256 `0b6b1508...4b01d886` |
| Dois empacotamentos | Iguais byte a byte (`cmp` sem diferença) |
| `sha256sum -c SHA256SUMS.txt` na pasta aberta | 33 de 33 `OK` |
| PC: `gcc -std=c89 -O2` dos fontes do pacote, como no README | Compila; `conv_tool --lista-conversores` dá 3.327 entradas (as da versão 3) |
| PC: `convert_iso.py` do pacote com um arquivo de zeros de 1 MB | `convert_iso: ISO truncada ou de tamanho incompatível`, saída 1, nenhuma saída gravada |
| Verificador na pasta aberta e limpa | Saída 0 |
| Verificador na pasta onde se compilou o `conv_tool` | Saída 1: `pc-tools/port/build/conv_tool: fora da lista branca` e `assinatura de ELF no início` (achado sem querer: a pasta do usuário depois da conversão no PC não é mais o pacote) |
| Verificador na pasta com um `environment.txt` plantado | Saída 1, `proibido: environment.txt` |
| `--fontes` numa árvore igual e numa com `som.c` alterado e um `novo.h` | 0 na igual; 1 na estragada, `som.c: diferente` e `novo.h: falta` |
| `--estrito` com a build de outra árvore | Recusa: `a build, os fontes e os textos têm de vir desta mesma árvore` |

[Medido] `teste_pacote.py`, saída 0. Cada estrago numa cópia do zip; o `SHA256SUMS.txt` refeito em todos, menos
nos dois casos que são dele, para só a regra do caso poder acusar:

| Estrago plantado | Saída | Motivo dado pelo verificador |
|---|---|---|
| (intacto) | 0 | `pacote conferido: 34 arquivos, 2100390 bytes, nenhum fora da regra` |
| `bt3-x360/bt3.iso` (cabeçalho ISO 9660 inventado) | 1 | `proibido: imagem de disco` |
| `bt3-x360/bt3-x360.iso` | 1 | `proibido: imagem de disco` |
| ISO com o nome `pc-tools/port/x360/conv/tabela.h` | 1 | `assinatura de ISO 9660 no setor 16` |
| `bt3-x360/pzs3us0/00004.bin` (cabeçalho AFS) | 1 | `proibido: arquivo .bin ou AFS do jogo` |
| AFS no lugar do `default.swap` | 1 | `assinatura de AFS no início` |
| `bt3-x360/SLUS_216.78` (ELF) | 1 | `proibido: executável do PS2 (ELF)` |
| `pc-tools/slus.elf` | 1 | `proibido: executável do PS2 (ELF)` |
| `bt3-x360/ZS3USOP.PSS` | 1 | `proibido: filme ou som do jogo` |
| `bt3-x360/environment.txt` | 1 | `proibido: environment.txt (configuração de desenvolvimento)` |
| `bt3-x360/ambiente_brilho.txt` | 1 | `proibido: ambiente*.txt (configuração de desenvolvimento)` |
| `bt3-x360/bt3xbx.log` | 1 | `proibido: log` |
| `bt3-x360/output.txt` | 1 | `proibido: saída de diagnóstico` |
| `bt3-x360/crash.txt` | 1 | `proibido: crash.txt` |
| `bt3-x360/shots/0001.png` | 1 | `proibido: captura ou imagem` |
| `bt3-x360/saves/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3` | 1 | `proibido: save` |
| `bt3-x360/default.map` | 1 | `proibido: artefato de build fora do pacote` |
| `bt3-x360/default.pdb` | 1 | `proibido: artefato de build fora do pacote` |
| `NOTES.txt` na raiz | 1 | `fora da lista branca` |
| Um byte do README trocado, somas antigas | 1 | `README.txt: SHA256 não bate com o SHA256SUMS.txt` |
| Linha do `LICENSE.txt` tirada das somas | 1 | `LICENSE.txt: fora do SHA256SUMS.txt` |
| `default.xex` com 20 MiB a mais | 1 | `acima do teto de tamanho (22495232 > 16777216 bytes)` |
| Title ID do `.xex` trocado para `0x41560001` | 1 | `Title ID 0x41560001, esperado 0xFFFF4254` |
| `default.swap` tirado | 1 | `default.swap: falta no pacote` |
| README citando `environment.txt` | 1 | `texto do usuário cita nome de desenvolvimento: 'environment.txt'` |
| README com travessão | 1 | `texto do usuário com travessão` |
| Link simbólico no zip | 1 | `link simbólico` |
| `bt3-x360-1.0.0/../fora.txt` | 1 | `caminho com '..', '.' ou parte vazia` |

[Medido] `--mutantes`: cinco cópias estragadas do verificador, cada uma acusada pelo teste (a bateria acima deixa de
bater): sempre sai com 0; não confere os hashes; sem os nomes proibidos; sem as assinaturas; sem o teto.

O log está em `$S/teste_pacote.log` (scratchpad desta sessão, fora do git).

## 5. Escolhas feitas sem base, para o Lucas decidir

**Respostas do Lucas (29/10/2026):** 1, sem resposta específica (fica como está); 2, as mensagens do `convert_iso.py`
traduzidas para o inglês (o README diz "all done"); 3, Linux e WSL bastam; 5 e 6, a linha "Copyright (c) 2026 Lucas
Tito (Xbox 360 port)" no `LICENSE`; 8, pediu explicação antes de decidir; 9, ok.

1. **O leiaute `pc-tools/port/x360/...`** no zip, para o `convert_iso.py` ir sem mudança. A outra saída é mudar o
   script para achar os fontes ao lado dele (muda a identidade dos fontes na marca, que o console não confere).
2. **As mensagens do `convert_iso.py` estão em português** ("tudo certo", "ISO truncada..."). O README avisa e
   traduz o "tudo certo". A pendência 3 pede inglês em todo aviso que o usuário vê; traduzir antes da release seria
   um item pequeno (muda só o hash dos fontes na marca).
3. **A conversão no PC só para Linux e WSL.** O script aceita Windows nativo, mas exige o `gcc` (MinGW) e não foi
   provado lá; o macOS é recusado pelo próprio script (`publicar`).
4. **O endereço do repositório** (`https://github.com/Lucas-Tito/bt3xbx`) nos formulários, no `config.yml`, no
   CONTRIBUTING e no README. Se o nome mudar na abertura, trocar nos quatro.
5. **Os créditos do port do 360** no README: "Lucas Tito, https://github.com/Lucas-Tito/bt3xbx". O README do upstream
   diz "This project was made with the help of AI"; o do 360 não diz nada sobre isso.
6. **O `LICENSE.txt`** é o da raiz ("Copyright (c) 2026 z3xox and contributors"). Pode-se acrescentar uma linha de
   copyright do port do 360.
7. **As bibliotecas do XDK** dentro do `.xex`: o THIRD-PARTY só diz que são da Microsoft e que nada do kit vai no
   pacote além do que a build põe no `.xex`. É a prática dos homebrews do 360, mas não há licença a citar.
8. **Regras das issues:** 14 dias para o `crash.txt` e para a informação faltante; pedido de recurso não aceito
   como issue por enquanto; emuladores (Xenia) fora; bloqueio no caso de pirataria ou abuso. Nada disso tinha base
   escrita.
9. **Textos do README que dependem de builds ainda em branch ou por fazer:** a vibração ("works as in the original
   game, and can be turned off in the game's options", decisão 33 item 4, branch `claude/f8-vibracao` sem commit
   ainda), a saída pela Guide (pendência 11, sem prova), a caixa de queda e o `crash.txt` (branch
   `teste/queda-console`), e a lista de modos do formulário de queda (os nomes dos modos do jogo em inglês).
10. **O que o `crash.txt` leva:** o README diz que não há dado pessoal (é o carimbo, os registradores e a pilha,
    `04-queda-e-logs.md`); conferir num `crash.txt` real.

## 5a. A triagem automática das issues (29/10/2026, pedido do Lucas)

O Lucas não quis fechar nada por prazo (os 14 dias saíram das regras e dos rótulos) e pediu duas automações:

1. **Queda sem `crash.txt` fecha na hora.** O formulário "Crash report" põe o rótulo `crash`; a ação
   (`.github/workflows/issues-x360.yml`, lógica em `.github/scripts/triagem.js`) procura no texto o cabeçalho que o
   jogo grava (`BT3 crash report`, `Build:` e `IAR:`, de `port/x360/src/queda360.h` do branch da queda) ou um anexo
   `crash*.txt` do GitHub. Sem nenhum dos dois: `needs-crash-txt`, comentário explicando e fechada. Se o autor editar a
   issue ou comentar com o arquivo, ela reabre sozinha. Quem cuida do repositório pode reabrir à mão sem o arquivo.
   O "Bug report" (defeito sem queda) não exige `crash.txt`.
2. **Rótulo de motivo dá a resposta pronta:** `duplicate`, `not-supported`, `off-topic`, `known-issue`,
   `feature-request` (rótulo novo) e `wontfix` comentam e fecham; `piracy` comenta, fecha e tranca (apagar a issue e
   bloquear a conta continuam à mão, o token da ação não pode); `needs-crash-txt` e `needs-info` só comentam. A mesma
   resposta não sai duas vezes (marca escondida no comentário).
3. **Moderação** (31/10/2026; a preocupação do Lucas é spam, palavrão e baixaria): issue ou comentário de quem não é
   dono, membro nem colaborador passa por uma lista de palavrões e ofensas em inglês e português (`OFENSAS`), outra de
   propaganda (`PROPAGANDA`) e um teto de 4 links que não sejam anexos do GitHub. O texto se compara sem acento, em
   minúsculas, com as trocas de letra comuns (`sh1t`, `p0rra`) e por palavra inteira (`Scunthorpe`, `computador` e
   `disputa` passam). Issue pega: rótulo `spam` (novo, o 15º), fechada e trancada, sem resposta. Comentário pego: oculto
   pelo `minimizeComment` como abuso (ofensa) ou spam. Quem cuida revê pelo rótulo e pelos ocultos e desfaz à mão.

**Prova** (`node .github/scripts/teste_triagem.js`, GitHub falso): 28 casos desde a moderação, `tudo certo`; a
moderação vista falhando com sete cópias estragadas (sem a chamada, sem a isenção do dono, sem cada uma das duas
fronteiras de palavra, sem tirar o acento, sem o teto de links, classificação errada do oculto), todas com saída 1. Vista falhando com seis
cópias estragadas da triagem, cada uma acusada com saída 1: aceitar qualquer texto como `crash.txt` (4 falhas), anexo
de qualquer nome (1), não fechar a queda sem arquivo (1), não trancar a pirataria (2), reabrir por comentário de
qualquer pessoa (2), comentar a mesma resposta de novo (2). Não roda no GitHub de verdade até o repositório subir;
na primeira issue real, conferir a execução na aba Actions.

**Fechado em 31/10/2026** (pendência 16): o Lucas manteve os formulários e a triagem e pediu a moderação (item 3).

## 6. O que o Lucas deve auditar

| Arquivo | O que conferir |
|---|---|
| `port/x360/package/versao.env` | A versão `1.0.0` e que nenhum outro lugar a repete |
| `port/x360/package/README.txt` | Ler inteiro como usuário: requisitos e hashes, os passos de instalação (nome `bt3.iso`), os textos das caixas da conversão (iguais ao `iso_avisos.h` aprovado), o caminho do PC, controles, saves, queda, os problemas conhecidos e os créditos; o inglês e a ausência de travessão |
| `port/x360/package/THIRD-PARTY.txt` | Que nenhuma parte de terceiros falte (newlib e Kenney sim; ImGui e xxhash não entram na build do 360, conferido pela lista de objetos de `bt3xbx-teste`); a frase sobre a base MIT de 10/10; a seção do XDK |
| `port/x360/tools/pacote.py` | A lista do que entra (`montar`), o `--estrito` e que o zip vai para `build/x360/pacote` |
| `port/x360/tools/pacote_confere.py` | As listas `LISTA`, `PROIBIDOS`, `INICIOS`, `SETORES` e os tetos; se falta algum tipo de arquivo do jogo ou do desenvolvimento |
| `port/x360/tools/teste_pacote.py` | Os 27 estragos e os 5 mutantes; rodar uma vez para ver a tabela |
| `.github/ISSUE_TEMPLATE/crash_report.yml` | Os campos obrigatórios (versão, `crash.txt`, o que fazia, modo, personagens, estágio, conversão, frequência, console) e as três caixas de marcar |
| `.github/ISSUE_TEMPLATE/bug_report.yml` | Os mesmos campos para defeito sem queda, com o ajuste de vídeo da TV |
| `.github/ISSUE_TEMPLATE/config.yml` | A issue em branco desligada e os dois links (README, upstream do PC) |
| `.github/CONTRIBUTING.md` | O que não se aceita, os prazos, o que acontece com quem descumpre |
| `.github/labels.yml` | Nomes, cores e descrições dos 14 rótulos (com `feature-request`), para criar no GitHub na abertura |
| `.github/scripts/triagem.js` e `.github/workflows/issues-x360.yml` | Os textos das respostas prontas (em inglês) e o que cada rótulo faz; a regra do `crash.txt` |
| `docs/x360/plano/F8/06-publicacao.md` | Este documento: a prova da seção 4 e as escolhas da seção 5 |

[Estimado] Antes da release, além da checagem final do escopo (4.4): rodar o `pacote.py --estrito` na árvore do
commit marcado, o `teste_pacote.py --mutantes` sobre o zip dela, e copiar ao console a pasta `bt3-x360` do zip
aberto para o teste do README ao pé da letra (escopo 1.1, Documentação).

## 7. A publicação da 1.0.0 (1/11/2026)

Publicada em https://github.com/Lucas-Tito/bt3xbx/releases/tag/bt3-x360-v1.0.0 pelo Lucas, primeiro como rascunho e
depois aberta. A tag ficou `bt3-x360-v1.0.0` (o Lucas quis o "bt3" no nome, igual ao do zip; continua fora do padrão
`v*` do workflow do upstream, que está desligado no fork), sobre o commit 37ae9db7, o da build do zip (carimbo
`37ae9db7 diff:da39a3ee5e`). O README do repositório passou a apontar para a página de releases (a6fed116).

| Item | Valor |
|---|---|
| Zip | `bt3-x360-1.0.0.zip`, 1.697.648 bytes, SHA256 `1ae881296f8f3249447b9f00a2164b0ed764ddf452dc55e7c738486c9565565d` |
| `default.xex` | md5 `1ef9cb5ca8cbb869f3148c62779a3aa0` |
| ISO convertida por esta release (console ou PC) | MD5 `8d32772bcd10c0d9385bbb51d810b41e` (a marca guarda a identidade dos fontes dos conversores) |
| `.map` da build | guardado fora do git, em `build/x360/pacote/default-37ae9db7.map`, para ler os `crash.txt` |

Conferência depois de publicada: o zip baixado da página pública, sem login, deu o mesmo SHA256, e os 37 arquivos
do `SHA256SUMS.txt` conferiram. Os 15 rótulos do `labels.yml` foram criados no GitHub antes (o formulário de queda
dependia do `crash`).

Checagem final no console com essa build (1/11/2026, todas aprovadas pelo Lucas): instalação numa pasta nova a
partir do zip, conversão no console em 9 min 21 s no HD interno com a tela nova, caixas de ISO ausente e ilegível
(textos revistos), queda de teste (caixa, `crash.txt` com versão, build e cadeia, volta à FreeStyle sem laço) e a
sessão longa com tela dividida e saída pela Guide. Sem ISO PAL para testar a caixa de versão não suportada.

**Problemas conhecidos em dois lugares** (1/11/2026, pedido do Lucas): a seção 6 do `README.txt` do pacote e a seção
"Known issues" do `.github/CONTRIBUTING.md`, que pede para não reportar o que já está lá. Toda mudança numa vale para
a outra no mesmo commit; o item "nem todo conteúdo foi jogado no console" fica fora da lista do CONTRIBUTING, porque
uma queda nesse conteúdo deve ser reportada.

**O README.txt de dentro do zip 1.0.0** ficou com a frase antiga da lentidão ("the planet explosion after a Final Flash
on Broly's stage... split-screen battles when many effects are on screen at once"), corrigida pelo Lucas no repositório
e nas notas da release em 1/11/2026 (a4296757). Ele decidiu não trocar o zip publicado: a frase nova entra no pacote da
próxima versão.

## 8. A publicação da 1.0.1 (1/11/2026)

Publicada em https://github.com/Lucas-Tito/bt3xbx/releases/tag/bt3-x360-v1.0.1, a tag sobre 040b0fa4 (carimbo
`040b0fa4 diff:da39a3ee5e`). Motivo: um tester num console com Bad Avatar, abrindo pela Aurora, via a pergunta de
conversão (a caixa do sistema, `XShowMessageBoxUI`) abrir e fechar sozinha, e o jogo voltava ao painel; também não
aparecia nenhum log com `BT3_LOG=1`, o que ainda não tem explicação (suspeita: o jogo não grava na pasta dele). Esperar
os controles soltarem antes da caixa não resolveu (build de teste revertida, ramo apagado).

- A pergunta de conversão e as mensagens de sucesso e falha passaram a ser desenhadas pelo jogo na tela de terminal
  ([03-conversao-console](03-conversao-console.md)), com o controle lido direto e só botão solto e apertado de novo
  valendo; conferida pelo Lucas no console dele. Ainda não testada no Bad Avatar.
- O problema conhecido novo (README do pacote e CONTRIBUTING): no Bad Avatar as caixas do sistema que sobraram (ISO
  ausente ou ilegível, queda) podem fechar sozinhas.
- O `.dat`, o `.swap` e os conversores não mudaram: a ISO convertida pela 1.0.0 serve, sem reconversão.

| Item | Valor |
|---|---|
| Zip | `bt3-x360-1.0.1.zip`, SHA256 `cbc35dd16d5e74e68316915bacd54813d9917e550a26de29a2e2d27a0345ba85` |
| `default.xex` | md5 `f445c8ac4ac927085433c61e4e2362d4` |
| `.map` da build | `build/x360/pacote/default-040b0fa4.map`, fora do git |

Conferência: o zip baixado da página pública deu o mesmo SHA256 e os 37 arquivos do `SHA256SUMS.txt` conferiram.

## 9. A publicação da 1.0.2 (3/11/2026)

Publicada em https://github.com/Lucas-Tito/bt3xbx/releases/tag/bt3-x360-v1.0.2, a tag sobre ea439bb5 (carimbo
`ea439bb5 diff:da39a3ee5e`; o código é o da build 963e690f testada no console, só com a versão trocada).

- **O save em pendrive e HD externo** (issues #1 e #2): o FAT32 do 360 grava nome com dois pontos sob outro nome
  (`icon.sys.tmp` virou `icon.tmp`), e a troca de nome seguinte falhava com "arquivo não encontrado". Os temporários
  passaram a `NOME_tmp` e `NOME_new` (as sobras antigas `.tmp`/`.new` ainda se resolvem). Achado pela build de
  diagnóstico (ramo `diag-save`, não publicado) e testado no console no pendrive e no HD interno.
- **Quem tentou salvar no externo antes** fica com a pasta do save vazia e vê "Load failed" uma vez; confirma, faz o New
  Game e salva (visto no console). Uma limpeza automática dessas pastas chegou a entrar (e2c6c5a1) e foi desfeita
  (ea439bb5): o Lucas a achou destrutiva demais para um caso que acontece uma vez.
- **Importação e exportação de saves do PS2 em `.psu`**, em `saves\import` (com `done`, `skipped`, `failed` e o backup
  em `saves\backup\NNN`) e `saves\export`. Testadas no console com o save 100% do JKLX (GameFAQs): tudo liberado no
  jogo; o `.psu` exportado bateu com o original no WSL, fora o som (0x16A4), a soma (0x0004) e o relógio do jogo
  (0x0A0C, que o menu avança). Formato e provas em `docs/x360/formatos/psu.md`, `save-larguras.md`,
  `save-conversao.md` e `save-importacao.md`.
- O `.dat` e o `.swap` não mudaram desde a 1.0.0: quem atualiza troca os três arquivos e não reconverte a ISO.

| Item | Valor |
|---|---|
| Zip | `bt3-x360-1.0.2.zip`, SHA256 `026b5f5970856505fbca7db4e90f6eaed218a1c00dda07d061647c1467bf6c25` |
| `default.xex` | md5 `ab09e2f9971c9c69654ececda73b4343` |
| `.map` da build | `build/x360/pacote/default-ea439bb5.map`, fora do git |

Conferência: o zip baixado da página pública deu o mesmo SHA256 e os 43 arquivos do `SHA256SUMS.txt` conferiram.
