#!/usr/bin/env python3
"""Exige as três provas positivas e sua reprovação em cópias adulteradas."""
from pathlib import Path
import subprocess
import sys
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
CHECA = RAIZ / 'port/x360/tools/confere_limites_xdk.py'


def roda(args, falha=None):
    r = subprocess.run([sys.executable, str(CHECA), *map(str, args)], text=True, capture_output=True)
    if falha is None:
        assert r.returncode == 0, r.stdout + r.stderr
        print(r.stdout, end='')
    else:
        assert r.returncode == 1 and falha in r.stdout + r.stderr, r.stdout + r.stderr
        print('Cópia adulterada recusada: ' + falha)


def main():
    if len(sys.argv) != 4:
        raise SystemExit('Uso: teste_limites_xdk.py PASTA_C89 PASTA_OBJ_PPC PROGRAMA_PPC_GPU')
    c89, objetos, ppc = map(Path, sys.argv[1:])
    roda(['c89', c89])
    roda(['matematica', objetos])
    roda(['layout', ppc])
    with tempfile.TemporaryDirectory(prefix='bt4-limites-', dir=Path.home()) as tmp:
        pasta = Path(tmp)
        # O defeito de inicialização por valor de struct e o de ponteiro local.
        gs = (c89 / 'gs_gs_draw.c').read_text()
        f = pasta / 'gs_gs_draw.c'
        for extra in (
            '\nvoid negativo(void) { Vtx w[3]; Vtx a[3] = {w[0],w[1],w[2]}; (void)a; }\n',
            '\nvoid negativo(void) { Vtx w[3]; const Vtx *a[3] = {&w[0],&w[1],&w[2]}; (void)a; }\n',
        ):
            f.write_text(gs + extra)
            roda(['c89', pasta], 'initializer element is not computable at load time')
        f.write_text(gs)
        roda(['c89', pasta])
        fonte = (RAIZ / 'port/x360/src/f2/float360.c').read_text()
        assert fonte.count('int Port_FastMtxApply(') == 1
        f = pasta / 'float360.c'
        f.write_text(fonte.replace('#include "../../include/vu0_nativo.h"', '#include "vu0_nativo.h"').replace('int Port_FastMtxApply(', 'int Port_FastMtxApply_retirado('))
        roda(['matematica', objetos, '--float-fonte', f], 'VU0 sem provedor: Port_FastMtxApply')
        f.write_text(fonte.replace('#include "../../include/vu0_nativo.h"', '#include "vu0_nativo.h"'))
        roda(['matematica', objetos, '--float-fonte', f])
        fonte = (RAIZ / 'port/x360/src/f2/vu0_nativo.c').read_text()
        assert fonte.count('int Vn_MtxApply(') == 1
        f = pasta / 'vu0_nativo.c'
        f.write_text(fonte.replace('#include "../../include/vu0_nativo.h"', '#include "vu0_nativo.h"')
                     .replace('int Vn_MtxApply(', 'int Vn_MtxApply_retirado('))
        roda(['matematica', objetos, '--nativo-fonte', f], 'VU0 sem provedor: Vn_MtxApply')
        fonte = (RAIZ / 'port/x360/src/gs/gs_d3d.cpp').read_text()
        assert fonte.count('sizeof(GsDraw) != 128') == 1
        f = pasta / 'gs_d3d.cpp'
        f.write_text(fonte.replace('sizeof(GsDraw) != 128', 'sizeof(GsDraw) != 124'))
        roda(['layout', ppc, '--backend', f], 'backend espera 124; PPC tem [128]')
        f.write_text(fonte)
        roda(['layout', ppc, '--backend', f])
    print('Limites do XDK e controles negativos: tudo certo')


if __name__ == '__main__':
    main()
