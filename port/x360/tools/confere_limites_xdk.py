#!/usr/bin/env python3
"""Antecipações no WSL de três falhas observadas no XDK e no console.

c89 PASTA: sintaxe de todos os preparados; C89 estrito também no gravador GS.
matematica OBJETOS [--float-fonte F]: referências matemáticas contra provedores do 360.
layout PROGRAMA [--backend F]: sizeof esperados pelo D3D9 contra DWARF da PPC GPU.
Não substitui compilação pelo cl, ligação do XDK nem execução no console.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]


def executar(cmd):
    return subprocess.run(list(map(str, cmd)), text=True, capture_output=True,
                          env={**os.environ, 'LC_ALL': 'C'})


def c89(pasta):
    fontes = sorted(pasta.glob('*.c'))
    if not fontes or not (pasta / 'gs_gs_draw.c').is_file():
        raise ValueError('faltam fontes preparados, incluindo gs_gs_draw.c')
    erros = []
    for f in fontes:
        cmd = ['gcc', '-m32', '-std=c89', '-fsyntax-only', '-fno-builtin',
               '-Werror=declaration-after-statement', '-D__int64=long long',
               '-D__inline=__inline__', '-D__declspec(x)=']
        if f.name == 'gs_gs_draw.c':
            # O cl aceita __int64 e macros variádicas; GCC pode acusar essas extensões.
            cmd += ['-pedantic-errors', '-Wno-long-long', '-Wno-variadic-macros']
        r = executar(cmd + [f])
        if r.returncode:
            erros.append(f.name)
            print(r.stderr, end='')
    if erros:
        raise ValueError('sintaxe recusada: ' + ', '.join(erros))
    print(f'C89: {len(fontes)} fontes; gravador GS também com -pedantic-errors: tudo certo')


def simbolos(objetos, indefinidos=False):
    if not objetos:
        raise ValueError('lista de objetos vazia')
    r = executar(['nm', '-u' if indefinidos else '--defined-only', '-g', *objetos])
    if r.returncode:
        raise ValueError(r.stderr)
    return {q[-1] for linha in r.stdout.splitlines() if len(q := linha.split()) >= 2}


def matematica(pasta, fonte, nativo=None):
    cab = (RAIZ / 'port/include/port_libm.h').read_text()
    nomes = set(re.findall(r'^(?:float|double)\s+(Port_\w+)\(', cab, re.M))
    objetos = sorted(pasta.glob('*.o'))
    pedidos = simbolos(objetos, True) & nomes
    if not {'Port_sinf', 'Port_powf'} <= pedidos:
        raise ValueError('objetos não exercitam sinf/powf; prova incompleta')
    with tempfile.TemporaryDirectory(dir=Path.home()) as tmp:
        obj = Path(tmp) / 'float360.o'
        r = executar(['gcc', '-m32', '-std=c89', '-Dinline=__inline__', '-fno-builtin', '-I'+str(RAIZ/'port/x360/include'), '-c', fonte, '-o', obj])
        if r.returncode:
            raise ValueError(r.stderr)
        nucleo = Path(tmp) / 'vu0_nativo.o'
        nativo = nativo or RAIZ / 'port/x360/src/f2/vu0_nativo.c'
        r = executar(['gcc', '-m32', '-std=c89', '-Dinline=__inline__', '-fno-builtin', '-I'+str(RAIZ/'port/x360/include'),
                      '-c', nativo, '-o', nucleo])
        if r.returncode:
            raise ValueError(r.stderr)
        requeridos = simbolos([obj], True) | simbolos([nucleo], True) | simbolos(objetos, True)
        locais = simbolos([obj, nucleo])
        faltam_nativos = {n for n in requeridos if n.startswith(('Vn_', 'Port_Fast'))} - locais
        if faltam_nativos:
            raise ValueError('VU0 sem provedor: ' + ', '.join(sorted(faltam_nativos)))
        print('VU0 nativa: referências entre núcleo e float360 resolvidas, sem stub')
        # plat_libm.c é exclusivo do PC e nunca serve de prova para o 360.
        provedores = simbolos([obj, *sorted(pasta.glob('nl_*.o'))])
    faltam = pedidos - provedores
    if faltam:
        raise ValueError('matemática sem provedor no 360: ' + ', '.join(sorted(faltam)))
    print(f'Matemática: {len(pedidos)} entradas referidas com provedor do 360, incluindo Port_powf: tudo certo')


def layout(programa, backend):
    with programa.open('rb') as f:
        if f.read(20)[:6] != b'\x7fELF\x01\x02':
            raise ValueError('a prova de layout exige ELF de 32 bits big-endian')
    esperados = re.findall(r'sizeof\((\w+)\)\s*!=\s*(\d+)', backend.read_text())
    if not {'Vtx', 'GsDraw'} <= {n for n, _ in esperados}:
        raise ValueError('faltam conferências Vtx/GsDraw no backend')
    r = executar(['llvm-dwarfdump', '--debug-info',
                  *['--name=' + n for n, _ in esperados], programa])
    if r.returncode:
        raise ValueError(r.stderr)
    tamanhos = {}
    for bloco in re.split(r'\n(?=0x[0-9a-f]+:)', r.stdout):
        n = re.search(r'DW_AT_name\s+\("(\w+)"\)', bloco)
        tam = re.search(r'DW_AT_byte_size\s+\((0x[0-9a-f]+|\d+)\)', bloco)
        if n and tam and 'gs_draw.h' in bloco:
            tamanhos.setdefault(n[1], set()).add(int(tam[1], 0))
    for nome, tam in esperados:
        reais = tamanhos.get(nome, set())
        if reais != {int(tam)}:
            raise ValueError(f'layout {nome}: backend espera {tam}; PPC tem {sorted(reais)}')
        print(f'Layout {nome}: backend e PPC com {tam} bytes')
    print('Layout compartilhado D3D9/PPC: tudo certo')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='modo', required=True)
    p = sub.add_parser('c89'); p.add_argument('pasta', type=Path)
    p = sub.add_parser('matematica'); p.add_argument('pasta', type=Path)
    p.add_argument('--float-fonte', type=Path, default=RAIZ / 'port/x360/src/f2/float360.c')
    p.add_argument('--nativo-fonte', type=Path)
    p = sub.add_parser('layout'); p.add_argument('programa', type=Path)
    p.add_argument('--backend', type=Path, default=RAIZ / 'port/x360/src/gs/gs_d3d.cpp')
    args = ap.parse_args()
    try:
        if args.modo == 'c89': c89(args.pasta)
        elif args.modo == 'matematica': matematica(args.pasta, args.float_fonte, args.nativo_fonte)
        else: layout(args.programa, args.backend)
    except (ValueError, OSError) as erro:
        ap.exit(1, f'FALHA: {erro}\n')


if __name__ == '__main__':
    main()
