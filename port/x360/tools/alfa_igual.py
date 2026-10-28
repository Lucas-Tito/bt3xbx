#!/usr/bin/env python3
"""F5, seção 36: o que a build normal compila dos fontes do alfa é o texto da base, token a token.

    alfa_igual.py BASE            (na raiz da worktree; BASE: a worktree da main sem o alfa)
    alfa_igual.py BASE --mutantes (o mesmo verificador tem de recusar cópias estragadas)

Tira de cada fonte os blocos de GS_ALFA como a build normal os vê (GS_ALFA indefinido: o #ifdef some, o #ifndef fica), e a
definição da chave no gs_draw.h com o comentário dela, e exige o texto da BASE byte a byte em gs_draw.c, gs_draw.h e
gs_d3d.cpp, os três que o console compila. Não substitui a comparação dos objetos (XDK, headless): delimita o fonte.
"""
import argparse, re, sys, tempfile
from pathlib import Path

FONTES = ('port/src/gs/gs_draw.c', 'port/src/gs/gs_draw.h', 'port/x360/src/gs/gs_d3d.cpp')


def normal(texto):
    out, pilha = [], []  # (bloco do alfa?, mantém?)
    for x in texto.split('\n'):
        t = x.strip()
        vivo = all(m for _, m in pilha)
        if t.startswith('#if'):
            alfa = t in ('#ifdef GS_ALFA', '#ifndef GS_ALFA') or t.startswith('#if defined(BT3_ALFA_ORIGEM)')
            pilha.append((alfa, (t == '#ifndef GS_ALFA') if alfa else True))
            if not alfa and vivo: out.append(x)
        elif t.startswith('#el') and pilha:
            alfa, m = pilha[-1]
            assert not (alfa and t.startswith('#elif')), 'elif num bloco do alfa'
            if alfa: pilha[-1] = (alfa, not m)
            elif vivo: out.append(x)
        elif t.startswith('#endif') and pilha:
            alfa, _ = pilha.pop()
            if not alfa and all(m for _, m in pilha): out.append(x)
        elif vivo:
            out.append(x)
    assert not pilha, 'bloco sem #endif'
    s = '\n'.join(out)
    return re.sub(r'/\* \(F5, seção 36\) BT3_ALFA_ORIGEM: o alfa exato calculado.*?\*/\n', '', s, count=1, flags=re.S)


def confere(base, raiz):
    for f in FONTES:
        a, b = (base / f).read_text(), normal((raiz / f).read_text())
        if a != b:
            la, lb = a.split('\n'), b.split('\n')
            i = next((k for k in range(min(len(la), len(lb))) if la[k] != lb[k]), min(len(la), len(lb)))
            raise AssertionError(f'{f}: difere da base na linha {i + 1} do texto normal')
    return len(FONTES)


def mutantes(base, raiz):
    estragos = [('port/src/gs/gs_draw.c', '#ifdef GS_ALFA\n    sAlfaCpu |= a > 0x80;\n#endif\n',
                 '#ifdef GS_ALFA\n    sAlfaCpu |= a > 0x80;\n#endif\n    (void)0;\n'),
                ('port/src/gs/gs_draw.h', '#ifdef GS_ALFA\n    const unsigned char *alfa;', '    const unsigned char *alfa;\n#ifdef GS_ALFA\n'),
                ('port/x360/src/gs/gs_d3d.cpp', '#ifndef GS_ALFA\nstatic void alfa_alto(void) {', '#ifdef GS_ALFA\nstatic void alfa_alto(void) {'),
                ('port/x360/src/gs/gs_d3d.cpp', 'alto = u->color0[3] > 128.0f;', 'alto = u->color0[3] >= 128.0f;')]
    for f, a, b in estragos:
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            for g in FONTES:
                (t / g).parent.mkdir(parents=True, exist_ok=True)
                s = (raiz / g).read_text()
                if g == f:
                    assert a in s, (f, a[:40])
                    s = s.replace(a, b, 1)
                (t / g).write_text(s)
            try:
                confere(base, t)
            except AssertionError as e:
                print(f'mutante recusado: {f}: {e}')
            else:
                raise AssertionError(f'mutante aceito em {f}')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('base', type=Path); p.add_argument('--mutantes', action='store_true')
    a = p.parse_args()
    raiz = Path(__file__).resolve().parents[3]
    try:
        n = confere(a.base, raiz)
        if a.mutantes:
            mutantes(a.base, raiz)
        print(f'{n} fontes do console: a build normal é o texto da base; tudo certo')
    except (OSError, AssertionError) as e:
        p.exit(1, f'FALHA: {e}\n')
