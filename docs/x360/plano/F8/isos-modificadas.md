# ISOs modificadas: o que testamos e o que funcionou

[Conversão no console](03-conversao-console.md) | [Pendências](pendencias.md)

Registro para as notas da release: cada ISO modificada que passar pelo conversor entra aqui com onde foi testada e o
resultado. Nenhum byte do jogo vem para o repositório: só nomes, hashes e o resultado de cada teste.

O conversor aceita mods que **só mudam dados** e mantêm o executável do BT3 USA (`SLUS_216.78` e `BIN/DBZP.BIN`
iguais aos da SLUS-21678). Mods que **mudam o executável** precisam do código portado e ficam em outra frente (o BT4).
Patches `.pnach` do PCSX2 alteram o código MIPS original e não funcionam em port por decompilação.

## Fila de teste (escolhida em 04/11/2026)

O Lucas vai procurar as ISOs; a ordem é a de prioridade. Fonte do levantamento: busca na web de 04/11 (fóruns como o
Kanzenshuu, o guia de modding em vivethemodder.github.io e o LaunchBox).

| # | Mod | O que muda (pelo levantamento) | Executável | Por que testar |
|---|---|---|---|---|
| 1 | Sparking! Reborn | balanceamento competitivo de personagens e golpes | a conferir | o mais ativo; balanceamento costuma ser só dados |
| 2 | Versão Latino (Chuchoman / Kinnikuchu Projects) | dublagem e texto em espanhol latino | a conferir | o mesmo tipo de mudança que uma dublagem PT-BR |
| 3 | PT-BR montada direito, ou a Versão Brasileira (2017) | texto e dublagem PT-BR | a conferir | substitui a PT-BR defeituosa de 04/11 |
| 4 | ISO Canon ou ISO Transformation | personagens, mapas e história novos; transformações novas | a conferir | testa personagens novos nas vagas existentes |

## Resultados

| Data | ISO | Executável | Onde testamos | Resultado |
|---|---|---|---|---|
| 04/11/2026 | Dragon Ball Z: Budokai Tenkaichi 3 (Dub&Leg PT-BR), 3.730.440.192 bytes | igual ao USA | WSL: conversor no PC e no caminho do console (qemu); jogo na x86 e na PPC sem conversão | **recusada**. Vozes (16.095 entradas do AFS2) e 50 das 61 entradas com conversor convertem; 3 só tinham zeros a mais no fim e passaram. As outras 8 entradas do AFS1 (o menu de Opções e 7 modelos extras de golpes) são o mesmo pacote de menu do Duelo copiado 8 vezes: defeito da ISO. A própria x86, sem conversão, cai ao abrir Opções. Não insistir nela (decisão do Lucas). |
| 04/11/2026 | Budokai Tenkaichi 4 B14 Rev2 (ENG), 5.914.853.376 bytes | **outro** (`SLUS_219.78`, umas 170 funções mudadas) | análise da estrutura no WSL | fora do conversor de mods: vai pela frente do BT4 (decisão 16 revista), com a conversão pelo programa de PC em partes de até 4 GiB |

Para cada ISO nova: conferir `SYSTEM.CNF` e os hashes de `SLUS_216.78` e `BIN/DBZP.BIN`, rodar o inventário dos três
AFS e o conversor no WSL; depois, no console, a conversão, o menu principal, o menu de Opções, a seleção e uma luta
com os personagens mudados. Anotar aqui o que passou em cada lugar.
