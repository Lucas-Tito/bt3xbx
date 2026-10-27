#!/usr/bin/env python3
"""F5: instrumenta uma CÓPIA do gs_draw.c ou extrai o clut_hash real para o teste.
Uso: clut16.py diagnostico|extrai|sabota gs_draw.c [saida]
A instrumentação não muda a chave e roda antes de qualquer acerto do cache.
"""
import pathlib
import sys

modo, nome = sys.argv[1:3]
p = pathlib.Path(nome)
s = p.read_text()
if modo == 'diagnostico':
    a = 'static GsTex texture_get(int ctx) {'
    assert s.count(a) == 1
    s = s.replace(a, '#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)\n'
                  '#include "clut16_diag.h"\n#endif\n' + a)
    a = '    if (sLast != NULL && sLast->tex0 == t0 && sLast->texa == texa && sLast->gen == gen) {'
    assert s.count(a) == 1
    s = s.replace(a, '#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)\n'
                  '    if (bits == 4) clut16_observa(t0, texa, gen, cbp, cpsm);\n#endif\n' + a)
    p.write_text(s)
elif modo == 'extrai':
    macro = next(l for l in s.splitlines() if l.startswith('#define COR_MISTURA'))
    ini = s.index('static uint32_t clut_hash(')
    fim = s.index('\n}\n', ini) + 3
    pathlib.Path(sys.argv[3]).write_text(macro + '\n' + s[ini:fim])
elif modo == 'sabota':
    a = 'COR_MISTURA(h, Gs_VramRead(cbp, 1, cpsm, count == 16 ? i & 7 : i & 15, count == 16 ? i >> 3 : i >> 4));'
    assert s.count(a) == 1, 'a sabotagem não casou'
    p.write_text(s.replace(a, 'COR_MISTURA(h, Gs_VramRead(cbp, 1, cpsm, i & 15, i >> 4));'))
else:
    sys.exit(__doc__)
