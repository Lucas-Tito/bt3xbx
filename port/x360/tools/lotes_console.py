#!/usr/bin/env python3
"""F5, seção 38: prepara as rodadas de console do relógio da gravação e da imagem do BT3_GS_LOTES, sem executar build
nem tocar no console.

relogio: duas builds do mesmo commit, as duas de perfil (o relógio só existe nelas), pela build_f2.sh:
  perfil       BT3_PERFIL_CADEIA=1
  perfil+alfa  BT3_PERFIL_CADEIA=1 BT3_ALFA_ORIGEM=1      (o log diz "gs_d3d: alfa exato na origem")
Cada build abre alternando com os carimbos por desenho (BT3_RELOGIO_GRAV com os quadros) e sem eles
(BT3_RELOGIO_MARCAS=0: o controle da mesma build, que escreve só J, G, P e U dos mesmos quadros e diz o que os carimbos
custam, sem depender do limiar do picos). As marcas da cadeia ficam desligadas (BT3_CADEIA_CPU=0) e o picos e a tela
por imagem ligados (J e G do relógio vêm do anel do picos). Saves novos e únicos por abertura.

imagem: a build normal e a dos lotes (BT3_GS_LOTES=1) do mesmo commit, e a dos lotes com BT3_LOTES=0 (o controle da
mesma build), alternadas, capturando os mesmos quadros na resolução interna (BT3_SHOT_LISTA, BT3_SHOT_CHEIA=1), cada
abertura na sua pasta. A prova do plano previsto e do DATE curto é a captura igual byte a byte à da normal, mais os
contadores do log (fora da vaga, alvo lido sem Resolve e DATE lendo fora em 0).

    lotes_console.py relogio SAIDA --rodada rl37a2810 [--quadros 6900,6930,6960,7000,7170] [--repeticoes 2]
    lotes_console.py imagem SAIDA --rodada li37a2810 [--quadros ...] [--repeticoes 2]
    lotes_console.py --mutantes   (os ambientes estragados têm de ser recusados pelo conferidor)
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile

ANTIGAS = ('rp32a2810', 'rp33a2810', 'ap35a2810')
QUADROS_IMAGEM = '6900,6928,6929,6930,6931,6932,6958,6959,6960,6961,6962,7000,7100,7168,7169,7170,7171,7172,7300,7500,8000,8300'
COMUM = ['-BT3_DEMO', 'BT3_NOMOVIE=1', 'BT3_DESBLOQUEIO=1', r'BT3_ROTEIRO=game:\f5_duelo.txt',
         'BT3_GS_THREAD=1', 'BT3_GS_DESENHO=1', 'BT3_GS_DESENHO_HW=3', 'BT3_SOM=1', 'BT3_WIDE=1',
         'BT3_ESCALA=1.33', 'BT3_FXAA=1', 'BT3_VU1_DIRETO=1', 'BT3_ENVIO_RAPIDO=1', 'BT3_TEX_ORIGEM=1',
         'BT3_TEMPOS=0', 'BT3_PICOS=36', 'BT3_PICOS_TELA=1', 'BT3_ALFA_CONFERE=0', 'BT3_VU_CONSTANTES=0',
         'BT3_VU_FECHO=0', 'BT3_CADEIA_CPU=0', 'BT3_CORTES=0', 'BT3_CORTES_GPU=0', 'BT3_COMUM_GPU=0']


def prepara(saida, rodada, quadros, repeticoes, modo='relogio'):
    if not rodada.isascii() or not rodada.isalnum() or not 1 <= len(rodada) <= 12:
        raise ValueError('identificador ASCII alfanumérico de 1 a 12 caracteres')
    if rodada in ANTIGAS:
        raise ValueError(f'a rodada {rodada} já foi usada')
    if not quadros or len(quadros) > (16 if modo == 'relogio' else 32) or any(q <= 0 for q in quadros):
        raise ValueError('de 1 a 16 quadros positivos (32 nas imagens)')
    saida.mkdir(parents=True, exist_ok=False)
    raiz = Path(__file__).resolve().parents[3]
    for n in ('boot', 'f5_duelo'):
        shutil.copyfile(raiz / 'port/x360/roteiros' / (n + '.txt'), saida / (n + '.txt'))
    casos = []
    for i in range(1, repeticoes + 1):  # builds e chaves alternadas na mesma sessão
        if modo == 'relogio':
            casos += [(f'rel{i}', 'perfil', 1), (f'ctl{i}', 'perfil', 0), (f'rela{i}', 'perfil+alfa', 1), (f'ctla{i}', 'perfil+alfa', 0)]
        else:
            casos += [(f'nor{i}', 'normal', 0), (f'lot{i}', 'lotes', 1), (f'lct{i}', 'lotes', 0)]
    lista = ','.join(map(str, quadros))
    manifesto = []
    for nome, build, liga in casos:
        save = f'sv_{rodada}_{nome}'
        if len(save) > 42:
            raise ValueError('save excede FATX')
        if modo == 'relogio':
            extra = [f'BT3_RELOGIO_GRAV={lista}', f'BT3_RELOGIO_MARCAS={liga}']
            m = {'relogio': lista, 'marcas': liga}
        else:
            pasta = f'shots_{rodada}_{nome}'
            extra = ['BT3_RELOGIO_GRAV=0', f'BT3_LOTES={liga}', f'BT3_SHOT_LISTA={lista}', 'BT3_SHOT_CHEIA=1', f'BT3_SHOT_PASTA={pasta}']
            m = {'relogio': '0', 'lotes': liga, 'pasta': pasta}
        linhas = COMUM + extra + [f'BT3_SAVES=game:\\{save}']
        (saida / f'environment_{nome}.txt').write_text('\n'.join(linhas) + '\n')
        manifesto.append(dict({'abertura': nome, 'build': build, 'save': save, 'modo': modo}, **m))
    (saida / 'manifesto.json').write_text(json.dumps(manifesto, indent=2) + '\n')
    (saida / 'ordem.txt').write_text('\n'.join(f'{c[0]} {c[1]}' for c in casos) + '\n')
    shutil.copyfile(saida / f'environment_{casos[0][0]}.txt', saida / 'environment.txt')
    if modo == 'imagem':
        (saida / 'LEIA.txt').write_text(f'''Rodada {rodada}: a imagem do BT3_GS_LOTES, primeira parte (F5, seção 38 do 08-gravacao.md).
Duas builds do mesmo commit (ordem.txt: abertura e build): normal (build_f2.sh) e lotes (BT3_GS_LOTES=1). Antes de abrir:
conferir o carimbo, a linha "gs_d3d: lotes (BT3_GS_LOTES), primeira parte ligada" só nas lot* ("desligada" nas lct*), e a
inexistência de TODOS os saves e pastas de captura do manifesto. Se qualquer um existir, gerar outra rodada inteira.
Executar na ordem, cada abertura com o ambiente_<nome>.txt como environment.txt. Trazer o log e a pasta game:\\<pasta> de cada uma.
A prova: compara_quadros.py <pasta lot> <pasta nor> com todos os pixels iguais em todos os quadros {lista}, e o mesmo para
lct contra nor (o controle); no log das lot, "lotes, no trecho" com planos previstos e, em todas, "fora da vaga 0", "alvo lido
sem Resolve 0" e o DATE "lendo fora do que o alvo tinha" em 0 na normal. Imagem igual não é ganho: o envio ainda espera a entrega.
''')
        return manifesto
    (saida / 'LEIA.txt').write_text(f'''Rodada {rodada}: o relógio da gravação por desenho no Xenon (F5, seção 38 do 08-gravacao.md).
Duas builds de perfil do mesmo commit (ordem.txt: abertura e build): perfil (BT3_PERFIL_CADEIA=1) e perfil+alfa
(BT3_PERFIL_CADEIA=1 BT3_ALFA_ORIGEM=1). Antes de abrir: conferir o carimbo, a linha "gs_d3d: alfa exato na origem" só
na perfil+alfa, a linha "relogio: o relógio da gravação ligado" só nas aberturas rel*, e a inexistência de TODOS os saves
do manifesto. Se qualquer save existir, gerar outra rodada inteira. Não reutilizar {", ".join(ANTIGAS)}.
Executar na ordem, cada abertura com o ambiente_<nome>.txt como environment.txt. Preservar os logs com o nome da abertura e SHA256.
O que se lê: nas rel*, as linhas "relogio:" dos quadros {lista} com os carimbos por desenho (prefixos_teto.py --console
LOG --so-relogio confere; com LISTAS e DADOS, o modelo C); nas ctl*, só a linha "relogio: fim" dos mesmos quadros (J, G, P
e U), para comparar com e sem os carimbos.
É medida, não aceite: a build de perfil não é a normal (seção 35), e nenhum ganho dos prefixos foi integrado.
''')
    return manifesto


def confere(pasta, rodada):
    """o que a pasta tem de ter antes de ir ao console; ValueError no primeiro problema"""
    man = json.loads((pasta / 'manifesto.json').read_text())
    saves = [m['save'] for m in man]
    if len(set(saves)) != len(saves) or any(not s.startswith(f'sv_{rodada}_') or len(s) > 42 for s in saves):
        raise ValueError('saves repetidos, de outra rodada ou longos demais')
    for m in man:
        t = (pasta / f'environment_{m["abertura"]}.txt').read_text().splitlines()
        if t[:len(COMUM)] != COMUM:
            raise ValueError(f'{m["abertura"]}: ambiente comum diferente')
        rel = [x for x in t if x.startswith('BT3_RELOGIO_GRAV=')]
        if rel != [f'BT3_RELOGIO_GRAV={m["relogio"]}'] or not re.fullmatch(r'0|\d+(,\d+){0,15}', m['relogio']):
            raise ValueError(f'{m["abertura"]}: relógio diferente do manifesto')
        if m['modo'] == 'relogio':
            mar = [x for x in t if x.startswith('BT3_RELOGIO_MARCAS=')]
            if m['relogio'] == '0' or mar != [f'BT3_RELOGIO_MARCAS={m["marcas"]}'] or \
                    m['abertura'].startswith('rel') != (m['marcas'] == 1):
                raise ValueError(f'{m["abertura"]}: abertura de relógio sem carimbos, controle com eles, ou sem a lista')
        if t[-1] != f'BT3_SAVES=game:\\{m["save"]}':
            raise ValueError(f'{m["abertura"]}: save diferente do manifesto')
        if m['modo'] == 'relogio' and m['build'] not in ('perfil', 'perfil+alfa'):
            raise ValueError(f'{m["abertura"]}: build {m["build"]} sem o relógio')
        if m['modo'] == 'imagem':
            pastas = [x['pasta'] for x in man]
            if len(set(pastas)) != len(pastas) or len(m['pasta']) > 30 or not m['pasta'].startswith(f'shots_{rodada}_'):
                raise ValueError(f'{m["abertura"]}: pasta de captura repetida, longa ou de outra rodada')
            if f'BT3_SHOT_PASTA={m["pasta"]}' not in t or f'BT3_LOTES={m["lotes"]}' not in t or 'BT3_SHOT_CHEIA=1' not in t:
                raise ValueError(f'{m["abertura"]}: captura ou lotes diferentes do manifesto')
            if (m['build'] == 'lotes') != m['abertura'].startswith('l') or (m['lotes'] == 1) != m['abertura'].startswith('lot'):
                raise ValueError(f'{m["abertura"]}: build ou chave trocada')
    if (pasta / 'environment.txt').read_text() != (pasta / f'environment_{man[0]["abertura"]}.txt').read_text():
        raise ValueError('environment.txt não é o da primeira abertura')
    return len(man)


def mutantes():
    with tempfile.TemporaryDirectory() as d:
        base = Path(d) / 'boa'
        prepara(base, 'teste1', [6930, 6960], 2)
        assert confere(base, 'teste1') == 8
        estragos = {
            'save repetido': ('manifesto.json', lambda s: s.replace('sv_teste1_ctl1', 'sv_teste1_rel1')),
            'carimbos no controle': ('environment_ctl1.txt', lambda s: s.replace('BT3_RELOGIO_MARCAS=0', 'BT3_RELOGIO_MARCAS=1')),
            'relógio desligado': ('environment_rel2.txt', lambda s: s.replace('BT3_RELOGIO_GRAV=6930,6960', 'BT3_RELOGIO_GRAV=0')),
            'controle sem a lista': ('manifesto.json', lambda s: s.replace('"relogio": "6930,6960",\n    "marcas": 0', '"relogio": "0",\n    "marcas": 0', 1)),
            'cadeia ligada': ('environment_rela1.txt', lambda s: s.replace('BT3_CADEIA_CPU=0', 'BT3_CADEIA_CPU=1')),
            'picos desligado': ('environment_rel1.txt', lambda s: s.replace('BT3_PICOS=36', 'BT3_PICOS=0')),
            'build normal': ('manifesto.json', lambda s: s.replace('"perfil+alfa"', '"alfa"', 1)),
            'save de outra rodada': ('environment_ctla2.txt', lambda s: s.replace('sv_teste1_ctla2', 'sv_ap35a2810_ctla2')),
        }
        for nome, (arq, f) in estragos.items():
            c = Path(d) / re.sub(r'\W', '_', nome)
            shutil.copytree(base, c)
            (c / arq).write_text(f((c / arq).read_text()))
            try:
                confere(c, 'teste1')
            except ValueError as e:
                print(f'mutante recusado: {nome} ({e})')
            else:
                raise AssertionError(f'mutante aceito: {nome}')
        img = Path(d) / 'img'
        prepara(img, 'teste2', [6930, 6960], 2, 'imagem')
        assert confere(img, 'teste2') == 6
        for nome, (arq, f) in {
            'lotes desligado na lot': ('environment_lot1.txt', lambda s: s.replace('BT3_LOTES=1', 'BT3_LOTES=0')),
            'pasta repetida': ('environment_lct2.txt', lambda s: s.replace('shots_teste2_lct2', 'shots_teste2_lot2')),
            'build trocada': ('manifesto.json', lambda s: s.replace('"build": "normal"', '"build": "lotes"', 1)),
            'captura reduzida': ('environment_nor2.txt', lambda s: s.replace('BT3_SHOT_CHEIA=1', 'BT3_SHOT_CHEIA=0')),
        }.items():
            c = Path(d) / ('i_' + re.sub(r'\W', '_', nome))
            shutil.copytree(img, c)
            (c / arq).write_text(f((c / arq).read_text()))
            try:
                confere(c, 'teste2')
            except ValueError as e:
                print(f'mutante recusado: {nome} ({e})')
            else:
                raise AssertionError(f'mutante aceito: {nome}')
        for ruim in ('rp33a2810', 'nome-ruim'):
            try:
                prepara(Path(d) / ruim, ruim, [6960], 1)
            except ValueError as e:
                print(f'rodada recusada: {ruim} ({e})')
            else:
                raise AssertionError(f'rodada aceita: {ruim}')
    print('tudo certo nos mutantes do lotes_console')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('modo', nargs='?', choices=('relogio', 'imagem')); p.add_argument('saida', type=Path, nargs='?')
    p.add_argument('--rodada'); p.add_argument('--quadros'); p.add_argument('--repeticoes', type=int, default=2)
    p.add_argument('--mutantes', action='store_true')
    a = p.parse_args()
    if a.mutantes:
        mutantes(); sys.exit(0)
    if a.modo is None or a.saida is None or not a.rodada:
        p.error('informe o modo, SAIDA e --rodada')
    qs = a.quadros or ('6900,6930,6960,7000,7170' if a.modo == 'relogio' else QUADROS_IMAGEM)
    try:
        m = prepara(a.saida, a.rodada, [int(x) for x in qs.split(',')], a.repeticoes, a.modo)
        confere(a.saida, a.rodada)
    except (ValueError, OSError) as e:
        p.exit(1, f'FALHA: {e}\n')
    print(f'{len(m)} aberturas, {len({x["save"] for x in m})} saves exclusivos preparados; conferir inexistência no console.')
