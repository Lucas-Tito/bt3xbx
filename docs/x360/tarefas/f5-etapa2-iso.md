# Tarefa F5-etapa 2: a ISO convertida no PC

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md) | decisões 18, 23 e 24

Branch: `codex/f5-iso` (a partir de `troca/main`). Relatório: `~/entrega-f5-iso.md`. Orçamento: 4 a 6 h.

## O que fazer

A decisão 23 (`docs/x360/decisoes.md`): um programa de PC lê a ISO do próprio usuário e grava uma ISO convertida do
mesmo tamanho, com os conversores trocando no lugar, sem mudar tamanho nem posição; o jogo no 360 passa a exigi-la.
Leia a decisão 23, a 18, a etapa 2 da tabela e a pergunta 3 da seção 5 de `docs/x360/plano/F5/00-escopo.md`, e a
resposta 3 da decisão 24 (a marca embutida na ISO só depois de provar que a região é livre; os `mods/` recusados).

**Nesta tarefa, só o lado do PC.** A parte do 360 (exigir a ISO convertida, recusar a original com mensagem na tela,
tirar o `Conv_Arquivo` da carga, recusar os `mods/`) fica para depois da etapa 1 da F5, porque mexe no boot do console
e só se prova lá. Não mude `port/x360/src/`.

1. **A ferramenta:** de preferência um modo novo do `conv_tool` (`port/x360/conv/`, que já tem todos os conversores e
   o `--disco`), que lê a ISO, converte no lugar cada arquivo e conteúdo que o `--disco` converte, e grava a saída num
   arquivo temporário renomeado só no fim, depois de conferida. Os conteúdos comprimidos (BPE) continuam como estão:
   eles só se convertem no console, depois de descomprimidos (decisão 23). As tabelas ISO 9660 e AFS não mudam.
2. **A marca:** prove, pelos dados e pelo formato da ISO e do AFS, uma região que o jogo e o leitor do port nunca
   leem, e grave nela a marca e a versão (o que a converteu e de qual origem). Se não achar região provadamente livre,
   PARE nesse ponto e entregue as opções como pergunta ao Lucas (a decisão 24 manda não escolher um deslocamento sem
   prova); o resto da ferramenta segue sem a marca.
3. **As recusas:** a ferramenta recusa, com mensagem clara e sem gravar nada, uma ISO que não seja a original
   americana (os hashes conhecidos), uma truncada, uma já convertida (a marca) e uma saída que já exista. A original
   nunca muda (confira o md5 antes e depois).

## Critério

- Especificação em `docs/x360/formatos/iso-convertida.md`: a marca, a região e a prova de que é livre, o que é
  convertido e o que é preservado.
- A ISO convertida reaberta: cada arquivo convertido igual, byte a byte, ao que o `conv_tool --saida` grava para ele
  a partir dos dados extraídos (`/srv/bt3/dados/gamedata`), e todo o resto igual à original byte a byte (os 68.611
  arquivos e as áreas fora deles). Mesmo tamanho. A original com o mesmo md5 antes e depois.
- Cada recusa vista acontecendo numa cópia estragada (truncada, um byte mudado, já convertida, saída existente), e o
  verificador da reabertura visto falhando numa saída estragada de propósito.
- Os conversores continuam como estão: `conv_tool --disco` sem erro e a 100%, como hoje.
- Ao fim, `docs/x360/plano/F5/02.md` com a tabela da rodada e as perguntas, e a linha da etapa 2 em
  `docs/x360/plano/F5/00-escopo.md` apontando para ele.

## Dados

A ISO original do Lucas está em `/srv/bt3/dados/iso/bt3.iso` (só leitura; md5 `350404a81de912347c8b262542fed970`,
3.020.816.384 bytes). As saídas (3 GB cada) ficam no seu `~/`, nunca no repositório; apague as que não precisar mais.
Nenhum emulador é necessário nesta tarefa; se rodar algum, um por vez.

## Fora desta tarefa

A parte do 360 (acima), a etapa 1 da F5 e qualquer recompressão de BPE.
