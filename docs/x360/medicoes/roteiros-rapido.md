# Coleta conjunta dos roteiros

Base: `c57e807`, `SEMMAPA=1 IMG=1 SOM=1`, sem console. A conferência do branch
`codex/f6-etapa9-prep` usa 21 roteiros; `f5_final.sh` usa 19. Os comparadores
`roteiros.py`, `f5_final_confere.py` e `f6_final_confere.py` continuam com os
mesmos nomes de arquivos e os mesmos critérios. A conferência da `.text` da F5 aceita a referência antiga para a base e exige
`e6e284c6064a064a9230b88bea90c16b3efc31e7` no final, junto da prova dos
objetos. O [patch de integração da F6](f6-roteiros-rapido.patch) delega essa
mesma checagem a `f5_final_confere.texto` e limpa `RAPIDO` herdado no executor,
sem tocar no outro branch.
Foi aplicado numa cópia do conferente da F6 e testado com os dois binários;
um byte alterado na `.text` final foi recusado com saída 1 nas duas CLIs.

## Inventário antes da alteração

Tempos aproximados de `historia_novo` na rodada de 28/10/2026,
`~/f6-final-a989c9b5/casos.json` e `roteiros/historia_novo/`. As diferenças entre
horários de modificação dos arquivos delimitam cada execução; incluem alguns
segundos de preparação/análise. Não são medidas de CPU. O caso completo levou
4.968 s. A primeira execução x86 foi estimada pela repetição, 27,8 s.

Todos herdam `BT3_GS=none`, `BT3_SETTINGS=`, `BT3_SOUND_TICKS=1`,
`BT3_NOMOVIE=1`, `BT3_MENU_ITEM4=0`, `BT3_DESBLOQUEIO=1`, o mesmo
`BT3_ROTEIRO` e cartões vazios distintos em `BT3_SAVES`.

| Execução | Build, diferenças | Prova e arquivos | Tempo |
|---|---|---|---:|
| x86 a | x86, `BT3_HASH=ha.txt`, `BT3_SOM=som_a.txt` | rastro de referência, luta, som, cartão `sa` | 27,8 s |
| x86 b | x86, hash `hb`, som `som_b`, cartão `sb` | repetibilidade do rastro e do som | 27,8 s |
| x86a | x86a, `BT3_SOUND=1`, `BT3_GRAVADOR=grav_x.bin` | rastro/som iguais à x86, PCM de referência | 28,6 s |
| x86 gdb | x86, `unpack.gdb`, hash `hg` | `Sprite_Unpack` alcançados e arquivos lidos, `gdb.txt` | 27,4 s |
| x86r | x86r, `BT3_RASTREIO_BITS=24`, `BT3_RASTREIO`, `setarch -R` | larguras lidas, mapa e cobertura dos conversores | 159,2 s |
| PPC sem mapa | ppc, `BT3_DATA`, `BT3_CONVERTIDOS`, `BT3_LEU=1`, hash `hq`, `BT3_HASH_AT=14971`, som `som_p` | rastro, arquivos, luta, memória final e save, `ppcsm.txt` | 531,9 s |
| x86 memória final | x86, hash `hm`, mesmo `BT3_HASH_AT` | contraparte da memória final | 27,8 s |
| x86 donos final | x86 sob gdb por `campos.py --donos` | tipos e donos vivos, `donos.*` | 55,5 s com memdiff |
| PPC gravador | ppca, `BT3_SOUND=1`, `BT3_GRAVADOR=grav_p.bin`, hash `hqa`, som `som_pa` | mixer contra x86a, som contra ppc | 533,9 s |
| PPC memória tela 6 | ppc, hash `mem_6/hq`, `BT3_HASH_AT=4523`, `BT3_MEMORIA_SAIR=1` | conteúdo vivo da tela, dump/pages/pool | 128,9 s |
| x86 memória tela 6 | x86, hash `mem_6/hm`, mesmo ponto e saída | contraparte | 5,2 s |
| x86 donos tela 6 | x86 sob gdb, ponto 4523 | tipos e donos | 10,6 s com memdiff |
| PPC memória tela 7 | ppc, ponto 5512, saída após dump | conteúdo vivo da tela | 157,2 s |
| x86 memória tela 7 | x86, ponto 5512, saída após dump | contraparte | 6,5 s |
| x86 donos tela 7 | x86 sob gdb, ponto 5512 | tipos e donos | 12,0 s com memdiff |
| PPC memória tela 8 | ppc, ponto 14217, saída após dump | conteúdo vivo da tela | 509,3 s |
| x86 memória tela 8 | x86, ponto 14217, saída após dump | contraparte | 26,7 s |
| x86 donos tela 8 | x86 sob gdb, ponto 14217 | tipos e donos | 53,8 s com memdiff |
| PPC memória tela 10 | ppc, ponto 14821, saída após dump | conteúdo vivo da tela | 523,3 s |
| x86 memória tela 10 | x86, ponto 14821, saída após dump | contraparte | 27,6 s |
| x86 donos tela 10 | x86 sob gdb, ponto 14821 | tipos e donos | 53,2 s com memdiff |
| x86 imagens | x86, `BT3_GS=1`, `BT3_GS_FROM=1000000000`, `BT3_IMAGENS`, janela 8, sem hash | PPM e momento das capturas | 83,1 s |
| PPC imagens | ppc, mesmos parâmetros, dados convertidos, sem hash | PPM idênticos à x86 e rastro inteiro | 1880,3 s |

O mapa/conversor tomou mais 64,5 s entre x86r e PPC. Somadas, as delimitações
acima dão 4962 s, próximas dos 4968 s do caso. As execuções x86 sem gdb somam
420 s, cerca de 7 min; com os gdbs e donos, cerca de 632 s. As PPC somam 4265 s,
71 min, dos quais 31 min nas imagens. Portanto, os 6 min do enunciado são uma
aproximação que não inclui toda a análise x86.

## Coleta nova

`RAPIDO=1` é o padrão com `SEMMAPA=1`, salvo `MEM=menu`. `RAPIDO=0` conserva
as execuções separadas para conferência. `BT3_MEMORIAS` recebe uma lista de
vblanks e destinos. O gancho de teste grava os mesmos dumps, páginas e
subalocações em cada ponto e continua. A memória final continua no logger
original. A mesma passagem recebe as imagens, pintando só as janelas pedidas.
Assim a PPC normal faz uma passagem e a ppca outra; a x86 de memória também
coleta todas as telas e imagens de uma vez. Os gdbs de donos continuam
independentes para preservar a prova de tipos e de alocação.

O gravador continua em outra build e outra passagem. Juntá-lo à PPC normal
eliminaria a comparação entre a execução sem dispositivo e a execução que abre
e alimenta o dispositivo. O mesmo vale para x86a. As duas x86 de repetição,
a x86 sob gdb e a x86r continuam distintas porque provam coisas diferentes.

`tempos.tsv` mede cada execução nova com `/usr/bin/time`, incluindo as que
continuam separadas. `REUSA=1` reaplica os mesmos julgamentos; não cria
uma exceção para a coleta conjunta.

## Conferência reproduzível

Depois de compilar as cinco builds do mesmo commit limpo, executar os dois
modos com cartões e saídas próprios, um processo de cada vez sob a trava:

```sh
SEMMAPA=1 IMG=1 SOM=1 GUARDA_SOM=1 RAPIDO=0 O="$HOME/rapido/separado" port/x360/tools/roteiros.sh opcoes duelo
SEMMAPA=1 IMG=1 SOM=1 GUARDA_SOM=1 O="$HOME/rapido/junto" port/x360/tools/roteiros.sh opcoes duelo
python3 port/x360/tools/compara_roteiros_rapido.py "$HOME/rapido/separado/opcoes" "$HOME/rapido/junto/opcoes"
python3 port/x360/tools/compara_roteiros_rapido.py "$HOME/rapido/separado/duelo" "$HOME/rapido/junto/duelo"
```

O comparador exige rastros, leituras, todas as amostras de luta e cartões
iguais; subalocações e PPM byte a byte; capturas no mesmo quadro/vblank.
A memória também é byte a byte, salvo os caminhos do host em `sMc.path`,
`gPortAdxView.path` e `gPortMoviePath`, normalizados para o mesmo arquivo
lógico, e os dois ponteiros de locais da pilha `gColWalkBox`/`gColWalkCtx`.
Também se normalizam os endereços malloc de `sMapa` e dos campos IOP de
`SndMgr`, preservando NULL contra não NULL. São ponteiros do host já tratados
em `DIFERENTES_CONHECIDAS`. No heap se excluem apenas bytes livres, enchimento
e `unk1C`, como em `memdiff --so-em-uso`; cabeçalhos, tamanhos, rodapés e dados
vivos continuam obrigatórios. A comparação bruta das Opções falhou nesses
bytes entre processos, embora as provas oficiais dos dois modos passassem.
Os controles exercitam dados vivos, metadados e as exclusões nas duas ordens
de bytes; alterar um dado vivo, mesmo recalculando suas páginas, reprova. O nome lógico do arquivo e os campos numéricos continuam
obrigatórios. As páginas são recalculadas sobre cada dump original pela
`libxxhash.so.0`, antes da normalização, para provar sua integridade. `--mutantes` vê alteração e ausência de memória, imagem e hash
falharem com saída 1. `teste_memorias.py` exercita três destinos em dois
momentos, a continuidade e cinco listas inválidas sem emulador. Os controles
sobre a rodada real usam `REUSA=1` numa cópia, com memória truncada e pixel
alterado, conservando as saídas originais.

## Medição final e estimativas (28/10/2026)

A rodada limpa de `7fa20dee`, em `~/rapido-rodada.log`, terminou os quatro
roteiros com `tudo certo`. Não se contam a tentativa anterior nem as builds.
Cada modo foi executado uma vez, sequencialmente sob `flock ~/wsl.lock`.

| Roteiro | Separado | Conjunto | Ganho | Equivalência | Imagens por arquitetura |
|---|---:|---:|---:|---:|---:|
| opcoes | 998,93 s | 829,74 s | 169,19 s (16,9%) | 79 arquivos/provas | 11 |
| duelo | 1676,15 s | 1294,51 s | 381,64 s (22,8%) | 93 arquivos/provas | 9 |

As passagens PPC caem de 4 para 2 nas Opções e de 7 para 2 no Duelo.
As da x86, incluindo gdb e donos, caem de 10 para 8 e de 16 para 11.
Todas as memórias finais e de tela, subalocações, imagens, leituras, rastros,
amostras de luta e saves passaram na comparação dos dois modos.

Para projetar, preserva-se o custo de imagens e do gravador e retiram-se as
passagens de memória final e por tela da PPC e da x86. Soma-se de volta o
acréscimo de hashes observado na passagem com rasterização: nas Opções,
`(370,75 - 222,00) / 153,79 = 0,967` vezes a PPC sem imagens; no Duelo,
`(639,47 - 474,62) / 168,98 = 0,976`. Na x86 os fatores são 2,693 e 1,306.
Esses dois pares de fatores geram a faixa abaixo; a estimativa é sua média.
Os custos históricos por passagem usam os mesmos limites de mtime do
inventário. É extrapolação de duas amostras, sem intervalo estatístico nem
promessa para o console. Os tempos medidos agora incluem a carga desta sessão.

| Rodada | Roteiros | Antes, observado na F6 | Estimativa conjunta | Faixa |
|---|---:|---:|---:|---:|
| F5, somente roteiros | 19 | 13,39 h | 9,46 h | 9,42 a 9,51 h |
| F6, somente roteiros | 21 | 13,71 h | 9,79 h | 9,74 a 9,83 h |

A F6 observada soma outros 0,48 h de casos com duração válida: o subtotal
comparável passa de 14,19 h para 10,26 h. Não entram espera externa, duração
inválida por ajuste do relógio nem fases que não chegaram a rodar. Quatro dos
21 roteiros históricos reprovaram referência ou imagem, embora tenham executado
as passagens: a tabela estima custo, não aprova aquela rodada. A F5 completa
ainda soma seus testes fixos, sem medida completa nesta tarefa. Portanto o
relato de mais de 15 h não se confunde com 13,71 h de execuções de roteiros.

O controle real de memória truncada e pixel alterado recusou as duas cópias.
Sua primeira restauração falhou porque o teste não preservou `grav_*.bin`:
`roteiros.sh` os apaga por padrão após julgar o mixer. Para reaplicar `SOM=1`
com `REUSA=1`, é obrigatório coletar com `GUARDA_SOM=1` e manter essa variável
nas verificações das cópias. A recuperação regenera apenas as duas gravações
por roteiro, confere rastros e mixer e reaplica os julgamentos completos sem
repetir as passagens caras de memórias e imagens. Isso não entra no tempo do
ganho. O relatório de entrega registra o resultado final dos controles e os
logs conservados em `~/rapido/`.
