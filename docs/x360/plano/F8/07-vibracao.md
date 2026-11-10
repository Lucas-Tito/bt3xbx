# F8: a vibração do controle

[Índice do plano](../../plano.md) · [escopo da F8](00-escopo.md) · decisões 21 ("para depois") e 33, item 4

Branch `claude/f8-vibracao` (base `main` `e005ac94`), 29/10/2026. Código no commit `07fb874e`; compilado no XDK nesta
máquina; **falta a prova de console (seção 5)**. Nada rodou no WSL, no Deck nem no emulador: a mudança só existe na
build do XDK (`PORT_X360`).

## 1. Onde o jogo vibra [lido]

- **Quem pede:** só a luta. Os golpes, as animações e os efeitos chamam `BtlChar_SetVibration` (motor grande, força de
  0 a 1 e duração), `BtlChar_SetSmallVibration` (motor pequeno, duração) ou `BtlChar_Vibrate` (os dois), em
  `src/battle/btl_char_get.c` e nos `btl_act_*.c`, `btl_char_action.c`, `btl_char_fx*.c`. Exemplos: 0,7 por 0,1 s em
  vários golpes (`btl_act_d.c`, `btl_act_e.c`), 0,8 por 0,2 s, 1,0 por 0,2 s no evento 45 das animações
  (`btl_char_fx_c.c`).
- **Quem decide:** `BtlChar_UpdateVibration`, uma vez por quadro e por lutador (no fim do quadro da luta,
  `BtlChar_EndFrame`). Não manda nada com a opção de vibração daquele jogador desligada (`vib.enabled`, lido de
  `gSaveData->flags & (2 << pad)` no reset do lutador; a opção é por jogador, na página do controle das Opções), no
  replay, ou para a CPU (`injectOn`). O motor grande sai modulado por um seno (entre metade e o total da força); o
  pequeno liga num quadro e desliga no outro.
- **Quem manda ao controle:** `Pad_AddVibration` soma os pedidos do quadro e o `Pad_Update` (`src/sys/pad.c`) chama
  `Pad_SendActuator` para os dois controles **todo quadro**, com ou sem pedido, e zera a soma. O valor vai pelo
  `sceVibSetActParam` da libpad2 (não pelo `scePadSetActDirect` da libpad antiga) em 16 bits: bit 0 o motor pequeno,
  bits 1 a 8 o grande. O jogo prende a força em 0x7F, então o grande vai de 0 a 127, nunca até 255.
- **O que vem de graça:** como o jogo manda zero em todo quadro sem pedido, a opção desligada, os menus, a carga
  (`BATTLE_FLAG_LOADING`) e a pausa da luta (`BATTLE_FLAG_PAUSE` pula o `BtlChar_EndFrame`) já param os motores no
  quadro seguinte, sem nada do port.
- **No port de PC:** `sceVibGetProfile` e `sceVibSetActParam` são stubs que devolvem 0 (`port/src/plat_stub.c`); o
  upstream não liga a vibração no SDL. Ficam assim no `#else`.

## 2. Como ficou no 360

- `port/src/plat_stub.c`, só sob `PORT_X360`: o `sceVibSetActParam` lê os 16 bits do `vibData` (na ordem da máquina:
  é um campo do jogo escrito pelo próprio código) e chama `Port_PadVibrar(socket, valor)`. O original fica no `#else`.
- `port/x360/src/f2/pad360.c` (no fim): o motor grande vai para o `wLeftMotorSpeed` (o grave), com 127 na força
  máxima (`valor * 65535 / 127`); o pequeno para o `wRightMotorSpeed` (o agudo), 65535 ligado ou 0. O controle é o
  do jogador agora (`sQuadrante`, o mesmo mapeamento da entrada, decisão 21): com dois jogadores, cada um vibra no
  seu. O `XInputSetState` só é chamado quando o valor ou o controle muda.
- **A vigia**, uma thread na de hardware 1 (com o diário e o vigia do som; nunca a 4 e a 5 do XAudio2), olha a cada
  20 ms e para os motores ligados quando:
  - a **Guide** ou outra tela do sistema está aberta (`XN_SYS_UI`, por um ouvinte de `XNOTIFY_SYSTEM` próprio; o do
    `som360.cpp` é outro, e cada ouvinte recebe as notificações). O jogo não pausa na Guide (F6, etapa 8: só os
    efeitos e as vozes do som param), então, enquanto ela está aberta, o próprio `Port_PadVibrar` também só manda
    zero. A **caixa de queda** dos branches da queda é um `XShowMessageBoxUI`, a mesma notificação;
  - o jogo **deixou de mandar** por mais de 250 ms: a queda (a thread do jogo morre), a saída (`Port_Sair` encerra a
    thread do jogo), uma carga ou trava longa. Cobre também a volta ao painel pela tela final, que só vem depois.
  A vigia só tenta a trava (`TryEnterCriticalSection`); se o jogo a tiver, para na volta seguinte.
- **Controle que desconecta:** a vibração dele some com ele; o `Port_PadVibrar` esquece o último valor, e o controle
  que entrar no lugar começa parado. Se o jogador troca de controle com o antigo vibrando, o antigo para antes.
- **Roteiro** (`BT3_ROTEIRO`): nada é enviado, nem a vigia nasce.
- **Diário:** quatro tipos de linha, todas raras (`vibração: pronta`, `a primeira do jogador N`, `tela do sistema
  aberta|fechada`, `motores parados (motivo)`). São `Diario_Escrever` comuns: na main o diário está sempre ligado;
  depois do merge dos logs (pendência 7, `teste/queda-console`) o diário só abre com `BT3_LOG=1`, e elas somem por
  padrão sem mudança aqui.
- **Fora daqui:** nada no `main360.cpp` (a saída e a queda que os branches da queda reescrevem) e nada fora de
  `PORT_X360`. Nenhuma opção do port (decisão 33, item 2): quem liga e desliga é a opção do próprio jogo.

## 3. Provas desta máquina [medido]

| Prova | Resultado |
|---|---|
| Build do XDK (`build_f2.sh`) no `07fb874e`, árvore limpa | 317 de 317 fontes sem erro, `pad360.c` e `plat_stub.c` sem aviso; carimbo `07fb874e diff:da39a3ee5e` |
| Stubs | 103, lista igual à da main `e005ac94` |
| `default.xex` | md5 `158db1224c111ff86bf98e138afdc516`, 1.511.424 bytes (main: `68ba1cdabcc784f9ba15687ba20d745e`, 1.507.328) |
| `default.dat` / `default.swap` | md5 `4fe9d0495c144043fdccda41281655fa` / `2e97a3e533accf0ae852e4d25df5283a`, **iguais aos da main** |
| Objetos contra a main (`xdk_objetos.py`, do branch `claude/f5-lotes`; `--so` com os 348 `obj/*.obj` e `x360/*.obj`) | 346 com a mesma desmontagem; diferentes só `obj/pc_plat_stub.obj` (o `sceVibSetActParam`: `lhz` do `vibData` e o salto para `Port_PadVibrar`) e `x360/pad360.obj`; funções novas no `.map` só do `pad360.obj` (`Port_PadVibrar`, `Vigia`, `iniciarVibracao`, `motores`, `pararMotores`) |
| O mesmo verificador sem `--espera-diferente` | sai com 1, acusando os dois objetos e as cinco funções |
| x86 e PPC do WSL sem mudança: `gcc -E -P` do `plat_stub.c` da main e do branch, sem `PORT_X360` (com e sem `PORT_BIG_ENDIAN`) | textos pré-processados idênticos (1.305 e 1.310 linhas); com `PORT_X360` diferem, como devem |

No `pad360.obj` as funções que já existiam (`Port_PadConnected`, `Port_PadRead`) também saem diferentes na
desmontagem: só registradores e endereços das variáveis estáticas, que agora são mais. O fonte delas não mudou.

## 4. Onde encontra os branches da queda [lido]

`git diff main...teste/queda-console --stat` não toca `pad360.c` nem `plat_stub.c`: o merge com este branch não tem
conflito de texto. O encontro é de comportamento: a caixa de queda (`Queda_Mostrar`) e a saída com teto
(`saida360.h`) param os motores pela vigia (tela do sistema aberta; thread do jogo parada), sem chamada explícita. Se
um dia a saída quiser parar já, sem os até 250 ms, o lugar é o `Port_Sair` e o `AoMorrer` do `main360.cpp` desses
branches, com uma função nova do `pad360.c`.

## 5. A prova de console (preparada)

Enviar com o console parado na FreeStyle, sem `environment.txt` (os padrões da main bastam):
`python3 port/x360/tools/subir.py build/x360/f2/default.xex build/x360/f2/default.dat build/x360/f2/default.swap`
(da worktree `bt3xbx-vibra`; conferir o md5 da seção 3). No fim, `subir.py --baixar bt3xbx.log`.

Com **dois controles com fio ou sem fio da Microsoft** (um controle emulado pelo hiddriver pode não vibrar: anotar
qual é qual). Em cada passo, olhar o motor grande (a vibração grave, mais "pesada") e o pequeno (o zumbido fino).

| # | O que fazer | O que observar |
|---|---|---|
| 1 | Duelo, jogador contra CPU, opção de vibração do jogador 1 **ligada** (a padrão). Golpes comuns, um golpe forte (carga), receber um golpe, um Blast 2 e um Ultimate | o controle 1 vibra nos golpes e nos impactos e para logo depois de cada um; nada vibra nos menus nem na carga |
| 2 | Pausar a luta (START) no meio de uma vibração (logo depois de um Ultimate acertar) | os motores param na hora e não voltam com a pausa aberta; voltam a funcionar ao despausar |
| 3 | Abrir a **Guide** no meio de uma vibração; esperar uns 5 s; fechar | param ao abrir e ficam parados com a Guide aberta, mesmo se a luta seguir andando por baixo; voltam no golpe seguinte depois de fechar |
| 4 | Opções, página do controle: **vibração desligada** do jogador 1; gravar; Duelo de novo | nada vibra, nem no Ultimate |
| 5 | Religar a opção; Duelo **jogador contra jogador**, com a opção do jogador 2 ligada | cada controle vibra só nos golpes do seu jogador e nos que ele recebe; o controle 1 não vibra pelos do 2 |
| 6 | No jogador contra jogador, desligar a opção só do jogador 2 | o 1 vibra, o 2 nunca |
| 7 | Tirar a bateria (ou o cabo) do controle 1 logo depois de um golpe forte; religar | o aviso de controle desconectado do jogo; religado, o controle não volta vibrando |
| 8 | Logo depois de um golpe, abrir a Guide e escolher voltar ao painel | nenhum motor fica ligado no painel |
| 9 | (Opcional, só com a queda de teste dos branches da queda) | a caixa `Unexpected error` sem motor ligado |

**No log:** `vibração: pronta (ouvinte do sistema aberto, vigia na thread de hardware 1)`;
`vibração: a primeira do jogador 1, quadrante N` (e do 2 no passo 5) com `XInputSetState 0` (outro número é erro do
sistema: anotar); `vibração: tela do sistema aberta` e `fechada` no passo 3, com `motores parados (tela do sistema
aberta)` se vibrava; `motores parados (o jogo parou de mandar)` só em carga longa ou saída, nunca no meio da luta.
Nenhuma linha de vibração no passo 4.

**Se falhar:** força fraca demais ou forte demais é a escala (127 no máximo do 360; o PS2 tinha 127 de 255 no motor
grande): ajustar só com o mantenedor. O ouvinte recusado (`ouvinte do sistema recusado`) quer dizer que o 360 não deu um
segundo ouvinte: aí a Guide passa a valer só pela vigia de 250 ms se o jogo parar, e é preciso ler a Guide do
`som360.cpp`.
