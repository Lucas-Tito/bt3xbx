#!/usr/bin/env python3
"""Julga cada caso da F6, hashes completos dos filmes e controles em cópias."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from f5_final_confere import objetos, save_mc
from f6_final_arquivo import atomico, compactar, conferir

ROTEIROS = ('f5_duelo f5_treino duelo duelo_time treino opcoes historia historia_novo '
            'historia_perde ultimate missao sobrevivencia simdragon discfusion torneio '
            'evolution datacenter referencia shenlong f6_final f6_final_start').split()
FIXOS = ['build-base', 'build-final', 'objetos', 'text', 'inventario', 'cores', 'pacotes64',
         'disco', 'iso', 'leitor-iso', 'som-ambiente', 'som360', 'som-amostras', 'som-fluxo',
         'som-controles', 'filme-testes', 'filme-blocos', 'filme-build', 'filme-op-x86',
         'filme-op-ppc', 'filme-op-compara', 'filme-ed-x86', 'filme-ed-ppc', 'filme-ed-compara',
         'filme-referencia-upstream', 'deriva', 'save-mc', 'save-mc-julgamento', 'save-entre',
         'soma', 'limpeza', 'negativos', 'negativos-retomada', 'filme-testes-bash']
CASOS = FIXOS + ['roteiro-' + n for n in ROTEIROS]
TEXT = '43444ea4d03a6d448aa1ae74a631e35b356a7caa'


def exigir(c, m):
    if not c:
        raise ValueError(m)


def filmes(a, b, tipo):
    total = 2810 if tipo == 'op' else 4200
    def ler(p):
        linhas = p.read_text().splitlines()
        exigir(len(linhas) == total + 1 and linhas[-1] == f'FIM {tipo} {total}',
               f'{p}: filme incompleto')
        for i, l in enumerate(linhas[:-1]):
            exigir(re.fullmatch(rf'{tipo} {i} [0-9a-f]{{16}}', l), f'{p}: índice/hash inválido {i}')
        return linhas
    exigir(ler(a) == ler(b), f'{tipo}: RGBA diferente entre x86 e PPC')
    print(f'{tipo}: {total} hashes RGBA iguais, índices completos')


def text(pasta):
    for nome in ('base', 'final'):
        p = pasta / f'text-{nome}.bin'
        exigir(hashlib.sha1(p.read_bytes()).hexdigest() == TEXT, f'.text {nome} diferente')
    print(f'.text: {TEXT}, base e final iguais')


def julgar(pasta):
    meta = json.loads((pasta / 'origem.json').read_text())
    exigir(re.fullmatch('[0-9a-f]{40}', meta['commit']), 'commit inválido')
    casos = json.loads((pasta / 'casos.json').read_text())
    exigir(len(casos) == len(CASOS) and {r['nome'] for r in casos} == set(CASOS),
           'casos ausentes, duplicados ou inesperados')
    falhas = []
    for r in casos:
        log = pasta / (r['nome'] + '.log')
        exigir(isinstance(r['rc'], int) and isinstance(r['segundos'], (int, float)),
               'resultado sem código ou duração')
        exigir(log.is_file() and log.stat().st_size > 0, f'log ausente/vazio: {r["nome"]}')
        exigir(hashlib.sha256(log.read_bytes()).hexdigest() == r['sha256'],
               f'log alterado: {r["nome"]}')
        # Resultado ausente nunca passa; saída 0 do shell não substitui o julgamento específico.
        print(f'| {r["nome"]} | {r["rc"]} | {r["segundos"]:.1f} s | {log.name} |')
        if r['rc'] != 0:
            falhas.append(r['nome'])
        if r['segundos'] < 0:
            falhas.append(r['nome'] + ' (duração inválida, relógio civil recuou)')
    exigir(not falhas, 'casos reprovados: ' + ', '.join(falhas))
    for alvo in ('x86', 'x86r', 'ppc', 'x86a', 'ppca'):
        c = (pasta / 'final/port/build' / alvo / 'CARIMBO').read_text().strip()
        exigir(c == f'{meta["curto"]} diff:da39a3ee5e', f'carimbo diferente: {alvo}')
    text(pasta)
    for tipo in ('op', 'ed'):
        filmes(pasta / f'filme-{tipo}-x86.log', pasta / f'filme-{tipo}-ppc.log', tipo)
    print('tudo certo no WSL; console continua pendente')


def negativos():
    """Executa a CLI sobre cópias; toda recusa exige saída 1 e motivo específico."""
    with tempfile.TemporaryDirectory(prefix='f6-conferente-', dir=Path.home()) as d:
        p = Path(d)
        def cli(args, rc, motivo):
            r = subprocess.run([sys.executable, __file__, *map(str, args)], capture_output=True, text=True)
            exigir(r.returncode == rc and motivo in r.stdout, f'controle {args}: {r.returncode}: {r.stdout}')
        a, b = p / 'a', p / 'b'
        bom = ''.join(f'op {i} {i:016x}\n' for i in range(2810)) + 'FIM op 2810\n'
        a.write_text(bom)
        b.write_text(bom)
        cli(['filmes', a, b, 'op'], 0, '2810 hashes')
        for nome, estrago, motivo in (
            ('pixel', bom.replace('op 10 000000000000000a', 'op 10 000000000000000b'), 'RGBA diferente'),
            ('corte', bom[:100], 'incompleto'),
            ('índice', bom.replace('op 10 ', 'op 11 '), 'índice/hash inválido'),
        ):
            b.write_text(estrago)
            cli(['filmes', a, b, 'op'], 1, motivo)
            print(f'controle {nome}: saída 1 confirmada')
        (p / 'origem.json').write_text(json.dumps({'commit': 'a' * 40, 'curto': 'aaaaaaa'}))
        casos = []
        for nome in CASOS:
            log = p / (nome + '.log')
            log.write_text('caso sintético\n')
            casos.append(dict(nome=nome, rc=0, segundos=1, sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
        def grava():
            (p / 'casos.json').write_text(json.dumps(casos))
        # O conjunto sem builds não pode ser aprovado mesmo com todos os códigos em zero.
        grava()
        cli(['rodada', p], 1, 'FALHA:')
        casos[0]['rc'] = 1
        grava()
        cli(['rodada', p], 1, 'casos reprovados: build-base')
        print('controle código 1: saída 1 confirmada')
        casos[0]['rc'] = 0
        removido = casos.pop()
        grava()
        cli(['rodada', p], 1, 'casos ausentes')
        casos.append(removido)
        grava()
        (p / 'build-base.log').write_text('estragado\n')
        cli(['rodada', p], 1, 'log alterado')
        (p / 'build-base.log').unlink()
        cli(['rodada', p], 1, 'log ausente/vazio')
        print('controles caso ausente, log alterado e log ausente: saída 1 confirmada')
        # O conferente de objetos herdado também é exercitado sobre objetos reais sintéticos.
        for n in ('base', 'final'):
            (p / n / 'obj').mkdir(parents=True)
        fonte = p / 'obj.c'
        fonte.write_text('int valor(void) { return 1; }\n')
        subprocess.run(['gcc', '-g', '-c', str(fonte), '-o', str(p / 'base/obj/jogo.o')], check=True)
        (p / 'final/obj/jogo.o').write_bytes((p / 'base/obj/jogo.o').read_bytes())
        cli(['objetos', p / 'base', p / 'final'], 0, '1 iguais')
        fonte.write_text('int valor(void) { return 2; }\n')
        subprocess.run(['gcc', '-g', '-c', str(fonte), '-o', str(p / 'final/obj/jogo.o')], check=True)
        cli(['objetos', p / 'base', p / 'final'], 1, 'objeto fora dos testes diferente')
        print('controle objeto do jogo alterado: saída 1 confirmada')
        # Cópias sintéticas para a retomada e o descarte, sem qualquer dado do jogo.
        q = p / 'retomada'; q.mkdir()
        atomico(q / 'origem.json', dict(commit='a' * 40, base='b' * 40))
        (q / 'teste.log').write_text('resultado\n')
        linha = dict(nome='teste', rc=1, segundos=2,
                     sha256=hashlib.sha256((q / 'teste.log').read_bytes()).hexdigest())
        atomico(q / 'casos.json', [linha])
        conferir(q, 'a' * 40, 'b' * 40)
        arquivo = Path(__file__).with_name('f6_final_arquivo.py')
        def recusa(motivo):
            r = subprocess.run([sys.executable, str(arquivo), 'conferir', str(q),
                                'a' * 40, 'b' * 40], capture_output=True, text=True)
            exigir(r.returncode == 1 and motivo in r.stdout, 'retomada aceitou ' + motivo)
            print('controle retomada ' + motivo + ': saída 1 confirmada')
        atomico(q / 'origem.json', dict(commit='c' * 40, base='b' * 40))
        recusa('origem diferente')
        atomico(q / 'origem.json', dict(commit='a' * 40, base='b' * 40))
        atomico(q / 'casos.json', [linha, linha]); recusa('duplicado')
        atomico(q / 'casos.json', [linha])
        (q / 'teste.log').write_text('estrago\n'); recusa('log alterado')
        (q / 'teste.log').unlink(); recusa('log alterado')
        d = q / 'roteiros/teste'; d.mkdir(parents=True)
        (d / 'gdb.txt').write_text('prova\n')
        (d / 'som_a.txt').write_text('controle posterior\n')
        (d / 'rastreio.tsv').write_text('intermediário\n')
        externo = p / 'somente-leitura'; externo.mkdir()
        (externo / 'intocavel.bin').write_bytes(b'abc')
        (d / 'dados').symlink_to(externo, target_is_directory=True)
        compactar(q, 'roteiro-teste')
        exigir((d / 'gdb.txt').read_text() == 'prova\n' and (d / 'som_a.txt').exists()
               and not (d / 'rastreio.tsv').exists() and not (d / 'dados').exists()
               and (externo / 'intocavel.bin').read_bytes() == b'abc', 'retenção incorreta')
        compactar(q, 'roteiro-teste')
        print('retenção: relatórios preservados, intermediários apagados, link não seguido, idempotente')
    print('negativos: tudo certo')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('modo', choices=['rodada', 'filmes', 'objetos', 'save-mc', 'text', 'negativos'])
    p.add_argument('args', nargs='*')
    a = p.parse_args()
    try:
        if a.modo == 'rodada':
            julgar(Path(a.args[0]))
        elif a.modo == 'filmes':
            filmes(Path(a.args[0]), Path(a.args[1]), a.args[2])
        elif a.modo == 'objetos':
            objetos(*map(Path, a.args))
        elif a.modo == 'save-mc':
            save_mc(Path(a.args[0]))
        elif a.modo == 'text':
            text(Path(a.args[0]))
        else:
            negativos()
    except (OSError, ValueError, KeyError, IndexError, subprocess.CalledProcessError) as e:
        print(f'FALHA: {e}')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
