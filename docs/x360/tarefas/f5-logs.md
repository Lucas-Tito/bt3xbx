# Tarefa: os logs do 360 travam o jogo? (F5, etapa 8)

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md), etapa 8

Branch: `cursor/f5-logs` (a partir de `troca/main`). Relatório: `~/entrega-f5-logs.md`. Orçamento: 1 a 2 h.

## A suspeita do Lucas

O jogo no console engasga desde sempre: nos menus, a média é de 19 a 20 ms por quadro (acima dos 16,7 de 60 fps)
com picos de 170 a 430 ms a cada um ou dois segundos (`docs/x360/plano/F5/01.md` e as rodadas em
`/srv/bt3/dados/capturas/etapa6_r1.log` e `etapa6_r2.log`). A suspeita é que o nosso jeito de gravar os logs trave o
jogo. Dois projetos do Lucas para o mesmo console fazem log de outro jeito; as fontes estão em
`/srv/bt3/dados/referencias_log/` (só leitura): o **hiddriver** (`hiddriver/hiddriver/main.cpp`: um anel em memória
formatado depois por outra thread, descarregado só em momentos seguros) e o **CollectionUI**
(`CollectionUI/app/src/diario.cpp` e `.h`).

## O que fazer

1. **Inventário do que o nosso 360 grava em disco durante o jogo**, com a frequência: o diário
   (`port/x360/src/diario.c`: `WriteFile` + `FlushFileBuffers` por linha, com trava, pela regra do `CLAUDE.md`), o
   `game:\hash.txt` por quadro (`port/x360/src/f2/estado360.c`), a saída de texto `game:\saida.txt` com `fflush` por
   `printf` (`port/x360/src/f2/posix360.c`), as linhas periódicas do backend (`gs_d3d.cpp`, a cada 60 quadros), as
   capturas (`BT3_SHOT`), e qualquer outro. Em que thread cada um roda (a do jogo, a de desenho, a main).
2. **Comparação** com os dois projetos: o que eles fazem diferente (buffer em memória, thread própria, descarga em
   lote, sem `FlushFileBuffers` por linha, o que acontece numa queda) e o custo provável de cada escrita no HD do
   360 (uma escrita com flush no FATX custa milissegundos e pode bloquear a thread que chama).
3. **Correlação com os picos:** nos logs das rodadas, os picos de tempo de quadro coincidem com as escritas
   (os trechos de 60 quadros que gravam linhas, o hash por quadro)? Diga o que dá para afirmar só pelos logs e o que
   precisa de uma medição.
4. **Proposta e implementação** de um diário que não trave: por exemplo, linhas num anel em memória e uma thread de
   baixa prioridade que grava em lote, com a regra do `CLAUDE.md` preservada onde ela importa (numa queda, o que já
   foi escrito tem de estar no disco: o filtro de exceção descarrega o anel antes de terminar); o `hash.txt` por
   quadro desligado por padrão nas builds de jogo (ligável pelo `ambiente.txt`) se ele custar; o `fflush` por
   `printf` revisto. A interface `Diario_Escrever` não muda (outros arquivos a chamam). **Não mexa em
   `port/x360/src/gs/gs_d3d.cpp`**: outra frente está mexendo nele agora.

Você não consegue compilar o código do 360 (o compilador do Xbox fica em outra máquina): escreva com cuidado, no C89 do
`cl.exe` (`-TC`: declarações no começo do bloco, sem `inline`), e diga no relatório o que não pôde ser conferido. Sem
emulador nem build. Registre a análise em `docs/x360/plano/F5/08.md`, numa seção "Os logs e os engasgos", com a
tabela do inventário, a comparação e a proposta, e a lição no `docs/x360/guia-de-port.md` se ela custou rodada.

Ao final: relatório em `~/entrega-f5-logs.md`, `git push troca cursor/f5-logs`, git limpo, nenhum processo seu.
