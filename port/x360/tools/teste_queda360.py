#!/usr/bin/env python3
"""Executa a main real com API determinística e sabotagens em cópias, sem emulador."""
import pathlib
import shutil
import subprocess
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]
CASOS = ('jogo', 'desenho', 'audio', 'log-ligado', 'caixa-ocupada', 'caixa-negada',
         'caixa-falhou', 'resultado-falhou', 'disco-falhou', 'escrita-curta', 'flush-falhou')


def compilar(pasta):
    subprocess.run(['g++', '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unknown-pragmas', '-Wno-unused-parameter', '-Wno-int-to-pointer-cast',
                    '-I'+str(pasta), '-I'+str(RAIZ/'port/x360/tools/teste_queda'),
                    '-I'+str(RAIZ/'port/x360/src'), '-I'+str(RAIZ/'port/x360/src/f2'),
                    str(RAIZ/'port/x360/tools/teste_queda/teste.cpp'), '-o', str(pasta/'teste')], check=True)


def executar(pasta, caso):
    return subprocess.run([str(pasta/'teste'), caso], cwd=pasta, capture_output=True, text=True, timeout=5)


def main():
    with tempfile.TemporaryDirectory(prefix='elegante-') as d:
        p = pathlib.Path(d)
        arquivos = {'main360.cpp': 'f2/main360.cpp', 'queda360.h': 'queda360.h', 'saida360.h': 'saida360.h'}
        for nome, origem in arquivos.items():
            shutil.copyfile(RAIZ/'port/x360/src'/origem, p/nome)
        compilar(p)
        for caso in CASOS:
            r = executar(p, caso)
            print(r.stdout, end='')
            if r.returncode:
                raise SystemExit(r.stderr)
        mutantes = (
            ('sem relatório', 'queda360.h', 'InterlockedExchange(&sRelatorioSalvo, Queda_Gravar());',
             'InterlockedExchange(&sRelatorioSalvo, 1); (void)&Queda_Gravar;', 'jogo'),
            ('registro truncado', 'main360.cpp', '(unsigned __int64)c->Gpr3;', '(unsigned)c->Gpr3;', 'jogo'),
            ('sem flush', 'queda360.h', 'if (!FlushFileBuffers(f)) ok = 0;', '', 'flush-falhou'),
            ('E/S no filtro do áudio', 'main360.cpp', 'sRelatorioThread = CreateThread(NULL, 64 * 1024, Queda_Relatar,',
             'Queda_Gravar(); sRelatorioThread = CreateThread(NULL, 64 * 1024, Queda_Relatar,', 'audio'),
            ('sem apresentação na caixa', 'queda360.h', 'while (!XHasOverlappedIoCompleted(&ov)) apresentar();',
             'while (!XHasOverlappedIoCompleted(&ov)) {}', 'jogo'),
            ('sem caixa', 'main360.cpp', 'saidaDireta = Queda_Mostrar(Saida_Apresentar);',
             'saidaDireta = true; (void)&Queda_Mostrar;', 'jogo'),
            ('desmontagem antes do OK', 'main360.cpp', 'sQuedaTomada ? Saida_Graficos : Saida_Threads, t, 0, NULL);',
             'Saida_Threads, t, 0, NULL);', 'jogo'),
        )
        for i, (nome, arquivo, antes, depois, caso) in enumerate(mutantes):
            m = p/str(i); m.mkdir()
            for cab in arquivos:
                t = (p/cab).read_text()
                if cab == arquivo:
                    assert t.count(antes) == 1, nome
                    t = t.replace(antes, depois)
                (m/cab).write_text(t)
            compilar(m)
            r = executar(m, caso)
            if r.returncode != 1 or 'FALHA:' not in r.stderr:
                raise SystemExit(f'mutante {nome} não acusado: {r.stdout} {r.stderr}')
            print(f'mutante {nome}: acusado: {r.stderr.strip()}')
    print('queda e aviso: tudo certo')


if __name__ == '__main__':
    main()
