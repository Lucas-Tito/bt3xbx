#!/usr/bin/env python3
"""Exercita a guarda com PE PPC sintético e cópias com instruções fundidas.

Não depende do XDK nem contém bytes do jogo. A saída de dump simulada testa a
orquestração; a decisão usa os opcodes reais no PE. O dump real cabe ao XDK.
"""
import pathlib
import struct
import subprocess
import sys
import tempfile
from guarda_vu0_fusao import conferir


def main():
    with tempfile.TemporaryDirectory(prefix='vu0-guarda-') as d:
        d = pathlib.Path(d)
        exe = bytearray(0x300)
        struct.pack_into('<I', exe, 0x3c, 0x80)
        exe[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<H', exe, 0x86, 1)
        struct.pack_into('<H', exe, 0x94, 0xe0)
        struct.pack_into('<I', exe, 0x80+24+28, 0x82000000)
        sec = 0x80+24+0xe0
        struct.pack_into('<IIII', exe, sec+8, 32, 0x1000, 32, 0x200)
        struct.pack_into('<I', exe, sec+36, 0x20000000)
        for i in range(8):
            struct.pack_into('>I', exe, 0x200+4*i, 0x60000000)
        mapa = '\n'.join(f' 0001:{8*i:08X} {nome} {0x82001000+8*i:08X} f {obj}.obj'
                         for i, (nome, obj) in enumerate([
                             ('Vn_MtxApply', 'vu0_nativo'), ('Port_FastMtxApply', 'float360'),
                             ('op_vmaddbc', 'pc_vu0_a'), ('Ref_Mtx_Mul', 'pc_vu0_b')]))
        (d/'default.map').write_text(mapa)
        # Substitui só a ferramenta de dump, mantendo a leitura real do PE.
        dump = d/'dump.py'
        dump.write_text('import sys\na=sys.argv[-2].split(":")[1].split(",")[0]\nprint(a+": 60000000 nop")\n')
        def roda(copia, esperado):
            (d/'default.exe').write_bytes(copia)
            p = subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('guarda_vu0_fusao.py')),
                                str(d/'default.exe'), str(d/'default.map'), '--comando-dump',
                                sys.executable, str(dump)], capture_output=True, text=True)
            print(p.stdout.strip())
            if p.returncode != esperado:
                raise RuntimeError(f'saída {p.returncode}, esperada {esperado}: {p.stderr}')
        roda(exe, 0)
        # Todos os quatro tipos, nos núcleos e em quem os recebe inline.
        for i, xo in enumerate((28, 29, 30, 31)):
            copia = bytearray(exe)
            struct.pack_into('>I', copia, 0x200+8*i, (59 << 26) | (xo << 1))
            roda(copia, 1)
        for xo in (28, 29, 30, 31):
            copia = bytearray(exe)
            struct.pack_into('>I', copia, 0x200, (63 << 26) | (xo << 1) | 1)
            if not conferir(copia, mapa)[1]:
                raise RuntimeError('fusão dupla com Rc aceita')
        # Falta de símbolo e falha do dump também não podem aprovar.
        (d/'default.map').write_text(mapa.replace('pc_vu0_b.obj', 'outro.obj'))
        roda(exe, 1)
        (d/'default.map').write_text(mapa)
        dump.write_text('print("sem desmontagem")\n')
        roda(exe, 1)
        print('tudo certo: controles negativos da guarda recusados')


if __name__ == '__main__':
    main()
