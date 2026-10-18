# Tarefa: F5, etapa 8, o degrau 1 da divisão em threads, as medições baratas e o corte do hash de página

[Índice do plano](../plano.md) | estudo: [F5/08-threads.md](../plano/F5/08-threads.md)

Branch: `cursor/f5-degrau1` (a partir de `troca/main`). Relatório: `~/entrega-f5-degrau1.md`. Orçamento: 6 a 10 h.

Leia antes, inteiro, `docs/x360/plano/F5/08-threads.md` (o estudo; você o escreveu) e a seção "Rodada no console
(18/10/2026)" do `docs/x360/plano/F5/08.md`. O Lucas aprovou a recomendação do estudo, com as respostas conservadoras
da seção 9 (as capacidades das listas não encolhem agora; o gatilho da decisão 24, resposta 5, vale só para o degrau
1, desligável pelo `ambiente.txt`; a thread de desenho dona do dispositivo com a main só vigiando; gravação na thread
de hardware 0, desenho na 1, nunca na 3 até medir). Faça, nesta ordem, cada parte com commit próprio:

1. **As medições baratas da seção 8** que rodam no WSL: o contador de bytes por quadro nas builds `--gpu`, e a rodada
   do `BT3_GS_THREAD=1` sem mudar nada (a linha de base de quadros divergentes). Registre os números no `08-threads.md`.
2. **O corte do hash de página** (seção 5, item 2 do estudo: o `Gs_PageHash` de 8 KB por página, 351 páginas por quadro):
   a forma mais segura que você achar, no código portável e dentro de `#ifdef PORT_BIG_ENDIAN` com o original no
   `#else`, provada pelas listas de desenho quadro a quadro (x86g contra x86gv e ppcg antes contra depois, como na
   primeira parte da etapa 8) e pelos roteiros.
3. **O degrau 1, a opção (c):** a gravação inteira numa thread, com as esperas que o PS2 já fazia (`sceGsSyncPath`,
   `Dma_ResetBuffers`, `Dma_Lend/ReclaimBufferTails`, o início das cargas esperando a thread, o contador de quadros do
   lado do jogo, o `gGsFrame` e o descarte com dois quadros). A parte portável no `port/src/gs` dentro de
   `#ifdef PORT_BIG_ENDIAN` (ou de uma chave de teste que só as builds de teste definem, como o `BT3_TESTE_VU1` da
   etapa 8), provada no WSL pelas listas de desenho quadro a quadro e pelo `BT3_GS_RACE` nos `f5_duelo` e `f5_treino`
   (nenhum quadro divergente). A parte do 360 (a thread, a posse do dispositivo, a saída, a Guide, o vigia) em
   `port/x360/src`, desligável por uma chave no `ambiente.txt` (por exemplo `BT3_GS_THREAD`); você NÃO consegue compilar
   o 360: escreva no C89 do `cl.exe` (declarações no começo do bloco, sem `inline`) e no C++ do backend, com cuidado, e
   diga no relatório o que não pôde ser conferido.

## Critério

- No commit final: as três builds refeitas, `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh f5_duelo f5_treino` e
  `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo` tudo certo, um emulador por vez; as listas de desenho iguais
  antes e depois em cada parte; a `.text` da x86 (`cc46fa0752f092fca305bc8bf2e2e54390763af0`) só muda por código de
  teste, provado pelos objetos.
- Cada verificador novo visto falhando de propósito.
- Ao fim, a seção "Degrau 1" no `08-threads.md` com o que foi feito, as provas, a estimativa de ganho no 360, o que
  falta compilar e conferir no console e o roteiro da rodada (o `ambiente.txt` com a chave ligada e desligada).

## Limites

O WSL tem uns 7 GB de memória e o disco do Windows é limitado: processe arquivo por arquivo e apague as saídas grandes
de `port/build/roteiros` depois de cada rodada conferida. Ao final: relatório em `~/entrega-f5-degrau1.md`,
`git push troca cursor/f5-degrau1`, git limpo, nenhum processo seu.
