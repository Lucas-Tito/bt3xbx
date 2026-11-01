#!/usr/bin/env python3
"""Compila um caso com os leitores reais da transição. Executar os binários sob flock, um por vez.
Saídas: port/build/transicao/{x86,ppc}/teste_transicao. Nenhum dado é incorporado.
"""
from pathlib import Path
import subprocess
import headless

raiz = Path(__file__).resolve().parents[3]
for nome in ('x86', 'ppc'):
    alvo = headless.Alvo(nome, False)
    pasta = raiz / 'port/build/transicao' / nome
    pasta.mkdir(parents=True, exist_ok=True)
    objs = []
    for caminho in ('src/sys/gfxm_b_b.c', 'src/battle/eft_d_b.c', 'src/battle/btl_scene.c',
                    'port/x360/headless/teste_transicao.c'):
        original = raiz / caminho
        src = headless.portsrc.prepare(original)[0] if caminho.startswith('src/') else original
        obj = pasta / (original.stem + '.o')
        cmd = alvo.cc + ['-std=gnu89', '-c', '-O2', '-g', '-ffunction-sections', '-fdata-sections',
                        '-fno-pic', '-fno-strict-aliasing', '-fno-builtin', '-w'] + alvo.soft + [
                            '-include', 'port_libm.h', '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                            '-I' + str(original.parent)]
        r = headless.compile_ee(cmd, src, obj, be=alvo.be)
        if r.returncode:
            raise SystemExit(r.stderr)
        objs.append(obj)
    subprocess.run(alvo.ld + ['-no-pie', '-Wl,--gc-sections', '-o', str(pasta / 'teste_transicao')] +
                   list(map(str, objs)), cwd=raiz, check=True)
    print(nome + ': compilação do caso: tudo certo')
