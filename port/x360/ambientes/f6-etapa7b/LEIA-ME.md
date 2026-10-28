# Próxima rodada da F6, etapas 7 e 8

Uma build do commit deste branch, feita e conferida no Linux. Este preparo não acessou o console.
Cada `.txt` desta pasta substitui `game:\ambiente.txt`. Saves e capturas usam o nome
`f6e7b_` seguido do nome do arquivo, sem `.txt`: oito nomes diferentes, ainda não usados.
As pastas devem estar ausentes ou vazias antes da primeira abertura. Se repetir uma abertura, acrescentar
`_r2`, `_r3` etc. **tanto** a `BT3_SAVES` quanto a `BT3_SHOT_PASTA`; não reaproveitar capturas nem cartões.
Guardar `bt3xbx.log` e, se houver, `saida.txt`, antes de abrir o próximo ambiente, em pastas com esses mesmos nomes.

| Ordem e ambiente | Ação e prova |
|---|---|
| `01_capturas_op.txt` | Deixar a abertura inteira. Escala 1, FXAA desligado. Trazer os quatro `filme_op_*.ppm` de `game:\f6e7b_01_capturas_op`. Exigir a 00000 e ausência de `captura do filme ... não chegou em 2 s`. Rodar `filme.py compara PASTA ~/f6-etapa7/referencia/op --tipo op`. Este log não mede ritmo. |
| `02_boot.txt` | Abertura inteira, sem apertar nada. Ouvir, ver e rodar `filme.py log LOG --tipo op`. Conferir também a primeira observação: a rodada anterior foi recusada por `geracao=0`. |
| `03_modo2.txt` | Boot inteiro, título ocioso, duas demonstrações e outra abertura inteira; não pular. Duas aberturas completas no log; `filme.py log LOG --tipo op`. |
| `04_start.txt` | START manual por volta de 20 s da abertura. Fade de 0,5 s, título e som do filme parado. Guardar log e escuta; o analisador de filme inteiro deve recusar esta rodada parcial. |
| `05_final.txt` | Cartão vazio obrigatório, desbloqueio pelo roteiro. Copiar `boot_start.txt`, `boot.txt`, `f6_final_start.txt` e `f6_final.txt` de `port/x360/roteiros/` para a raiz. O START repetido atravessa as logos e pula só o boot; depois o roteiro espera o final inteiro e a volta à História. Ouvir, ver e rodar `filme.py log LOG --tipo ed`. |
| `06_escuta.txt` | Etapa 8: Treino livre, Goku (Early) contra Kuririn, traje 0, estágio 5, 11 min contínuos na mesma luta. Guide três vezes por 5 a 10 s, uma sobre fala/efeito; observar se a luta congela. Conferir música contínua, pausa/retomada das vozes, START separado, mono/estéreo e extremos das opções. Sair pelo jogo, guardar log e saída, rodar `som_deriva.py LOG`. |
| `07_tom.txt` | Etapa 8: tom de 1 kHz por 900 s desde o boot. Pular o filme, chegar ao mesmo Treino em menos de 5 min e ouvir 10 min sob carga. Guide uma vez; depois conferir mono de 24 kHz, esquerda, direita e ganho 0,25. Guardar log e saída. |
| `08_sem_som.txt` | Etapa 8: mesmo Treino sem dispositivo, comparar desempenho e saída ao dashboard com a escuta. Para repetir as saídas pelo jogo e pela Guide, usar novos sufixos nas duas pastas e guardar cada log antes de reabrir. |

Nas três últimas, conservar os demais critérios de [08.md](../../../../docs/x360/plano/F6/08.md), inclusive
notificações da Guide, erros e faltas zero, fila e saída. Esses ambientes foram copiados dos já preparados na
etapa 8, mudando só isolamento e desligando capturas/medida de filme. Na saída, o log e o diário devem fechar.
Nenhum resultado desta lista já está provado no console. Não compensar o atraso inicial do som nem flexibilizar
o aceite se a medida falhar novamente. A comparação dos quatro quadros do final anterior já passou.
