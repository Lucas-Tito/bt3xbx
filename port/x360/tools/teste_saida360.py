#!/usr/bin/env python3
"""Compila a desmontagem real com API simulada; --mutantes exige falhas em cópias."""
import argparse
import pathlib
import shutil
import subprocess
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]
CASOS = ('normal', 'jogo-atrasado', 'gravacao-atrasada', 'desenho-atrasado',
         'espera-falhou', 'carga-atrasada', 'som-preso', 'gpu-presa', 'diario-preso', 'sem-auxiliar', 'chave')


def compilar(pasta):
    subprocess.run(['g++', '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(pasta), str(RAIZ / 'port/x360/tools/teste_saida360.cpp'),
                    '-o', str(pasta / 'teste')], check=True)


def executar(pasta, caso):
    return subprocess.run([str(pasta / 'teste'), caso], text=True, capture_output=True, timeout=3)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--mutantes', action='store_true')
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as temporario:
        pasta = pathlib.Path(temporario)
        for nome in ('saida360.h', 'queda_teste360.h'):
            shutil.copyfile(RAIZ / 'port/x360/src' / nome, pasta / nome)
        compilar(pasta)
        for caso in CASOS:
            r = executar(pasta, caso)
            print(r.stdout, end='')
            if r.returncode:
                raise SystemExit(r.stderr)
        if args.mutantes:
            mutantes = (
                ('sem espera do desenho', 'saida360.h', 'if (!Saida_Esperar("GS desenho", (HANDLE)GsDraw_Thread360(), 2000)) ok = 0;', '', 'normal'),
                ('timeout considerado sucesso', 'saida360.h', 'return r == WAIT_OBJECT_0;', 'return 1;', 'desenho-atrasado'),
                ('sem pedido de parada', 'saida360.h', '    Port_GsPararJa();', '', 'normal'),
                ('sem GPU ociosa', 'saida360.h', '    v->dev->BlockUntilIdle();', '', 'normal'),
                ('liberação insegura', 'saida360.h', 'if (liberarVideo) {', 'if (1) { (void)liberarVideo;', 'jogo-atrasado'),
                ('sem teto externo', 'saida360.h', 'GetTickCount() - inicio >= teto', '(GetTickCount() - inicio >= teto && false)', 'gpu-presa'),
                ('sem som', 'saida360.h', '    Som360_Parar();', '    (void)&Som360_Parar;', 'normal'),
                ('teste armado por padrão', 'queda_teste360.h', 'if (valor == NULL || *valor == \'\\0\') return;', 'if (valor == NULL || *valor == \'\\0\') valor = "1";', 'chave'),
            )
            for i, (nome, arquivo, antes, depois, caso) in enumerate(mutantes):
                mut = pasta / str(i)
                mut.mkdir()
                for cab in ('saida360.h', 'queda_teste360.h'):
                    fonte = (pasta / cab).read_text()
                    if cab == arquivo:
                        assert fonte.count(antes) == 1, nome
                        fonte = fonte.replace(antes, depois)
                    (mut / cab).write_text(fonte)
                compilar(mut)
                try:
                    r = executar(mut, caso)
                except subprocess.TimeoutExpired:
                    assert nome == 'sem teto externo'
                    print(f'mutante {nome}: acusado por tempo excedido')
                    continue
                if r.returncode != 1 or 'FALHA:' not in r.stderr:
                    raise SystemExit(f'mutante {nome} não acusado: {r.stdout} {r.stderr}')
                print(f'mutante {nome}: acusado: {r.stderr.strip()}')
    print('saída do 360: tudo certo')


if __name__ == '__main__':
    main()
