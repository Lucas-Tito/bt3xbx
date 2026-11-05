#!/usr/bin/env python3
"""Confere e resume as contagens por passo do combate, inclusive os zeros.

Uso: vu0_conta_resumo.py LOG [--json SAIDA]. Linhas de outras ferramentas
são ignoradas. Falta de passos, linha truncada, duplicata e conta sem retorno
ou com recuo sem motivo são erros, não contagens iguais a zero.
"""
import argparse, collections, json, re

def ler(caminho):
    nomes, totais, quadros = {}, {}, []
    vistos, atual = set(), None
    def fechar():
        if atual is not None: quadros.append(atual)
    with open(caminho) as f:
        for linha in f:
            if not linha.startswith('vu0conta: '): continue
            s=linha.strip().split()
            if s[1]=='motivos': continue
            if s[1]=='funcao':
                i=int(s[2])
                if atual is not None or i in nomes: raise ValueError('inventário duplicado ou fora de ordem')
                nomes[i]=s[3]; totais[s[3]]=[0]*8
            elif s[1]=='passo':
                if len(s)!=5 or s[3]!='vb': raise ValueError('passo truncado')
                fechar()
                if int(s[2])!=len(quadros)+1: raise ValueError('passo ausente ou repetido')
                atual={'passo':int(s[2]),'vb':int(s[4]),'contas':{}}; vistos=set()
            elif s[1]=='conta':
                if len(s)!=11 or atual is None: raise ValueError('conta truncada ou fora de passo')
                i=int(s[2]); v=list(map(int,s[3:])); nome=nomes[i]
                if i in vistos or any(n<0 for n in v): raise ValueError('conta duplicada ou negativa')
                vistos.add(i)
                if nome.rsplit('/',1)[1] in ('Vn_Op4','Vn_Bc4','Vn_MtxApply','Port_FastMtxApply'):
                    if v[0] != sum(v[1:]) or v[2]: raise ValueError('retornos ou motivos não fecham: '+nome)
                elif any(v[1:]): raise ValueError('retorno em função sem instrumentação de resultado')
                atual['contas'][nome]=v
                totais[nome]=[a+b for a,b in zip(totais[nome],v)]
            else: raise ValueError('linha desconhecida: '+linha)
    fechar()
    if not nomes or not quadros: raise ValueError('inventário ou passos ausentes')
    return {'passos':len(quadros),'totais':totais,'quadros':quadros,
            'colunas':['chamadas','nativo','sem-motivo','desligado','nan-inf','denormal','pequeno','grande']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log');p.add_argument('--json')
    a=p.parse_args();r=ler(a.log)
    if a.json:
        with open(a.json,'w') as f:json.dump(r,f,indent=1)
    print('Passos de combate:',r['passos'])
    print('função | chamadas | por passo | nativo | recuos (desligado, nan-inf, denormal, pequeno, grande)')
    for nome,v in r['totais'].items():
        print(f'{nome} | {v[0]} | {v[0]/r["passos"]:.3f} | {v[1]} | {v[3:]}')
    print('tudo certo: contagens e retornos fecham em cada passo')
if __name__=='__main__':main()
