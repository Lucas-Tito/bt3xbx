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
# (09.md, seção 7) As lacunas da rodada a989c9b5, refeitas com destino novo: os casos reprovados mais o que eles
# exigem (builds, objetos e .text para a proveniência, a ISO do leitor, os negativos do conferente alterado) e a
# regravação revisada das três referências. A lista é fixa: uma origem com outra lista é recusada.
REGRAVADAS = ('datacenter', 'f6_final', 'f6_final_start')
LACUNAS = ['build-base', 'build-final', 'objetos', 'text', 'negativos', 'iso', 'leitor-iso', 'filme-testes',
           'filme-build', 'referencias-regravadas'] + ['roteiro-' + n for n in
                                                       ('discfusion', 'datacenter', 'f6_final', 'f6_final_start')] + [
           'limpeza']
# Não refeito: nada mudou nele, e a decisão é do Lucas (09.md, seção 7). Continua reprovando a rodada integral.
PENDENTES = {'filme-referencia-upstream': 'mpeg2test do upstream ausente; pergunta ao Lucas'}


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
    lacunas = 'casos' in meta
    exigir(not lacunas or meta['casos'] == LACUNAS, 'lista de casos da origem diferente da das lacunas')
    esperados = LACUNAS if lacunas else CASOS
    casos = json.loads((pasta / 'casos.json').read_text())
    exigir(len(casos) == len(esperados) and {r['nome'] for r in casos} == set(esperados),
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
    if lacunas:
        # Nunca a frase da aprovação integral: os demais casos são os da rodada original, com outra build.
        print(f'lacunas: {len(casos)} casos refeitos com saída 0; não refeito: '
              + '; '.join(f'{k} ({v})' for k, v in PENDENTES.items())
              + '; a rodada integral continua reprovada por ele')
        return
    for tipo in ('op', 'ed'):
        filmes(pasta / f'filme-{tipo}-x86.log', pasta / f'filme-{tipo}-ppc.log', tipo)
    print('tudo certo no WSL; console continua pendente')


def revisa_referencia(nome, antiga, nova):
    """A regravação de uma referência de telas só vale com a diferença explicada pelo roteiro novo. Data Center: o
    roteiro ampliado (eb968268) coincide com o antigo até a entrada na senha, a menos dos números de linha, e termina
    no fim da linha 165. f6_final(_start): tudo igual, menos o fim, dois quadros depois e uma linha abaixo (quadros 2)."""
    a, b = (p.read_text().splitlines() for p in (antiga, nova))
    exigir(a and b, f'{nome}: referência vazia')
    sem_linha = lambda l: re.sub(r'^(roteiro: \S+?):\d+ ', r'\1 ', l)
    if nome == 'datacenter':
        k = next((i + 1 for i, l in enumerate(a)
                  if re.fullmatch(r'roteiro: datacenter\.txt:\d+ ok tela 54 vblank \d+ quadro \d+', l)), None)
        exigir(k is not None and len(b) > k, 'datacenter: a referência antiga não chega à senha')
        exigir([sem_linha(l) for l in a[:k]] == [sem_linha(l) for l in b[:k]],
               'datacenter: o começo diverge da referência antiga')
        exigir(re.fullmatch(r'roteiro: datacenter\.txt:165 fim vblank \d+ quadro \d+', b[-1]),
               'datacenter: não termina no fim do roteiro ampliado')
        print(f'datacenter: {k} linhas iguais à antiga até a senha; {len(b)} linhas, fim na 165')
    elif nome in ('f6_final', 'f6_final_start'):
        exigir(a[:-1] == b[:-1], f'{nome}: rastro diferente da referência antiga antes do fim')
        m = re.fullmatch(r'roteiro: f6_final\.txt:19 fim vblank (\d+) quadro (\d+)', a[-1])
        n = re.fullmatch(r'roteiro: f6_final\.txt:20 fim vblank (\d+) quadro (\d+)', b[-1])
        exigir(m and n and int(n[2]) == int(m[2]) + 2 and int(n[1]) >= int(m[1]) + 2,
               f'{nome}: o fim não é o antigo dois quadros depois')
        print(f'{nome}: {len(a) - 1} linhas iguais; fim do quadro {m[2]} ao {n[2]}')
    else:
        raise ValueError(f'{nome}: sem revisão definida')


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
        # A rodada das lacunas: a lista é fixa, e mesmo inteira e em zero nunca dá a frase da aprovação integral.
        lac = p / 'lacunas'; lac.mkdir()
        (lac / 'origem.json').write_text(json.dumps({'commit': 'a' * 40, 'curto': 'aaaaaaa', 'casos': LACUNAS}))
        linhas = []
        for nome in LACUNAS:
            (lac / (nome + '.log')).write_text('caso sintético\n')
            linhas.append(dict(nome=nome, rc=0, segundos=1,
                               sha256=hashlib.sha256((lac / (nome + '.log')).read_bytes()).hexdigest()))
        (lac / 'casos.json').write_text(json.dumps(linhas))
        r = subprocess.run([sys.executable, __file__, 'rodada', str(lac)], capture_output=True, text=True)
        exigir(r.returncode == 1 and 'FALHA: ' in r.stdout and 'tudo certo' not in r.stdout,
               f'lacunas sem builds: {r.returncode}: {r.stdout}')
        (lac / 'casos.json').write_text(json.dumps(linhas[:-1]))
        cli(['rodada', lac], 1, 'casos ausentes')
        (lac / 'casos.json').write_text(json.dumps(linhas))
        (lac / 'origem.json').write_text(json.dumps({'commit': 'a' * 40, 'curto': 'aaaaaaa',
                                                     'casos': LACUNAS[:-1]}))
        cli(['rodada', lac], 1, 'lista de casos da origem diferente')
        print('controles das lacunas (sem builds, caso ausente, lista trocada): saída 1 confirmada')
        # A revisão das referências regravadas, sobre rastros sintéticos (só texto de telas, nenhum dado do jogo).
        ant, nov = p / 'antiga.telas', p / 'nova.telas'
        comum = 'tela 1 vblank 10 quadro 9\nroteiro: boot.txt:3 ok tela 1 vblank 10 quadro 9\n'
        ant.write_text(comum + 'roteiro: f6_final.txt:19 fim vblank 100 quadro 90\n')
        nov.write_text(comum + 'roteiro: f6_final.txt:20 fim vblank 102 quadro 92\n')
        cli(['revisa-referencia', 'f6_final', ant, nov], 0, 'fim do quadro 90 ao 92')
        for estrago, motivo in (
                (comum + 'roteiro: f6_final.txt:20 fim vblank 103 quadro 93\n', 'dois quadros depois'),
                (comum.replace('vblank 10 ', 'vblank 11 ', 1) + 'roteiro: f6_final.txt:20 fim vblank 102 quadro 92\n',
                 'antes do fim')):
            nov.write_text(estrago)
            cli(['revisa-referencia', 'f6_final', ant, nov], 1, motivo)
        dc = 'roteiro: datacenter.txt:{} ok tela 54 vblank 30 quadro 29\n'
        ant.write_text(comum + dc.format(12) + 'roteiro: datacenter.txt:41 fim vblank 50 quadro 49\n')
        nov.write_text(comum + dc.format(14) + 'marca senha\nroteiro: datacenter.txt:165 fim vblank 90 quadro 89\n')
        cli(['revisa-referencia', 'datacenter', ant, nov], 0, 'até a senha')
        nov.write_text(comum + dc.format(14).replace('vblank 30', 'vblank 31')
                       + 'roteiro: datacenter.txt:165 fim vblank 90 quadro 89\n')
        cli(['revisa-referencia', 'datacenter', ant, nov], 1, 'começo diverge')
        nov.write_text(comum + dc.format(14) + 'roteiro: datacenter.txt:41 fim vblank 50 quadro 49\n')
        cli(['revisa-referencia', 'datacenter', ant, nov], 1, 'roteiro ampliado')
        print('controles da revisão das referências (fim fora, começo diverge, roteiro antigo): saída 1 confirmada')
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
    p.add_argument('modo', choices=['rodada', 'filmes', 'objetos', 'save-mc', 'text', 'negativos',
                                    'revisa-referencia', 'lacunas'])
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
        elif a.modo == 'revisa-referencia':
            revisa_referencia(a.args[0], Path(a.args[1]), Path(a.args[2]))
        elif a.modo == 'lacunas':
            print('\n'.join(LACUNAS))
        else:
            negativos()
    except (OSError, ValueError, KeyError, IndexError, subprocess.CalledProcessError) as e:
        print(f'FALHA: {e}')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
