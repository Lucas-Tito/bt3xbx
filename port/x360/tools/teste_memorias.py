#!/usr/bin/env python3
"""Exercita a coleta de vários pontos sem jogo, dados ou emulador."""
from pathlib import Path
import os
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
CODIGO = r'''
#include <sys/mman.h>
#include "port/x360/headless/memoria.c"
unsigned gPortVBlanks;
void *gBtlPool;
static unsigned dados[4], chamadas;
void Port_HeapRegion(uint8_t **p, uint32_t *n) { *p = (uint8_t *)dados; *n = sizeof(dados); }
void *__real_BtlPool_Alloc(int s, int n) { return dados; }
void __real_BtlPool_Reset(int s) {}
void __real_BtlPool_Free(int s, void *p) {}
void __real_BtlPool_Init(int a) {}
void __real_BtlPool_Term(void) {}
void __real_Port_StateLog(unsigned v, const void *f, unsigned n) { chamadas++; }
int main(int argc, char **argv) {
    FILE *fp;
    unsigned i;
    void *p = mmap((void *)0x70000000, 0x4000, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) return 3;
    fp = fopen(argv[1], "w");
    fprintf(fp, "%llx %llx\n", (unsigned long long)(uintptr_t)dados, (unsigned long long)sizeof(dados));
    fclose(fp);
    __wrap_BtlPool_Init(0);
    for (i = 1; i <= 4; i++) {
        dados[0] = i;
        __wrap_Port_StateLog(i, 0, 0);
    }
    return chamadas == 4 ? 0 : 4;
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix='teste-memorias-', dir=Path.home()) as tmp:
        d = Path(tmp)
        (d / 'teste.c').write_text(CODIGO)
        exe = d / 'teste'
        subprocess.run(['gcc', '-std=gnu99', '-O2', '-no-pie', '-I', str(RAIZ), str(d / 'teste.c'), '-o', str(exe)], check=True)
        lista = d / 'pontos'
        lista.write_text(f'2 {d}/dois\n4 {d}/quatro\n2 {d}/repetido\n')
        env = {k: v for k, v in os.environ.items() if not k.startswith('BT3_')}
        env['BT3_MEMORIAS'] = str(lista)
        subprocess.run([str(exe), str(exe) + '.mem'], env=env, check=True)
        for ext in ('dump', 'pages', 'pool'):
            assert (d / f'dois.{ext}').read_bytes() == (d / f'repetido.{ext}').read_bytes()
        assert (d / 'dois.dump').read_bytes() != (d / 'quatro.dump').read_bytes()
        assert (d / 'dois.pool').read_text().startswith('POOL 1 2 ')
        assert (d / 'quatro.pool').read_text().startswith('POOL 1 4 ')
        for texto in ('', 'inválido\n', '0 destino\n', '2 destino lixo\n', '2 destino\n' * 129):
            lista.write_text(texto)
            r = subprocess.run([str(exe), str(exe) + '.mem'], env=env, capture_output=True, text=True)
            assert r.returncode == 2 and 'FALHA:' in r.stderr, r
        print('memórias: três destinos, dois momentos, continuidade e cinco listas inválidas: tudo certo')


if __name__ == '__main__':
    main()
