#!/usr/bin/env python3
"""Vê o comparador aceitar diferenças escalares e recusar cópias estragadas."""
from copy import deepcopy
from vu0_conta_confere import conferir, NUCLEOS

def main():
    rs = {}
    for a in ('x86', 'ppc'):
        for n in (1, 0):
            contas = {nome:[2,2*n,0,2*(1-n),0,0,0,0] for nome in NUCLEOS}
            contas['pc_vu0_b/mtx_apply'] = [2,0,0,0,0,0,0,0]
            contas['pc_vu0_a/op_vmaddabc'] = [3,0,0,0,0,0,0,0]
            contas['pc_vu0_a/RefVu0_ItofBits'] = [5 if a=='x86' else 7,0,0,0,0,0,0,0]
            rs[a,n] = {'passos':1, 'totais':deepcopy(contas),
                       'quadros':[{'passo':1, 'vb':42, 'contas':contas}],
                       'janelas':[{'janela':1, 'passos':1, 'vb_fim':42, 'totais':deepcopy(contas)}]}
    conferir(rs)
    print('tudo certo: diferenças escalares preservadas')
    def recusar(nome, mutar):
        copia=deepcopy(rs); mutar(copia['ppc',0])
        try: conferir(copia)
        except ValueError: print('recusado:',nome)
        else: raise AssertionError('mutante sobreviveu: '+nome)
    recusar('passos incompletos', lambda r:r.update(quadros=[]))
    recusar('passo diferente', lambda r:r['quadros'][0].update(vb=43))
    recusar('chamada comum a mais', lambda r:r['quadros'][0]['contas']['pc_vu0_a/op_vmaddabc'].__setitem__(0,4))
    recusar('gancho ausente', lambda r:r['quadros'][0]['contas'].__delitem__('ponte/Port_FastMtxApply'))
    recusar('nativo ignora zero', lambda r:r['quadros'][0]['contas']['vu0_nativo/Vn_Op4'].__setitem__(1,1))
    recusar('inventário incompleto', lambda r:r['totais'].__delitem__('vu0_nativo/Vn_Op4'))
    recusar('janelas ausentes', lambda r:r.update(janelas=[]))
    recusar('janela diferente', lambda r:r['janelas'][0].update(passos=2))

if __name__ == '__main__': main()
