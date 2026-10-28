#!/usr/bin/env python3
"""Confere a coleta conjunta contra as rodadas separadas da mesma build, sem emulador."""
import argparse
from pathlib import Path
import sys
import tempfile
import subprocess


def exigir(c, motivo):
    if not c:
        raise ValueError(motivo)


def comparar(a, b):
    total = 0
    def igual(nome):
        nonlocal total
        x, y = a / nome, b / nome
        exigir(x.is_file() and y.is_file(), f'ausente: {nome}')
        exigir(x.read_bytes() == y.read_bytes(), f'diferente: {nome}')
        total += 1
    for nome in ('telas_a.txt', 'telas_b.txt', 'telas_r.txt', 'lidos.txt', 'mem_telas.txt',
                 'imagens.txt', 'som_a.txt', 'som_b.txt', 'som_p.txt', 'som_pa.txt', 'som_xa.txt'):
        igual(nome)
    # O checksum global inclui buffers mortos. A prova de luta é a mesma do semmapa.
    for nome in ('ha.txt', 'hq.txt', 'hm.txt', 'hqa.txt', 'hxa.txt'):
        logs = [(p / nome).read_text().splitlines() for p in (a, b)]
        exigir(all(logs), f'hash vazio: {nome}')
        luta = [[(l.split()[0], l.split()[2:]) for l in linhas if len(l.split()) > 3] for linhas in logs]
        exigir(luta[0] == luta[1], f'luta diferente: {nome}')
        total += 1
    for nome in ('ppcsm.txt', 'gdb.txt'):
        x, y = ((p / nome).read_text(errors='replace').splitlines() for p in (a, b))
        for prefixo in ('roteiro:', 'LEU '):
            exigir([l for l in x if l.startswith(prefixo)] == [l for l in y if l.startswith(prefixo)],
                   f'rastro/leituras diferentes: {nome}, {prefixo}')
        total += 1
    arquivos = set()
    for p in (a, b):
        arquivos.update(str(f.relative_to(p)) for padrao in ('**/*.dump', '**/*.pages', '**/*.pool', 'img/*/*.ppm',
                        's*/card*/**/*') for f in p.glob(padrao) if f.is_file())
    exigir(any(n.endswith('.dump') for n in arquivos), 'sem memórias')
    exigir(any(n.endswith('.ppm') for n in arquivos), 'sem imagens')
    # Os cartões das rodadas suprimidas não existem na coleta conjunta. Os três cartões que
    # provam jogo, gravador e captura são comparados aos seus equivalentes.
    for nome in sorted(arquivos):
        igual(nome)
    for alvo in ('x86', 'ppc'):
        nome = f'img/{alvo}/saida.txt'
        x, y = ((p / nome).read_text(errors='replace').splitlines() for p in (a, b))
        exigir([l for l in x if l.startswith('imagem:')] == [l for l in y if l.startswith('imagem:')],
               f'momento de captura diferente: {alvo}')
    print(f'equivalência: {total} arquivos/provas iguais, memórias e imagens byte a byte')


def mutantes():
    with tempfile.TemporaryDirectory(prefix='equivalencia-', dir=Path.home()) as tmp:
        a, b = (Path(tmp) / n for n in ('separado', 'junto'))
        nomes = ('telas_a.txt telas_b.txt telas_r.txt lidos.txt mem_telas.txt imagens.txt '
                 'som_a.txt som_b.txt som_p.txt som_pa.txt som_xa.txt '
                 'ha.txt hq.txt hm.txt hqa.txt hxa.txt ppcsm.txt gdb.txt '
                 'mem_62/hq.txt.dump img/x86/teste.ppm img/ppc/teste.ppm '
                 'img/x86/saida.txt img/ppc/saida.txt').split()
        for raiz in (a, b):
            for nome in nomes:
                f = raiz / nome
                f.parent.mkdir(parents=True, exist_ok=True)
                f.write_bytes(b'1 0000 0000 hp 10 10\n')
        def cli(rc, motivo):
            r = subprocess.run([sys.executable, __file__, str(a), str(b)], capture_output=True, text=True)
            exigir(r.returncode == rc and motivo in r.stdout, f'controle falhou: {r.stdout}')
        cli(0, 'equivalência:')
        for nome in ('mem_62/hq.txt.dump', 'img/ppc/teste.ppm', 'hq.txt'):
            f = b / nome
            original = f.read_bytes()
            f.write_bytes(original.replace(b'10', b'11'))
            cli(1, 'diferente: ' + nome)
            f.write_bytes(original)
            f.unlink()
            cli(1, 'FALHA:')
            f.write_bytes(original)
            print(f'controle {nome}: alteração e ausência recusadas com saída 1')
        cli(0, 'equivalência:')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('separado', type=Path, nargs='?')
    p.add_argument('junto', type=Path, nargs='?')
    p.add_argument('--mutantes', action='store_true')
    a = p.parse_args()
    try:
        if a.mutantes:
            mutantes()
        else:
            exigir(a.separado is not None and a.junto is not None, 'informe as duas pastas')
            comparar(a.separado, a.junto)
    except (OSError, ValueError) as e:
        print(f'FALHA: {e}')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
