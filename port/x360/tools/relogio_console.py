#!/usr/bin/env python3
"""F5, seção 37: prepara a rodada de console do relógio da gravação, sem executar build nem tocar no console.

Duas builds do mesmo commit, as duas de perfil (o relógio só existe nelas), pela build_f2.sh:
  perfil       BT3_PERFIL_CADEIA=1
  perfil+alfa  BT3_PERFIL_CADEIA=1 BT3_ALFA_ORIGEM=1      (o log diz "gs_d3d: alfa exato na origem")
Cada build abre alternando com o relógio ligado (BT3_RELOGIO_GRAV com os quadros) e desligado (controle da mesma build:
diz o que os carimbos custam em G, P e U). As marcas da cadeia ficam desligadas (BT3_CADEIA_CPU=0) e o picos e a tela
por imagem ligados (J e G do relógio vêm do anel do picos). Saves novos e únicos por abertura.

    relogio_console.py SAIDA --rodada rl37a2810 [--quadros 6900,6930,6960,7000,7170] [--repeticoes 2]
    relogio_console.py --mutantes   (os ambientes estragados têm de ser recusados pelo conferidor)
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile

ANTIGAS = ('rp32a2810', 'rp33a2810', 'ap35a2810')
COMUM = ['-BT3_DEMO', 'BT3_NOMOVIE=1', 'BT3_DESBLOQUEIO=1', r'BT3_ROTEIRO=game:\f5_duelo.txt',
         'BT3_GS_THREAD=1', 'BT3_GS_DESENHO=1', 'BT3_GS_DESENHO_HW=3', 'BT3_SOM=1', 'BT3_WIDE=1',
         'BT3_ESCALA=1.33', 'BT3_FXAA=1', 'BT3_VU1_DIRETO=1', 'BT3_ENVIO_RAPIDO=1', 'BT3_TEX_ORIGEM=1',
         'BT3_TEMPOS=0', 'BT3_PICOS=36', 'BT3_PICOS_TELA=1', 'BT3_ALFA_CONFERE=0', 'BT3_VU_CONSTANTES=0',
         'BT3_VU_FECHO=0', 'BT3_CADEIA_CPU=0', 'BT3_CORTES=0', 'BT3_CORTES_GPU=0', 'BT3_COMUM_GPU=0']


def prepara(saida, rodada, quadros, repeticoes):
    if not rodada.isascii() or not rodada.isalnum() or not 1 <= len(rodada) <= 12:
        raise ValueError('identificador ASCII alfanumérico de 1 a 12 caracteres')
    if rodada in ANTIGAS:
        raise ValueError(f'a rodada {rodada} já foi usada')
    if not quadros or len(quadros) > 16 or any(q <= 0 for q in quadros):
        raise ValueError('de 1 a 16 quadros positivos')
    saida.mkdir(parents=True, exist_ok=False)
    raiz = Path(__file__).resolve().parents[3]
    for n in ('boot', 'f5_duelo'):
        shutil.copyfile(raiz / 'port/x360/roteiros' / (n + '.txt'), saida / (n + '.txt'))
    casos = []
    for i in range(1, repeticoes + 1):  # builds e relógio alternados na mesma sessão
        casos += [(f'rel{i}', 'perfil', 1), (f'ctl{i}', 'perfil', 0), (f'rela{i}', 'perfil+alfa', 1), (f'ctla{i}', 'perfil+alfa', 0)]
    lista = ','.join(map(str, quadros))
    manifesto = []
    for nome, build, liga in casos:
        save = f'sv_{rodada}_{nome}'
        if len(save) > 42:
            raise ValueError('save excede FATX')
        linhas = COMUM + [f'BT3_RELOGIO_GRAV={lista if liga else 0}', f'BT3_SAVES=game:\\{save}']
        (saida / f'ambiente_{nome}.txt').write_text('\n'.join(linhas) + '\n')
        manifesto.append({'abertura': nome, 'build': build, 'save': save, 'relogio': lista if liga else '0'})
    (saida / 'manifesto.json').write_text(json.dumps(manifesto, indent=2) + '\n')
    (saida / 'ordem.txt').write_text('\n'.join(f'{c[0]} {c[1]}' for c in casos) + '\n')
    shutil.copyfile(saida / f'ambiente_{casos[0][0]}.txt', saida / 'ambiente.txt')
    (saida / 'LEIA.txt').write_text(f'''Rodada {rodada}: o relógio da gravação por desenho no Xenon (F5, seção 37 do 08-gravacao.md).
Duas builds de perfil do mesmo commit (ordem.txt: abertura e build): perfil (BT3_PERFIL_CADEIA=1) e perfil+alfa
(BT3_PERFIL_CADEIA=1 BT3_ALFA_ORIGEM=1). Antes de abrir: conferir o carimbo, a linha "gs_d3d: alfa exato na origem" só
na perfil+alfa, a linha "relogio: o relógio da gravação ligado" só nas aberturas rel*, e a inexistência de TODOS os saves
do manifesto. Se qualquer save existir, gerar outra rodada inteira. Não reutilizar {", ".join(ANTIGAS)}.
Executar na ordem, cada abertura com o ambiente_<nome>.txt como ambiente.txt. Preservar os logs com o nome da abertura e SHA256.
O que se lê: nas rel*, as linhas "relogio:" dos quadros {lista} (prefixos_teto.py --console LOG --so-relogio confere;
com LISTAS e DADOS, o modelo C); nas ctl*, as mesmas linhas "picos: tela" para comparar J/G/P/U com e sem os carimbos.
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
        t = (pasta / f'ambiente_{m["abertura"]}.txt').read_text().splitlines()
        if t[:len(COMUM)] != COMUM:
            raise ValueError(f'{m["abertura"]}: ambiente comum diferente')
        rel = [x for x in t if x.startswith('BT3_RELOGIO_GRAV=')]
        if rel != [f'BT3_RELOGIO_GRAV={m["relogio"]}'] or not re.fullmatch(r'0|\d+(,\d+){0,15}', m['relogio']):
            raise ValueError(f'{m["abertura"]}: relógio diferente do manifesto')
        if (m['abertura'].startswith('rel')) == (m['relogio'] == '0'):
            raise ValueError(f'{m["abertura"]}: abertura de relógio sem relógio, ou controle com ele')
        if t[-1] != f'BT3_SAVES=game:\\{m["save"]}':
            raise ValueError(f'{m["abertura"]}: save diferente do manifesto')
        if m['build'] not in ('perfil', 'perfil+alfa'):
            raise ValueError(f'{m["abertura"]}: build {m["build"]} sem o relógio')
    if (pasta / 'ambiente.txt').read_text() != (pasta / f'ambiente_{man[0]["abertura"]}.txt').read_text():
        raise ValueError('ambiente.txt não é o da primeira abertura')
    return len(man)


def mutantes():
    with tempfile.TemporaryDirectory() as d:
        base = Path(d) / 'boa'
        prepara(base, 'teste1', [6930, 6960], 2)
        assert confere(base, 'teste1') == 8
        estragos = {
            'save repetido': ('manifesto.json', lambda s: s.replace('sv_teste1_ctl1', 'sv_teste1_rel1')),
            'relógio no controle': ('ambiente_ctl1.txt', lambda s: s.replace('BT3_RELOGIO_GRAV=0', 'BT3_RELOGIO_GRAV=6930')),
            'relógio desligado': ('ambiente_rel2.txt', lambda s: s.replace('BT3_RELOGIO_GRAV=6930,6960', 'BT3_RELOGIO_GRAV=0')),
            'cadeia ligada': ('ambiente_rela1.txt', lambda s: s.replace('BT3_CADEIA_CPU=0', 'BT3_CADEIA_CPU=1')),
            'picos desligado': ('ambiente_rel1.txt', lambda s: s.replace('BT3_PICOS=36', 'BT3_PICOS=0')),
            'build normal': ('manifesto.json', lambda s: s.replace('"perfil+alfa"', '"alfa"', 1)),
            'save de outra rodada': ('ambiente_ctla2.txt', lambda s: s.replace('sv_teste1_ctla2', 'sv_ap35a2810_ctla2')),
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
        for ruim in ('rp33a2810', 'nome-ruim'):
            try:
                prepara(Path(d) / ruim, ruim, [6960], 1)
            except ValueError as e:
                print(f'rodada recusada: {ruim} ({e})')
            else:
                raise AssertionError(f'rodada aceita: {ruim}')
    print('tudo certo nos mutantes do relogio_console')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('saida', type=Path, nargs='?'); p.add_argument('--rodada')
    p.add_argument('--quadros', default='6900,6930,6960,7000,7170'); p.add_argument('--repeticoes', type=int, default=2)
    p.add_argument('--mutantes', action='store_true')
    a = p.parse_args()
    if a.mutantes:
        mutantes(); sys.exit(0)
    if a.saida is None or not a.rodada:
        p.error('informe SAIDA e --rodada')
    try:
        m = prepara(a.saida, a.rodada, [int(x) for x in a.quadros.split(',')], a.repeticoes)
        confere(a.saida, a.rodada)
    except (ValueError, OSError) as e:
        p.exit(1, f'FALHA: {e}\n')
    print(f'{len(m)} aberturas, {len({x["save"] for x in m})} saves exclusivos preparados; conferir inexistência no console.')
