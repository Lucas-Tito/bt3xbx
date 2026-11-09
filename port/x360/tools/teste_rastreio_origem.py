#!/usr/bin/env python3
"""Confere a origem de uma leitura adiada quando a memória é reutilizada."""
from pathlib import Path
import os
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
ENSAIO = r'''
#include "rastreio.c"
static unsigned dados[2];
int main(int argc, char **argv) {
    uint32_t a = (uint32_t)(uintptr_t)&dados[0];
    uint32_t b = (uint32_t)(uintptr_t)&dados[1];
    uint64_t antes = (uint64_t)arquivo("antes") << 40;
    uint64_t depois = (uint64_t)arquivo("depois") << 40;
    uint64_t outro = (uint64_t)arquivo("outro") << 40;
    unsigned i;
    (void)argc;
    /* Ativar depois do construtor evita pedir as tabelas ELF do jogo. */
    setenv("BT3_RASTREIO", argv[2], 1);
    sAtivo = 1;
    marcar(a, antes | 64);
    ler(a, 4, 0x1000);
    /* Outra função reutilizou o endereço antes de a leitura sair do anel. */
    marcar(a, depois | 128);
    if (strcmp(argv[1], "anel") == 0) {
        marcar(b, outro | 256);
        for (i = 0; i < ANEL; i++) ler(b, 4, 0x2000);
    } else if (strcmp(argv[1], "reescrita") == 0) {
        gravar(a, 4, 0x3000);
    }
    despejar();
    return 0;
}
'''


def confere(pasta, modo, ambiente):
    r = subprocess.run([pasta / 'ensaio', modo, pasta / 'rastro.tsv'], env=ambiente,
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stdout + r.stderr)
    linhas = [l.split('\t') for l in (pasta / 'rastro.tsv').read_text().splitlines()]
    usos = {(c[0], int(c[1])): (int(c[2]), int(c[3])) for c in linhas}
    assert usos.get(('antes', 64)) == (4, 1), ('leitura original perdida', usos)
    assert usos.get(('depois', 128), (0, 0))[1] == 0, ('leitura atribuída à memória nova', usos)


def main():
    fonte = (RAIZ / 'port/x360/headless/rastreio.c').read_text()
    with tempfile.TemporaryDirectory(prefix='rastreio-origem-', dir=Path.home()) as tmp:
        p = Path(tmp)
        (p / 'ensaio.c').write_text(ENSAIO)
        ambiente = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
        ambiente['BT3_RASTREIO_BITS'] = '16'
        for variante in ('integro', 'origem-tardia'):
            s = fonte
            if variante != 'integro':
                for indice in ('i', 'k'):
                    s = s.replace(f'anotar_uso(sAnel[{indice}].original,',
                                  f'anotar_uso(origem(sAnel[{indice}].addr),')
                assert s != fonte
            (p / 'rastreio.c').write_text(s)
            subprocess.run(['clang', '-m32', '-std=gnu89', '-O2', '-ffunction-sections',
                            '-fdata-sections', '-Wl,--gc-sections', p / 'ensaio.c',
                            '-o', p / 'ensaio'], check=True)
            for modo in ('anel', 'reescrita', 'fim'):
                try:
                    confere(p, modo, ambiente)
                except AssertionError:
                    assert variante != 'integro', (variante, modo)
                    print('Mutante de origem tardia recusado: ' + modo)
                else:
                    assert variante == 'integro', ('mutante aceito', modo)
                    print('Origem preservada: ' + modo)
    print('Origem das leituras adiadas e controles negativos: tudo certo')


if __name__ == '__main__':
    main()
