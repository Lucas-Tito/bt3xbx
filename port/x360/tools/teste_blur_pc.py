#!/usr/bin/env python3
"""Exercita a política real da cópia do PC, inclusive com marcadores adulterados."""
import pathlib
import re
import subprocess
import tempfile

from blur_pc import RAIZ, preparar_fase2, preparar_cadeias


def conferir(pc, pasta):
    for nome, codigos in (("src/battle/stg_b.c", ("0x80", "0x81")),
                          ("src/sys/gfxm_a.c", ("0x20", "0x21"))):
        texto = (pc / nome).read_text()
        if 'if (Port_DesfoqueNative(' in texto:
            raise RuntimeError("marcador ainda condicionado ao alvo nativo")
        for codigo in codigos:
            if texto.count('Port_GsMarker(%s);' % codigo) != 1:
                raise RuntimeError("marcador ausente ou repetido")
    s = (pc / "port/src/gs/gs_draw.c").read_text()
    baixo = re.search(r'static int lowres_now\(int t\) \{.*?\n\}', s, re.S).group()
    despacho = re.search(r'void GsGpu_Native\(int effect\) \{.*?\n\}', s, re.S).group()
    c = '''#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
static int sInGlowChain, brilho, distancia;
static struct { unsigned fbp; } gsTargets[1] = {{0x150}};
static int Port_DesfoqueNative(const char *s) {
    return strcmp(s,"BT3_DISTANCIA_NATIVA") == 0 ? distancia : brilho;
}
static int target_lowres(unsigned p) { return p == 0x150; }
/* Só os recursos externos são falsos; o despacho inteiro vem do PC atual. */
#define GS_NO_PICTURE 0
#define MAX_DRAWS 4
#define SCALE 2
typedef struct { int native, target, scissor[4]; } GsDraw;
static GsDraw gsDraws[MAX_DRAWS];
static struct { uint64_t scissor[1], frame[1]; } gGs;
static int gsDrawCount, gsFxOff, gsNative, sHudScope, gsAnchor;
static int sEdgeTop, sFillWide, sMenuFree;
static float sNudge, sNudgeY;
static void grade_add(void) {}
static int GsDraw_TargetGet(uint32_t pagina, int cria) {
    (void)pagina; (void)cria; return 0;
}
'''
    c += baixo + '\n' + despacho + '\n'
    c += '''int main(void) {
    int b, d, sharp, cadeia;
    for (b=0;b<2;b++) for(d=0;d<2;d++) for(sharp=0;sharp<2;sharp++) {
        brilho=b; distancia=d;
        for(cadeia=0;cadeia<3;cadeia++) {
            GsGpu_Native(cadeia == 1 ? 0x20 : cadeia == 2 ? 0x80 : 0x21);
            assert(sInGlowChain == cadeia);
            assert(!!lowres_now(0) == (cadeia == 1 ? b : cadeia == 2 ? d : 0));
            assert(!lowres_now(-1));
            /* Sharp deve reconhecer uma cadeia mesmo com b=d=0. */
            assert((sharp && !sInGlowChain) == (sharp && cadeia == 0));
            GsGpu_Native(cadeia == 2 ? 0x81 : 0x21);
            assert(sInGlowChain == 0);
            assert(!lowres_now(0));
            GsGpu_Native(0x22);
            assert(gsAnchor == 9 && sInGlowChain == 0);
            GsGpu_Native(0x23);
            assert(gsAnchor == 10 && sInGlowChain == 0);
        }
    }
    puts("despacho PC real: 24 combinações e menus preservados, tudo certo");
    return 0;
}
'''
    fonte = pasta / 'cadeias.c'
    fonte.write_text(c)
    exe = pasta / 'cadeias'
    subprocess.run(['gcc', '-std=c99', '-pedantic', '-Wall', '-Wextra', '-Werror', str(fonte), '-o', str(exe)], check=True)
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
        antes = [(pc / nome).read_bytes() for nome in nomes]
        preparar_cadeias(pc)
        if antes != [(pc / nome).read_bytes() for nome in nomes]:
            raise RuntimeError('instrumentação repetida alterou a cópia')
        print('migração e repetição da instrumentação preservadas', flush=True)
        # O mesmo verificador deve detectar a seleção de resolução pela cadeia errada.
        p = pc / 'port/src/gs/gs_draw.c'
        original = p.read_text()
        # Reproduz a colisão real que o teste do bloco isolado não enxergava.
        p.write_text(original.replace('effect == 0x80', 'effect == 0x22').replace('effect == 0x81', 'effect == 0x23'))
        try:
            conferir(pc, pasta)
        except subprocess.CalledProcessError:
            print('negativo da colisão com menus recusado', flush=True)
        else:
            raise RuntimeError('aceitou marcadores consumidos pelos menus')
        p.write_text(original)
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
