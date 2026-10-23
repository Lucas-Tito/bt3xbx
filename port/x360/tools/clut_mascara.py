#!/usr/bin/env python3
"""(F5, 09.md, a correção da colisão das tabelas; temporário, só no WSL) No gpu_falso.c: a linha "normal_semtabela quadro N H",
o hash normal do quadro com o conteúdo das texturas de 256 x 1 (as tabelas do clut_texture) trocado por uma constante.
Duas builds com essa linha igual em todo quadro só diferem, nos desenhos, no conteúdo das tabelas. Uso: clut_mascara.py gpu_falso.c"""
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
def rep(a, b):
    global s
    assert s.count(a) == 1, a[:60]
    s = s.replace(a, b)
rep('''static int fake_init(void) { return 1; }''', '''static uint8_t *sTabela;
static uintptr_t sTabelaCap;
static int fake_init(void) { return 1; }''')
rep('''    sConteudo[sNextTex - 16] = 0;
''', '''    sConteudo[sNextTex - 16] = 0;
    if (sNextTex - 16 >= sTabelaCap) {
        sTabelaCap = (sNextTex - 16 + 1) * 2;
        sTabela = (uint8_t *)realloc(sTabela, sTabelaCap);
    }
    sTabela[sNextTex - 16] = w == 256 && h == 1;
''')
rep('''/* (degrau 2) o quadro que o fim do quadro lê''', '''static uint64_t normal_semtabela(const GsDraw *draws, uint32_t n) {
    uint64_t d = 1469598103934665603ull;
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        GsDraw g = draws[i];
        const uint32_t *w;
        if (!g.tex_is_target && g.tex >= 16 && g.tex - 16 < sConteudoN) {
            g.tex = sTabela[g.tex - 16] ? (GsTex)0x7AB1E : (GsTex)(sConteudo[g.tex - 16] ^ sConteudo[g.tex - 16] >> 32);
        }
        w = (const uint32_t *)&g;
        for (k = 0; k < (uint32_t)(sizeof(GsDraw) / 4); k++) {
            d = (d ^ w[k]) * 1099511628211ull;
        }
    }
    return d;
}

/* (degrau 2) o quadro que o fim do quadro lê''')
rep('''        fprintf(stderr, "gpu_falso: normal quadro %u %016llx\\n", q.quadro, (unsigned long long)n);
''', '''        fprintf(stderr, "gpu_falso: normal quadro %u %016llx\\n", q.quadro, (unsigned long long)n);
        if (!q.dividido) {
            n = 1469598103934665603ull;
            n = (n ^ sSecao[S_CONTAGENS]) * 1099511628211ull;
            n = (n ^ normal_semtabela(q.draws, q.drawCount)) * 1099511628211ull;
            for (i = S_VERTICES; i < S_N; i++) {
                n = (n ^ sSecao[i]) * 1099511628211ull;
            }
            fprintf(stderr, "gpu_falso: normal_semtabela quadro %u %016llx\\n", q.quadro, (unsigned long long)n);
        }
''')
open(p, 'w', encoding='utf-8').write(s)
