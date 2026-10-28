#!/usr/bin/env python3
"""Prepara a próxima rodada comum da GPU. Sem build, FTP ou acesso ao console."""
import argparse
from pathlib import Path
import shutil

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('saida', type=Path)
p.add_argument('--rodada', required=True, help='identificador novo, alfanumérico, até 12 caracteres')
a = p.parse_args()
if not a.rodada.isalnum() or not a.rodada.isascii() or len(a.rodada) > 12:
    p.error('use identificador alfanumérico ASCII de até 12 caracteres')
# Recusar diretório preexistente evita sobrescrever a ordem de uma rodada preparada.
a.saida.mkdir(parents=True, exist_ok=False)
raiz = Path(__file__).resolve().parents[3]
for nome in ('boot', 'f5_duelo'):
    shutil.copyfile(raiz/'port/x360/roteiros'/(nome+'.txt'), a.saida/(nome+'.txt'))
ordem = []
for i in range(1, 4):
    for nome, constantes, fecho in [('base', 0, 0), ('ambos', 2, 1)]:
        ordem.append((f'{nome}{i}', constantes, fecho, '0'))
for i in range(1, 3):
    for q in (6930, 6960):
        ordem.append((f'g{q}_{i}', 0, 0, f'{q},{q}'))
for nome, constantes, fecho, gpu in ordem:
    save = f'sv_{a.rodada}_{nome}'
    assert len(save) <= 42
    linhas = ['-BT3_DEMO', 'BT3_NOMOVIE=1', 'BT3_DESBLOQUEIO=1', r'BT3_ROTEIRO=game:\f5_duelo.txt',
              'BT3_GS_THREAD=1', 'BT3_GS_DESENHO=1', 'BT3_SOM=1', 'BT3_WIDE=1',
              'BT3_ESCALA=1.33', 'BT3_FXAA=1', 'BT3_VU1_DIRETO=1', 'BT3_ENVIO_RAPIDO=1',
              'BT3_TEMPOS=0', 'BT3_TEX_ORIGEM=1', 'BT3_PICOS=36', 'BT3_PICOS_TELA=1',
              f'BT3_VU_CONSTANTES={constantes}', f'BT3_VU_FECHO={fecho}',
              'BT3_CORTES=0', 'BT3_CORTES_GPU=0', f'BT3_COMUM_GPU={gpu}', f'BT3_SAVES=game:\\{save}']
    (a.saida/f'ambiente_{nome}.txt').write_text('\n'.join(linhas)+'\n')
shutil.copyfile(a.saida/'ambiente_base1.txt', a.saida/'ambiente.txt')
(a.saida/'ordem.txt').write_text('\n'.join(x[0] for x in ordem)+'\n')
print('10 ambientes preparados:', a.saida)
print('Seis controles sem marcas, depois quatro diagnósticos, um quadro por abertura.')
print('Na repetição, usar outro identificador de rodada e saves novos.')
