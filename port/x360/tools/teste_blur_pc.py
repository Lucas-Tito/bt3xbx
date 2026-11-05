#!/usr/bin/env python3
"""Exercita a política real da cópia do PC, inclusive com marcadores adulterados."""
import pathlib
import re
import subprocess
import tempfile

from blur_pc import RAIZ, preparar_fase2, preparar_cadeias


def conferir(pc, pasta):
    for nome, codigos in (("src/battle/stg_b.c", ("0x22", "0x23")),
                          ("src/sys/gfxm_a.c", ("0x20", "0x21"))):
        texto = (pc / nome).read_text()
        if 'if (Port_DesfoqueNative(' in texto:
            raise RuntimeError("marcador ainda condicionado ao alvo nativo")
        for codigo in codigos:
            if texto.count('Port_GsMarker(%s);' % codigo) != 1:
                raise RuntimeError("marcador ausente ou repetido")
    s = (pc / "port/src/gs/gs_draw.c").read_text()
    baixo = re.search(r'static int lowres_now\(int t\) \{.*?\n\}', s, re.S).group()
    marcador = re.search(r'    if \(effect == 0x20.*?\n    \}', s, re.S).group()
    c = '''#include <assert.h>
#include <string.h>
#include <stdio.h>
static int sInGlowChain, brilho, distancia;
static struct { unsigned fbp; } gsTargets[1] = {{0x150}};
static int Port_DesfoqueNative(const char *s) {
    return strcmp(s,"BT3_DISTANCIA_NATIVA") == 0 ? distancia : brilho;
}
static int target_lowres(unsigned p) { return p == 0x150; }
'''
    c += baixo + '\nstatic void marca(int effect) {\n' + marcador + '\n}\n'
    c += '''int main(void) {
    int b, d, sharp, cadeia;
    for (b=0;b<2;b++) for(d=0;d<2;d++) for(sharp=0;sharp<2;sharp++) {
        brilho=b; distancia=d;
        for(cadeia=0;cadeia<3;cadeia++) {
            marca(cadeia == 1 ? 0x20 : cadeia == 2 ? 0x22 : 0x21);
            assert(sInGlowChain == cadeia);
            assert(!!lowres_now(0) == (cadeia == 1 ? b : cadeia == 2 ? d : 0));
            assert(!lowres_now(-1));
            /* Sharp deve reconhecer uma cadeia mesmo com b=d=0. */
            assert((sharp && !sInGlowChain) == (sharp && cadeia == 0));
            marca(cadeia == 2 ? 0x23 : 0x21);
            assert(sInGlowChain == 0);
            assert(!lowres_now(0));
        }
    }
    puts("cadeias PC: 24 combinações, tudo certo");
    return 0;
}
'''
    fonte = pasta / 'cadeias.c'
    fonte.write_text(c)
    exe = pasta / 'cadeias'
    subprocess.run(['gcc', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror', str(fonte), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def main():
    with tempfile.TemporaryDirectory() as temp:
        pasta = pathlib.Path(temp)
        pc = pasta / 'pc'
        nomes = ['src/battle/stg_b.c', 'src/sys/gfxm_a.c', 'port/src/gs/gs_draw.c']
        for nome in nomes:
            p = pc / nome
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(subprocess.check_output(['git', 'show', 'troca/ref/upstream-main:' + nome], cwd=RAIZ))
        (pc / 'port/src/blur_teste.c').write_text('/* cópia do teste */\n')
        preparar_fase2(pc)
        try:
            conferir(pc, pasta)
        except RuntimeError as erro:
            print('negativo do adaptador anterior recusado:', erro, flush=True)
        else:
            raise RuntimeError('aceitou marcadores condicionados')
        preparar_cadeias(pc)
        conferir(pc, pasta)
        # O mesmo verificador deve detectar a seleção de resolução pela cadeia errada.
        p = pc / 'port/src/gs/gs_draw.c'
        original = p.read_text()
        p.write_text(original.replace('sInGlowChain == 2 && Port_DesfoqueNative', 'sInGlowChain == 1 && Port_DesfoqueNative', 1))
        try:
            conferir(pc, pasta)
        except subprocess.CalledProcessError:
            print('negativo de seleção da cadeia recusado', flush=True)
        else:
            raise RuntimeError('aceitou resolução da cadeia errada')
        p.write_text(original)
        conferir(pc, pasta)


if __name__ == '__main__':
    main()
