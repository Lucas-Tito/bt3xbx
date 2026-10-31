#!/usr/bin/env python3
"""Exercita os leitores reais e a parada do som do 360 com API simulada e mutantes."""
import pathlib
import shutil
import subprocess
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]


def corpo(fonte, nome):
    inicio = fonte.index(nome)
    fim = fonte.index('{', inicio) + 1
    nivel = 1
    while nivel:
        nivel += (fonte[fim] == '{') - (fonte[fim] == '}')
        fim += 1
    return fonte[inicio:fim]


def main():
    with tempfile.TemporaryDirectory() as d:
        pasta = pathlib.Path(d)
        (pasta / 'xtl.h').write_text('/* API fornecida pelo ensaio */\n')
        for nome in ('blocos.c', 'blocos.h'):
            shutil.copyfile(RAIZ / 'port/x360/src/f2' / nome, pasta / nome)
        fontes = {
            'BLOCOS': ('blocos.c', (pasta / 'blocos.c').read_text(), ('normal', 'atraso', 'trava'),
                       'b->sair = 1;', 'b->sair = 0;', 'normal'),
            'FLUXO': ('parar.inc', corpo((RAIZ / 'port/x360/src/som/fluxo.c').read_text(), 'void Fluxo_Parar(void)'),
                      ('normal', 'atraso', 'terminando'), 'sSair = 1;', 'sSair = 0;', 'normal'),
            'SOM': ('parar.inc', corpo((RAIZ / 'port/x360/src/som/som360.cpp').read_text(), 'extern "C" void Som360_Parar(void)'),
                    ('normal', 'vigia', 'misturador'), 'if (!ok) {', 'if (0) { (void)ok;', 'misturador'),
        }
        for tipo, (arquivo, fonte, casos, antes, depois, negativo) in fontes.items():
            for mutante in (False, True):
                assert fonte.count(antes) >= 1
                (pasta / arquivo).write_text(fonte.replace(antes, depois, 1) if mutante else fonte)
                subprocess.run(['g++', '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                                '-D_XBOX', f'-DTESTE_{tipo}', '-I', str(pasta), '-I', str(RAIZ / 'port/x360/src'),
                                str(RAIZ / 'port/x360/tools/teste_paradas360.cpp'), '-o', str(pasta / 'teste')], check=True)
                for caso in (negativo,) if mutante else casos:
                    r = subprocess.run([str(pasta / 'teste'), caso], text=True, capture_output=True, timeout=3)
                    if mutante:
                        assert r.returncode == 1 and 'FALHA:' in r.stderr, (tipo, r.stdout, r.stderr)
                        print(f'mutante {tipo}: acusado: {r.stderr.strip()}')
                    else:
                        assert r.returncode == 0, (tipo, caso, r.stderr)
                        print(f'{tipo}: {r.stdout.strip()}')
        (pasta / 'interromper.inc').write_text(corpo((RAIZ / 'port/x360/src/f2/main360.cpp').read_text(), 'extern \"C\" void Port_GsInterromperJogo360(void)'))
        core = (RAIZ / 'port/src/gs/gs_core.c').read_text()
        inicio = core.index('            /* Na queda, a main espera ambos os handles.')
        fim = core.index('            gGsThreadEstado = 0;', inicio)
        parada = core[inicio:fim].replace('#endif', '')
        fonte = corpo((RAIZ / 'port/src/gs/gs_core.c').read_text(), 'static void render_wait(void) {')
        for mutante in ('nenhum', 'checkpoint', 'handle'):
            antes = '            Port_GsInterromperJogo360();'
            assert fonte.count(antes) == 1
            (pasta / 'espera.inc').write_text(fonte.replace(antes, '', 1) if mutante == 'checkpoint' else fonte)
            guarda = '            if (sParar) {\n                gGsDesenhoParar = NULL;\n            }'
            assert parada.count(guarda) == 1
            (pasta / 'parada.inc').write_text(parada.replace(guarda, '', 1) if mutante == 'handle' else parada)
            subprocess.run(['g++', '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror', '-DPORT_X360',
                            '-I', str(pasta), str(RAIZ / 'port/x360/tools/teste_espera_saida360.cpp'),
                            '-o', str(pasta / 'espera')], check=True)
            r = subprocess.run([str(pasta / 'espera')], capture_output=True, text=True, timeout=3)
            if mutante != 'nenhum':
                assert r.returncode == 1 and 'FALHA:' in r.stderr, r
                print(f'mutante {mutante} do GS: acusado: {r.stderr.strip()}')
            else:
                assert r.returncode == 0, r.stderr
                print(r.stdout.strip())
    print('paradas do 360: tudo certo')


if __name__ == '__main__':
    main()
