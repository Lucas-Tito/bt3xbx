#!/usr/bin/env python3
"""Conferências da rodada final: objetos sem depuração e saída do save_mc.sh."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile


def exigir(condicao, mensagem):
    if not condicao:
        raise ValueError(mensagem)


def objetos(base, final):
    a = {str(p.relative_to(base)): p for p in base.rglob('*.o')}
    b = {str(p.relative_to(final)): p for p in final.rglob('*.o')}
    exigir(a and a.keys() == b.keys(), 'objetos ausentes, novos ou lista vazia')
    iguais, teste = 0, []
    with tempfile.TemporaryDirectory(dir=Path.home(), prefix='f5-objetos-') as d:
        x, y = (Path(d) / n for n in ('base.o', 'final.o'))
        for nome in sorted(a):
            for origem, destino in ((a[nome], x), (b[nome], y)):
                subprocess.run(['objcopy', '--strip-debug', str(origem), str(destino)], check=True)
            rc = subprocess.run(['cmp', '-s', str(x), str(y)]).returncode
            exigir(rc in (0, 1), f'cmp falhou: {nome}')
            if rc == 0:
                iguais += 1
            else:
                # Lista explícita do código de teste compilado pelo headless.py.
                permitir = {'obj/x360_passos.o', 'obj/x360_roteiro.o', 'obj/x360_memoria.o',
                            'obj/x360_sem_janela.o', 'obj/x360_imagens.o', 'obj/x360_som.o'}
                exigir(nome in permitir, f'objeto fora dos testes diferente: {nome}')
                teste.append(nome)
    print(f'objetos: {iguais} iguais; diferentes só dos testes: {teste}; total {len(a)}')


# Mudanças de teste dos roteiros: o jogo e o port continuam provados pelos objetos.
TEXT_ANTIGA = '43444ea4d03a6d448aa1ae74a631e35b356a7caa'
TEXT_ROTEIROS = 'e6e284c6064a064a9230b88bea90c16b3efc31e7'


def texto(pasta):
    for nome in ('base', 'final'):
        sha = hashlib.sha1((pasta / f'text-{nome}.bin').read_bytes()).hexdigest()
        aceitos = {TEXT_ANTIGA, TEXT_ROTEIROS} if nome == 'base' else {TEXT_ROTEIROS}
        exigir(sha in aceitos, f'.text {nome} diferente: {sha}')
        print(f'.text {nome}: {sha}')
    print('.text: referências aprovadas; conferir também objetos sem depuração')


def save_mc(pasta):
    for alvo in ('x86', 'ppc'):
        s = (pasta / f't_{alvo}.txt').read_text()
        exigir(s.rstrip().endswith('ok: 0 falha(s)') and 'FALHOU' not in s,
               f'teste_mc {alvo} incompleto ou falhou')
    for caso, referencia in (('A', 1), ('B1', 2), ('B2', 2)):
        a = (pasta / f'estados_{caso}.txt').read_bytes()
        b = (pasta / f'estados_x86_{referencia}.txt').read_bytes()
        exigir(a.strip() and a == b, f'estados diferentes ou ausentes: {caso}')
    exigir('ESTADO 11 msg' in (pasta / 'estados_F.txt').read_text(), 'falha de escrita sem estado de erro')
    exigir('FALHOU (escrita (teste: BT3_MC_FALHA))' in (pasta / 'ppc_F.txt').read_text(),
           'falha de escrita não foi provocada')
    nome = 'BASLUS-21678DBZT3'
    def arq(caso):
        return pasta / f'c_{caso}' / 'card1' / nome / nome
    dados = arq('A').read_bytes()
    exigir(len(dados) == 16384, 'save A ausente ou truncado')
    for caso in ('B1', 'B2'):
        exigir(arq(caso).read_bytes() == dados, f'save recuperado diferente: {caso}')
        exigir(not Path(str(arq(caso)) + '.tmp').exists(), f'temporário não limpo: {caso}')
    exigir(not arq('F').exists(), 'falha de escrita publicou save')
    for caso in ('A', 'F', 'B1', 'B2'):
        exigir('$1 = ' in (pasta / f'gdb_{caso}.txt').read_text(), f'não chegou ao menu: {caso}')
    print('save_mc: tudo certo')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='modo', required=True)
    o = sub.add_parser('objetos')
    o.add_argument('base', type=Path)
    o.add_argument('final', type=Path)
    t = sub.add_parser('text')
    t.add_argument('pasta', type=Path)
    s = sub.add_parser('save-mc')
    s.add_argument('pasta', type=Path)
    args = p.parse_args()
    try:
        if args.modo == 'objetos':
            objetos(args.base, args.final)
        elif args.modo == 'text':
            texto(args.pasta)
        else:
            save_mc(args.pasta)
    except (OSError, ValueError, subprocess.CalledProcessError) as e:
        print(f'FALHA: {e}')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
