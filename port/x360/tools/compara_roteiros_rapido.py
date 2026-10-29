#!/usr/bin/env python3
"""Confere a coleta conjunta contra as rodadas separadas da mesma build, sem emulador."""
import argparse
import ctypes
import ctypes.util
import re
import struct
import memdiff
from pathlib import Path
import sys
import tempfile
import subprocess


def exigir(c, motivo):
    if not c:
        raise ValueError(motivo)


# Os campos abaixo já estão em DIFERENTES_CONHECIDAS de roteiros.py. Aqui a
# comparação é da mesma arquitetura e conserva os dados vivos, com as mesmas
# exclusões de --so-em-uso. Números de sMc/PortAdxView continuam byte a byte.
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
            if len(q) == 3 and q[-1] in ('gColWalkBox', 'gColWalkCtx', 'gSndMgr'):
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


def ponteiro_host(dados, endian):
    valor = int.from_bytes(dados, endian)
    exigir(valor == 0 or valor >= 0x10000000 and valor % 4 == 0, 'ponteiro do host inválido')
    # Preserva NULL contra não NULL; só o endereço alocado no host pode variar.
    return int(bool(valor)).to_bytes(4, endian)


def heap_vivo(dados, addr, be, snd):
    endian = 'big' if be else 'little'
    fmt = '>' if be else '<'
    blocos = memdiff.blocos(dados, be)
    exigir(blocos, 'heap sem blocos válidos')
    for off, tam in blocos:
        exigir(tam >= 0x24, 'bloco do heap menor que cabeçalho e rodapé')
        usado = struct.unpack_from(fmt + 'I', dados, off + 4)[0]
        exigir(usado in (0, 1), 'estado inválido no bloco do heap')
        dados[off + 0x1C:off + 0x20] = bytes(4)
        if not usado:
            dados[off + 0x20:off + tam - 4] = bytes(tam - 0x24)
            continue
        data, pedido = struct.unpack_from(fmt + 'II', dados, off + 0x14)
        ini = data - addr
        exigir(off + 0x20 <= ini <= ini + pedido <= off + tam - 4, 'dados fora do bloco do heap')
        dados[off + 0x20:ini] = bytes(ini - off - 0x20)
        dados[ini + pedido:off + tam - 4] = bytes(off + tam - 4 - ini - pedido)
        if data == snd:
            exigir(pedido == 0x2A8, 'SndMgr com tamanho inesperado')
            # include/sys/snd.h: SndBank.seqIop/hdIop e SndMgr.iopBuf.
            for d in [k * 0x4C + j for k in range(8) for j in (4, 8)] + [0x260]:
                dados[ini + d:ini + d + 4] = ponteiro_host(dados[ini + d:ini + d + 4], endian)
    return dados


def memoria(p, raiz):
    bruto = p.read_bytes()
    regioes, paginas = [], []
    o = 0
    snd = None
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
            a_snd, _ = syms['gSndMgr']
            if addr <= a_snd and a_snd + 4 <= addr + tam:
                snd = int.from_bytes(dados[a_snd - addr:a_snd - addr + 4], 'big' if alvo == 'ppc' else 'little')
            # sMapa é o ponteiro malloc do conversor, não estado do jogo. Na PPC
            # seus outros campos e todos os dados convertidos continuam comparados.
            if alvo == 'ppc' and 'sMapa' in syms:
                a_mapa, n_mapa = syms['sMapa']
                if addr <= a_mapa and a_mapa + n_mapa <= addr + tam:
                    exigir(n_mapa == 4, 'sMapa com tamanho inesperado')
                    dados[a_mapa - addr:a_mapa - addr + 4] = ponteiro_host(dados[a_mapa - addr:a_mapa - addr + 4], 'big')
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
        if nome == b'heap':
            dados = heap_vivo(dados, addr, alvo == 'ppc', snd)
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
          'caminhos/ponteiros do host e exclusões de --so-em-uso; páginas íntegras')


def mutantes():
    for be, fmt in ((False, '<'), (True, '>')):
        usado = struct.pack(fmt + '8I', memdiff.MAGIC, 1, 0, 0x20, 0x40, 0x1020, 8, 0)
        usado += bytes(0x1C) + struct.pack(fmt + 'I', 0x40)
        livre = struct.pack(fmt + '8I', memdiff.MAGIC, 0, 0, 0x20, 0x40, 0, 0x20, 0)
        livre += bytes(0x1C) + struct.pack(fmt + 'I', 0x40)
        original = usado + livre
        esperado = heap_vivo(bytearray(original), 0x1000, be, None)
        for off in (0x1C, 0x28, 0x60):
            copia = bytearray(original)
            copia[off] ^= 1
            exigir(heap_vivo(copia, 0x1000, be, None) == esperado, 'exclusão do alocador incorreta')
        for off in (8, 0x20, 0x24):
            copia = bytearray(original)
            copia[off] ^= 1
            exigir(heap_vivo(copia, 0x1000, be, None) != esperado, 'dado vivo ou metadado escondido')
        snd = bytearray(struct.pack(fmt + '8I', memdiff.MAGIC, 1, 0, 0x20, 0x2D0, 0x1020, 0x2A8, 0))
        snd += bytes(0x2AC) + struct.pack(fmt + 'I', 0x2D0)
        for valor in (0x40000000, 0x50000000):
            struct.pack_into(fmt + 'I', snd, 0x24, valor)
            atual = heap_vivo(bytearray(snd), 0x1000, be, 0x1020)
            if valor == 0x40000000:
                esperado = atual
            else:
                exigir(atual == esperado, 'endereço do host não normalizado')
        struct.pack_into(fmt + 'I', snd, 0x24, 0)
        exigir(heap_vivo(snd, 0x1000, be, 0x1020) != esperado, 'NULL confundido com ponteiro')
    print('controles do heap: dados vivos e metadados sensíveis; exclusões e ponteiros conferidos nas duas ordens')
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
            dados = struct.pack('>8I', memdiff.MAGIC, 1, 0, 0x20, 0x40, 0x1020, 8, 0)
            dados += bytes(0x1C) + struct.pack('>I', 0x40)
            (raiz / 'mem_62/hq.txt.dump').write_bytes(b'heap 1000 40\n' + dados)
            (raiz / 'mem_62/hq.txt.pages').write_text(f'heap 1000 {xxh(dados):016x}\n')
        def cli(rc, motivo):
            r = subprocess.run([sys.executable, __file__, str(a), str(b)], capture_output=True, text=True)
            exigir(r.returncode == rc and motivo in r.stdout, f'controle falhou: {r.stdout}')
        cli(0, 'equivalência:')
        for nome in ('mem_62/hq.txt.dump', 'img/ppc/teste.ppm', 'hq.txt'):
            f = b / nome
            original = f.read_bytes()
            if nome.endswith('.dump'):
                ini = original.index(b'\n') + 1 + 0x20
                alterado = original[:ini] + bytes([original[ini] ^ 1]) + original[ini + 1:]
            else:
                alterado = original.replace(b'10', b'11')
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
