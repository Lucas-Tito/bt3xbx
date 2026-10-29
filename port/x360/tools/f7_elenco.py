#!/usr/bin/env python3
"""F7, etapa 4: o censo de carga do elenco em lotes de luta em time (plano/F7/04.md).

  f7_elenco.py sonda --saida ~/f7e4/grade.tsv
      a grade de personagens da TeamSel com tudo desbloqueado, lida da x86 sem janela sob o gdb (a mesma que os
      roteiros percorrem); a saída fica fora do git.
  f7_elenco.py gera --dados gamedata --grade ~/f7e4/grade.tsv
      os roteiros port/x360/roteiros/f7_elenco_NN.txt e o manifesto dos casos port/x360/roteiros/f7_elenco.json.
  f7_elenco.py confere [--grade ...]
      os roteiros versionados são exatamente os que o manifesto dos casos gera (sem a grade: só a forma).

Cada caso é um Duelo 1P contra COM em time de cinco contra cinco, escolhido pela entrada na TeamSel (cada membro
com o personagem e o traje conferidos no estado da tela), num estágio normal; na luta, "termina luta perde apos N"
derruba cada membro que entra, e o jogo carrega o seguinte (BtlLoad_StepChara). Cada caso carrega dez pares
id/traje na variante 0, os rostos dos dez membros pelo lado (arquivo 8 + id * 2 + lado), as animações e o banco de
voz do conjunto do caso (0xC7B + id com SAVE_FLAG_VOICE, o do New Game; 0xBDA + id com a outra voz das Opções).
O conjunto de casos cobre: os 349 pares id/traje uma vez; cada id dos dois lados e com as duas vozes; os 35 estágios
normais. A variante 1 (danificada) não é selecionável: fica para outra família (04.md).
"""
import argparse
from collections import defaultdict
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from f7_dados import Dados, exigir

RAIZ = Path(__file__).resolve().parents[3]
ROTEIROS = RAIZ / 'port/x360/roteiros'
MANIFESTO = ROTEIROS / 'f7_elenco.json'
MEMBROS = 5
N_IDS = 161
N_ESTAGIOS = 35
VOZ_ALT, VOZ_NORMAL = 0xC7B, 0xBDA   # battle_load.c: BTL_FILE_VOICE_ALT (SAVE_FLAG_VOICE), BTL_FILE_VOICE
CELULA_ALEATORIA, CELULA_CUSTOM, CELULA_VAZIA = 0xA1, 0xA3, 0xA4


def tabela(dados):
    """[(trajes, {ligações})] por id, do ChrTbl (comum 3, seção 1; include/battle/view_b.h)"""
    d = Dados(dados)
    t = d.secao(d.arquivo(3), 1)
    out = []
    for i in range(N_IDS):
        trajes = d.u16(t, i * 0x3C + 0x0A)
        exigir(1 <= trajes <= 4, f'id {i}: trajes {trajes}')
        ligacoes = {b for b in t[i * 0x3C + 0x2C:i * 0x3C + 0x30] if b != 0xFF}
        out.append((trajes, ligacoes))
    return out


def relacionados(tab, a, b):
    """ChrTbl_IsRelated (src/battle/view_b_c.c): o mesmo id, ou um nas ligações do outro; 0x4F, 0x6D e 0x80 nunca"""
    if {a, b} & {0x4F, 0x6D, 0x80}:
        return False
    return a == b or b in tab[a][1] or a in tab[b][1]


def ler_grade(caminho):
    """{id: (linha, coluna, com_formas)} da sonda: a primeira célula que é o próprio id sem formas, senão a primeira
    que o tem entre as formas (a TeamSel abre o carretel das formas quando formCount != 0)"""
    celulas = []
    for l in Path(caminho).read_text().splitlines():
        p = l.split()
        if p and p[0] == 'CEL' and p[1] == '0':
            celulas.append((int(p[3]), int(p[4]), int(p[5]), [int(x) for x in p[7:7 + int(p[6])]]))
    exigir(celulas, 'grade vazia')
    pos = {}
    for linha, coluna, cid, formas in celulas:
        if cid < CELULA_ALEATORIA and not formas and cid not in pos:
            pos[cid] = (linha, coluna, False)
    for linha, coluna, cid, formas in celulas:
        for f in formas:
            if f not in pos:
                pos[f] = (linha, coluna, True)
    faltam = [i for i in range(N_IDS) if i not in pos]
    exigir(not faltam, f'ids fora da grade: {faltam}')
    return pos


def ler_estagios(caminho):
    """os estágios da grade da TeamSel (stageIds, seção 39 do pacote): só os reais (0..34), na ordem da grade"""
    for l in Path(caminho).read_text().splitlines():
        p = l.split()
        if p and p[0] == 'ESTAGIOS':
            return [int(x) for x in p[2:] if int(x) < N_ESTAGIOS]
    raise ValueError('grade sem a linha ESTAGIOS (refazer a sonda)')


def montar(tab, estagios):
    """Os casos: [{nome, voz, estagio, lados: [[[id, traje], ...], [...]]}], deterministas.

    Cada id aparece max(trajes, 2) vezes: a primeira no lado 0 e a segunda no lado 1, com vozes cruzadas (o par
    alterna por id), os outros trajes onde couber. Num lado, nenhum par de membros relacionados (TeamSel_IsCharaFree)."""
    pedidos = []  # (id, traje, lado ou None, voz ou None)
    for i, (trajes, _) in enumerate(tab):
        voz0 = 'alt' if i % 2 == 0 else 'normal'
        voz1 = 'normal' if voz0 == 'alt' else 'alt'
        pedidos.append((i, 0, 0, voz0))
        pedidos.append((i, 1 if trajes > 1 else 0, 1, voz1))
        for t in range(2, trajes):
            pedidos.append((i, t, None, None))
    # famílias grandes primeiro: as mais difíceis de espalhar sem parentes no mesmo lado
    familia = {i: len({j for j in range(N_IDS) if relacionados(tab, i, j)}) for i in range(N_IDS)}
    pedidos.sort(key=lambda p: (p[2] is None, -familia[p[0]], p[0], p[1]))
    casos = []

    def cabe(caso, lado, i):
        membros = caso['lados'][lado]
        return len(membros) < MEMBROS and not any(relacionados(tab, i, j) for j, _ in membros)

    for i, t, lado, voz in pedidos:
        opcoes = [(c, l) for c in casos for l in ((lado,) if lado is not None else (0, 1))
                  if (voz is None or c['voz'] == voz) and cabe(c, l, i)]
        if not opcoes:
            c = dict(voz=voz or ('alt' if sum(x['voz'] == 'alt' for x in casos) <= len(casos) / 2 else 'normal'),
                     lados=[[], []])
            casos.append(c)
            opcoes = [(c, lado if lado is not None else 0)]
        # o caso menos cheio, para fechar os lotes em dez
        c, l = min(opcoes, key=lambda o: (len(o[0]['lados'][0]) + len(o[0]['lados'][1]), casos.index(o[0]), o[1]))
        c['lados'][l].append([i, t])
    # os lados vazios: um lado precisa de ao menos um membro; empresta um par já coberto noutro caso
    for c in casos:
        for l in (0, 1):
            exigir(c['lados'][l], 'caso com um lado vazio: rever a montagem')
    casos.sort(key=lambda c: (c['voz'] != 'alt',))
    for k, c in enumerate(casos):
        c['nome'] = f'f7_elenco_{k:02d}'
        c['estagio'] = estagios[k % len(estagios)]
    exigir(len(casos) >= len(estagios), f'{len(casos)} casos: menos que os {len(estagios)} estágios do Duelo')
    return [dict(nome=c['nome'], voz=c['voz'], estagio=c['estagio'], lados=c['lados']) for c in casos]


def cobertura(casos):
    """o que o conjunto de casos promete cobrir (o verificador exige o mesmo nos rastros)"""
    pares, rostos, vozes, estagios = set(), set(), set(), set()
    for c in casos:
        estagios.add(c['estagio'])
        for lado, membros in enumerate(c['lados']):
            for i, t in membros:
                pares.add((i, t))
                rostos.add((i, lado))
                vozes.add((i, c['voz']))
    return pares, rostos, vozes, estagios


def roteiro(caso, grade, tab):
    L = []
    w = L.append
    w(f"# F7, etapa 4 (gerado por port/x360/tools/f7_elenco.py; não editar à mão): censo de carga do elenco,")
    w(f"# caso {caso['nome']}. Duelo 1P contra COM em time, voz {caso['voz']}, estágio {caso['estagio']}.")
    for lado, membros in enumerate(caso['lados']):
        w(f"# lado {lado}: " + ' '.join(f'{i}/{t}' for i, t in membros))
    w("# Cada membro escolhido pela entrada na TeamSel, conferido no estado da tela (time L chara/cor); na luta,")
    w("# \"termina luta\" derruba cada membro que entra, e o jogo carrega o seguinte; \"rastreia elenco\" registra cada")
    w("# carga concluída. O resultado volta à TeamSel (memória no fim).")
    w("inclui boot.txt")
    w("rastreia elenco")
    w("quadros 90")
    if caso['voz'] == 'normal':
        w("# a outra voz: Opções, página do som, voz (limpa SAVE_FLAG_VOICE, que o New Game liga)")
        w("aperta CIMA 2                     # Opções")
        w("aperta X ate tela 62 max 300")
        w("quadros 150")
        w("aperta BAIXO 2                    # som")
        w("quadros 20")
        w("aperta X                          # a página do som, no estéreo")
        w("quadros 60")
        w("aperta BAIXO 3                    # voz")
        w("quadros 20")
        w("aperta X                          # o seletor")
        w("quadros 30")
        w("aperta DIREITA")
        w("quadros 20")
        w("aperta X")
        w("quadros 40")
        w("confere voz 0")
        w("aperta TRIANGULO")
        w("quadros 60")
        w("aperta TRIANGULO ate mcflow 32 max 600   # sair: a opção mudada pede para gravar (McFlow 4)")
        w("repete ate tela 4 max 3000        # \"sim\" (a pergunta começa no \"não\")")
        w("    quadros 50")
        w("    aperta ESQUERDA")
        w("    quadros 10")
        w("    aperta X")
        w("volta")
        w("confere voz 0")
        w("quadros 90")
        w("aperta BAIXO 5                    # das Opções ao Duelo")
    else:
        w("confere voz 1")
        w("aperta BAIXO 3                    # Duelo")
    w("aperta X ate tela 38 max 120       # DuelMenu")
    w("quadros 120")
    w("aperta X                          # 1P contra COM")
    w("quadros 40")
    w("aperta BAIXO                      # luta em time")
    w("quadros 20")
    w("aperta X")
    w("espera tela 40 max 600            # TeamSel")
    w("quadros 150")
    for lado, membros in enumerate(caso['lados']):
        for k, (i, t) in enumerate(membros):
            linha, coluna, formas = grade[i]
            w(f"# lado {lado}, membro {k}: id {i}, traje {t} (linha {linha}, coluna {coluna}{', nas formas' if formas else ''})")
            if k > 0:
                w(f"aperta X ate time {lado} estado 0 max 300")
            w(f"aperta BAIXO ate time {lado} linha {linha} max 600")
            w(f"aperta DIREITA ate time {lado} coluna {coluna} max 300")
            if formas:
                w(f"aperta X ate time {lado} estado 1 max 300")
                w(f"aperta DIREITA ate time {lado} chara {i} max 300")
            else:
                w(f"espera time {lado} chara {i} max 60")
            w(f"aperta X ate time {lado} estado 2 max 300")
            w(f"aperta CIMA ate time {lado} placa 0 max 300")
            w(f"aperta X ate time {lado} estado 5 max 300")
            w(f"aperta DIREITA ate time {lado} cor {t} max 300")
            w(f"aperta X ate time {lado} estado 6 max 300")
        w(f"espera time {lado} membros {len(membros)} max 60")
        w(f"aperta DIREITA ate time {lado} cursor 5 max 300   # time completo")
        w(f"aperta X ate time {lado} estado 8 max 300")
    w("quadros 60")
    w(f"repete ate time 0 estagio {caso['estagio']} max 3000")
    w("    aperta DIREITA 7                # as seis colunas e a placa da música (nela o BAIXO não anda)")
    w("    aperta BAIXO")
    w("volta")
    w("quadros 30")
    w("aperta X")
    w("espera luta max 3000")
    w("espera seq 3 max 1200")
    w(f"termina luta perde apos {len(caso['lados'][1]) - 1} troca max 40000")
    w("# o resultado (estado 6): a segunda opção volta à seleção, e a confirmação pede \"sim\" (para cima)")
    w("espera seq 6 max 1200")
    w("quadros 60")
    w("marca ponto resultado")
    w("aperta BAIXO")
    w("quadros 20")
    w("aperta X")
    w("quadros 60")
    w("aperta CIMA")
    w("quadros 20")
    w("aperta X")
    w("espera tela 40 max 3000")
    w("quadros 200")
    w("fim")
    return '\n'.join(L) + '\n'


def validar(casos, tab=None):
    nomes = [c['nome'] for c in casos]
    exigir(len(set(nomes)) == len(nomes), 'nomes repetidos')
    for c in casos:
        exigir(c['voz'] in ('alt', 'normal'), f"{c['nome']}: voz")
        exigir(0 <= c['estagio'] < N_ESTAGIOS, f"{c['nome']}: estágio")
        for lado in (0, 1):
            m = c['lados'][lado]
            exigir(1 <= len(m) <= MEMBROS, f"{c['nome']}: lado {lado} com {len(m)} membros")
            if tab:
                for a in range(len(m)):
                    exigir(m[a][1] < tab[m[a][0]][0], f"{c['nome']}: traje {m[a]} fora do id")
                    for b in range(a):
                        exigir(not relacionados(tab, m[a][0], m[b][0]), f"{c['nome']}: {m[a]} e {m[b]} relacionados")


def sonda(saida):
    """a grade das duas TeamSel da x86 sob o gdb, em `saida` (fora do git)"""
    saida = Path(saida).resolve()
    exigir(not saida.is_relative_to(RAIZ), 'saída da sonda dentro do repositório')
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        rot = td / 'sonda.txt'
        rot.write_text(f"inclui {os.path.relpath(ROTEIROS / 'boot.txt', td)}\nquadros 90\naperta BAIXO 3\naperta X ate tela 38\nquadros 120\n"
                       "aperta X\nquadros 40\naperta BAIXO\nquadros 20\naperta X\nespera tela 40 max 600\nquadros 400\nfim\n")
        gdb = td / 'sonda.gdb'
        gdb.write_text('''set pagination off
set confirm off
break TeamSel_Draw
ignore 1 300
run
python
import gdb
inf = gdb.selected_inferior()
def s32(a):
    v = int.from_bytes(bytes(inf.read_memory(a, 4)), 'little')
    return v - (1 << 32) if v >= 1 << 31 else v
ts = s32(int(gdb.parse_and_eval('(unsigned)&gTeamSel')))
ids = s32(ts + 0x3C5C); n = s32(ts + 0x3C64)
print('ESTAGIOS', n, ' '.join(str(s32(ids + 4 * k)) for k in range(n)))
for lado in (0, 1):
    cells = s32(ts + 0x9B0 + 4 * lado); rows = s32(ts + 0x3C20 + 4 * lado)
    print('LADO', lado, 'linhas', rows)
    for i in range(rows * 7):
        c = cells + i * 0x24
        fc = s32(c + 4)
        print('CEL', lado, i, i // 7, i % 7, s32(c), fc, ' '.join(str(s32(c + 8 + 4 * k)) for k in range(min(fc, 7))))
end
kill
quit
''')
        env = dict(os.environ, BT3_GS='none', BT3_SETTINGS='', BT3_SOUND_TICKS='1', BT3_NOMOVIE='1',
                   BT3_MENU_ITEM4='0', BT3_DESBLOQUEIO='1', BT3_ROTEIRO=str(rot), BT3_SAVES=str(td / 's'),
                   DEBUGINFOD_URLS='')
        r = subprocess.run(['gdb', '-batch', '-nx', '-x', str(gdb), '--args', str(RAIZ / 'port/build/x86/bt3_x86')],
                           env=env, capture_output=True, text=True, timeout=600, cwd=RAIZ)
    linhas = [l for l in r.stdout.splitlines() if l.startswith(('LADO', 'CEL', 'ESTAGIOS'))]
    exigir(sum(l.startswith('CEL') for l in linhas) > 0, 'sonda sem a grade: ' + r.stdout[-500:])
    saida.parent.mkdir(parents=True, exist_ok=True)
    saida.write_text('\n'.join(linhas) + '\n')
    print(f'sonda: {saida} ({sum(l.startswith("CEL") for l in linhas)} células)')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('acao', choices=['sonda', 'gera', 'confere'])
    p.add_argument('--dados', type=Path, default=RAIZ / 'gamedata')
    p.add_argument('--grade', type=Path)
    p.add_argument('--saida', type=Path)
    a = p.parse_args()
    try:
        if a.acao == 'sonda':
            exigir(a.saida, 'sonda pede --saida')
            sonda(a.saida)
            return 0
        tab = tabela(a.dados)
        if a.acao == 'gera':
            exigir(a.grade, 'gera pede --grade (a saída da sonda)')
            grade = ler_grade(a.grade)
            casos = montar(tab, ler_estagios(a.grade))
            validar(casos, tab)
            for velho in ROTEIROS.glob('f7_elenco_*.txt'):
                velho.unlink()
            for c in casos:
                (ROTEIROS / f"{c['nome']}.txt").write_text(roteiro(c, grade, tab))
            estagios = ler_estagios(a.grade)
            MANIFESTO.write_text(json.dumps(dict(
                gerador='port/x360/tools/f7_elenco.py', membros_por_lado=MEMBROS,
                estagios_do_duelo=sorted(estagios),
                estagios_fora_do_duelo=sorted(set(range(N_ESTAGIOS)) - set(estagios)), casos=casos), indent=1) + '\n')
        m = json.loads(MANIFESTO.read_text())
        casos = m['casos']
        validar(casos, tab)
        pares, rostos, vozes, estagios = cobertura(casos)
        esperados = sum(t for t, _ in tab)
        exigir(len(pares) == esperados, f'pares cobertos {len(pares)} de {esperados}')
        exigir(len(rostos) == 2 * N_IDS and len(vozes) == 2 * N_IDS, f'rostos {len(rostos)}, vozes {len(vozes)}')
        exigir(estagios == set(m['estagios_do_duelo']) and not estagios & set(m['estagios_fora_do_duelo']) and
               len(estagios) + len(m['estagios_fora_do_duelo']) == N_ESTAGIOS, f'estágios {sorted(estagios)}')
        versionados = sorted(x.stem for x in ROTEIROS.glob('f7_elenco_*.txt'))
        exigir(versionados == sorted(c['nome'] for c in casos), 'roteiros versionados diferentes do manifesto dos casos')
        if a.grade:
            grade = ler_grade(a.grade)
            for c in casos:
                exigir((ROTEIROS / f"{c['nome']}.txt").read_text() == roteiro(c, grade, tab),
                       f"{c['nome']}: o roteiro não é o que o manifesto gera")
        cargas = sum(len(c['lados'][0]) + len(c['lados'][1]) for c in casos)
        print(f'elenco: {len(casos)} casos, {cargas} cargas; {len(pares)} pares, {len(rostos)} rostos, {len(vozes)} '
              f'vozes, {len(estagios)} estágios (fora do Duelo: {m["estagios_fora_do_duelo"]}); tudo certo')
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as e:
        print(f'FALHA: {e}')
        return 1


if __name__ == '__main__':
    sys.exit(main())
