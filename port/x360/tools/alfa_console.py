#!/usr/bin/env python3
"""Prepara controles cruzados do alfa, sem executar build nem tocar no console."""
import argparse
import json
from pathlib import Path
import shutil


def prepara(saida, rodada):
    if not rodada.isascii() or not rodada.isalnum() or not 1<=len(rodada)<=12:
        raise ValueError('identificador ASCII alfanumérico de 1 a 12 caracteres')
    saida.mkdir(parents=True, exist_ok=False)
    raiz=Path(__file__).resolve().parents[3]
    for n in ('boot','f5_duelo'):
        shutil.copyfile(raiz/'port/x360/roteiros'/(n+'.txt'),saida/(n+'.txt'))
    casos=[]
    # A mesma build nos três caminhos, sem misturar marcas com medida de U.
    for i in range(1,4):
        for nome,alfa,c,f in (('base',0,0,0),('alfa',1,0,0),('ambos',0,2,1),('junto',1,2,1)):
            casos.append((f'{nome}{i}',alfa,c,f,0,'0'))
    casos += [(f'cpu{q}_{m}',m,0,0,0,f'{q},{q}') for q in (6930,6960,7170) for m in (0,1)]
    casos += [('oraculo',1,0,0,1,'0')]
    manifesto=[]
    for nome,alfa,c,f,confere,cpu in casos:
        save=f'sv_{rodada}_{nome}'
        if len(save)>42: raise ValueError('save excede FATX')
        linhas=['-BT3_DEMO','BT3_NOMOVIE=1','BT3_DESBLOQUEIO=1',r'BT3_ROTEIRO=game:\f5_duelo.txt',
                'BT3_GS_THREAD=1','BT3_GS_DESENHO=1','BT3_GS_DESENHO_HW=3','BT3_SOM=1','BT3_WIDE=1',
                'BT3_ESCALA=1.33','BT3_FXAA=1','BT3_VU1_DIRETO=1','BT3_ENVIO_RAPIDO=1','BT3_TEX_ORIGEM=1',
                'BT3_TEMPOS=0','BT3_PICOS=36','BT3_PICOS_TELA=1',f'BT3_ALFA_FORA={alfa}',
                f'BT3_ALFA_CONFERE={confere}',f'BT3_VU_CONSTANTES={c}',f'BT3_VU_FECHO={f}',
                f'BT3_CADEIA_CPU={cpu}','BT3_CORTES=0','BT3_CORTES_GPU=0','BT3_COMUM_GPU=0',
                f'BT3_SAVES=game:\\{save}']
        (saida/f'ambiente_{nome}.txt').write_text('\n'.join(linhas)+'\n')
        manifesto.append({'abertura':nome,'save':save,'alfa':alfa,'oraculo':confere,'cpu':cpu})
    (saida/'manifesto.json').write_text(json.dumps(manifesto,indent=2)+'\n')
    (saida/'ordem.txt').write_text('\n'.join(c[0] for c in casos)+'\n')
    shutil.copyfile(saida/'ambiente_base1.txt',saida/'ambiente.txt')
    (saida/'LEIA.txt').write_text('''Rodada de alfa, mesma build. Conferir carimbo e inexistência de TODOS os saves do manifesto antes de abrir.
Se qualquer save existir, gerar outra rodada inteira. Não reutilizar rp32a2810/rp33a2810.
Executar na ordem. Preservar logs originais com o nome da abertura e SHA256.
Comparar base/alfa, ambos/junto na mesma sessão: longas e respectivas associações, J/G/P/U,
folga mínimo/p1/p10/p50, pressão, recursos, resultado, áudio e Guide/saída.
Comparar a build anterior também com este controle desligado, para não esconder a regressão do 7170.
CPU e oráculo ficam DEPOIS dos controles; não descontar seus relógios para certificar ganho em U.
Alfa máximo histórico ~5,4 ms no 6930; nenhum alfa medido no 6960. Medir 7170 e quadros seguintes.
Imagem: comparar intermediários/final byte a byte na mesma arquitetura, ambas as escalas.
Entrada: mesmo vblank/botão/estado/hash/primeiro quadro que reage. Faltam ganchos para essa certificação completa.
Depois: três janelas longas fria/quente e sessão livre, incluindo pressão/duas vistas e áudio.
Prefixos: ensaio apenas no backend falso, sem BT3_GS_LOTES no D3D. Não há abertura de prefixos no console.
Nenhum aceite de desempenho, imagem, entrada, duas vistas ou 720p foi concedido nesta preparação.
''')
    return manifesto


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('saida',type=Path); p.add_argument('--rodada',required=True)
    a=p.parse_args()
    try: m=prepara(a.saida,a.rodada)
    except (ValueError,OSError) as e: p.exit(1,f'FALHA: {e}\n')
    print(f'{len(m)} aberturas, {len({x["save"] for x in m})} saves exclusivos preparados; conferir inexistência no console.')
