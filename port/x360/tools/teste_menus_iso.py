#!/usr/bin/env python3
"""ISO v4: descompressão independente e equivalência dos menus antecipados.

Cada pacote é comparado com o conversor antigo do conteúdo, sem recompressão.
Os estragos são feitos em cópias na memória, sem escrever nos dados do jogo.
"""
import argparse
import ctypes as C
from pathlib import Path
import struct
import subprocess
import sys

RAIZ = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(RAIZ / 'port/x360/tools'))
import memdiff


def exigir(ok, mensagem):
    if not ok:
        raise ValueError(mensagem)


def abrir(b, ordem):
    """Leitor BPE independente do C, com expansão limitada ao rawSize."""
    bruto, pack = struct.unpack_from(ordem + 'II', b)
    exigir(0 < bruto <= 0x900000 and pack <= len(b) - 8, 'cabeçalho BPE inválido')
    p, fim, out = 8, pack + 8, bytearray()
    while p < fim:
        tabela = [(i,) for i in range(256)]
        c = 0
        while c < 256:
            cont = b[p]; p += 1
            if cont > 127:
                c += cont - 127
                cont = 0
            exigir(c <= 256, 'tabela BPE inválida')
            for _ in range(cont + 1):
                if c == 256:
                    break
                a = b[p]; p += 1
                if a != c:
                    tabela[c] = (a, b[p]); p += 1
                c += 1
        n = int.from_bytes(b[p:p + 2], 'big'); p += 2
        exigir(0 <= n <= 32767 and p + n <= fim, 'bloco BPE inválido')
        for ch in b[p:p + n]:
            pilha = [ch]
            passos = 0
            while pilha:
                passos += 1
                exigir(passos <= 0x1200000, 'ciclo BPE')
                ch = pilha.pop()
                if tabela[ch] == (ch,):
                    out.append(ch)
                    exigir(len(out) <= bruto, 'expansão BPE excessiva')
                else:
                    pilha.extend(reversed(tabela[ch]))
        p += n
    exigir(p == fim and len(out) == bruto, 'tamanho BPE diferente')
    return bytes(out)


def conferir(lib, nome, original, convertido):
    exigir(len(original) == len(convertido), 'tamanho do arquivo mudou')
    formato = lib.Conv_FormatoDe(nome.encode()).decode()
    if formato == 'menu_bpe':
        secoes = [(0, len(original), 0)]
    else:
        n = int.from_bytes(original[:4], 'little')
        exigir(int.from_bytes(convertido[:4], 'big') == n, 'contagem externa mudou')
        offsets = struct.unpack_from('<' + 'I' * (n + 1), original, 4)
        exigir(struct.unpack_from('>' + 'I' * (n + 1), convertido, 4) == offsets, 'tabela externa mudou')
        secoes = [(a, b, i + 1) for i, (a, b) in enumerate(zip(offsets, offsets[1:])) if a != b]
    for a, b, i in secoes:
        f = lib.Conv_FormatoSecao(nome.encode(), i)
        exigir(f is not None, f'{nome}: seção {i} sem formato')
        raw = abrir(original[a:b], '<')
        esperado = C.create_string_buffer(raw, len(raw))
        marcas = C.create_string_buffer(len(raw))
        msg = C.create_string_buffer(256)
        exigir(lib.Conv_Formato(f, f, esperado, len(raw), marcas, msg, 256) == 1, msg.value.decode())
        exigir(abrir(convertido[a:b], '>') == esperado.raw, f'{nome} seção {i}: conteúdo diferente')
    return len(secoes)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dados', type=Path, default=RAIZ / 'gamedata')
    ap.add_argument('--trabalho', type=Path, required=True)
    args = ap.parse_args()
    args.trabalho.mkdir(parents=True, exist_ok=True)
    so = args.trabalho.resolve() / 'menus.so'
    fontes = sorted((RAIZ / 'port/x360/conv').glob('*.c'))
    subprocess.run(['gcc', '-std=c89', '-O2', '-Wall', '-shared', '-fPIC', '-o', str(so),
                    *map(str, (f for f in fontes if f.name not in ('conv_tool.c', 'conv_jogo.c')))], check=True)
    lib = C.CDLL(str(so))
    lib.Conv_FormatoDe.argtypes = [C.c_char_p]
    lib.Conv_FormatoDe.restype = C.c_char_p
    lib.Conv_FormatoSecao.argtypes = [C.c_char_p, C.c_int]
    lib.Conv_FormatoSecao.restype = C.c_char_p
    lib.Conv_Arquivo.argtypes = [C.c_char_p, C.c_void_p, C.c_ulong, C.c_void_p, C.c_void_p, C.c_int]
    lib.Conv_Formato.argtypes = [C.c_char_p, *lib.Conv_Arquivo.argtypes]
    arquivos = conteudos = 0
    amostra = None
    for p in sorted((args.dados / 'pzs3us1').glob('*.bin')):
        nome = 'pzs3us1/' + p.name
        if lib.Conv_FormatoDe(nome.encode()) not in (b'menu_bpe', b'menu_pacote'):
            continue
        original = p.read_bytes()
        convertido = C.create_string_buffer(original, len(original))
        marcas = C.create_string_buffer(len(original)); msg = C.create_string_buffer(256)
        exigir(lib.Conv_Arquivo(nome.encode(), convertido, len(original), marcas, msg, 256) == 1,
               f'{nome}: {msg.value.decode()}')
        conteudos += conferir(lib, nome, original, convertido.raw)
        arquivos += 1
        if amostra is None and b'menu_bpe' == lib.Conv_FormatoDe(nome.encode()):
            amostra = (nome, original, convertido.raw, marcas.raw)
    exigir(arquivos > 600 and conteudos > 650, 'cobertura de menus incompleta')
    print(f'{arquivos} arquivos, {conteudos} conteúdos antecipados iguais ao conversor por formato', flush=True)
    nome, original, convertido, marcas = amostra
    # O contrato anterior convertia só os cabeçalhos, deixando o conteúdo cru.
    antigo = C.create_string_buffer(original, len(original))
    m = C.create_string_buffer(len(original)); msg = C.create_string_buffer(256)
    exigir(lib.Conv_Formato(b'menu_bpe', nome.encode(), antigo, len(original), m, msg, 256) == 1,
           'referência do contrato anterior falhou')
    # O verificador acima tem de reprovar conteúdo cru e conteúdo alterado.
    for rotulo, ruim in [('cru', antigo.raw), ('byte', convertido[:8] + bytes([convertido[8] ^ 1]) + convertido[9:])]:
        try:
            conferir(lib, nome, original, ruim)
        except (ValueError, IndexError, struct.error):
            print(f'estrago {rotulo}: reprovado na equivalência', flush=True)
        else:
            raise ValueError(f'estrago {rotulo} passou')
    # A comparação de memória usa os bytes esperados do fluxo antecipado.
    ax = 0x1000
    cab = bytearray(32); struct.pack_into('<I', cab, 0x14, ax + 32)
    dx = bytes(cab) + original
    def julgar(b):
        return memdiff.pelo_formato(ax, dx, bytes(cab) + b, marcas, original, convertido)[2]['fora-do-formato']
    exigir(julgar(convertido) == 0, 'memória íntegra reprovada')
    exigir(julgar(antigo.raw) > 0, 'memória crua passou')
    ruim = bytearray(convertido); ruim[-1] ^= 1
    exigir(julgar(ruim) > 0, 'byte alterado passou na memória')
    print('estragos memória crua e byte: reprovados; íntegro conforme', flush=True)
    # O C também precisa recusar contêineres impossíveis, antes de recomprimir.
    for rotulo, b in [('truncado', original[:7]), ('fluxo', original[:4] + b'\xff' * 4 + original[8:])]:
        buf = C.create_string_buffer(b, len(b)); m = C.create_string_buffer(len(b)); msg = C.create_string_buffer(256)
        exigir(lib.Conv_Arquivo(nome.encode(), buf, len(b), m, msg, 256) < 0, f'{rotulo} aceito pelo C')
        print(f'estrago {rotulo}: recusado pelo C', flush=True)
    print('teste_menus_iso: tudo certo', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as e:
        sys.exit(f'FALHA: {e}')
