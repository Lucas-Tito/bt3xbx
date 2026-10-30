#!/usr/bin/env python3
"""Prepara a rodada da segunda parte dos lotes, sem acessar o console.
Controles da mesma build isolam as duas partes. Todos os saves são novos.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import tempfile
from lotes_console import COMUM, QUADROS_IMAGEM, ANTIGAS

RAIZ = Path(__file__).resolve().parents[3]
RELOGIO = '6900,6929,6930,6960,7000,7170'


def casos(modo, repeticoes):
    if modo == 'longas':
        return [(n, 'lotes-alfa', 1, 1) for n in ('frio', 'quente1', 'quente2')]
    return [(f'{n}{a}{i}', ('perfil-' if modo == 'tempo' else '') + b + ('-alfa' if a else ''), l, e)
            for i in range(1, repeticoes + 1) for a in ('', 'a')
            for n, b, l, e in (('nor', 'normal', 0, 0), ('ctl', 'lotes', 0, 0),
                                ('pln', 'lotes', 1, 0), ('lot', 'lotes', 1, 1))]


def prepara(p, rodada, modo, repeticoes=2):
    if not re.fullmatch(r'[a-zA-Z0-9]{1,12}', rodada) or rodada in (*ANTIGAS, 'rl37a2810', 'li37a2810'):
        raise ValueError('identificador inválido ou já usado')
    if repeticoes not in (1, 2):
        raise ValueError('uma ou duas repetições')
    p.mkdir(parents=True, exist_ok=False)
    for n in ('boot', 'f5_duelo', 'f5_treino'):
        shutil.copyfile(RAIZ / 'port/x360/roteiros' / (n + '.txt'), p / (n + '.txt'))
    man = []
    for n, b, l, e in casos(modo, repeticoes):
        env = dict(x.split('=', 1) for x in COMUM if '=' in x)
        env.update(BT3_LOTES=str(l), BT3_LOTES_ENVIO=str(e), BT3_RELOGIO_MARCAS='0', BT3_RELOGIO_GRAV='0')
        save = 'sv_' + rodada + '_' + n
        env['BT3_SAVES'] = 'game:\\' + save
        if modo == 'tempo':
            env['BT3_RELOGIO_GRAV'] = RELOGIO
        elif modo == 'imagem':
            env.update(BT3_SHOT_LISTA=QUADROS_IMAGEM, BT3_SHOT_CHEIA='1', BT3_SHOT_PASTA='shots_' + rodada + '_' + n)
        else:
            env['BT3_ROTEIRO'] = r'game:\f5_treino.txt'
        (p / f'ambiente_{n}.txt').write_text('-BT3_DEMO\n' + '\n'.join(f'{k}={v}' for k, v in env.items()) + '\n')
        man.append(dict(abertura=n, build=b, save=save, modo=modo, lotes=l, envio=e, ambiente=env))
    (p / 'manifesto.json').write_text(json.dumps(man, indent=2) + '\n')
    (p / 'ordem.txt').write_text(''.join(f'{m["abertura"]} {m["build"]}\n' for m in man))
    shutil.copyfile(p / f'ambiente_{man[0]["abertura"]}.txt', p / 'ambiente.txt')
    (p / 'LEIA.txt').write_text(f'''Rodada {rodada}, F5 seção 40: {modo}, {len(man)} aberturas.
Builds do mesmo commit, com carimbo e manifesto de hashes antes de enviar. Build normal sem chaves;
lotes com BT3_GS_LOTES=1; sufixo alfa com BT3_ALFA_ORIGEM=1; prefixo perfil com BT3_PERFIL_CADEIA=1.
Nunca reutilizar save/pasta já existente: conferir TODOS os nomes do manifesto antes da primeira abertura.
Abrir na ordem.txt, copiando ambiente_NOME.txt para ambiente.txt e o xex da build indicada.
ctl: ambas as partes desligadas; pln: só plano/DATE da seção 38; lot: envio durante a gravação.
Sem VU1 direto, EDRAM residente ou plano anterior válido não há envio antecipado. Conferir linhas lotes dos quadros.
Tempo: nenhuma captura; BT3_RELOGIO_MARCAS=0. prefixos_teto.py --console LOG --so-relogio deve aceitar cada log.
Comparar G/P/U e prazo de 6929/6930/6960/7170 nas duas repetições, contra ctl e pln da mesma build e nor.
Não somar ao ganho antigo do alfa. O teto C anterior não prevê o custo das faixas nem a barreira DATE nova.
Imagem: trazer diário e cada pasta shots SOMENTE ao WSL. Comparar todos os 22 quadros, pixels e dimensões exatos,
contra nor do mesmo alfa/repetição e contra ctl/pln. compara_quadros.py sozinho não reprova pixel diferente.
Exigir fora da vaga=0, alvo lido sem Resolve=0, recursos/descartes íntegros, cercas VU sem liberação forçada.
Longas opcionais: frio após reiniciar console, quente1/2 sem reiniciar; três janelas >=2700 passos, sem captura,
Guide ou pausa dentro delas. passos.py --console e tela por imagem, zero longas no combate continua sendo o aceite.
Guide: verificar também gravação parada entre faixas, retomada e saída. Só o fecho apresenta e sinaliza a cerca VU.
''')
    confere(p, rodada, modo, repeticoes)
    return man


def confere(p, rodada, modo, repeticoes):
    man = json.loads((p / 'manifesto.json').read_text())
    esperado = casos(modo, repeticoes)
    if [(m['abertura'], m['build'], m['lotes'], m['envio']) for m in man] != esperado:
        raise ValueError('matriz/build/controles incompletos')
    saves, shots = set(), set()
    for m in man:
        n, env = m['abertura'], m['ambiente']
        save = 'sv_' + rodada + '_' + n
        if m['save'] != save or save in saves or len(save) > 42:
            raise ValueError('save repetido, estranho ou longo')
        saves.add(save)
        linhas = (p / f'ambiente_{n}.txt').read_text().splitlines()
        real = dict(x.split('=', 1) for x in linhas if '=' in x)
        if len(linhas) != len(real) + 1 or linhas[0] != '-BT3_DEMO' or real != env:
            raise ValueError('ambiente adulterado ou chave repetida')
        comum = dict(x.split('=', 1) for x in COMUM if '=' in x)
        if modo == 'longas': comum['BT3_ROTEIRO'] = r'game:\f5_treino.txt'
        if any(real.get(k) != v for k, v in comum.items()):
            raise ValueError('ambiente comum diferente')
        if real['BT3_SAVES'] != 'game:\\' + save or real['BT3_LOTES'] != str(m['lotes']) or real['BT3_LOTES_ENVIO'] != str(m['envio']):
            raise ValueError('controle/save diferente')
        if real['BT3_RELOGIO_MARCAS'] != '0' or real['BT3_RELOGIO_GRAV'] != (RELOGIO if modo == 'tempo' else '0'):
            raise ValueError('relógio incorreto')
        if modo == 'imagem':
            shot = 'shots_' + rodada + '_' + n
            if real.get('BT3_SHOT_PASTA') != shot or shot in shots or len(shot) > 42 or real.get('BT3_SHOT_LISTA') != QUADROS_IMAGEM or real.get('BT3_SHOT_CHEIA') != '1':
                raise ValueError('captura incorreta')
            shots.add(shot)
        elif any(k.startswith('BT3_SHOT') for k in real):
            raise ValueError('captura fora da rodada de imagem')
    if (p / 'ordem.txt').read_text() != ''.join(f'{m["abertura"]} {m["build"]}\n' for m in man):
        raise ValueError('ordem diferente')
    if (p / 'ambiente.txt').read_bytes() != (p / f'ambiente_{man[0]["abertura"]}.txt').read_bytes():
        raise ValueError('primeiro ambiente diferente')


def mutantes():
    with tempfile.TemporaryDirectory() as t:
        for modo in ('tempo', 'imagem', 'longas'):
            p = Path(t) / modo
            prepara(p, 'teste40', modo)
            m = json.loads((p / 'manifesto.json').read_text())[0]
            f = p / f'ambiente_{m["abertura"]}.txt'
            original = f.read_text()
            for a, b in (('BT3_LOTES_ENVIO=', 'BT3_LOTES_ENVIO=9'), ('sv_teste40_', 'sv_antigo_'),
                         ('BT3_VU1_DIRETO=1', 'BT3_VU1_DIRETO=0'), ('BT3_CADEIA_CPU=0', 'BT3_CADEIA_CPU=1')):
                assert a in original
                f.write_text(original.replace(a, b, 1))
                # Adulterar também o manifesto: a regra semântica precisa recusar,
                # além da igualdade superficial do arquivo com seu manifesto.
                adulterado = json.loads((p / 'manifesto.json').read_text())
                adulterado[0]['ambiente'] = dict(x.split('=', 1) for x in f.read_text().splitlines() if '=' in x)
                (p / 'manifesto.json').write_text(json.dumps(adulterado))
                try: confere(p, 'teste40', modo, 2)
                except ValueError as e: print('mutante recusado:', modo, a, e)
                else: raise AssertionError('mutante aceito')
                adulterado[0] = m
                (p / 'manifesto.json').write_text(json.dumps(adulterado))
            f.write_text(original)
            confere(p, 'teste40', modo, 2)
    print('tudo certo nos doze mutantes dos ambientes')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('modo', nargs='?', choices=('tempo', 'imagem', 'longas'))
    p.add_argument('saida', type=Path, nargs='?'); p.add_argument('--rodada')
    p.add_argument('--mutantes', action='store_true')
    a = p.parse_args()
    if a.mutantes: mutantes()
    else:
        if not (a.modo and a.saida and a.rodada): p.error('modo SAIDA --rodada obrigatórios')
        print(len(prepara(a.saida, a.rodada, a.modo)), 'aberturas preparadas; saves novos, conferir inexistência no console')
