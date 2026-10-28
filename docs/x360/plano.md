# Plano do port BT3 para Xbox 360

Consolidação dos três estudos em `docs/x360/pesquisa/`:
- `01-collectionui-hiddriver.md`: o que já sabemos fazer no 360 (toolchain, deploy, depuração).
- `02-ports-referencia.md`: SM64, MK64 (MKart360), Virtua Fighter (wanszai) e ports de PC.
- `03-bt3-ps2-x360.md`: decompilação do BT3, port de PC existente, PS2 vs 360.


## 1 a 4. Ponto de partida, arquitetura, assets e layout

Resumo (texto completo em [plano/base.md](plano/base.md)):

- Base de código: `z3xox/Tenkaichi3Decomp`, o port de PC do mesmo autor da decomp; o 360 vira um terceiro backend da camada `port/`.
- Toolchain XDK 2.0.21256 (`cl`, `link -XEX:NO`, `imagexex`), jogo em C compilado para PowerPC big-endian, gráficos em D3D9, áudio em XAudio2, entrada XInput.
- Assets: nenhum byte do jogo no repositório nem no release; o usuário fornece a ISO USA SLUS-21678 (decisões 18 e 23).
- Layout: `bt3xbx` é um fork do upstream; backend em `port/x360/`, documentação em `docs/x360/`.

## 5. Roadmap

| Fase | Entrega verificável |
|---|---|
| F0 | Spike do compilador com arquivos reais da decomp + hello `.xex` com Title ID e ícone aparecendo no Aurora + medição de memória e reserva de endereços fixos do PS2 num título XDK. |
| F1 | Build headless big-endian no qemu-ppc reproduzindo o replay igual à build x86. |
| F2 | O mesmo replay headless rodando no 360, com tempo de frame medido. |
| F3 | Dados direto da ISO (decisão 18) + boot até a tela de título, com imagem. |
| F4 | Menus e save do console. |
| F5 | Uma luta a 30 fps. |
| F6 | Áudio (ADX e efeitos) e filmes. |
| F7 | Conteúdo completo e split-screen. |
| F8 | Acabamento (opções, 16:9, remapeamento) e release. |
| F9 | Depois do jogo base rodando bem: tela dividida para 4 jogadores, a partir do mod Tag Team (decisão 16; sem o online). |

### Estado e arquivo de cada fase

| Fase | Estado | Arquivo |
|---|---|---|
| F0 | concluída | entrega descrita acima; medições em `medicoes/` e decisões em `decisoes.md` |
| F1 | concluída | idem |
| F2 | concluída | idem |
| F3 | concluída | [plano/F3.md](plano/F3.md) |
| F4 | concluída (16/10/2026): rodada completa no WSL e sessão no console com o Lucas | [plano/F4/00-escopo.md](plano/F4/00-escopo.md) |
| F5 | em andamento: etapas 1 a 7 concluídas; a 8 (desempenho), decisão 29: reprojetar mantendo zero, imagem e entrada. A rp33a2810 confirma regressão da build nova no 7170: longa em 3/3 novas contra 0/3 antigas alternadas; F sobe 16,0 a 16,1 → 17,8 ms. O diagnóstico CPU inativo é a causa provável, única mudança no código executado; restaurar e provar o controle sem marcas antes de alfa/prefixos. Alfa no 7170 custa ~4,7 ms com marcas; nenhuma correção ou ganho certificado ([seção 35](plano/F5/08-gravacao.md#35-complemento-rp33a2810-o-7170-confirma-regressão-da-build-nova-28102026)). Permanecem os déficits e limites da seção 34; a 9 concluída no console, com o preto de Namek no fechamento de gaps ([F8/pendencias](plano/F8/pendencias.md)); a 10 preparada no WSL, sem rodar ([10](plano/F5/10.md)); aceite continua zero longas no combate | [plano/F5/00-escopo.md](plano/F5/00-escopo.md), [08-threads](plano/F5/08-threads.md), [08-degrau2](plano/F5/08-degrau2.md), [08-gravacao](plano/F5/08-gravacao.md) |
| F6 | em andamento: etapas 1 a 6 concluídas (o estado do som, as redes de teste, a camada do XAudio2, os efeitos, o ADX em blocos sem esperar o disco e o filme direto à textura, todos provados no console); a 7 (o filme com som, [07](plano/F6/07.md)) e a 8 (a Guide no som e a deriva, [08](plano/F6/08.md)) feitas no WSL; no console, as capturas e o boot passaram em 28/10 e o resto das provas saiu do plano por decisão do Lucas (pendência fora do plano, [F8](plano/F8/pendencias.md) item 10); a 9 com o ensaio completo no WSL em 28/10, reprovado com as lacunas registradas ([09](plano/F6/09.md)) | [plano/F6/00-escopo.md](plano/F6/00-escopo.md) |
| F7 | escopo aprovado (decisão 28); etapa 1 concluída na parte sem emulador: manifesto, denominadores e verificador; três pilotos preparados, aguardando execução | [plano/F7/00-escopo.md](plano/F7/00-escopo.md), [01](plano/F7/01.md) |
| F8 a F9 | por fazer | só a entrega da tabela acima; itens anotados para a F8 em [plano/F8/pendencias.md](plano/F8/pendencias.md) e o estudo do [16:9](plano/F8/16x9.md) |

### F4, etapas (13/10/2026)

Escopo, as quatro redes de cobertura e o critério da F4 em [plano/F4/00-escopo.md](plano/F4/00-escopo.md). Cada
etapa tem o seu arquivo; um agente que trabalha numa etapa lê este índice e o arquivo dela.

| # | Onde | O que | Critério | Estado | Arquivo |
|---|---|---|---|---|---|
| 1 | WSL | Inventário das 77 chamadas (id, seção, formato, leitores), censo do `--disco` por faixa, verificador `unpacks.py`, uma tabela de formatos só em `conv.c` | toda chamada com formato proposto; `unpacks.py` lista as pendentes | concluída | [01-02](plano/F4/01-02.md) |
| 2 | WSL | Guarda em `sprite.c` (`PORT_BIG_ENDIAN`); Dragon Net Battle escondido no 360 e nos roteiros | a PPC sem mapa para com "sem conversor em MainMenu_Init"; a `.text` da x86 inalterada | concluída | [01-02](plano/F4/01-02.md) |
| 3 | console | Save em `game:\saves` robusto: gravação em temporário e troca de nome, `fclose` conferido | salvar, sair e abrir de novo: o título tem Continue e a opção volta como estava | concluída | [03](plano/F4/03.md) |
| 4 | WSL | Roteiros de controle em texto, ancorados na troca de tela; desbloqueio de tudo no headless; um roteiro por modo | cada roteiro repete na x86 as telas de referência; o "sem conversor" do `--mapa` é igual ao inventário | concluída | [04](plano/F4/04.md) |
| 5 | WSL | Especificações e conversores do caminho essencial: menu principal, submenu do Duelo (`DuelMenu`), Duelo (personagem, time, estágio), Treino, Opções com o save delas | `--disco` 100% nessas faixas; `--mapa` sem divergência; PPC sem mapa igual à x86 em cada roteiro | concluída | [05](plano/F4/05.md) |
| 6 | WSL | Rasterizador x86 e PPC nas telas essenciais | PPM idênticos | concluída | [06](plano/F4/06.md) |
| 7 | console | Caminho essencial no console, pelos roteiros (`BT3_ROTEIRO`) | mesmas telas da x86; capturas no patamar da F3; nenhuma queda | concluída | [07](plano/F4/07.md) |
| 8 | WSL e console | Save entre builds (PPC e 360) | os dois sentidos carregam; save de PC cai no fluxo de "dados danificados" do jogo, sem queda | concluída | [08](plano/F4/08.md) |
| 9 | WSL | Os outros modos pela mesma linha de produção (9a História, 9b Ultimate Battle e 9c os demais) | `unpacks.py` sem pendência; `--disco` 100% em todas as faixas de menu | concluída no WSL, até a 9c-4; limites de execução e imagens nas notas; rodada completa no fim da F4 | [09a](plano/F4/09a.md), [09b](plano/F4/09b.md), [09c](plano/F4/09c.md) |
| 10 | console | Sessão de 30 min com o Lucas pelos modos | o critério da F4 sem queda | concluída | [10](plano/F4/10.md) |

## 6. Riscos principais

1. **Desempenho no Xenon.** No PC, softfloat e interpretador de VU1 dominam o frame. F2 existe para medir isso antes de investir em gráficos.
2. **Endianness.** Arquivos usados como structs, cerca de 210 KB de tabelas do ELF, 167 bitfields, 60 unions.
3. **Compilador.** Se nenhum candidato do F0 servir, reescrever partes para C89 é caro.
4. **Upstream instável.** O mantenedor é um só e muda o código rápido. Por isso o merge do upstream é manual e de propósito, nunca automático.
5. **Toolchain.** O wine da imagem vem do master sem tag. Fazer backup da imagem antes de qualquer rebuild.
