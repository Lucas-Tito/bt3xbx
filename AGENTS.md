# bt3xbx: instruções para agentes que trabalham no WSL

Port do Dragon Ball Z Budokai Tenkaichi 3 (PS2) para o Xbox 360. Você roda no WSL do Windows, como o usuário
`codex` (sem sudo), no clone `/home/codex/bt3xbx`. Os dados do jogo (`gamedata`, `disc`, `asm`, `assets`) são links
para `/srv/bt3/dados`, só leitura: não tente escrever neles. As referências dos roteiros ficam em `~/referencias`. O mesmo repositório tem um `CLAUDE.md` com o contexto do projeto: **leia-o**, ele vale para você também,
menos o que fala de `wsl.sh` (isso é do lado Linux: aqui você roda os comandos direto) e da build do 360 (que não
existe no WSL; ela é feita e conferida no Linux, na revisão).

## Antes de começar

- O índice do plano é `docs/x360/plano.md`. Leia só ele e o arquivo da sua tarefa (em `docs/x360/plano/`), não o
  plano inteiro.
- O guia de port (`docs/x360/guia-de-port.md`) tem as ferramentas (seção 2), o processo (3) e as armadilhas que já
  custaram horas (5 e 6). Leia as seções que a tarefa citar, e procure (grep) no guia antes de investigar algo do zero.
- Veja como etapas parecidas foram feitas pelo histórico (`git log`, `git show`): a 9a e a 9b são o modelo.

## Regras obrigatórias

- Tudo em português. **Nunca use travessão** (o caractere U+2014) em texto, comentário ou mensagem de commit.
- **Nenhuma atribuição** em commit: nada de `Co-authored-by`, nada que cite Codex, ChatGPT, OpenAI, Cursor, Claude ou
  qualquer ferramenta. A identidade do commit é a do repositório (Lucas Tito Sampaio).
- **Nenhum byte do jogo no repositório** (ISO, ELF, AFS, dados extraídos, imagens capturadas). Os dados ficam fora
  do git (`gamedata/`, `disc/`, `port/build/`, `~/`).
- O código vindo do upstream (`src/`, `include/`, `port/` fora de `port/x360/`) só muda **dentro de
  `#ifdef PORT_BIG_ENDIAN`** (ou `PORT_X360`), com a linha original no `#else`. Uma macro que mude o texto original
  fora do `#ifdef` não vale.
- A seção `.text` da build x86 sem janela só pode mudar por código de teste (`port/x360/headless/`), provado pelos
  objetos iguais sem a depuração (`objcopy --strip-debug` e `cmp` contra a base). A referência atual está no índice
  do plano ou no arquivo da última etapa.
- **Roteiros na PPC: `SEMMAPA=1`, nunca `PPC=1`.**
- **Um ciclo de emulador por vez** (dois `roteiros.sh` ou dois `qemu-ppc` juntos travam o WSL). Antes de rodar, confira
  `pgrep -af "qemu|roteiros"`.
- **Todo verificador novo tem de ser visto falhando de propósito** (entrada estragada numa cópia) antes de valer.
- Todo aprendizado que custou uma rodada entra no guia, no mesmo commit da correção.
- Processo longo: rode com `tee` para um log em `~/` (o `/tmp` do WSL some quando a distribuição para).
- **Esperar é com o shell, não com você.** Para esperar a trava, um serviço ou um processo longo, rode **um único
  comando bloqueante** em primeiro plano, com o maior tempo limite que a ferramenta aceitar, que só devolve quando
  acabar e imprime só o fim: o próprio `flock ~/wsl.lock <comando>`, ou um laço como
  `while pgrep -f bt3-xxx >/dev/null; do sleep 300; done; tail -5 ~/log`. Se o tempo limite cortar, rode o mesmo
  comando de novo, sem ler logs no meio. Nada de conferir de tempo em tempo (cada olhada gasta a cota sem avançar)
  nem de lançar vários `flock` em segundo plano: a ordem em que pegam a trava não é garantida. Junte os passos
  dependentes num script só, dentro de um `flock`.
- Não mexa no console (FTP) sem a tarefa pedir.

## Como entregar

- O remoto é `troca` (`/srv/bt3/bt3xbx.git`, um repositório local; não há acesso ao GitHub). Comece com
  `git fetch troca` e trabalhe no branch que a tarefa der (por exemplo `codex/9c-evo`), a partir de `troca/main` ou do
  branch existente com esse nome.
- Commits atômicos, mensagem em português explicando o porquê, como os do histórico.
- Ao terminar, `git push troca <branch>`. **Nunca** empurre para `main`: a revisão traz o branch, confere e envia ao
  GitHub.
- **A conferência oficial é sua:** rode os comandos do critério da tarefa (`roteiros.sh` com `SEMMAPA=1`, `save_mc.sh`
  e os outros), com as builds do commit final, até saírem "tudo certo". Não entregue como pronto o que não passou;
  se não passar, diga o que falta.
- Ao terminar, escreva o relatório em `~/entrega-<tarefa>.md`: os commits, o que ficou provado e como (a tabela da
  rodada final), o que não ficou provado, as perguntas com a opção conservadora aplicada, os verificadores vistos
  falhando e o sha1 da `.text` da x86. Deixe o git limpo e nenhum processo rodando.
- Se travar além do dobro do tempo previsto, registre o estado e as hipóteses no relatório, commite o que estiver
  provado e pare.
