#!/usr/bin/env python3
"""Manifesto nominal da F7. Só metadados; saída obrigatoriamente fora do git.

python3 port/x360/tools/f7_manifesto.py --dados /srv/bt3/dados/gamedata --saida ~/f7-etapa1/manifesto.jsonl
--dados também aceita uma ISO convertida. Nenhum nome, texto, imagem ou senha é extraído.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from f7_dados import Dados, exigir
from f7_cobertura import faixas
import unpacks

RAIZ = Path(__file__).resolve().parents[3]


def fonte(nome):
    return (RAIZ / nome).read_text()


def trecho(nome, inicio, fim):
    s = fonte(nome)
    return s[s.index(inicio):s.index(fim, s.index(inicio))]


def numeros(s):
    s = re.sub(r'/\*.*?\*/|//[^\n]*', '', s, flags=re.S)
    return [int(n, 0) for n in re.findall(r'-?0x[0-9A-Fa-f]+|-?\b\d+\b', s)]


def constante(nome, chave):
    m = re.search(r'^#define\s+' + chave + r'\s+(0x[0-9A-Fa-f]+|\d+)\b', fonte(nome), re.M)
    exigir(m is not None, f'constante ausente: {chave}')
    return int(m[1], 0)


def defaults(campo, limite):
    s = trecho('src/sys/save.c', f'for (i = 0; i < {campo}; i++) {{\n        switch', '\n    for (i = 0;',)
    excluidos = {int(n, 0) for n in re.findall(r'case (0x[0-9A-Fa-f]+|\d+):', s)}
    return set(range(limite)) - excluidos


def gerar(d):
    linhas = []
    inv, erros, _ = unpacks.levantar()
    exigir(not erros and not any(s == 'pendente' for s, _, _ in inv), 'inventário com erro/pendência')
    for chave, esperado in [('SAVE_CHARA_COUNT', 161), ('SAVE_STAGE_COUNT', 35), ('SAVE_ITEM_COUNT', 350), ('SAVE_REC_COUNT', 14)]:
        exigir(constante('include/sys/save.h', chave) == esperado, f'{chave} mudou; rever o contrato do censo')
    chamadas = defaultdict(list)
    for _, onde, _ in inv:
        chamadas[onde.split()[1]].append(onde)
    leitores_conhecidos = set()
    locais_leitores = defaultdict(list)
    for p in (RAIZ / 'src').glob('**/*.c'):
        texto = p.read_text()
        for m in re.finditer(r'^\w[\w\s*]*?\b(\w+)\s*\([^;]*?\)\s*\{', texto, re.M):
            leitores_conhecidos.add(m[1])
            locais_leitores[m[1]].append(f'{p.relative_to(RAIZ)}:{texto[:m.start()].count(chr(10)) + 1} {m[1]}')

    def linha(tipo, id, arquivos, leitor, condicao, roteiros=(), estado='selecionável', **campos):
        leitores = leitor.split(',')
        for f in leitores:
            exigir(f.strip() in leitores_conhecidos, f'leitor inexistente: {f}')
        ligacoes = [c for f in leitores for c in chamadas[f.strip()]]
        linhas.append(dict(tipo=tipo, id=id, estado=estado, arquivos=[f'pzs3us1/{i:05d}.bin' for i in arquivos],
                           leitor=leitor, chamadas=ligacoes, leitura_direta=not bool(ligacoes),
                           provas_leitores=[p for f in leitores for p in locais_leitores[f.strip()]],
                           condicao=condicao, roteiros=list(roteiros), **campos))

    # O menu base tem dez entradas efetivas, incluindo Shenlong e excluindo rede.
    modos = [(0, 'História', 460, 'ModeMenu_Init', ('historia', 'historia_novo', 'historia_perde')),
             (1, 'Ultimate Battle', 451, 'UbMenu_Init', ('ultimate',)),
             (2, 'Torneio', 450, 'TourMenu_Init', ('torneio',)),
             (3, 'Duelo', 448, 'DuelMenu_Init', ('duelo', 'duelo_time', 'f5_duelo')),
             (5, 'Evolution Z', 452, 'EvoTop_Init', ('evolution',)),
             (6, 'Treino', 461, 'Train_Init', ('treino', 'f5_treino')),
             (7, 'Data Center', 459, 'DcMenu_Init', ('datacenter',)),
             (8, 'Referência', 463, 'CharRef_Init', ('referencia',)),
             (9, 'Opções', 453, 'Option_Init', ('opcoes',)),
             (10, 'Shenlong', 472, 'Shen_Init', ('shenlong',))]
    for i, nome, arq, leitor, rot in modos:
        linha('modo', i, [arq], leitor, 'unlockFlags & 0x7F == 0x7F' if i == 10 else 'entrada disponível desde New Game', rot, nome=nome)
    linha('modo', 4, [449], 'MainMenu_Init', 'decisão 22; MainMenu_Init omite item 4', estado='excluído', nome='Dragon Net Battle')
    for i, leitor, rot, cond in [(0, 'SimTop_Init', 'simdragon', 'desde New Game'),
                                (1, 'MisSel_Init', 'missao', 'desde New Game'),
                                (2, 'SurvSel_Init', 'sobrevivencia', 'unk208 & 1; MisResult_MarkCleared na missão inédita 30'),
                                (3, 'DiscFusion_Init', 'discfusion', 'placa disponível; dois ramos exigem flags de sessão por X no port')]:
        linha('submodo_ultimate', i, [451], leitor, cond, (rot,))

    # O número do script é calculado pelo leitor, não pelo total de arquivos.
    corpo = trecho('src/menu/menu_a_d.c', 's32 count[8]', 's32 line =')
    contagens = numeros(corpo[corpo.index('{') + 1:corpo.index('}')])
    exigir(len(contagens) == 8, 'História: oito sagas esperadas')
    casos = dict((int(a), int(b)) for a, b in re.findall(r'case (\d+):\s*line = (\d+);', fonte('src/menu/menu_a_d.c')))
    recompensas = defaultdict(list)
    episodios = []
    for saga, n in enumerate(contagens):
        for ep in range(n):
            ordinal = sum(contagens[:saga]) + ep
            script = casos.get(ordinal, ordinal)
            acoes = d.gsc(510 + script)
            for acao, comandos in acoes:
                for cmd, args, opts in comandos:
                    if cmd == 16:
                        exigir(len(args) == 15, 'História: recompensa sem 15 operandos')
                        # SetRewards escreve operandos 6..8 em +0x24 (stage), 9..11 em +0x18 (chara).
                        for tipo, ini in [('item', 3), ('estagio', 6), ('personagem', 9), ('episodio', 12)]:
                            for alvo in args[ini:ini + 3]:
                                if alvo >= 0:
                                    recompensas[tipo, alvo].append(dict(fonte='HistResult_BuildRewards após vitória', saga=saga, episodio=ep, script=script, acao=acao))
            episodios.append((saga, ep, script, ordinal, acoes))
    for saga, ep, script, ordinal, acoes in episodios:
        historias = sorted({a[0] for _, cs in acoes for c, a, _ in cs if c == 2 and a})
        arquivos = [464 + saga, 510 + script, 973 + ordinal]
        arquivos += [base + h for h in historias for base in (560, 610, 660, 710)]
        cond = f'slot[{saga}].flags & 1 e slot[{saga}].val[0] & (1 << {ep})'
        linha('episodio', f'{saga}:{ep}', arquivos, 'ModeMenu_Init,ModeMenu_UpdateImage,BtlLoad_StepInitial', cond,
              ('historia', 'historia_novo', 'historia_perde') if ordinal == 0 else (), saga=saga, episodio=ep,
              script=script, ordinal=ordinal, eventos=[a for a, _ in acoes if a >= 10000],
              origens_desbloqueio=['Save_SetDefaults'] if ordinal == 0 else recompensas['episodio', script])
    for script in range(50):
        linha('script_historia', script, [510 + script], 'BtlLoad_StepInitial',
              'ModeMenu_GetLine só retorna 0..47' if script >= 48 else 'episódio correspondente liberado',
              ('historia', 'historia_novo', 'historia_perde') if script == 0 else (),
              estado='inativo' if script >= 48 else 'selecionável')

    selecao = d.pacote(451, 2)
    mis = d.secao(selecao, 10)
    for i in range(100):
        exigir((i + 1) * 0x34 <= len(mis), 'tabela de missão truncada')
        linha('missao', i, [451], 'MisSel_Init', f'missionPages (save +0x20C) > {i // 5}; começa em 5 páginas (25 missões); SimEv17 acrescenta página até 20',
              ('missao',) if i == 0 else (), campo_save=f'0x{0x28c + 12 * i:x}', regra=d.s32(mis, i * 0x34 + 0x18),
              dp=d.s32(mis, i * 0x34 + 0x1c), secao=2, tabela=10)
    for i in range(3):
        curso = d.secao(selecao, 12)[i * 0xe0:(i + 1) * 0xe0]
        exigir(len(curso) == 0xe0, 'curso Sobrevivência truncado')
        linha('curso_sobrevivencia', i, [451], 'SurvSel_Init', 'unk208 & 1; 30 missões inéditas ganhas',
              ('sobrevivencia',) if i == 0 else (), adversarios=50, tabela=12,
              indices_adversarios=[d.s32(curso, 24 + k * 4) for k in range(50)])
    cursos = d.pacote(476)
    tab, foes = d.secao(cursos, 8), d.secao(cursos, 9)
    for i in range(5):
        ids = [d.s32(tab, i * 0x38 + 24 + k * 4) for k in range(8)]
        n = 0
        for j in ids:
            exigir(0 <= j * 0x2c <= len(foes) - 0x2c, 'curso Disc Fusion: adversário inválido')
            if d.s32(foes, j * 0x2c) == 999:
                break
            n += 1
        linha('curso_discfusion', i, [476], 'UbzSel_Init', 'discFlags & 2; X na placa Disc Fusion libera a sessão',
              ('discfusion',) if i == 0 else (), adversarios=n, campo_save=f'0x{0x780 + 12 * i:x}', tabela=8)
    ranking = d.pacote(475)
    exigir(len(d.secao(ranking, 8)) >= 100 * 0x1c, 'ranking truncado')
    for i in range(100):
        linha('posto_discfusion', i, [475], 'UbRank_Init', 'discFlags & 1; desafiar posto superior conforme UbRank_Input; rank em 0x77C',
              ('discfusion',) if i == 98 else (), tabela=8)
        linha('regra_ranking', i, [475], 'UbRank_Init',
              'UbRank_PlaceToRule elimina o posto do jogador: índices 0..98; registro 99 nunca é escolhido',
              estado='selecionável' if i < 99 else 'inativo', tabela=8)
    for i in range(37):
        linha('intruso_discfusion', i, [475], 'UbRank_Init', 'UbRank_PickFoe: sorteio e condição do posto corrente', tabela=9)

    sim = d.pacote(474)
    pesos = d.secao(sim, 28)
    somas = [sum(d.s32(pesos, i * 20 + j * 4) for i in range(32)) for j in range(5)]
    exigir(all(s == 256 for s in somas[:4]), 'SimDay: soma dos pesos permite evento fora da tabela')
    for i in range(37):
        colunas = [] if i < 5 else [j for j in range(4) if d.s32(pesos, (i - 5) * 20 + j * 4) > 0]
        estado = 'selecionável' if i < 5 or colunas else 'inativo'
        cond = ('treino 0..2 pelo submenu' if i < 3 else f'botão do tabuleiro {i - 1}' if i < 5 else
                f'SimDay_ListItems: coluna em {colunas}; 0/1 capacidade !=20, 2/3 capacidade==20; ímpar tem item elegível')
        linha('evento_sim', i, [474], 'SimDay_Init', cond,
              ('simdragon',) if i in (0, 3) else (), estado=estado, tabela=28 if i >= 5 else None,
              handler=f'SimEv{i:02d}', colunas=colunas)
    for i in range(7):
        exigir(len(d.secao(sim, 25)) >= (i + 1) * 28, 'rodadas Sim truncadas')
        linha('rodada_sim', i, [474], 'SimDay_Init', f'turn / 10 == {i}; continuar após vitória, termina no turno 69',
              ('simdragon',) if i == 0 else (), tabela=25)

    tabela = d.secao(d.arquivo(3), 1)
    iniciais = defaults('SAVE_CHARA_COUNT', 161)
    iniciais_stg = defaults('SAVE_STAGE_COUNT', 35)
    relacoes = defaultdict(list)
    for i in range(161):
        trajes = d.u16(tabela, i * 0x3c + 10)
        exigir(1 <= trajes <= 4, 'personagem: número de trajes inválido')
        cond = f'charaBits[{i // 64}] & (1 << {i % 64})'
        linha('personagem', i, [3, 760 + i], 'CharSel_UpdateFaceLoad', cond,
              ('duelo', 'f5_duelo', 'f5_treino', 'referencia') if i == 0 else ('f5_duelo', 'f5_treino') if i == 25 else (), trajes=trajes,
              origens_desbloqueio=['Save_SetDefaults'] if i in iniciais else recompensas['personagem', i])
        for traje in range(trajes):
            for variante in range(2):
                arq = i * 10 + traje + 0x590 + variante * 4 - 1
                parametro = d.secao(d.arquivo(arq), 0x12)
                exigir(len(parametro) >= 0x100, 'parâmetros de modelo truncados')
                linha('modelo', f'{i}:{traje}:{variante}', [arq, i * 10 + 1431, i * 10 + 1432],
                      'BtlLoad_StepChara', cond + '; traje por ChrTbl_WrapCostume; variante por estado da luta',
                      ('duelo', 'f5_duelo', 'f5_treino', 'referencia') if (i, traje, variante) == (0, 0, 0) else
                      ('f5_duelo', 'f5_treino') if (i, traje, variante) == (25, 0, 0) else (),
                      personagem=i, traje=traje, variante=variante)
                for tipo, alvos, custos, tipos, slots in [('transformacao', 0x98, 0x9c, 0xa4, 4), ('fusao', 0xb4, 0xae, 0xb7, 3)]:
                    for slot in range(slots):
                        alvo = parametro[alvos + slot]
                        if alvo == 255:
                            continue
                        config = dict(traje=traje, variante=variante, custo=parametro[custos + slot],
                                      classe=parametro[tipos + slot])
                        if tipo == 'fusao':
                            config['parceiros'] = list(parametro[0xba + slot * 4:0xbe + slot * 4])
                        else:
                            config['padrao'] = int.from_bytes(parametro[0xac:0xad], 'little', signed=True)
                            config['quantidade_slots'] = sum(x != 255 for x in parametro[0x98:0x9c])
                        relacoes[tipo, i, slot, alvo].append(config)
    for (tipo, i, slot, alvo), configs in sorted(relacoes.items()):
        exigir(0 <= alvo < 161, 'relação: alvo fora do elenco')
        possiveis = [c for c in configs if tipo == 'fusao' and any(p < 161 and p != i for p in c['parceiros']) or
                     tipo == 'transformacao' and (c['quantidade_slots'] != 1 or c['padrao'] < 0 or c['padrao'] == slot)]
        cond = ('BtlAct_CanTransform: form.allowed; sem habilidade 0x5E/flag 0xAF/memberUnk70; alvo nos bits do lado; '
                'estoque >= custo*100000; classe 3 exige lua (BtlStage_HasMoon)') if tipo == 'transformacao' else (
                'BtlAct_CanFuse: mesmas guardas; alvo nos bits do lado; estoque >= custo*100000; primeiro parceiro '
                'presente deve estar vivo e gauge.unk30 == 0')
        linha(tipo, f'{i}:{slot}:{alvo}', sorted({i * 10 + c['traje'] + 1423 + c['variante'] * 4 for c in configs}),
              'BtlParam_GetSlotId' if tipo == 'transformacao' else 'BtlParam_GetUnkB4', cond,
              estado='alcançável sob condições do leitor' if possiveis else 'excluído pelo leitor de entrada/parceiro',
              origem=i, slot=slot, alvo=alvo, configuracoes=configs,
              comando=f'R3 + direção, ação {100 + slot}' if tipo == 'transformacao' else f'R3 mantido 12 quadros + direção, ação {104 + slot}',
              prova_leitor='src/battle/btl_act_j.c:BtlAct_CheckTransformInput/BtlAct_CanTransform' if tipo == 'transformacao' else
              'src/battle/btl_act_j.c:BtlAct_CanFuse/BtlAct_CheckFusionInput')

    for i in range(35):
        for variante, base in [('normal', 368), ('dividida', 407)]:
            exigir(d.u32(d.arquivo(base + i)) == 25, 'cenário: tabela inválida')
            param = d.secao(d.arquivo(base + i), 2)
            flags = d.u32(param, d.u32(param, 0x74))
            info = d.u32(param, 0x14)
            linha('estagio', f'{i}:{variante}', [base + i, 924 + i], 'CharSel_UpdateStageLoad,BtlLoad_StepInitial',
                  f'stageBits & (1 << {i}); screenMode=' + ('1; modos Duelo/Torneio' if variante == 'dividida' else '0'),
                  ('f5_duelo', 'f5_treino') if i == 5 and variante == 'normal' else (),
                  estagio=i, variante=variante,
                  lua=bool(flags & 0x10), flags=flags, agua=bool(d.u32(param, info + 4) & 2),
                  troca=d.s32(param, info + 12),
                  origens_desbloqueio=['Save_SetDefaults'] if i in iniciais_stg else recompensas['estagio', i])
    for i in range(442, 446):
        linha('vaga_estagio', i - 407, [i], 'BtlLoad_StepInitial', 'StgGrid: 0..34; 35 aleatório resolvido antes da carga; 36/37 marcadores', estado='inativo')

    itens = d.secao(d.arquivo(3), 2)
    for i in range(350):
        pos = i * 40
        exigir(pos + 40 <= len(itens), 'itens truncados')
        flags, tipo, estoque = d.u32(itens, pos + 20), itens[pos], d.u32(itens, pos + 8)
        listado = not flags & 5 and tipo < 3 and bool(flags & 0x40)
        vendido = listado and bool(flags & 0x20)
        linha('item', i, [3, 452], 'Shop_Init',
              f'Shop_Init: !(flags&5), type<3, flags&0x40; compra exige flags&0x20, unk100C >= {estoque}; '
              'Shop_CanBuy exige não possuído; confirmar exige saldo >= preço',
              estado='comprável' if vendido else 'só coleção' if listado else 'excluído da loja',
              item_id=i + 1, classe=tipo, estoque=estoque, flags=flags, preco=d.s32(itens, pos + 4),
              origens_desbloqueio=(['Save_SetDefaults'] if flags & 2 else recompensas['item', i]))

    for i in range(14):
        linha('registro', i, [459], 'DcList_Init', 'senha válida por DcMenu; rec[i].chara != -1 lista ocupado; slot vazio aceita registro',
              ('datacenter',) if i == 0 else (), campo_save=f'0x{0x2d40 + 28 * i:x}')
    for i in range(7):
        linha('replay', i, [459], 'ReplayMenu_Init', 'gravar por McFlow modo 5 após luta; carregar por modo 6 exige replay[i].flags & 1',
              ('datacenter',) if i == 0 else (), limite_roteiro='apenas visita com slot vazio' if i == 0 else 'nenhum',
              campo_save='arquivo separado no cartão, fora do bloco 0x4000')
    desejos = d.secao(d.pacote(472, 2), 2)
    for dragao, inicio, n in [(0, 0, 4), (1, 32, 7), (2, 88, 4)]:
        for i in range(n):
            pos = inicio + i * 8
            alvo, classe = d.s32(desejos, pos), d.u32(desejos, pos + 4)
            if classe in (0, 1, 2):
                tipo = {0: 'item', 1: 'estagio', 2: 'personagem'}[classe]
                recompensas[tipo, alvo - 1].append(dict(fonte='Shen_Update', dragao=dragao, desejo=i))
                if classe == 2 and alvo == 0xa0:
                    recompensas['personagem', alvo].append(dict(fonte='Shen_Update, SHEN_STATE_GET: segunda forma do prêmio 0xA0', dragao=dragao, desejo=i))
            linha('desejo', f'{dragao}:{i}', [472], 'Shen_Init',
                  'sete esferas; Shen_BuildList sorteia dragão; dragão 2 exige slot[8].flags&1 (final); '
                  'Shen_IsOwned recusa prêmio possuído, dinheiro é repetível',
                  ('shenlong',) if dragao == 1 and classe == 3 else (), dragao=dragao, posicao=i,
                  alvo=alvo, classe=classe, tabela=2)

    # Aulas: a lista do leitor elimina páginas, e algumas conservam a imagem anterior.
    treino = d.pacote(461, 1)
    aulas, pulos, semimg = (d.secao(treino, n) for n in (22, 24, 26))
    pulos = {d.s32(pulos, 16 + i * 4) for i in range(d.u32(pulos))}
    semimg = {d.s32(semimg, 16 + i * 4) for i in range(d.u32(semimg))}
    p, paginas = 0, set()
    for classe in range(3):
        while True:
            id, n, flags, ini = d.u16(aulas, p), aulas[p + 2], aulas[p + 3], d.s32(aulas, p + 4)
            ids = [i for i in range(ini, ini + n) if i not in pulos]
            paginas.update(ids)
            linha('aula', f'{classe}:{id}', [461], 'Train_Init', 'desde New Game; Train_BuildLists remove páginas da seção 24',
                  ('treino',) if classe == 0 and p == 0 else (), estado='selecionável' if ids else 'inativo', paginas=ids, flags=flags)
            p += 16
            if flags & 4:
                break
    for i in range(398):
        linha('pagina_treino', i, [1025 + i], 'Train_UpdateImage', 'Train_BuildLists e Train_PageHasImage',
              estado='carregável' if i in paginas and i not in semimg else 'conserva imagem anterior' if i in paginas else 'omitida pelo leitor',
              selecionavel=i in paginas)
    for i in range(5):
        # A seção 41 de cada chave contém os cinco torneios, com 0x64 bytes cada.
        chave = d.pacote(454 + i)
        premios = d.secao(chave, 41)
        for nivel in range(3):
            for tipo, campo in [('item', 2), ('personagem', 3), ('personagem', 4), ('estagio', 5)]:
                alvo = d.s32(premios, i * 0x64 + 16 + (campo * 3 + nivel) * 4)
                if alvo >= 0:
                    recompensas[tipo, alvo].append(dict(fonte='Bracket_GivePrizes, campeão oficial', torneio=i, nivel=nivel))
            linha('torneio', f'{i}:{nivel}', [450, 454 + i], 'TourMenu_Init,Bracket_Init',
                  'livre: escolher tipo/nível; oficial: unkA08 bit tipo, relógio unkA0C e nível unkA10 por convite',
                  ('torneio',) if (i, nivel) == (0, 0) else (), tipo_torneio=i, nivel=nivel)

    # Recompensas Ultimate são tabelas do fonte, não conteúdo copiado da ISO.
    texto = trecho('src/menu/menu_p_d.c', 'const s32 gUbRewardItems', '/* Line 2')
    premios_ub = numeros(texto[texto.index('{') + 1:texto.index('}')])
    regras = ['SimResult: última rodada', 'MisResult: 50 missões inéditas', 'MisResult: 100 missões inéditas',
              'SurvResult: pelo menos dez adversários, unk208 & 8 ainda limpo',
              'UbResult: rank==1, vitória e discFlags & 8', 'UbResult: cinco cursos concluídos']
    for i, alvo in enumerate(premios_ub):
        recompensas['item', alvo].append(dict(fonte=regras[i]))
    for x in linhas:
        tipo = x['tipo']
        if tipo == 'item' and not x['flags'] & 2:
            x['origens_desbloqueio'] = recompensas['item', x['id']]
            if x['estado'] == 'comprável':
                x['origens_desbloqueio'].append(dict(fonte='Shop_CanBuy e Shop_UpdateSeq', estoque=x['estoque'], preco=x['preco']))
        elif tipo == 'personagem' and x['id'] not in iniciais:
            x['origens_desbloqueio'] = recompensas[tipo, x['id']]
            exigir(x['origens_desbloqueio'], f'personagem sem origem de desbloqueio: {x["id"]}')
        elif tipo == 'estagio' and x['estagio'] not in iniciais_stg:
            x['origens_desbloqueio'] = recompensas[tipo, x['estagio']]
            exigir(x['origens_desbloqueio'], f'estágio sem origem de desbloqueio: {x["id"]}')
        elif tipo == 'modelo':
            x['origens_desbloqueio'] = ['Save_SetDefaults'] if x['personagem'] in iniciais else recompensas['personagem', x['personagem']]

    # Cada arquivo reconhecido pelo --disco fica ligado aos itens, ou ao leitor direto.
    usos = defaultdict(list)
    detalhes = {f'{x["tipo"]}/{x["id"]}': x for x in linhas}
    for x in linhas:
        for arq in x['arquivos']:
            usos[arq].append(f'{x["tipo"]}/{x["id"]}')
    for nome, formato in sorted(faixas().items()):
        i = int(Path(nome).stem)
        if 1423 <= i <= 3032:
            k = i - 1423
            leitor = 'BtlLoad_StepChara,BtlLoad_StepObject'
            estado = 'auxiliar de lutador' if k % 10 in (8, 9) else 'modelo referenciado' if usos[nome] else 'traje fora do limite do leitor'
        elif formato == 'som':
            leitor, estado = 'BtlLoad_StepInitial', 'banco de som; carga por voz/cenário'
        elif formato == 'rosto':
            leitor, estado = 'BtlLoad_StepInitial', 'rosto por personagem e lado'
        elif 403 <= i <= 406:
            leitor, estado = 'ShenScene_StepLoad' if i < 406 else 'ChrView_StepLoadStage', 'fundo 3D auxiliar'
        elif i == 462:
            leitor, estado = 'MainMenu_Init', 'inativo: tabela com três seções vazias, sem leitor de conteúdo'
        elif i in (472, 473):
            leitor, estado = 'Shen_Init' if i == 472 else 'BootCard_Init', 'pacote ativo com seção 1 inativa'
        elif i >= 3355:
            leitor, estado = 'ShenScene_StepLoad', 'ator da cena do dragão/esferas'
        elif i in (1, 2, 3, 4):
            leitor, estado = 'Common_LoadBoot' if i == 4 else 'Common_Reload', 'comum residente'
        elif i in (5, 6):
            leitor, estado = 'BtlLoad_StepInitial', 'HUD por idioma'
        elif i == 477:
            leitor, estado = 'McFlow_Init', 'recursos do cartão'
        elif 560 <= i <= 759:
            leitor = 'BtlScriptCmd_LoadText' if i < 660 else 'BtlScriptCmd_LoadFile8'
            estado = 'história sem episódio selecionável' if (i - 560) % 50 >= 48 else 'texto/boca de episódio'
        elif 760 <= i <= 923:
            leitor, estado = 'CharSel_UpdateFaceLoad', 'retrato ou marcador da grade'
        elif 924 <= i <= 959:
            leitor, estado = 'CharSel_UpdateStageLoad', 'miniatura ou aleatório (não é arena adicional)'
        elif 960 <= i <= 962:
            leitor, estado = 'Load_InitScreen', 'folha de carregamento'
        elif 963 <= i <= 967:
            leitor, estado = 'TourMenu_Init', 'convite do torneio'
        elif 968 <= i <= 972:
            leitor, estado = 'TourBg_Init', 'fundo do torneio'
        elif 1021 <= i <= 1024:
            leitor, estado = 'HistResult_Init', 'fundo do resultado'
        elif usos[nome]:
            # A ligação detalhada a chamada está em cada item acima.
            leitor, estado = 'File_OpenById', 'classificado pelos itens referenciados'
        else:
            leitor, estado = 'File_OpenById', 'auxiliar estrutural; sem caso de conteúdo completo'
        linha('arquivo', nome, [i], leitor, 'id de arquivo = índice + 1; condições dos itens vinculados',
              estado=estado, formato=formato, itens=usos[nome])
        linhas[-1]['chamadas'] = sorted(set(linhas[-1]['chamadas']) |
                                       {c for item in usos[nome] for c in detalhes[item]['chamadas']})
        linhas[-1]['leitura_direta'] = not bool(linhas[-1]['chamadas'])

    return linhas, somas


def resumo(linhas, somas):
    cont = Counter(x['tipo'] for x in linhas)
    estados = defaultdict(Counter)
    for x in linhas:
        estados[x['tipo']][x['estado']] += 1
    return dict(total=len(linhas), tipos=dict(sorted(cont.items())), estados={k: dict(v) for k, v in sorted(estados.items())},
                pares_id_traje=len({(x['personagem'], x['traje']) for x in linhas if x['tipo'] == 'modelo'}),
                classes_transformacao=sorted({c['classe'] for x in linhas if x['tipo'] == 'transformacao' for c in x['configuracoes']}),
                pesos_sim_por_coluna=somas)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dados', required=True, type=Path)
    p.add_argument('--saida', required=True, type=Path)
    a = p.parse_args()
    try:
        exigir(not a.saida.resolve().is_relative_to(RAIZ), 'saída deve ficar fora do repositório')
        d = Dados(a.dados)
        linhas, somas = gerar(d)
        ids = [(x['tipo'], str(x['id'])) for x in linhas]
        exigir(len(ids) == len(set(ids)), 'ids duplicados no manifesto')
        a.saida.parent.mkdir(parents=True, exist_ok=True)
        texto = ''.join(json.dumps(x, ensure_ascii=False, sort_keys=True) + '\n' for x in linhas)
        a.saida.write_text(texto)
        r = resumo(linhas, somas)
        r.update(commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=RAIZ, text=True).strip(),
                 origem=str(d.origem), ordem=d.ordem, iso_convertida=d.convertida,
                 sha256_manifesto=hashlib.sha256(texto.encode()).hexdigest())
        a.saida.with_suffix('.resumo.json').write_text(json.dumps(r, indent=2, ensure_ascii=False) + '\n')
        print(json.dumps(r, indent=2, ensure_ascii=False))
        print('manifesto: tudo certo (leitura estática, sem prova de execução)')
        return 0
    except (ValueError, OSError, IndexError) as e:
        print(f'ERRO: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
