# bt3xbx: instruções para agentes que trabalham no WSL

Port do Dragon Ball Z Budokai Tenkaichi 3 (PS2) para o Xbox 360. Você roda no WSL do Windows, no clone
`~/bt3xbx`. O mesmo repositório tem um `CLAUDE.md` com o contexto do projeto: **leia-o**, ele vale para você também,
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
- **Nenhuma atribuição** em commit: nada de `Co-authored-by`, nada que cite Codex, ChatGPT, OpenAI, Claude ou
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
- Não mexa no console (FTP) sem a tarefa pedir.

## Como entregar

- Trabalhe num branch local com o nome que a tarefa der (por exemplo `codex/9c-evo`), criado a partir de `origin/main`.
- Commits atômicos, mensagem em português explicando o porquê, como os do histórico.
- **Não faça push** (a chave deste clone é só de leitura). A revisão busca o branch daqui e envia ao GitHub.
- Ao terminar, escreva o relatório em `~/entrega-<tarefa>.md`: os commits, o que ficou provado e como (a tabela da
  rodada final), o que não ficou provado, as perguntas com a opção conservadora aplicada, os verificadores vistos
  falhando e o sha1 da `.text` da x86. Deixe o git limpo e nenhum processo rodando.
- Se travar além do dobro do tempo previsto, registre o estado e as hipóteses no relatório, commite o que estiver
  provado e pare.
