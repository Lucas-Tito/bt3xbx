# Importação e exportação de saves pela pasta do jogo

Código: `port/x360/src/save_importa.c/.h` (a lógica, sem E/S direta), `port/x360/src/f2/save_importa_360.c/.h` (o sistema
de arquivos do Win32), `port/x360/src/f2/importa_tela.cpp` e `importa_tela_desenho.h` (a tela SAVE IMPORT), o gancho
`Port_X360SaveGravado` em `port/src/plat_mc.c` (atrás de `PORT_X360`) e a chamada em `main360.cpp`. A conversão em si é de
[save-conversao](save-conversao.md). O módulo mora em `src` e não em `conv` porque o selo da ISO convertida conta todo
`.c` e `.h` de `conv` (`convert_iso.py`): um arquivo novo lá invalida as ISOs já convertidas.

## Arquitetura

`SaveImp_*` fala com o disco por `SaveFs`, uma tabela de funções (`ler`, `gravar`, `mover`, `apagar`, `tipo`,
`criar_pasta`, `listar`, `log`) que devolvem 0 ou o código do sistema. O 360 as preenche com `CreateFile`, `MoveFile`,
`DeleteFile`, `CreateDirectory`, `FindFirstFile` e `GetFileAttributes`; o teste do Linux, com a libc e uma falha injetável
(`teste_save_importa_fs.c`). Os buffers são estáticos (uns 370 KB): uma chamada por vez.

## Importação

Pastas: `game:\import`, `game:\backup`, `<BT3_SAVES>\card1\BASLUS-21678DBZT3`. Na abertura do jogo, depois das conferências da
ISO e antes de o jogo ler o cartão, `ImportaTela_Executar` chama `SaveImp_Procurar`: lista os `*.psu` soltos em `import` (as
subpastas não contam; qualquer caixa de letra), em ordem de nome, cada um conferido por `Psu_Ler` e `SaveConv_Ps2Para360`.
Sem nenhum, nada aparece (custa uma procura de arquivo).

A tela abre com SKIP selecionado e o cursor no primeiro arquivo OK. Cima e baixo escolhem o arquivo, esquerda e direita a
ação, A confirma, B vale SKIP. IMPORT num arquivo recusado só mostra um aviso. O controle é lido direto com `XInputGetState`
e só vale botão visto solto e apertado de novo (como a pergunta da conversão da ISO).

`SaveImp_Importar`, nesta ordem:

1. Lê o `.psu` de novo do disco e converte (não se confia na procura). Falha: nada foi tocado.
2. Se há save, copia os três arquivos para `backup\NNN` (NNN = o próximo número livre, 001, 002...; o console não tem
   relógio confiável). Falha: nada foi tocado no save.
3. Grava `NOME.tmp` dos três arquivos na pasta do save. Falha: os `.tmp` se apagam e o save atual segue intacto.
4. Troca arquivo por arquivo como o `mc_commit`: `.tmp` vira `.new`, o antigo sai, `.new` toma o nome (`MoveFile` não substitui).
   Ordem: `icon.sys`, `dbzsm.ico`, dados (o que o jogo lê, por último). Falha em qualquer ponto: o que havia é recolocado a
   partir da cópia em memória (`restaurado = 1`), e um arquivo que não existia antes some. Se a recolocação também falhar
   (`restaurado = 0`), a tela diz onde está a cópia (`backup\NNN`); no pior ponto (arquivo de dados já apagado e o `.new` que
   não renomeia) sobra um `.new` completo, que o `mc_recover` do jogo adota na próxima abertura, e a cópia fica no backup.
5. Move os `.psu`: o escolhido para `import\done`, os recusados para `import\failed`, os outros para `import\skipped`. Nada é
   apagado; nome que já existe no destino ganha `_2`, `_3`... Falha ao mover: o save já está no lugar e não se desfaz; a
   tela diz quantos arquivos ficaram, e a tela de importação pode reaparecer na próxima abertura (importar de novo é inofensivo,
   cria só outro backup).

SKIP (e B): só o passo 5 (os válidos para `skipped`). Depois de qualquer decisão a pasta `import` fica sem `.psu` solto.

## Exportação

`Port_X360SaveGravado(caminho)` é chamada por `mc_commit` depois de gravar com sucesso. Se `caminho` é o arquivo de dados
(`BASLUS-21678DBZT3\BASLUS-21678DBZT3`) e existe a pasta `game:\export`, `SaveImp_Exportar` lê os três arquivos atuais,
converte (`SaveConv_360ParaPs2`, que confere a soma), monta o `.psu` (`Psu_Escrever`) e o grava em
`export\BASLUS-21678DBZT3.psu` por temporário e troca. Sem a pasta, nada (nem a pasta é criada). A função do gancho devolve
`void` e `SaveImp_AposGravar` sempre 0: uma falha vai só ao log (`exporta: FALHOU ...`) e nunca ao save do jogo. As datas do
`.psu` são fixas (2007-11-13 12:00:00, o lançamento nos EUA), porque o console não tem relógio confiável.

## Testes

`teste_save_importa.py` (lógica, 15 mutantes), `teste_importa_tela.py` (entrada da tela com o XInput falso, 12 mutantes),
`prova_importa.py` (desenho, PNGs, 13 mutantes). Saves sintéticos: nenhum byte do jogo. Falta o teste no console (FATX real,
o `MoveFile`, `FindFirstFile` e a tela na TV).
