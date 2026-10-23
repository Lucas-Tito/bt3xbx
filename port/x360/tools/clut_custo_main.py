#!/usr/bin/env python3
"""(temporário, só no WSL) O custo do clut_texture da main: o mesmo envelope do diag_clut.patch (BT3_CLUT_CUSTO), sem a
conferência. Uso: clut_custo_main.py gs_draw.c"""
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
def rep(a, b):
    global s
    assert s.count(a) == 1, a[:60]
    s = s.replace(a, b)
rep('''/* A 256-colour table at `cbp` (32-bit, the GS's index order undone) as a 256 x 1 texture; alpha as stored. */
static GsTex clut_texture(uint32_t cbp) {''', '''static uint64_t sDiagNs, sDiagCham, sDiagLeit;
static void diag_custo_linha(void) {
    fprintf(stderr, "clutdiag custo quadro %u chamadas %llu leituras %llu conferencias 0 (0 diferentes) ns_conferencia 0 ns_clut %llu\\n",
            (unsigned)gGsFrame, (unsigned long long)sDiagCham, (unsigned long long)sDiagLeit, (unsigned long long)sDiagNs);
}
static GsTex clut_texture_real(uint32_t cbp);
static GsTex clut_texture(uint32_t cbp) {
    static int custo = -1;
    static unsigned ultQ = ~0u;
    uint64_t t;
    GsTex r;
    if (custo < 0) {
        custo = getenv("BT3_CLUT_CUSTO") != NULL;
        if (custo) { atexit(diag_custo_linha); }
    }
    t = custo ? gpu_now() : 0;
    r = clut_texture_real(cbp);
    if (custo) {
        sDiagNs += gpu_now() - t;
        sDiagCham++;
        if (gGsFrame % 500 == 0 && gGsFrame != ultQ) { ultQ = gGsFrame; diag_custo_linha(); }
    }
    return r;
}

/* A 256-colour table at `cbp` (32-bit, the GS's index order undone) as a 256 x 1 texture; alpha as stored. */
static GsTex clut_texture_real(uint32_t cbp) {''')
rep('''        lido = 1;
''', '''        lido = 1;
        sDiagLeit++;
''')
rep('''    if (!lido) { /* (as páginas não mudaram desde o hash lembrado: as cores são as que deram esse hash) */
''', '''    if (!lido) { /* (as páginas não mudaram desde o hash lembrado: as cores são as que deram esse hash) */
        sDiagLeit++;
''')
open(p, 'w', encoding='utf-8').write(s)
