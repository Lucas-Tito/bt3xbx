#!/usr/bin/env python3
"""Relê os dois logs preservados e a diferença d6 a d10 da coleta2.

Uso: vu0_reler_perfil.py PASTA [--json SAIDA]. Exige o mapa correto por
âncoras e ligador. Histogramas truncados não viram uma medição exata do trecho.
Nenhum endereço ou dado do jogo é incorporado a este script.
"""
import argparse,json,pathlib
from amostra_perfil import Mapa,ler_despejos,conferir_build,somar
OBJETOS=('pc_vu0_a.obj','pc_vu0_b.obj','float360.obj')
TROCADAS={'mtx_apply','op_vadd','op_vmul','op_vaddbc','op_vsubbc','op_vmulbc','op_vmulabc','op_vmaddabc','op_vmaddbc','Vec4_Add','Vec4_Scale'}
INDIRETAS={'Mtx_Mul','Vu0_SinCos'}

def releitura(pasta):
    mapa=Mapa(pasta/'build/default.map');dados={};recusas={}
    for coleta in ('coleta','coleta2'):
        ds=ler_despejos(pasta/coleta/'bt3xbx.log')
        if not ds:raise ValueError('nenhum despejo: '+coleta)
        for n,d in ds.items():
            erros=conferir_build(d,mapa)
            if erros:recusas[f'{coleta}/d{n}']=erros;continue
            dados[f'{coleta}/d{n}']=somar(d,mapa)
    saida={'recusas':recusas,'classes':{}}
    for classe in ('normal','lento'):
        a=dados['coleta2/d6'][classe];b=dados['coleta2/d10'][classe]
        uteis=b['uteis']-a['uteis'];quadros=b['quadros']-a['quadros'];media=(b['ms']-a['ms'])/quadros
        funcoes={}
        for f in a['proprio'].keys()|b['proprio'].keys():
            if not any(o in f for o in OBJETOS):continue
            nome=f.split(' [')[0];delta=b['proprio'].get(f,0)-a['proprio'].get(f,0)
            funcoes[f]={'amostras':delta,'porcentagem':100*delta/uteis,'ms_estimados':media*delta/uteis,
                        'alterada':'direta' if nome in TROCADAS else 'indireta' if nome in INDIRETAS else 'não',
                        'presente_nos_dois':f in a['proprio'] and f in b['proprio']}
        omitidas=[s['uteis']-sum(s['proprio'].values()) for s in (a,b)]
        vu0=sum(v['amostras'] for v in funcoes.values())
        saida['classes'][classe]={'quadros':quadros,'ms_por_quadro':media,'uteis':uteis,'funcoes':funcoes,
            'vu0_visivel':vu0,'omitidas_d6_d10':omitidas,
            'intervalo_conservador_vu0':[max(0,vu0-omitidas[0]),min(uteis,vu0+omitidas[1])],
            'suspensao':'O cabeçalho mede a suspensão da thread, além do intervalo pedido; ver log original.'}
    return saida

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('pasta',type=pathlib.Path);p.add_argument('--json');a=p.parse_args()
    r=releitura(a.pasta)
    for nome,erros in r['recusas'].items():print('recusado:',nome,'; '.join(erros))
    for classe,c in r['classes'].items():
        print(f'\n{classe}: {c["quadros"]} quadros, {c["ms_por_quadro"]:.3f} ms/quadro, {c["uteis"]} amostras úteis')
        print('função | amostras | % úteis | ms estimados | alteração | presente nos dois')
        for f,v in sorted(c['funcoes'].items(),key=lambda kv:-kv[1]['amostras']):
            print(f'{f} | {v["amostras"]} | {v["porcentagem"]:.3f} | {v["ms_estimados"]:.3f} | {v["alterada"]} | {v["presente_nos_dois"]}')
        print('VU0 visível:',c['vu0_visivel'],'; omitidas d6/d10:',c['omitidas_d6_d10'],'; intervalo conservador:',c['intervalo_conservador_vu0'])
    if a.json:a.json=pathlib.Path(a.json);a.json.write_text(json.dumps(r,ensure_ascii=False,indent=1))
if __name__=='__main__':main()
