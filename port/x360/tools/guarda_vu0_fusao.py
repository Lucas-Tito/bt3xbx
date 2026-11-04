#!/usr/bin/env python3
"""Recusa fusões na VU0 ligada e desmonta suas faixas pelo mapa do XDK.

Uso: guarda_vu0_fusao.py default.exe default.map --comando-dump wine /xdk/bin/win32/link.exe
O comando recebe /dump /disasm /range:INICIO,FIM EXE. Os .dis ficam ao lado do EXE.
Sem LTCG, inlining só chega às funções do mesmo objeto: conferimos todos os
símbolos de vu0_nativo, float360, pc_vu0_a e pc_vu0_b, inclusive os estáticos.
A inspeção dos opcodes do PE independe da grafia dos mnemônicos do link.exe.
"""
import argparse
import pathlib
import re
import struct
import subprocess
from chamadores_bl import secoes

OBJETOS = {'vu0_nativo', 'float360', 'pc_vu0_a', 'pc_vu0_b'}
FUSOES = {28: 'fmsub', 29: 'fmadd', 30: 'fnmsub', 31: 'fnmadd'}


def faixas(exe, mapa):
    funcoes = {}
    presentes = set()
    for linha in mapa.splitlines():
        m = re.match(r'\s*[\da-fA-F]{4}:[\da-fA-F]{8}\s+(\S+)\s+([\da-fA-F]{8})\s+f\s+(?:i\s+)?(\S+)', linha)
        if not m:
            continue
        nome, endereco, objeto = m.groups()
        objeto = objeto.replace('\\', '/').split('/')[-1].split(':')[-1]
        objeto = pathlib.Path(objeto).stem
        alvo = objeto in OBJETOS or nome.startswith('Vn_')
        if alvo:
            presentes.add(objeto)
        funcoes.setdefault(int(endereco, 16), []).append((nome, alvo))
    if not OBJETOS <= presentes:
        raise ValueError('objetos ausentes no mapa: '+', '.join(sorted(OBJETOS-presentes)))
    saida = []
    cobertos = set()
    for va, vs, rp, rs in secoes(exe):
        fim = va + min(vs, rs)
        inicios = sorted(a for a in funcoes if va <= a < fim)
        for i, a in enumerate(inicios):
            if not any(alvo for _, alvo in funcoes[a]):
                continue
            b = inicios[i+1] if i+1 < len(inicios) else fim
            if a % 4 or b % 4 or a >= b:
                raise ValueError('faixa de código inválida')
            cobertos.add(a)
            nomes = ','.join(nome for nome, _ in funcoes[a])
            saida.append((a, b, rp+a-va, nomes))
    esperados = {a for a, nomes in funcoes.items() if any(alvo for _, alvo in nomes)}
    if esperados != cobertos:
        raise ValueError('símbolo da VU0 fora das seções executáveis do PE')
    if not saida:
        raise ValueError('nenhuma faixa executável da VU0')
    return saida


def conferir(exe, mapa):
    intervalos = faixas(exe, mapa)
    falhas = []
    for a, b, off, nomes in intervalos:
        for pc in range(a, b, 4):
            w = struct.unpack_from('>I', exe, off+pc-a)[0]
            # Formato A, instruções simples (59) e duplas (63), com ou sem Rc.
            if w >> 26 in (59, 63) and (w >> 1) & 31 in FUSOES:
                op = FUSOES[(w >> 1) & 31] + ('s' if w >> 26 == 59 else '')
                falhas.append(f'FALHA fusão {op} em {nomes} {pc:08X}: {w:08X}')
    return intervalos, falhas


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('exe', type=pathlib.Path)
    ap.add_argument('mapa', type=pathlib.Path)
    ap.add_argument('--comando-dump', nargs=argparse.REMAINDER,
                    default=['wine', '/xdk/bin/win32/link.exe'])
    args = ap.parse_args()
    try:
        intervalos, falhas = conferir(args.exe.read_bytes(), args.mapa.read_text(errors='replace'))
        # Junta faixas adjacentes para não iniciar o Wine para cada função.
        grupos = []
        for a, b, _, _ in intervalos:
            if grupos and grupos[-1][1] == a:
                grupos[-1] = (grupos[-1][0], b)
            else:
                grupos.append((a, b))
        for a, b in grupos:
            p = subprocess.run([*args.comando_dump, '/dump', '/disasm',
                                f'/range:{a:08X},{b-4:08X}', str(args.exe)],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            log = args.exe.with_name(f'vu0-{a:08x}-{b:08x}.dis')
            log.write_text(p.stdout)
            if p.returncode or not re.search(rf'\b{a:08x}:', p.stdout, re.I):
                raise ValueError(f'desmontagem ausente ou inválida: {log}')
        for falha in falhas:
            print(falha)
        if falhas:
            return 1
        print(f'tudo certo: VU0 sem fusão em {len(intervalos)} funções, {len(grupos)} faixas desmontadas')
        return 0
    except (OSError, ValueError, struct.error) as e:
        print('FALHA guarda VU0:', e)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
