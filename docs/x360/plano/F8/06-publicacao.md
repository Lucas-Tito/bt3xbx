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
| `.github/labels.yml` | Nomes, cores e descrições dos 13 rótulos, para criar no GitHub na abertura |
| `docs/x360/plano/F8/06-publicacao.md` | Este documento: a prova da seção 4 e as escolhas da seção 5 |

[Estimado] Antes da release, além da checagem final do escopo (4.4): rodar o `pacote.py --estrito` na árvore do
commit marcado, o `teste_pacote.py --mutantes` sobre o zip dela, e copiar ao console a pasta `bt3-x360` do zip
aberto para o teste do README ao pé da letra (escopo 1.1, Documentação).
