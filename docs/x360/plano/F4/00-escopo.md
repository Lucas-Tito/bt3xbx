# F4: escopo, redes de cobertura e critério

[Índice do plano](../../../plano.md) | próxima: [01-02](01-02.md)

### F4 em etapas (13/10/2026)

Escopo e decisões na decisão 22. A queda do menu principal (`MainMenu_Init`, `src/menu/menu_a.c`) é a primeira das
77 chamadas de `Sprite_Unpack` sem conversor (`formatos/menus.md` seção 11; a contagem antiga dizia 76). Para não ir caindo uma por uma no console,
quatro redes de cobertura:

- o código: verificador que falha em `Sprite_Unpack` sem gancho;
- o disco inteiro: o "sem conversor" do `conv_tool --disco` por faixa de id;
- roteiros de controle no WSL, com rastreio;
- uma guarda no jogo, que nomeia o chamador sem conversor em vez de cair num endereço inválido.

| # | Onde | O que | Critério |
|---|---|---|---|
| 1 | WSL | Inventário das 77 chamadas (id, seção, formato, leitores), censo do `--disco` por faixa, verificador `unpacks.py`, uma tabela de formatos só em `conv.c` | toda chamada com formato proposto; `unpacks.py` lista as pendentes |
| 2 | WSL | Guarda em `sprite.c` (`PORT_BIG_ENDIAN`); Dragon Net Battle escondido no 360 e nos roteiros | a PPC sem mapa para com "sem conversor em MainMenu_Init"; a `.text` da x86 inalterada |
| 3 | console | Save em `game:\saves` robusto: gravação em temporário e troca de nome, `fclose` conferido | salvar, sair e abrir de novo: o título tem Continue e a opção volta como estava |
| 4 | WSL | Roteiros de controle em texto, ancorados na troca de tela; desbloqueio de tudo no headless; um roteiro por modo | cada roteiro repete na x86 as telas de referência; o "sem conversor" do `--mapa` é igual ao inventário |
| 5 | WSL | Especificações e conversores do caminho essencial: menu principal, submenu do Duelo (`DuelMenu`), Duelo (personagem, time, estágio), Treino, Opções com o save delas | `--disco` 100% nessas faixas; `--mapa` sem divergência; PPC sem mapa igual à x86 em cada roteiro |
| 6 | WSL | Rasterizador x86 e PPC nas telas essenciais | PPM idênticos |
| 7 | console | Caminho essencial no console, pelos roteiros (`BT3_ROTEIRO`) | mesmas telas da x86; capturas no patamar da F3; nenhuma queda |
| 8 | WSL e console | Save entre builds (PPC e 360) | os dois sentidos carregam; save de PC cai no fluxo de "dados danificados" do jogo, sem queda |
| 9 | WSL | Os outros modos pela mesma linha de produção (9a História e 9b Ultimate Battle, feitas; 9c o resto) | `unpacks.py` sem pendência; `--disco` 100% em todas as faixas de menu |
| 10 | console | Sessão de 30 min com o Lucas pelos modos | o critério da F4 sem queda |

Critério da F4: no console, com o controle, do boot ao começo da luta pelo Duelo e pelo Treino, Opções com salvar e
carregar, e os outros modos navegáveis; a sequência de telas igual à x86. Antes da etapa 5, um revisor independente
(guia, seção 4).
