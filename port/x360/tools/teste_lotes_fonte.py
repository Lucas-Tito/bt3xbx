#!/usr/bin/env python3
"""F5, seção 38: o BT3_GS_LOTES não pode mudar o que conta no plano da EDRAM, só quando.

O lotes_guarda_plano (gs_d3d.cpp, só na build com a chave) refaz no fim do quadro o laço do ed_plano que conta as
entradas, a vaga grande e até onde cada alvo desenha. Os dois laços têm de ser iguais letra por letra; e a regra da
vaga grande e da tesoura no lotes_vaga, a mesma do laço. Sem jogo, XDK ou emulador; as cópias estragadas são recusadas.

    teste_lotes_fonte.py [--fonte port/x360/src/gs/gs_d3d.cpp]
"""
import argparse
from pathlib import Path
import re
import sys

RAIZ = Path(__file__).resolve().parents[3]


def bloco(s, inicio):
    """o texto de `inicio` até a chave que fecha o bloco aberto nele"""
    i = s.index(inicio)
    j = s.index('{', i)
    nivel = 0
    for k in range(j, len(s)):
        nivel += {'{': 1, '}': -1}.get(s[k], 0)
        if nivel == 0:
            return s[i:k + 1]
    raise ValueError(f'bloco sem fim: {inicio}')


def funcao(s, nome):
    m = re.search(r'^static [^\n(]*\b' + nome + r'\(', s, re.M)
    if not m:
        raise ValueError(f'função {nome} ausente')
    return bloco(s, m.group(0))


def confere(s):
    plano = funcao(s, 'ed_plano')
    guarda = funcao(s, 'lotes_guarda_plano')
    laco = 'for (n = 0; n < q->drawCount; n++) {'
    a, b = bloco(plano, laco), bloco(guarda, laco)
    if a != b:
        linha = next((x, y) for x, y in zip(a.splitlines() + [''], b.splitlines() + ['']) if x != y)
        raise ValueError(f'o laço do lotes_guarda_plano difere do ed_plano: {linha[0].strip()!r} != {linha[1].strip()!r}')
    vaga = funcao(s, 'lotes_vaga')
    regra_z = 'if (!sPipes[d->pipeline].fx && (sPipes[d->pipeline].zwrite || sPipes[d->pipeline].ztst != 1)) {'
    regra_ret = ['x0 = d->scissor[0];', 'y0 = d->scissor[1];', 'x1 = d->scissor[0] + d->scissor[2];',
                 'y1 = d->scissor[1] + d->scissor[3];', 'esc_prende(&x0, &y0, &x1, &y1, sAreaW, sAreaH);',
                 'esc_px(x1, sEscN, sEscD)', 'esc_px(y1, sEscN, sEscD)']
    for r in [regra_z] + regra_ret:
        if r not in a or r not in vaga:
            raise ValueError(f'a regra {r!r} não está igual no laço e no lotes_vaga')
    # cabe na pequena: a vaga tem (ext + 7) & ~7 >= ext, então a tesoura até a largura e a altura dela, inclusive
    if '(esc_px(x1, sEscN, sEscD) > g->w || esc_px(y1, sEscN, sEscD) > g->h)' not in vaga or \
            'w = (ext[t].x1 + 7) & ~7;' not in plano or 'h = (ext[t].y1 + 7) & ~7;' not in plano:
        raise ValueError('o lotes_vaga não mede a tesoura contra a vaga como o ed_plano a dimensiona')
    nat = 'if (d->native == 1 || d->native == 3 || d->native == 4 || d->native >= 100) {'
    if nat not in a or 'LOTES_GRANDE(t, cur);' not in funcao(s, 'nat_entra'):
        raise ValueError('as nativas e o filme sem a vaga grande no lotes')
    # na build normal, as marcas do lotes são vazias
    if not re.search(r'#else\n#define LOTES_VAGA\(d, cur\) \(\(void\)0\)\n#define LOTES_GRANDE\(t, cur\) \(\(void\)0\)\n'
                     r'#define LOTES_GUARDA\(q\) \(\(void\)0\)\n#endif', s):
        raise ValueError('as marcas do lotes não são vazias sem a chave')
    return a.count('\n') + 1


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--fonte', type=Path, default=RAIZ / 'port/x360/src/gs/gs_d3d.cpp')
    a = p.parse_args()
    s = a.fonte.read_text()
    try:
        n = confere(s)
    except ValueError as e:
        print(f'FALHA: {e}')
        sys.exit(1)
    print(f'o laço do plano igual nos dois lugares ({n} linhas), as regras do lotes_vaga iguais às dele')
    estragos = {
        'laço com outra regra de z': ('ztst != 1)) {\n            grande[t] = 1;\n        }\n        x0', 'ztst > 1)) {\n            grande[t] = 1;\n        }\n        x0', 1),
        'vaga com outra tesoura': ('(esc_px(x1, sEscN, sEscD) > g->w', '(esc_px(x1, sEscN, sEscD) >= g->w', 0),
        'nativa sem vaga grande': ('    LOTES_GRANDE(t, cur);', '', 0),
        'marca na build normal': ('#define LOTES_VAGA(d, cur) ((void)0)', '#define LOTES_VAGA(d, cur) lotes_vaga(d, cur)', 0),
        'laço sem o filme': ('d->native == 4 || d->native >= 100) {\n                grande[t] = 1; /* (as nativas e o filme entram no alvo e pedem a vaga grande) */\n                ent[t] += t != cur;\n                cur = t;\n            }\n            continue;\n        }\n        if (d->vu && sVuIni[n] == ~0u) {\n            continue;\n        }\n        ent[t] += t != cur;\n        cur = t;\n        if (!sPipes[d->pipeline].fx && (sPipes[d->pipeline].zwrite || sPipes[d->pipeline].ztst != 1)) {\n            grande[t] = 1;\n        }\n        x0 = d->scissor[0];\n        y0 = d->scissor[1];\n        x1 = d->scissor[0] + d->scissor[2];\n        y1 = d->scissor[1] + d->scissor[3];\n        esc_prende(&x0, &y0, &x1, &y1, sAreaW, sAreaH);\n        if (x1 > x0 && y1 > y0) {\n            ret_une(&ext[t], 0, 0, esc_px(x1, sEscN, sEscD), esc_px(y1, sEscN, sEscD));\n        }\n    }\n    sLtPlano', 'd->native == 4) {\n                grande[t] = 1; /* (as nativas e o filme entram no alvo e pedem a vaga grande) */\n                ent[t] += t != cur;\n                cur = t;\n            }\n            continue;\n        }\n        if (d->vu && sVuIni[n] == ~0u) {\n            continue;\n        }\n        ent[t] += t != cur;\n        cur = t;\n        if (!sPipes[d->pipeline].fx && (sPipes[d->pipeline].zwrite || sPipes[d->pipeline].ztst != 1)) {\n            grande[t] = 1;\n        }\n        x0 = d->scissor[0];\n        y0 = d->scissor[1];\n        x1 = d->scissor[0] + d->scissor[2];\n        y1 = d->scissor[1] + d->scissor[3];\n        esc_prende(&x0, &y0, &x1, &y1, sAreaW, sAreaH);\n        if (x1 > x0 && y1 > y0) {\n            ret_une(&ext[t], 0, 0, esc_px(x1, sEscN, sEscD), esc_px(y1, sEscN, sEscD));\n        }\n    }\n    sLtPlano', 0),
    }
    for nome, (de, para, ocorr) in estragos.items():
        if de not in s:
            print(f'FALHA: o estrago {nome} não acha o trecho')
            sys.exit(1)
        if ocorr:  # a primeira ocorrência (a do ed_plano)
            ruim = s.replace(de, para, 1)
        else:
            k = s.rindex(de)
            ruim = s[:k] + para + s[k + len(de):]
        try:
            confere(ruim)
        except ValueError as e:
            print(f'estrago recusado: {nome} ({e})')
        else:
            print(f'FALHA: estrago aceito: {nome}')
            sys.exit(1)
    print('tudo certo na fonte do lotes')


if __name__ == '__main__':
    main()
