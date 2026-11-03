# Contêiner .psu (save do PS2)

Formato de exportação de save do PS2 (EMS, uLaunchELF, wLaunchELF, mymcplus). Código em
`port/x360/conv/psu.c` e `psu.h`; teste em `port/x360/tools/teste_psu.py`. A biblioteca só trata
o contêiner: o conteúdo dos arquivos entra e sai como bytes.

## Fonte

mymcplus 3.0.5 (`pip install mymcplus`): `mymcplus/save/format_ems.py` (`load`, `save`, `poll`),
`mymcplus/ps2mc_dir.py` (formato da entrada, `_dirent_fmt`, `_tod_fmt`, modos `DF_*`) e
`mymcplus/ps2mc.py` (`export_save_file`, `import_save_file`, `mkdir`, `create_dir_entry`).

## Estrutura

Sem cabeçalho nem assinatura. Tudo little-endian, entradas de 512 bytes:

1. entrada da pasta do save; o comprimento (u32 em 0x04) é o número de entradas da pasta, com `.` e `..`;
2. entrada `.` e entrada `..` (modo de pasta, comprimento 0);
3. para cada arquivo (comprimento - 2 vezes): a entrada de 512 bytes, depois os dados, completados com zeros
   até múltiplo de 1024 (`cluster_size = 1024` em `format_ems.py`). Arquivo de tamanho 0 não ocupa nada.

Subpastas não existem no formato (o mymcplus recusa).

## Entrada de 512 bytes

`_dirent_fmt = "<HHL8sLL8sL28x448s"`

| Offset | Tamanho | Campo |
|---|---|---|
| 0x00 | u16 | modo |
| 0x02 | u16 | não usado (zero) |
| 0x04 | u32 | comprimento (bytes do arquivo; entradas da pasta + 2 na pasta) |
| 0x08 | 8 | data de criação |
| 0x10 | u32 | primeiro cluster FAT do cartão |
| 0x14 | u32 | entrada pai no cartão |
| 0x18 | 8 | data de modificação |
| 0x20 | u32 | atributo |
| 0x24 | 28 | zeros |
| 0x40 | 448 | nome ASCII terminado em zero (o PS2 usa até 31 caracteres) |

Data (`_tod_fmt = "<xBBBBBH"`): byte reservado, segundo, minuto, hora, dia, mês, ano (u16). O relógio
do PS2 é JST (UTC+9); a biblioteca guarda os campos como estão, sem converter.

Modos (`DF_*`): `DF_FILE 0x10`, `DF_DIR 0x20`, `DF_EXISTS 0x8000`, `DF_0400`, `DF_RWX 0x07`.
Arquivo comum do cartão: `0x8497` (`EXISTS|0400|0080|FILE|RWX`); pasta: `0x8427`
(`EXISTS|0400|DIR|RWX`). `mode_is_file` e `mode_is_dir` olham só `FILE|DIR|EXISTS`.

## O que a biblioteca valida

- pelo menos três entradas; primeira, `.` e `..` com modo de pasta; `.` e `..` com esses nomes; comprimento >= 2
  (senão "not a PS2 save (.psu)");
- cada arquivo com modo de arquivo (subpasta é "bad directory entry"), nome de 1 a 31 caracteres terminado em zero
  dentro dos 32 primeiros bytes e sem barra (senão "bad name");
- entrada, dados e o completamento até 1024 dentro do buffer ("truncated file"), nada sobrando no fim
  ("trailing data"); o número de entradas é o da pasta, então diferença aparece como um desses dois.

O mymcplus é mais frouxo (lê o nome até 448 bytes, não exige o completamento do último arquivo
além do que `read_fixed` pede). A biblioteca usa só 32 bytes de nome, que é o que o PS2 escreve.

## Conferência do BT3 (USA)

Pasta `BASLUS-21678DBZT3` com exatamente `icon.sys` (964 bytes), `dbzsm.ico` (40.090) e
`BASLUS-21678DBZT3` (16.384). Erros separados: pasta errada, arquivo faltando, tamanho errado, arquivo extra.

## Escrita e igualdade com o mymcplus

`Psu_Escrever` produz o mesmo arquivo que o mymcplus exporta, byte a byte, exceto os campos que são do
cartão e não do save: 0x02, 0x10 (primeiro cluster), 0x14 (entrada pai) e 0x20 (atributo), que o mymcplus
copia do cartão e a biblioteca grava como zero (o cartão de destino os refaz na importação; o mymcplus
importa o resultado sem problema). As entradas `.` e `..` levam o modo `0x8427` e a data de criação da pasta
nas duas datas, como no `format_ems.save`. O `teste_psu.py` confere tudo isso, e os mutantes em
`teste_psu_mutantes.py` mostram que ele falha quando a biblioteca é estragada.
