#!/usr/bin/env python3
"""Prepara gf29b e diagnósticos de CPU separados, sem build nem console."""
import argparse
from pathlib import Path
import shutil


def prepara(saida, rodada):
    if not rodada.isascii() or not rodada.isalnum() or not 1 <= len(rodada) <= 12:
        raise ValueError('use identificador alfanumérico ASCII de até 12 caracteres')
    saida.mkdir(parents=True, exist_ok=False)
    raiz = Path(__file__).resolve().parents[3]
    for nome in ('boot', 'f5_duelo'):
        shutil.copyfile(raiz/'port/x360/roteiros'/(nome+'.txt'), saida/(nome+'.txt'))
    casos = [(f'{n}{i}', c, f, '0', '0', '0') for i in range(1, 4)
             for n, c, f in [('base', 0, 0), ('ambos', 2, 1)]]
    # Prioridade: os dois alvos, depois vizinhos. Nunca CPU e GPU finas juntas.
    quadros = (6930, 6960, 6929, 6931, 6959, 6961)
    casos += [(f'cpu{q}_{i}', 0, 0, f'{q},{q}', '0', '0')
              for q in quadros for i in (1, 2)]
    casos += [(f'grav{q}_{n}', c, f, '0', f'{q},{q}', '0')
              for q in (6930, 6960) for n, c, f in [('b', 0, 0), ('a', 2, 1)]]
    casos += [(f'q{q}_{i}', 0, 0, '0', '0', f'{q},{q}')
              for i in (1, 2) for q in (6929, 6930, 6931, 6959, 6960, 6961)]
    for nome, constantes, fecho, cpu, grav, gpu in casos:
        save = f'sv_{rodada}_{nome}'
        if len(save) > 42:
            raise ValueError('nome de save excede FATX')
        linhas = ['-BT3_DEMO', 'BT3_NOMOVIE=1', 'BT3_DESBLOQUEIO=1', r'BT3_ROTEIRO=game:\f5_duelo.txt',
                  'BT3_GS_THREAD=1', 'BT3_GS_DESENHO=1', 'BT3_GS_DESENHO_HW=3',
                  'BT3_SOM=1', 'BT3_WIDE=1', 'BT3_ESCALA=1.33', 'BT3_FXAA=1',
                  'BT3_VU1_DIRETO=1', 'BT3_ENVIO_RAPIDO=1', 'BT3_TEMPOS=0', 'BT3_TEX_ORIGEM=1',
                  'BT3_PICOS=36', 'BT3_PICOS_TELA=1', f'BT3_VU_CONSTANTES={constantes}',
                  f'BT3_VU_FECHO={fecho}', f'BT3_CADEIA_CPU={cpu}', f'BT3_CORTES={grav}',
                  'BT3_CORTES_GPU=0', f'BT3_COMUM_GPU={gpu}', f'BT3_SAVES=game:\\{save}']
        (saida/f'ambiente_{nome}.txt').write_text('\n'.join(linhas)+'\n')
    shutil.copyfile(saida/'ambiente_base1.txt', saida/'ambiente.txt')
    (saida/'ordem.txt').write_text('\n'.join(c[0] for c in casos)+'\n')
    return len(casos)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('saida', type=Path)
    p.add_argument('--rodada', required=True)
    a = p.parse_args()
    try:
        n = prepara(a.saida, a.rodada)
    except (ValueError, OSError) as e:
        p.exit(1, f'FALHA: {e}\n')
    print(f'{n} ambientes preparados; conferir saves inexistentes no console antes de usar.')
