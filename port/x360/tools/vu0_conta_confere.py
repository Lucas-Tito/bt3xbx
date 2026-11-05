#!/usr/bin/env python3
"""Confere as quatro contagens, sem exigir igualdade das rotinas escalares.

Os rastros e a prova de memória continuam sendo critérios independentes.
As exceções abaixo são auxiliares de conversão, ABI e recuo, cujas chamadas
dependem do compilador ou dos operandos excepcionais. Todas continuam no JSON.
"""
import json
import pathlib
import sys

ESCALARES = {
    'pc_vu0_a/RefVu0_ItofBits', 'pc_vu0_a/RefVu0_LtBits',
    'pc_vu0_a/RefVu0_SubBits', 'pc_vu0_b/f_bits', 'pc_vu0_b/f_from',
    'pc_vu0_b/f_add', 'pc_vu0_b/f_mul',
    'pc_vu0_b/vn_vec4_add_lento', 'pc_vu0_b/vn_vec4_scale_lento',
}
NUCLEOS = ('vu0_nativo/Vn_Op4', 'vu0_nativo/Vn_Bc4',
           'vu0_nativo/Vn_MtxApply', 'ponte/Port_FastMtxApply')

def conferir(rs):
    base = rs['x86', 1]
    for (a, n), r in rs.items():
        if len(r['quadros']) != r['passos']: raise ValueError('passos incompletos')
        if r['passos'] != base['passos'] or r['totais'].keys() != base['totais'].keys():
            raise ValueError('inventário ou número de passos diferente')
        if not r['janelas'] or sum(j['passos'] for j in r['janelas']) != r['passos']:
            raise ValueError('janelas ausentes ou incompletas')
        for b, q in zip(base['quadros'], r['quadros']):
            if (b['passo'], b['vb']) != (q['passo'], q['vb']):
                raise ValueError('fronteira do passo diferente')
            for nome in base['totais']:
                if nome in ESCALARES or nome.startswith(('pc_softfloat_ps2/', 'ponte/')):
                    if nome != 'ponte/Port_FastMtxApply': continue
                if b['contas'].get(nome, [0])[0] != q['contas'].get(nome, [0])[0]:
                    raise ValueError(f'chamadas diferentes em {a}/{n}, passo {q["passo"]}: {nome}')
            matriz = q['contas'].get('pc_vu0_b/mtx_apply', [0])[0]
            for nome in ('ponte/Port_FastMtxApply', 'vu0_nativo/Vn_MtxApply'):
                if q['contas'].get(nome, [0])[0] != matriz:
                    raise ValueError('matriz sem passar pelo gancho: '+nome)
            for nome in NUCLEOS:
                v = q['contas'].get(nome, [0]*8)
                if v[0] != sum(v[1:]) or v[2]: raise ValueError('retornos não fecham')
                if not n and v != [v[0], 0, 0, v[0], 0, 0, 0, 0]:
                    raise ValueError('núcleo não desligou: '+nome)
        for nome in NUCLEOS:
            if n and not r['totais'][nome][1]: raise ValueError('núcleo não ligou: '+nome)
        if r['janelas'] != base['janelas']:
            # O número, o tamanho e o fechamento têm de coincidir; os totais
            # escalares e os motivos conservam as diferenças medidas.
            campos = lambda j: [(x['janela'], x['passos'], x['vb_fim']) for x in j]
            if campos(r['janelas']) != campos(base['janelas']):
                raise ValueError('janelas diferentes')

def main():
    p = pathlib.Path(sys.argv[1])
    rs = {(a,n): json.loads((p/f'conta-{a}-{n}.json').read_text())
          for a in ('x86','ppc') for n in (1,0)}
    conferir(rs)
    for nome, v in rs['x86',1]['totais'].items():
        w = rs['ppc',1]['totais'][nome]
        if v != w: print('diferença preservada:', nome, v, w)
    print('tudo certo: chamadas comuns, fronteiras, gancho e duas posições nas duas arquiteturas')

if __name__ == '__main__': main()
