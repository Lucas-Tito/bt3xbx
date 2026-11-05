#!/usr/bin/env python3
"""Política das cadeias e bits altos. Aceita header alternativo para mutantes."""
import os
import pathlib
import subprocess
import sys
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]


def conferir(header):
    with tempfile.TemporaryDirectory() as t:
        t = pathlib.Path(t)
        (t / 'teste.c').write_text('''#include <assert.h>
#include <stdio.h>
#include "desfoque_pc.h"
int main(void) {
    DesfoquePc p = {0,0,0,0,0,0};
    int c, n;
    for (c=0; c<3; c++) {
        p.cadeia=c;
        assert(!desfoque_pc_baixo(&p,0x150));
        assert(!desfoque_pc_bloco(&p,1,0,0,0,0));
    }
    p.distancia=1; p.cadeia=2;
    assert(desfoque_pc_baixo(&p,0x150));
    assert(!desfoque_pc_baixo(&p,0));
    assert(!desfoque_pc_baixo(&p,0x16C));
    p.cadeia=0;
    assert(!desfoque_pc_baixo(&p,0x150));
    p.cadeia=1;
    assert(!desfoque_pc_baixo(&p,0x150));
    p.brilho=1;
    assert(desfoque_pc_baixo(&p,0x1F8));
    p.media=1;
    assert(!desfoque_pc_bloco(&p,1,1,0,0,1));
    p.cadeia=2;
    assert(desfoque_pc_bloco(&p,1,1,0,0,1));
    assert(!desfoque_pc_bloco(&p,0,1,0,0,1));
    assert(!desfoque_pc_bloco(&p,1,1,0,1,1));
    p.cadeia=0; p.blocos=1;
    assert(desfoque_pc_bloco(&p,1,0,0,0,0));
    assert(!desfoque_pc_bloco(&p,1,0,1,0,0));
    assert(!desfoque_pc_bloco(&p,1,1,1,0,0));
    p.sharp=1;
    assert(!desfoque_pc_bloco(&p,1,0,0,0,0));
    for (n=1; n<=2; n++) {
        p.cadeia=n;
        assert(desfoque_pc_bloco(&p,1,0,0,0,0));
    }
    puts("política: tudo certo");
    return 0;
}
''')
        (t / 'desfoque_pc.h').write_bytes(header.read_bytes())
        subprocess.run(['gcc','-std=c89','-pedantic','-Wall','-Wextra','-Werror',str(t/'teste.c'),'-o',str(t/'teste')],check=True)
        subprocess.run([str(t/'teste')],check=True)
        (t / 'fx.c').write_text('''#include <stdio.h>
int Port_DesfoqueOff(int);
int Port_ClimaQuantidade(int);
int main(void) {
    printf("fx=%d,%d,%d\\n",Port_DesfoqueOff(1),Port_DesfoqueOff(6),Port_ClimaQuantidade(5));
    return 0;
}
''')
        subprocess.run(['gcc','-std=c89','-pedantic','-Wall','-Wextra','-Werror',str(t/'fx.c'),str(RAIZ/'port/x360/headless/desfoque.c'),'-o',str(t/'fx')],check=True)
        for mask in range(256):
            env={k:v for k,v in os.environ.items() if not k.startswith('BT3_SEM_')}
            env['BT3_FX_OFF']=str(mask)
            saida=subprocess.check_output([str(t/'fx')],env=env,text=True).splitlines()[-1]
            esperado='fx=%d,%d,%d'%(bool(mask&32),bool(mask&64),3 if mask&128 else 5)
            assert saida==esperado,(mask,saida,esperado)
        print('máscara: 256 combinações, tudo certo')


if __name__ == '__main__':
    conferir(pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else RAIZ/'port/x360/include/desfoque_pc.h')
