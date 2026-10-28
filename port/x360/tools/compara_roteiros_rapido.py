#!/usr/bin/env python3
"""Confere a coleta conjunta contra as rodadas separadas da mesma build, sem emulador."""
import argparse
import ctypes
import ctypes.util
import re
from pathlib import Path
import sys
import tempfile
import subprocess


def exigir(c, motivo):
    if not c:
        raise ValueError(motivo)


# Os campos abaixo já estão em DIFERENTES_CONHECIDAS de roteiros.py. Aqui a
# comparação é da mesma arquitetura: só caminhos do host e dois locais da pilha
# podem variar. O restante, inclusive números de sMc/PortAdxView, é byte a byte.
SIMBOLOS = {}
XXH = ctypes.CDLL(ctypes.util.find_library('xxhash') or 'libxxhash.so.0')
XXH.XXH3_64bits.argtypes = (ctypes.c_void_p, ctypes.c_size_t)
XXH.XXH3_64bits.restype = ctypes.c_uint64


def xxh(dados):
    return XXH.XXH3_64bits(dados, len(dados))


def simbolos(alvo):
    if alvo not in SIMBOLOS:
        prog = Path('port/build') / alvo / f'bt3_{alvo}'
        r = subprocess.run(['nm', '-n', '-S', str(prog)], capture_output=True, text=True, check=True)
        linhas = [l.split() for l in r.stdout.splitlines()]
        SIMBOLOS[alvo] = {q[-1]: (int(q[0], 16), int(q[1], 16)) for q in linhas if len(q) == 4}
        for q in linhas:
            if len(q) == 3 and q[-1] in ('gColWalkBox', 'gColWalkCtx'):
                SIMBOLOS[alvo][q[-1]] = (int(q[0], 16), 4)
    return SIMBOLOS[alvo]


def caminho(dados, raiz, cartao):
    texto = dados.split(b'\0', 1)[0]
    # Só a raiz da rodada pode mudar; o nome lógico do arquivo continua obrigatório.
    prefixo = str(raiz).encode() + b'/'
    if texto.startswith(prefixo):
        texto = texto[len(prefixo):]
        if cartao:
            partes = texto.split(b'/card1/', 1)
            exigir(len(partes) == 2 and re.fullmatch(rb'(?:mem_[^/]+/)?s[mq]', partes[0]),
                   'caminho de cartão inesperado')
            texto = b'cartao/card1/' + partes[1]
    exigir(len(texto) < len(dados), 'caminho sem terminador')
    return texto.ljust(len(dados), b'\0')


def memoria(p, raiz):
    bruto = p.read_bytes()
    regioes, paginas = [], []
    o = 0
    alvo = 'ppc' if p.name == 'hq.txt.dump' else 'x86'
    while o < len(bruto):
        fim = bruto.index(b'\n', o)
        nome, addr, tam = bruto[o:fim].split()
        addr, tam = int(addr, 16), int(tam, 16)
        exigir(tam > 0 and fim + 1 + tam <= len(bruto), f'dump truncado: {p.name}')
        dados = bytearray(bruto[fim + 1:fim + 1 + tam])
        o = fim + 1 + tam
        for off in range(0, tam, 4096):
            paginas.append(f'{nome.decode()} {addr + off:x} {xxh(bytes(dados[off:off + 4096])):016x}')
        if nome == b'globals':
            syms = simbolos(alvo)
            for s in ('gColWalkBox', 'gColWalkCtx'):
                a, n = syms[s]
                if addr <= a and a + n <= addr + tam:
                    exigir(n == 4, f'ponteiro do host com tamanho inesperado: {s}')
                    dados[a - addr:a - addr + n] = bytes(n)
            for s, passo, desloc, n in (('gPortMoviePath', 0, 0, 0), ('gPortAdxView', 0x118, 0x18, 256),
                                      ('sMc', 0x264, 0xC, 600)):
                a, tamanho = syms[s]
                if not (addr <= a and a + tamanho <= addr + tam):
                    continue
                for k in range(0, tamanho, passo or tamanho):
                    ini = a - addr + k + desloc
                    fimcampo = ini + (n or tamanho)
                    dados[ini:fimcampo] = caminho(bytes(dados[ini:fimcampo]), raiz, s == 'sMc')
        regioes.append((nome, addr, bytes(dados)))
    exigir(regioes, f'dump vazio: {p.name}')
    arquivo_paginas = Path(str(p).removesuffix('.dump') + '.pages')
    exigir(arquivo_paginas.read_text().splitlines() == paginas, f'páginas incoerentes: {p.name}')
    return regioes


def comparar(a, b):
    total = 0
    def igual(nome):
        nonlocal total
        x, y = a / nome, b / nome
        exigir(x.is_file() and y.is_file(), f'ausente: {nome}')
        if nome.endswith('.dump'):
            exigir(memoria(x, a) == memoria(y, b), f'diferente: {nome}')
        elif not nome.endswith('.pages'):
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
    print(f'equivalência: {total} arquivos/provas iguais; imagens byte a byte; memórias byte a byte salvo\n'
          'caminhos do host e gColWalkBox/gColWalkCtx já explicados no critério; páginas íntegras')


def mutantes():
    for cartao, x, y in ((True, b'/a/mem_62/sq/card1/save', b'/b/sq/card1/save'),
                         (False, b'/a/dados/disc/som', b'/b/dados/disc/som')):
        x, y = x.ljust(600, b'\0'), y.ljust(600, b'\0')
        exigir(caminho(x, Path('/a'), cartao) == caminho(y, Path('/b'), cartao), 'normalização de raiz')
        exigir(caminho(x, Path('/a'), cartao) != caminho(y.replace(b'save', b'outro').replace(b'som', b'voz'), Path('/b'), cartao),
               'normalização escondeu outro arquivo lógico')
    with tempfile.TemporaryDirectory(prefix='equivalencia-', dir=Path.home()) as tmp:
        a, b = (Path(tmp) / n for n in ('separado', 'junto'))
        nomes = ('telas_a.txt telas_b.txt telas_r.txt lidos.txt mem_telas.txt imagens.txt '
                 'som_a.txt som_b.txt som_p.txt som_pa.txt som_xa.txt '
                 'ha.txt hq.txt hm.txt hqa.txt hxa.txt ppcsm.txt gdb.txt '
                 'mem_62/hq.txt.dump mem_62/hq.txt.pages img/x86/teste.ppm img/ppc/teste.ppm '
                 'img/x86/saida.txt img/ppc/saida.txt').split()
        for raiz in (a, b):
            for nome in nomes:
                f = raiz / nome
                f.parent.mkdir(parents=True, exist_ok=True)
                f.write_bytes(b'1 0000 0000 hp 10 10\n')
            dados = b'00001010'
            (raiz / 'mem_62/hq.txt.dump').write_bytes(b'heap 1000 8\n' + dados)
            (raiz / 'mem_62/hq.txt.pages').write_text(f'heap 1000 {xxh(dados):016x}\n')
        def cli(rc, motivo):
            r = subprocess.run([sys.executable, __file__, str(a), str(b)], capture_output=True, text=True)
            exigir(r.returncode == rc and motivo in r.stdout, f'controle falhou: {r.stdout}')
        cli(0, 'equivalência:')
        for nome in ('mem_62/hq.txt.dump', 'img/ppc/teste.ppm', 'hq.txt'):
            f = b / nome
            original = f.read_bytes()
            alterado = original[:-1] + bytes([original[-1] ^ 1]) if nome.endswith('.dump') else original.replace(b'10', b'11')
            f.write_bytes(alterado)
            paginas = b / 'mem_62/hq.txt.pages'
            paginas_originais = paginas.read_bytes()
            if nome.endswith('.dump'):
                paginas.write_text(f'heap 1000 {xxh(alterado.split(bytes([10]), 1)[1]):016x}\n')
            cli(1, 'diferente: ' + nome)
            f.write_bytes(original)
            paginas.write_bytes(paginas_originais)
            f.unlink()
            cli(1, 'FALHA:')
            f.write_bytes(original)
            print(f'controle {nome}: alteração e ausência recusadas com saída 1')
        paginas = b / 'mem_62/hq.txt.pages'
        paginas.write_text('estragado\n')
        cli(1, 'páginas incoerentes')
        print('controle páginas: hash incoerente recusado com saída 1')


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
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as e:
        print(f'FALHA: {e}')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
