#!/usr/bin/env python3
"""Prepara ambientes do f5_duelo para a revisão no Linux; não compila nem acessa o console."""
import argparse
import pathlib
import shutil

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('saida', type=pathlib.Path)
p.add_argument('--rodada', default='cortes1', help='identificador novo a cada repetição, para não reaproveitar saves')
a = p.parse_args()
if not a.rodada.isalnum() or len(a.rodada) > 12:
    raise SystemExit('use um identificador alfanumérico de até 12 caracteres')
a.saida.mkdir(parents=True, exist_ok=True)
raiz = pathlib.Path(__file__).resolve().parents[3]
for nome in ('boot', 'f5_duelo'):
    shutil.copyfile(raiz / 'port/x360/roteiros' / (nome + '.txt'), a.saida / (nome + '.txt'))
# As aberturas de comparação ficam sem marcas finas. O diagnóstico vai em aberturas próprias.
for nome, constantes, fecho, diag, gpu in [
    ('base', 0, 0, '0', '0'), ('constantes', 1, 0, '0', '0'),
    ('fecho', 0, 1, '0', '0'), ('ambos', 1, 1, '0', '0'),
    ('gravacao', 0, 0, '6900,7025', '0'), ('gpu6930', 0, 0, '0', '6929,6931'),
    ('gpu6960', 0, 0, '0', '6959,6961')]:
    linhas = [
        '-BT3_DEMO', 'BT3_NOMOVIE=1', 'BT3_DESBLOQUEIO=1', r'BT3_ROTEIRO=game:\f5_duelo.txt',
        'BT3_GS_THREAD=1', 'BT3_GS_DESENHO=1', 'BT3_SOM=1', 'BT3_WIDE=1',
        'BT3_ESCALA=1.33', 'BT3_FXAA=1', 'BT3_VU1_DIRETO=1', 'BT3_ENVIO_RAPIDO=1', 'BT3_TEMPOS=0',
        'BT3_TEX_ORIGEM=1', 'BT3_PICOS=36', 'BT3_PICOS_TELA=1',
        f'BT3_VU_CONSTANTES={constantes}', f'BT3_VU_FECHO={fecho}',
        f'BT3_CORTES={diag}', f'BT3_CORTES_GPU={gpu}',
        f'BT3_SAVES=game:\\sv_{a.rodada}_{nome}',
    ]
    (a.saida / ('ambiente_' + nome + '.txt')).write_text('\n'.join(linhas) + '\n')
shutil.copyfile(a.saida / 'ambiente_base.txt', a.saida / 'ambiente.txt')
print('ambientes e roteiros preparados em', a.saida)
print('A build do 360 e a rodada são feitas na revisão. Use saves novos em toda abertura.')
