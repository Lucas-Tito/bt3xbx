#!/usr/bin/env python3
"""Confere as duas transições e exige recusa de cópias estragadas, sem emulador.
Uso: teste_conv_transicao.py CONV_TOOL [DADOS] [--estraga-saida].
A opção negativa troca um byte numa cópia da saída antes da conferência.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile

conv = Path(sys.argv[1]).resolve()
args = [a for a in sys.argv[2:] if a != '--estraga-saida']
dados = Path(args[0] if args else 'gamedata').resolve()
nomes = [f'pzs3us1/{i:05}.bin' for i in (446, 447)]

def rodar(raiz, rel, destino=None):
    cmd = [str(conv)] + (['--saida', str(destino)] if destino else []) + [str(raiz), rel]
    return subprocess.run(cmd, text=True, capture_output=True)

with tempfile.TemporaryDirectory(prefix='queda-conv-') as tmp:
    pasta = Path(tmp)
    for rel in nomes:
        origem = (dados / rel).read_bytes()
        r = rodar(dados, rel, pasta / 'be')
        assert r.returncode == 0 and '100.0%' in r.stdout and 'convertido;' in r.stdout, r.stdout + r.stderr
        b = (pasta / 'be' / rel).read_bytes()
        if '--estraga-saida' in sys.argv:
            b = bytearray(b)
            b[3] ^= 1
        # Valores independentes do parser C: tabela, modelo, grupos e tabela de texturas.
        le32 = lambda p: struct.unpack_from('<I', origem, p)[0]
        be32 = lambda p: struct.unpack_from('>I', b, p)[0]
        for p in range(0, 20, 4):
            assert le32(p) == be32(p), (rel, p)
        m, tex = le32(8), le32(12)
        for p in range(m + 4, m + 24, 4):
            assert le32(p) == be32(p), (rel, p)
        assert b[m:m + 4] == origem[m:m + 4]
        def iguais(pos, quantidade, largura):
            for q in range(pos, pos + quantidade * largura, largura):
                assert int.from_bytes(origem[q:q + largura], 'little') == int.from_bytes(b[q:q + largura], 'big'), (rel, q)

        def textura(pos, base, palavras):
            iguais(pos, 10, 4)
            assert origem[pos + 40:pos + 48] == b[pos + 40:pos + 48]
            iguais(pos + 48, 1, 8)
            iguais(pos + 56, 2, 4)
            for delta in (0, 4):
                ofs, tam = le32(pos + delta), le32(pos + delta + 8)
                if ofs:
                    a = base + ofs * palavras
                    iguais(a, 24, 4)
                    assert origem[a + 96:a + tam - 32] == b[a + 96:a + tam - 32]
                    iguais(a + tam - 32, 8, 4)

        g, nt = le32(m + 8), le32(m + 12)
        for i in range(nt):
            textura(m + le32(m + 16) + i * 64, m, 1)
        for i in range(le32(m + 4)):
            p = m + g + i * 24
            for q in range(p, p + 20, 4):
                assert le32(q) == be32(q), (rel, q)
            for q in (p + 20, p + 22):
                assert struct.unpack_from('<H', origem, q)[0] == struct.unpack_from('>H', b, q)[0]
            ntri, nv = struct.unpack_from('<HH', origem, p + 20)
            iguais(m + le32(p + 8), nv * 3, 2)
            iguais(m + le32(p + 12), ntri * 15, 2)
        assert be32(tex) == le32(tex) == 7
        for i in range(7):
            textura(tex + le32(tex + 4) * 4 + i * 64, tex, 4)
        # Negativos sobre cópias: tabela, assinatura, contagens, limites, índices e pacotes.
        casos = []
        def estragar(nome, pos, valor, formato='<I'):
            copia = bytearray(origem)
            struct.pack_into(formato, copia, pos, valor)
            casos.append((nome, copia))
        estragar('tabela', 0, 4)
        estragar('fim da tabela', 16, len(origem) + 64)
        estragar('assinatura do modelo', m, 0)
        estragar('contagem dos grupos', m + 4, 0xffffffff)
        estragar('grupos fora do arquivo', m + 8, len(origem))
        estragar('modelo já relocado', m + 20, 1)
        estragar('textura do grupo', m + g + 4, nt)
        estragar('vértices fora do arquivo', m + g + 8, len(origem))
        estragar('triângulo fora do grupo', m + le32(m + g + 12), 0x7fff, '<H')
        estragar('imagem fora do modelo', m + le32(m + 16), len(origem))
        estragar('contagem do TexFile', tex, 0xffffffff)
        estragar('tabela do TexFile', tex + 4, 0xffffffff)
        for nome, copia in casos:
            caminho = pasta / 'ruim' / rel
            caminho.parent.mkdir(parents=True, exist_ok=True)
            caminho.write_bytes(copia)
            r = rodar(pasta / 'ruim', rel)
            assert r.returncode != 0 and rel in r.stdout, (rel, nome, r.stdout, r.stderr)
            print(f'{rel}: recusa de {nome}: ok')
        assert hashlib.sha1((dados / rel).read_bytes()).digest() == hashlib.sha1(origem).digest()
        print(f'{rel}: conversão integral, valores nativos e origem intacta: tudo certo')
