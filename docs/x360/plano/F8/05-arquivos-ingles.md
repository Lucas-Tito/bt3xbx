# F8: nomes dos arquivos de diagnóstico em inglês

[Pendências](pendencias.md) | [Índice](../../plano.md)

O título lê apenas `game:\environment.txt`, e stdout/stderr vão para `game:\output.txt` com
`BT3_LOG=1`. O diário continua `game:\bt3xbx.log`. A configuração é entrada fornecida pelo projeto;
o título não a cria. Não há procura alternativa nem criação dos nomes anteriores.

Os três geradores, ferramentas de análise, roteiros e testes usam os nomes atuais. Foram renomeados
25 modelos `environment_*.txt`: 17 da F5/10, cinco da F6/7 e três da F6/8. Os ambientes com nomes de
cenários numerados conservam esses nomes, pois são modelos copiados como `environment.txt`, não a
saída de texto do título. Chaves e conteúdo permanecem em português.

O `subir.py` recebe nomes literalmente, tanto pelo basename quanto por `--nome`. Seus exemplos
agora usam os nomes atuais; a importação não abre rede. O ensaio usa FTP simulado e cobre envio,
troca atômica, md5, download e releitura da cópia `.lido`.

## Inventário reproduzível

Na raiz:

```sh
python3 port/x360/tools/teste_arquivos_ingles.py --mutantes \
  --inventario ~/arquivos-ingles-provas/inventario.json
```

A referência é `78dbb29c1187937d820ac15e634f5453f2bcdeee`. O JSON lista cada ocorrência em nome de
arquivo e conteúdo, com caminho, linha, texto e destino; inclui os nomes montados por concatenação
e globs. A busca considera nomes completos e seus prefixos de modelo, sem confundir identificadores
como `ambiente_txt` ou nomes de cenário como `18_saida.txt` com o arquivo de saída do título.

Antes: **492 ocorrências**. Depois: **37 antigas justificadas**, nenhuma ativa:

| Classe | Linhas | Por que ficaram |
|---|---:|---|
| Registros de rodadas anteriores | 34 | Preservam o nome dos artefatos, ambientes e textos observados na época |
| Controles negativos das caixas da ISO | 2 | Proíbem citar a configuração pelo nome anterior, além da proibição do nome atual |
| Comentário original no ramo upstream | 1 | O ramo `PORT_X360` tem o nome atual; o `#else` conserva a linha original |

[05-excecoes.json](05-excecoes.json) identifica cada linha por arquivo, número na base, SHA256 e motivo.
O verificador confere o texto na base e a quantidade de ocorrências para não autorizar uma cópia nova
da linha. Toda ocorrência que não esteja na lista faz o teste falhar. O inventário completo, inclusive
os nomes atuais e as justificativas individuais, fica fora do git no caminho escolhido acima.

## Provas locais

Executar com `set -euo pipefail` e `tee` para log em `~/arquivos-ingles-provas/`.

| Comando | Critério |
|---|---|
| `python3 port/x360/tools/teste_arquivos_ingles.py --mutantes` | Inventário, limite FATX, FTP simulado e quatro sabotagens: gravador, leitor, gerador e nome de modelo |
| `python3 port/x360/tools/teste_logs360.py` | Arquivos exatos com log, nenhum sem log; escrita real pelo nome anterior acusada; sete sabotagens de diagnóstico |
| `python3 port/x360/tools/teste_queda360.py` | Main real passa exclusivamente o caminho atual à carga do ambiente; nome anterior sabotado e sete sabotagens da queda acusadas |
| `python3 port/x360/tools/audita_logs360.py --mutantes` | 173 chaves, 69 ambientes versionados, 56 gerados e controles negativos |
| `python3 port/x360/tools/teste_cadeia_cpu.py --saida ~/arquivos-ingles-provas/cadeia` | Gerador com 34 ambientes, saves distintos, chamadas preservadas e sabotagens |
| `port/x360/tools/teste_diario.sh` e `--mutantes` | Diário normal e cinco sabotagens |
| `python3 port/x360/tools/teste_saida360.py --mutantes` | Saída normal e oito sabotagens |
| `python3 port/x360/tools/teste_paradas360.py` | Paradas dos leitores e do som, com controles negativos |
| `python3 port/x360/tools/teste_iso_avisos.py` | ISO sintética, caixas e controles negativos dos dois nomes da configuração |
| `port/x360/tools/teste_som360.sh` | Núcleo, Guide e leitura em blocos, com sabotagens |
| `python3 port/x360/tools/som_ambiente.py --mutantes` | Inventário do som e seis controles negativos |
| `python3 port/x360/tools/c89.py port/build/c89-ingles` | 317 fontes preparados, zero falhas |

## Objetos e limites

Preparar `asm`, `assets`, `disc` e `gamedata` como links para os dados somente leitura. Antes das
alterações, gerar as tabelas com `python3 port/tools/gen_data.py --asm "$PWD/asm"`, compilar
`python3 port/x360/tools/headless.py --alvo x86` e guardar a build em
`~/arquivos-ingles-provas/base-x86`. Recompilar do commit final e comparar:

```sh
python3 port/x360/tools/f5_final_confere.py objetos \
  ~/arquivos-ingles-provas/base-x86 port/build/x86
objcopy -O binary --only-section=.text port/build/x86/bt3_x86 \
  ~/arquivos-ingles-provas/final-text.bin
sha1sum ~/arquivos-ingles-provas/final-text.bin
```

São 399 objetos; a única diferença permitida é `obj/x360_passos.o`, cujo carimbo inclui o commit do
teste. A `.text` deve permanecer `43444ea4d03a6d448aa1ae74a631e35b356a7caa`.
Nesta tarefa não se executam emulador, console nem XDK. A build do 360 e a confirmação no FATX real
ficam para a revisão. Em pasta limpa, conferir que só o nome atual carrega as chaves, que o diagnóstico
cria `output.txt` e `bt3xbx.log`, e que sem `BT3_LOG=1` nenhum dos dois é criado.
