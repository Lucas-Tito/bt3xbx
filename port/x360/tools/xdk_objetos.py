#!/usr/bin/env python3
"""F5, seção 37: os objetos de duas builds do XDK têm o mesmo código, função a função.

Os .obj do cl mudam a cada compilação (carimbo de hora do COFF, linhas do fonte na depuração), então a comparação é
pela desmontagem: `link.exe /dump /disasm` dentro da imagem do XDK (collectionui-xdk:light), com os rótulos que dependem
do número da linha normalizados ($M123, $LN45, $rotulo$678). Também compara a lista de funções do .map (o -OPT:REF
descarta COMDAT sem uso: uma função vazia que some só aparece aqui) e, com --rdata, o despejo bruto das seções.

    xdk_objetos.py RAIZ BUILD_A BUILD_B [--so OBJ,...] [--espera-diferente OBJ,...] [--saida PASTA]
      RAIZ      a pasta montada em /app (os BUILD_* são relativos a ela, com obj/, x360/ e default.map)
      --so      só esses objetos (nomes como obj/gs_gs_draw.obj, x360/gs_d3d.obj)
      --espera-diferente  esses têm de diferir (a prova de que o verificador enxerga uma diferença real)
    xdk_objetos.py --mutantes RAIZ BUILD   (cópias estragadas da desmontagem e do .map têm de ser recusadas)
Sai com 1 em qualquer diferença não esperada.
"""
import argparse
from collections import Counter
import os
from pathlib import Path
import re
import subprocess
import sys

XDK = Path(os.environ.get('XDK_DIR', Path.home() / 'Documentos/CollectionUI/sdk/XDK'))
IMAGEM = os.environ.get('IMAGEM', 'collectionui-xdk:light')


def desmonta(raiz, objs, saida):
    """um .dis por objeto em saida (relativa à raiz), numa só chamada do docker"""
    (raiz / saida).mkdir(parents=True, exist_ok=True)
    lista = raiz / saida / 'objs.txt'
    lista.write_text('\n'.join(objs) + '\n')
    script = ('wineboot -i >/dev/null 2>&1 || true\n'
              f'while read f; do n=$(echo $f | tr / _); wine /xdk/bin/win32/link.exe /dump /disasm $f > {saida}/$n.dis 2>/dev/null; '
              f'done < {saida}/objs.txt\nchown -R {os.getuid()}:{os.getgid()} {saida}')
    subprocess.run(['docker', 'run', '--rm', '-v', f'{raiz}:/app', '-v', f'{XDK}/lib/xbox:/xdk/lib/xbox:ro', '-w', '/app',
                    '-e', 'WINEDEBUG=-all', IMAGEM, 'bash', '-c', script], check=True, capture_output=True)


def normaliza(texto):
    linhas = []
    for l in texto.splitlines():
        if l.startswith('Dump of file') or l.startswith('wine:') or 'Microsoft (R)' in l or 'Copyright (C)' in l:
            continue
        l = re.sub(r'\$(M|LN)\d+', r'$\1#', l)
        l = re.sub(r'\$([A-Za-z_]\w*)\$\d+', r'$\1$#', l)
        linhas.append(l)
    assert any(re.match(r'^\s+[0-9A-F]{8}: [0-9A-F]{8} ', l) for l in linhas), 'desmontagem vazia'
    return linhas


def funcoes(mapa):
    """as funções (f) do .map por nome e objeto, sem endereço; os __unwind$N numerados contam por objeto"""
    out = set()
    unw = Counter()
    for l in mapa.splitlines():
        m = re.match(r'^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+[0-9a-f]{8} f\s+(?:i\s+)?(\S+)', l)
        if m:
            if re.fullmatch(r'__unwind\$\d+', m[1]):
                unw[m[2]] += 1
            else:
                out.add((m[1], m[2]))
    assert out, 'mapa sem funções'
    return out | {(f'__unwind x{n}', o) for o, n in unw.items()}


def compara_textos(a, b):
    """o primeiro ponto de diferença, ou None"""
    na, nb = normaliza(a), normaliza(b)
    if na == nb:
        return None
    for i, (x, y) in enumerate(zip(na, nb)):
        if x != y:
            return f'linha {i}: {x.strip()!r} != {y.strip()!r}'
    return f'tamanhos {len(na)} != {len(nb)}'


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('raiz', type=Path); p.add_argument('a'); p.add_argument('b', nargs='?')
    p.add_argument('--so', default=''); p.add_argument('--espera-diferente', default='')
    p.add_argument('--saida', default='build/x360/dis'); p.add_argument('--mutantes', action='store_true')
    a = p.parse_args()
    raiz = a.raiz.resolve()
    try:
        if a.mutantes:
            mutantes(raiz, a.a, a.saida); return
        assert a.b, 'informe BUILD_B'
        objs = sorted(str(x.relative_to(raiz / a.a)) for x in (raiz / a.a).glob('*/*.obj'))
        objb = sorted(str(x.relative_to(raiz / a.b)) for x in (raiz / a.b).glob('*/*.obj'))
        assert objs == objb, f'objetos diferentes: {sorted(set(objs) ^ set(objb))[:10]}'
        if a.so:
            objs = [o for o in objs if o in a.so.split(',')]
            assert len(objs) == len(a.so.split(',')), 'objeto pedido ausente'
        espera = set(x for x in a.espera_diferente.split(',') if x)
        sa, sb = f'{a.saida}/a', f'{a.saida}/b'
        desmonta(raiz, [f'{a.a}/{o}' for o in objs], sa)
        desmonta(raiz, [f'{a.b}/{o}' for o in objs], sb)
        ruins = []
        for o in objs:
            n = lambda pre, s: (raiz / s / (f'{pre}/{o}'.replace('/', '_') + '.dis')).read_text(errors='replace')
            d = compara_textos(n(a.a, sa), n(a.b, sb))
            if (d is not None) != (o in espera):
                ruins.append(f'{o}: {d or "igual, mas devia diferir"}')
            elif d:
                print(f'diferente, como esperado: {o} ({d})')
        fa = funcoes((raiz / a.a / 'default.map').read_text(errors='replace'))
        fb = funcoes((raiz / a.b / 'default.map').read_text(errors='replace'))
        so_a = sorted(x for x in fa - fb if not a.so or x[1] in [Path(o).name for o in objs])
        so_b = sorted(x for x in fb - fa if not a.so or x[1] in [Path(o).name for o in objs])
        ok_map = all(o in [Path(e).name for e in espera] for _, o in so_a + so_b)
        if not ok_map:
            ruins.append(f'funções do .map: só em A {so_a[:8]}, só em B {so_b[:8]}')
        elif so_a or so_b:
            print(f'funções do .map, nos esperados: só em A {[f for f, _ in so_a]}, só em B {[f for f, _ in so_b]}')
        if ruins:
            print('\n'.join('FALHA: ' + r for r in ruins))
            sys.exit(1)
        print(f'{len(objs)} objetos com a mesma desmontagem normalizada ({len(espera)} diferentes como esperado); '
              f'{len(fa)} funções no .map')
    except (AssertionError, OSError, subprocess.CalledProcessError) as e:
        print(f'FALHA: {e}')
        sys.exit(1)


def mutantes(raiz, build, saida):
    """o verificador enxerga: uma instrução trocada, um registrador, uma função a mais no .map"""
    o = 'x360/gs_d3d.obj'
    desmonta(raiz, [f'{build}/{o}'], f'{saida}/m')
    t = (raiz / saida / 'm' / (f'{build}/{o}'.replace('/', '_') + '.dis')).read_text(errors='replace')
    assert compara_textos(t, t) is None, 'a mesma desmontagem tem de ser igual'
    linhas = t.splitlines()
    i = next(k for k, l in enumerate(linhas) if re.match(r'^\s+[0-9A-F]{8}: [0-9A-F]{8} +addi ', l))
    estragos = {
        'instrução trocada': '\n'.join(linhas[:i] + [linhas[i].replace('addi', 'subi', 1)] + linhas[i + 1:]),
        'registrador trocado': t.replace('r31', 'r30', 1),
        'instrução a menos': '\n'.join(linhas[:i] + linhas[i + 1:]),
        'rótulo de linha que não é de linha': t.replace('blr', '$blr$1', 1),
    }
    for nome, ruim in estragos.items():
        assert compara_textos(t, ruim) is not None, f'mutante aceito: {nome}'
        print(f'mutante recusado: {nome}')
    mapa = (raiz / build / 'default.map').read_text(errors='replace')
    f = funcoes(mapa)
    extra = mapa + '\n 0003:00000000       FuncaoNova                 82000000 f   gs_d3d.obj\n'
    assert funcoes(extra) != f, 'mutante aceito: função a mais no .map'
    print('mutante recusado: função a mais no .map')
    print('tudo certo nos mutantes do xdk_objetos')


if __name__ == '__main__':
    main()
