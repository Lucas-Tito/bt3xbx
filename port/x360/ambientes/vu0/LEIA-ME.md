# Medida da VU0 no mesmo executável

Compilar o commit deste branch uma vez. Levar o mesmo `default.xex`, `default.map`,
`roteiros/f5_duelo.txt` e `roteiros/boot.txt` do repositório ao console. Copiar cada
modelo como `game:\environment.txt`. Os demais valores ficam nos padrões da 1.0.2.
`BT3_DESBLOQUEIO=1` é exigido pelo passo `desbloqueia` do boot do roteiro.
Não muda o combate nem injeta dano.

| Modelo | Nativo | Amostrador | Pergunta |
|---|---|---|---|
| `environment_vu0_n0_a0.txt` | desligado | desligado | tempo do caminho anterior, no executável único |
| `environment_vu0_n1_a0.txt` | ligado | desligado | ganho real sem interferência da coleta |
| `environment_vu0_n0_a1000.txt` | desligado | 1.000 us | distribuição por função antes |
| `environment_vu0_n1_a1000.txt` | ligado | 1.000 us | distribuição por função depois |

A pasta de `BT3_SAVES` precisa estar vazia em **cada execução**. Para repetir, usar
um nome novo e curto no FATX. Guardar `bt3xbx.log`, `output.txt` e o mapa por rodada.
Repetir os pares em ordem invertida para separar aquecimento de diferença real.
Conferir vencedor, vida, fim do roteiro e os 2.334 passos de combate antes de
comparar p50, p95, p99, máximo e passos acima de 30 ms. Não comparar o tempo de
uma coleta com o de uma rodada sem coleta como se a coleta fosse gratuita.

`BT3_VU0_NATIVO` ausente ou `1` liga; exatamente `0` desliga os três núcleos,
inclusive as instruções interpretadas e os vetores que delegam a eles. O gancho
`Port_FastMtxApply` devolve 0 nesse caso. Os recuos conservam seus corpos anteriores.
A leitura ocorre antes da thread do jogo; durante as contas há uma consulta à
variável inteira, sem getenv. Esse desvio continua nos dois lados da medida.

O amostrador herdado só desliga com **ausência** de `BT3_AMOSTRA` (a linha
`-BT3_AMOSTRA` remove a chave). `BT3_AMOSTRA=0` seria limitado a 100 us e ligaria a
coleta. Com coleta, conferir o autoteste, as âncoras, o carimbo do ligador, amostras
sem Iar, falhas de suspensão, duração média/máxima da suspensão e truncamento
às 200 maiores entradas. Resolver com `amostra_perfil.py` e o mapa da própria build.

No XDK, confirmar que os glob de `build_f2.sh` compilaram `amostra_hist.c` e
`amostra360.cpp`, que não viraram stubs e que as chamadas de preparação, início e
parada estão ligadas. Repetir a guarda de fusão real e conferir seus dumps, inclusive
os núcleos com a consulta de `gVnAtivo`. A guarda sintética no WSL não substitui isso.
O teste de chave usa o `float360.c` real no host, com cópias sabotadas.

Os contadores ficam só nas builds de diagnóstico sem janela (`BT3_VU0_CONTA=1`
na compilação, junto de `BT3_TESTE_VU0_NATIVO=1`). Não há contador ou chamada extra
na build normal com essa opção desligada. Os incrementos e as linhas por passo
alteram o custo, por isso não foram levados à medição do console. `float360.c` não
é ligado no jogo sem janela: o inventário explicita seus zeros; `ponte` e
`pc_softfloat_ps2` contam as implementações escalares usadas ali. Não apresentar
esses zeros como ausência de trabalho escalar no console.
