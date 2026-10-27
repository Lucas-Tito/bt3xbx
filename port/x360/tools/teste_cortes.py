#!/usr/bin/env python3
"""Extrai o diagnóstico real, prova aninhamento e dreno, e exige os controles negativos."""
import pathlib
import subprocess
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]

def compila_roda(pasta, nome, fonte, compilador):
    arq = pasta / (nome + ('.cpp' if compilador == 'g++' else '.c'))
    arq.write_text(fonte)
    subprocess.run([compilador, '-O2', '-I' + str(RAIZ), str(arq), '-o', str(pasta / nome)], check=True)
    r = subprocess.run([str(pasta / nome)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (pasta / (nome + '.log')).write_bytes(r.stdout)
    return r.returncode

base = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "port/x360/include/gs_cortes.h"
static uint64_t relogio;
#define GS_FAM_TIQUE() relogio
static double Gs_FamMs(uint64_t t) { return (double)t; }
'''
main = r'''
int main(void) {
    setenv("BT3_CORTES", "6960,6960", 1);
    Gs_CortesInicio(6959); assert(!gGsCortesAtivo);
    Gs_CortesInicio(6960); assert(gGsCortesAtivo);
    relogio += 2; Gs_CortesEntra(COR_VIF);
    relogio += 3; Gs_CortesEntra(COR_VU1);
    relogio += 5; Gs_CortesEntra(COR_GIF);
    relogio += 7; Gs_CortesSai();
    relogio += 11; Gs_CortesSai();
    relogio += 13; Gs_CortesSai();
    relogio += 17; Gs_CortesFim();
    assert(sCortesT[COR_DMA] == 19 && sCortesT[COR_VIF] == 16);
    assert(sCortesT[COR_VU1] == 16 && sCortesT[COR_GIF] == 7);
    assert(sCortesChamadas[COR_GIF] == 1 && !gGsCortesAtivo);
    Gs_CortesInicio(6961); assert(!gGsCortesAtivo);
    return 0;
}
'''
impl = (RAIZ / 'port/x360/src/gs/cortes_impl.h').read_text()
gpu = (RAIZ / 'port/x360/src/gs/gs_d3d.cpp').read_text()
a = gpu.index('enum {\n    T_OUTROS, T_COMUM')
b = gpu.index('\nstatic void ret_une(', a)
gpu = gpu[a:b]
pre = r'''
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#define __int64 long long
#define WINAPI
#define D3DCALLBACK_IDLE 1
#define D3DCALLBACK_CPU1 2
typedef unsigned DWORD;
static unsigned long long relogio;
static unsigned long long __mftb() { return ++relogio; }
static void __lwsync() {}
static int Passos_Medindo() { return 1; }
static int cpu, gpu, errado;
static void Diario_Escrever(const char *f, ...) {
    char buf[1000]; va_list a; va_start(a, f); vsnprintf(buf, sizeof(buf), f, a); va_end(a);
    puts(buf);
    if (strstr(buf, "frame_end")) cpu++;
    if (strstr(buf, "total")) gpu++;
    if (!strstr(buf, "quadro 6960")) errado++;
}
struct Quadro { unsigned quadro; } q;
static Quadro *sQ = &q;
struct Dispositivo {
    void InsertCallback(int, void (*f)(DWORD), DWORD i) { f(i); }
    DWORD InsertFence() { return 1; }
    int IsFencePending(DWORD) { return 0; }
    void BlockOnFence(DWORD) { assert(false); }
} dev, *sDev = &dev;
'''
gmain = r'''
int main() {
    sTempo.ligado = sCortesGpu = 1; sTempo.freq = 1;
    sCortesGpuA = sCortesGpuB = 6960;
    for (q.quadro = 6959; q.quadro <= 6962; q.quadro++) {
        tempo_comeca();
        if (q.quadro == 6960) assert(sTempo.quadro);
        else assert(!sTempo.quadro);
        tempo_abre(); tempo_poe(T_COMUM); tempo_fecha_gpu(0); tempo_fecha_cpu(20, 1);
    }
    assert(cpu == 1 && gpu == 1 && errado == 0);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bt3-cortes-') as d:
    p = pathlib.Path(d)
    casos = [('gravacao', base + impl + main, 'gcc', False),
             ('gpu', pre + gpu + gmain, 'g++', False)]
    assert 'sCortesT[etapa] +=' in impl
    casos.append(('gravacao-sem-soma', base + impl.replace('sCortesT[etapa] +=', 'sCortesT[etapa] =') + main, 'gcc', True))
    assert 'sCortesGpuQ[b]' in gpu
    casos.append(('gpu-quadro-errado', pre + gpu.replace('sCortesGpuQ[b]', 'sQ->quadro') + gmain, 'g++', True))
    assert 'if (!sTempo.quadro) return;' in gpu
    casos.append(('gpu-sem-dreno', pre + gpu.replace('    tempo_le(sTempo.buf);', '    if (sQ->quadro <= sCortesGpuB) tempo_le(sTempo.buf);') + gmain, 'g++', True))
    for nome, fonte, cc, negativo in casos:
        rc = compila_roda(p, nome, fonte, cc)
        if (rc != 0) != negativo:
            print((p / (nome + '.log')).read_text())
            raise SystemExit(f'FALHA: {nome}, saída {rc}')
        print(f'{nome}: {"sabotagem acusada" if negativo else "tudo certo"} (saída {rc})')
print('tudo certo')
