"""F5: contexto vivo da arena, requisições, câmeras e listas GSC.

As marcas dinâmicas só substituem o tipo estático depois de validar o contexto nas duas memórias.
Não há teto de diferenças: contexto não demonstrado é erro, inclusive registro ausente ou truncado.
"""
import bisect
import json
import pathlib
import struct
import subprocess


class ContextoErro(ValueError):
    pass


def exigir(ok, texto):
    if not ok:
        raise ContextoErro(texto)


class Memoria:
    def __init__(self, regs, nm, be):
        self.regs = regs
        self.fmt = '>' if be else '<'
        self.simbolos = {q[-1]: int(q[0], 16) for q in (l.split() for l in nm) if len(q) >= 3}
        self.funcoes = sorted((int(q[0], 16), q[-1]) for q in (l.split() for l in nm)
                              if len(q) >= 3 and q[-2] in 'tT')
        self.ends = [a for a, _ in self.funcoes]

    def bytes(self, a, n):
        for _, base, dados in self.regs:
            if base <= a and a + n <= base + len(dados):
                return dados[a - base:a - base + n]
        raise ContextoErro(f'endereço {a:#x}, tamanho {n:#x} fora do dump')

    def u32(self, a):
        return struct.unpack(self.fmt + 'I', self.bytes(a, 4))[0]

    def global_(self, nome):
        exigir(nome in self.simbolos, f'símbolo ausente: {nome}')
        return self.u32(self.simbolos[nome])

    def funcao(self, a):
        i = bisect.bisect_right(self.ends, a - 1) - 1
        exigir(i >= 0, 'dono fora das funções')
        return self.funcoes[i][1]


def registro(arq, memoria):
    linhas = pathlib.Path(arq).read_text().splitlines()
    exigir(bool(linhas), 'testemunho do BtlPool vazio')
    h = linhas[0].split()
    exigir(len(h) == 5 and h[:2] == ['POOL', '1'] and h[4] == '0', 'cabeçalho do BtlPool inválido')
    slots, aloc = {}, []
    pcs = [int(l.split()[-1]) for l in linhas[1:] if l.startswith('A ')]
    donos = {}
    if pcs:
        saida = subprocess.run(['addr2line','-f','-e',memoria.programa]+[hex(pc-1) for pc in pcs],
                               capture_output=True,text=True,check=True).stdout.splitlines()
        donos = dict(zip(pcs,saida[::2]))
    for linha in linhas[1:]:
        q = linha.split()
        if q[:1] == ['S'] and len(q) == 4:
            s, g, n = map(int, q[1:])
            exigir(s not in slots and 0 <= s < 9, 'slot duplicado ou inválido')
            slots[s] = (g, n)
        elif q[:1] == ['A'] and len(q) == 7:
            s, g, n, a, tam, dono = map(int, q[1:])
            exigir(s in slots and slots[s][0] == g and 0 <= n < slots[s][1] and tam > 0,
                   'slot, geração ou ordem inválidos na subalocação')
            aloc.append((s, g, n, a, tam, donos[dono]))
        else:
            raise ContextoErro('linha inválida no testemunho do BtlPool')
    exigir(set(slots) == set(range(9)), 'testemunho sem os nove slots')
    for s, (_, n) in slots.items():
        ordens = [a[2] for a in aloc if a[0] == s]
        exigir(len(ordens) == len(set(ordens)), 'subalocação duplicada')
    exigir(int(h[3]) == memoria.global_('gBtlPool'), 'gerente do BtlPool diferente do testemunho')
    return int(h[2]), int(h[3]), slots, aloc


def parear_testemunhos(dx,dp,x,p,vblank):
    vx,gx,sx,ax=registro(dx,x)
    vp,gp,sp,ap=registro(dp,p)
    exigir(vx==vp==vblank,'vblank do contexto e dos testemunhos diferente')
    exigir(sx==sp,'slot ou geração diferente entre arquiteturas')
    exigir(ax==ap,'subalocações com slot, geração, dono, endereço ou tamanho diferente')
    return gx,gp,sx,ax


class Contexto:
    def __init__(self, x, p, lx, lp, dx, dp, base, programas):
        self.x, self.p = Memoria(x, lx, False), Memoria(p, lp, True)
        self.x.programa,self.p.programa = programas
        self.mapas = {}  # endereço x86 -> mapa completo, sem repetição implícita
        self.rotulos = []
        self.trabalhos_vivos = []
        self.formas_vivas = []
        self.base = base
        self.schema = json.loads(pathlib.Path(base + '.pool.json').read_text())
        self.tipos = {n: bytes.fromhex(m) for n, m in self.schema['tipos'].items()}
        self.pool(dx[:-5] + '.pool', dp[:-5] + '.pool')
        self.script()
        self.cameras()
        self.gsc()
        self.rigidos()
        self.objetos()

    def par(self, ax, ap, off=0):
        a, b = self.x.u32(ax + off), self.p.u32(ap + off)
        exigir(a == b, f'contexto diferente em {ax + off:#x}')
        return a

    def mapa(self, nome, n):
        exigir(nome in self.tipos, f'tipo vivo sem marcas: {nome}')
        m = self.tipos[nome]
        exigir(m and n % len(m) == 0, f'{nome}: tamanho {n:#x} incompatível com {len(m):#x}')
        return bytearray(m * (n // len(m)))

    def pool(self, dx, dp):
        gx,gp,sx,ax=parear_testemunhos(dx,dp,self.x,self.p,self.schema['vblank'])
        infos = self.schema['alocacoes']
        exigir(set(map(int, infos)) == {a[3] for a in ax}, 'donos do gdb e testemunho discordam')
        self.aloc = {}
        mascara = self.par(gx, gp, 0x98) if gx else 0
        arena = self.par(gx, gp, 0x90) if gx else 0
        self.arena = arena
        # A arena inteira fica coberta por sub-blocos, enchimento ou cauda nunca entregue na geração atual.
        if arena:
            self.mapas[arena] = bytearray(b'\xff' * 0x1a0800)
        for slot in range(9):
            if not (mascara & (2 << slot)):
                continue
            base = self.par(gx, gp, slot * 16)
            cur = self.par(gx, gp, slot * 16 + 4)
            cap = self.par(gx, gp, slot * 16 + 8)
            used = self.par(gx, gp, slot * 16 + 12)
            itens = sorted((a for a in ax if a[0] == slot), key=lambda a: a[2])
            exigir([a[2] for a in itens] == list(range(sx[slot][1])), 'subalocação ausente na geração da arena')
            fim = base
            for s, g, ordem, a, n, dono in itens:
                exigir(a == fim, f'slot {slot}: lacuna ou sobreposição nas subalocações')
                fim += (n + 31) // 32 * 32
            exigir(fim == cur == base + used and used <= cap, f'slot {slot}: cursor ou capacidade inválida')
        for slot, ger, ordem, a, n, dono in ax:
            info = infos[str(a)]
            exigir(info[:3] == [slot, n, dono], f'dono do gdb não corresponde em {a:#x}')
            tipo = info[4]
            # Trabalho de tarefa é reinterpretado abaixo pela classe ativa, com a lista livre validada.
            exigir(tipo is not None or dono == 'BtlTaskList_Create', f'subalocação viva sem tipo: {dono}:{info[3]}')
            m = bytearray(b'\xfe' * n) if tipo is None else self.mapa(tipo, n)
            if tipo == 'EftGlowMgr':
                perfil = self.tipos['EftPGlowMgr']
                m[0x20:0x2a0] = perfil[0x20:0x2a0]
                # A tolerância já existente da libm do host fica só nos y/z gerados por cosf/sinf.
                # 0x54: float32 calculado pela libm, com o mesmo limite de 64 ULP da F4.
                for i in range(20):
                    for o in (0x164+i*16,0x168+i*16):m[o]=0x54
            self.aloc[a] = (n, tipo, m, f'slot {slot} geração {ger} ordem {ordem} dono {dono}:{info[3]}')
        self.formas()
        self.auras()
        self.tarefas()
        for a, (n, tipo, m, dono) in self.aloc.items():
            if mascara & (2 << infos[str(a)][0]):
                off = a - arena
                exigir(0 <= off and off + n <= 0x1a0800, 'subalocação fora da arena')
                self.mapas[arena][off:off + n] = m
            else:
                self.mapas[a] = m
            self.rotulos.append(f'BtlPool: {dono}, {tipo or "trabalho contextual"}, {n:#x} bytes')
        if arena:
            self.rotulos.append(f'BtlPool: {len(ax)} subalocações pareadas, arena coberta sem teto de classe')

    def formas(self):
        if not self.aloc: return
        hx, hp = self.x.global_('gEftHitArena'), self.p.global_('gEftHitArena')
        exigir(hx == hp and hx in self.aloc, 'arena de colisões sem dono')
        base = self.par(hx,hp,4)
        cur = self.par(hx,hp)
        cap = self.par(hx,hp,8)
        exigir(base in self.aloc and self.aloc[base][0] == cap and base <= cur <= base+cap,
               'cursor ou capacidade da arena de colisões inválido')
        m = self.aloc[base][2]
        m[:] = b'\xff'*len(m)
        lst = self.x.global_('gEftHitList')
        exigir(lst == self.p.global_('gEftHitList') and lst in self.aloc, 'lista de colisões sem dono')
        n = self.par(lst,lst,0x6400)
        exigir(n <= 64, 'lista de colisões acima da capacidade')
        lm = self.aloc[lst][2]
        lm[n*0x190:0x6400] = b'\xff'*(0x6400-n*0x190)
        formas = {}
        for i in range(n):
            rec = lst+i*0x190
            kind = self.par(rec,rec,0x70)
            exigir(kind in (0,1), 'forma viva sem tipo na arena de colisões')
            tipo = 'EftHitSphere' if kind == 0 else 'EftHitBox'
            passo = 0x20 if kind == 0 else 0x30
            for o in (0x7c,0x80):
                a = self.par(rec,rec,o)
                exigir(base <= a and a+passo <= cur, 'forma fora da geração viva da arena de colisões')
                exigir(a not in formas or formas[a] == tipo, 'forma com dois tipos diferentes')
                formas[a] = tipo
                sub = self.tipos[tipo]
                exigir(len(sub) == passo,'tipo de forma com tamanho inesperado')
                m[a-base:a-base+passo] = sub
                self.formas_vivas.append((a,sub,rec,tipo))
                self.rotulos.append(f'forma viva: registro {i}, {tipo}, +{a-base:#x}')
        # Toda alocação desta geração deve ser explicada, inclusive formas compartilhadas pelos registros.
        fim = base
        for a,tipo in sorted(formas.items()):
            exigir(a == fim, 'forma alocada sem registro vivo')
            fim += len(self.tipos[tipo])
        exigir(fim == cur, 'cauda alocada sem tipo na arena de colisões')

    def auras(self):
        if not self.aloc: return
        mgr = self.x.global_('gEftAuraPool')
        if mgr == 0 and self.p.global_('gEftAuraPool') == 0:
            # (F5, etapa 9) A cena dos menus (BtlScene_Init no ChrView e no Shenlong) não cria o gerente da aura
            # (EftAuraMgr_Init é uma tarefa da luta): ponteiro nulo nas duas e nenhuma subalocação da aura no pool.
            exigir(not any('EftAura' in a[3] for a in self.aloc.values()), 'aura nula com subalocação da aura viva')
            self.rotulos.append('aura: gerente nulo nas duas, a cena não o cria e não há subalocação dela')
            return
        exigir(mgr == self.p.global_('gEftAuraPool') and mgr in self.aloc, 'gerente da aura sem dono')
        for offbase, offlivre, offvivo, offusado, offmax, passo, link in (
                (0xc,0x10,0x14,0x24,4,0xd0,0xc0), (0x18,0x1c,0x20,0x28,8,0xb0,0xa0)):
            base = self.par(mgr,mgr,offbase)
            usado = self.par(mgr,mgr,offusado)
            cap = self.par(mgr,mgr,offmax)
            exigir(base in self.aloc and self.aloc[base][0] == cap*passo and usado <= cap,
                   'pool de partículas da aura fora da capacidade')
            marcas = self.aloc[base][2]
            marcas[usado*passo:] = b'\xff'*(len(marcas)-usado*passo)
            vistos=set()
            for off, vivo in ((offlivre,False),(offvivo,True)):
                cur=self.par(mgr,mgr,off)
                while cur:
                    exigir(base <= cur < base+usado*passo and (cur-base)%passo == 0 and cur not in vistos,
                           'partícula da aura fora do vetor ou repetida nas listas')
                    vistos.add(cur)
                    o=cur-base
                    if not vivo:
                        marcas[o:o+passo]=b'\xff'*passo
                        marcas[o+link:o+link+4]=b'\x84\x80\x80\x80'
                    elif passo==0xd0:
                        # Vec3_Copy cria offset.xyz; SpawnFlame/UpdateFlame forçam pos.w=1 após somar offset.
                        marcas[o+0x4c:o+0x50]=b'\xff'*4
                    cur=self.par(cur,cur,link)
            exigir(len(vistos)==usado, 'partícula da aura sem dono na geração corrente')
            self.rotulos.append(f'aura: {usado} partículas entre listas conferidas, passo {passo:#x}')

    def rigidos(self):
        base=self.x.global_('gStgRigid')
        exigir(base==self.p.global_('gStgRigid'),'pool de corpos rígidos diferente')
        if not base: return
        m=self.mapa('StgRigidPool',0x12010)
        usado=self.par(base,base,0x12008)
        exigir(usado<=128,'corpos rígidos acima da capacidade')
        vistos=set();ativos=0
        for campo,vivo in ((0x12000,False),(0x12004,True)):
            cur=self.par(base,base,campo);ant=0
            while cur:
                exigir(base<=cur<base+usado*0x240 and (cur-base)%0x240==0 and cur not in vistos,
                       'corpo rígido fora do vetor ou duplicado')
                vistos.add(cur);o=cur-base
                exigir(self.par(cur,cur,0xc)==o//0x240+1,'id do corpo rígido não corresponde ao slot')
                if vivo:
                    exigir(self.par(cur,cur)==ant,'anterior do corpo rígido incorreto')
                    ativos+=1
                    # Rigid_GetMatrix monta a matriz com os 3x3 e xyz; o solver usa Mat3 e Vec3.
                    for b in (0x50,0x100):
                        for r in range(4):
                            a=o+0x10+b+r*16
                            if r<3:m[a+12:a+16]=b'\xff'*4
                            else:m[a:a+16]=b'\xff'*16
                    for a in (0x40,0xf0,0x1d0,0x1f0):
                        # Rigid_SyncSpheres copia pos, mas os testes de esfera consomem xyz e radius.
                        m[o+0x10+a+12:o+0x10+a+16]=b'\xff'*4
                else:
                    m[o:o+0x240]=b'\xff'*0x240
                    for a in (4,0xc):m[o+a:o+a+4]=b'\x04\x80\x80\x80'
                ant,cur=cur,self.par(cur,cur,4)
        exigir(len(vistos)==usado and self.par(base,base,0x1200c)==ativos,'contagem dos corpos rígidos incorreta')
        for i in range(usado,128):
            o=i*0x240;m[o:o+0x240]=b'\xff'*0x240
            exigir(self.par(base+o,base+o,0xc)==i+1,'id do corpo ainda não usado incorreto')
            m[o+0xc:o+0x10]=b'\x04\x80\x80\x80'
        self.mapas[base]=m
        self.rotulos.append(f'corpos rígidos: {ativos} vivos, {usado-ativos} livres, {128-usado} nunca usados')

    def objetos(self):
        base=self.x.global_('gBtlObjWork')
        exigir(base==self.p.global_('gBtlObjWork'),'dono dos objetos da luta diferente')
        if not base:return
        m=self.mapa('BtlObjWork',0x6b740)
        cur=self.par(base,base,0x10e0c);ant=0;vistos=set()
        while cur:
            exigir(base<=cur<base+12*0x1680 and (cur-base)%0x1680==0 and cur not in vistos,
                   'objeto vivo fora do vetor ou repetido')
            vistos.add(cur)
            exigir(self.par(cur,cur)==ant,'anterior do objeto vivo incorreto')
            o=cur-base
            # BObj.push.w propaga só o w de temporários; as correntes usam Vec3 e remontam w na matriz.
            m[o+0x10+0x164c:o+0x10+0x1650]=b'\xff'*4
            ant,cur=cur,self.par(cur,cur,4)
        exigir(self.par(base,base,0x10e10)==ant and self.par(base,base,0x10e14)==len(vistos),'lista de objetos incoerente')
        self.mapas[base]=m
        self.rotulos.append(f'objetos vivos: {len(vistos)} donos conferidos, push.xyz comparado')

    def tarefas(self):
        # As listas são a autoridade da vida útil, não a aparência dos bytes de uma tarefa antiga.
        for addr, (n, tipo, marcas, dono) in self.aloc.items():
            if tipo != 'BtlTaskList':
                continue
            exigir(n == 0x18, 'BtlTaskList com tamanho inesperado')
            taskbase = self.par(addr, addr, 0x10)
            workbase = self.par(addr, addr, 0x14)
            exigir(taskbase in self.aloc and self.aloc[taskbase][1] == 'BtlTask', 'lista sem vetor de tarefas')
            tn, _, tm, _ = self.aloc[taskbase]
            count = tn // 0x40
            usados, livres = [], []
            ant = 0
            atual = self.par(addr, addr, 4)
            while atual:
                exigir(atual not in usados and taskbase <= atual < taskbase + tn and (atual-taskbase) % 0x40 == 0,
                       'tarefa viva fora do vetor ou ciclo na lista')
                exigir(self.par(atual, atual, 0x2c) == ant, 'tarefa com anterior incorreto')
                usados.append(atual)
                ant, atual = atual, self.par(atual, atual, 0x30)
            exigir(self.par(addr, addr, 8) == ant, 'cauda da lista de tarefas incorreta')
            atual = self.par(addr, addr, 0xc)
            while atual:
                exigir(atual not in usados + livres and taskbase <= atual < taskbase + tn and (atual-taskbase) % 0x40 == 0,
                       'tarefa livre fora do vetor ou ciclo na lista')
                livres.append(atual)
                atual = self.par(atual, atual, 0x34)
            exigir(len(usados) + len(livres) == count, 'tarefas sem dono na lista')
            wn = 0
            if workbase:
                exigir(workbase in self.aloc, 'lista sem subalocação do trabalho')
                wn, _, wm, _ = self.aloc[workbase]
                exigir(wn % count == 0, 'trabalho não divisível pelas tarefas')
                passo = wn // count
            for i in range(count):
                a = taskbase + i * 0x40
                exigir(self.par(a, a, 0x20) == addr and self.par(a, a, 0x38) == (workbase + i * passo if workbase else 0),
                       'dono ou trabalho da tarefa incorreto')
                if a in livres:
                    # Na criação só index, list, nextFree e work são gravados; o resto não é lido enquanto livre.
                    tm[i*0x40:(i+1)*0x40] = b'\xff'*0x40
                    for o, w in ((2,2),(0x20,4),(0x34,4),(0x38,4)):
                        tm[i*0x40+o:i*0x40+o+w] = bytes([w])+b'\x80'*(w-1)
                    if workbase: wm[i*passo:(i+1)*passo] = b'\xff'*passo
                else:
                    clsx, clsp = self.x.u32(a+0x28), self.p.u32(a+0x28)
                    # Classes vêm das tabelas de dados do jogo e têm endereços de imagem distintos.
                    callbacks = []
                    for o in range(0, 0x18, 4):
                        cx, cp = self.x.u32(clsx+o), self.p.u32(clsp+o)
                        exigir(bool(cx) == bool(cp), 'callback de classe ausente numa arquitetura')
                        if cx:
                            fx, fp = self.x.funcao(cx+1), self.p.funcao(cp+1)
                            exigir(fx == fp, 'classe ativa de tarefa diferente')
                            callbacks.append(fx)
                    fx = self.x.funcao(self.x.u32(clsx+4)+1)
                    if workbase:
                        candidatos = {self.schema['trabalhos'][f] for f in callbacks if f in self.schema['trabalhos']}
                        if fx == 'EftBlade_Init':
                            # EftBlade contém a vista EftBladeTrail em t e os 16 bytes próprios do init.
                            exigir(candidatos <= {'EftBlade', 'EftBladeTrail'}, 'vista nova do trabalho da lâmina')
                            candidatos = {'EftBlade'}
                        exigir(len(candidatos) <= 1, f'visões conflitantes do trabalho de {fx}: {sorted(candidatos)}')
                        tipo = next(iter(candidatos), None)
                        if fx == 'EftShotNullGroup_Init':
                            # Os seis callbacks desse grupo nunca usam data; só criam a lista filha.
                            sub = b'\xff'*passo
                        else:
                            exigir(tipo is not None, f'trabalho vivo sem tipo em {fx}')
                            grupos = {'EftBlastMgr_Init':4, 'EftShotTechMgr_Init':0,
                                      'EftRushShotMgr_Init':0, 'EftRingShotMgr_Init':4, 'EftMultiMgr_Init':0x210}
                            if fx in grupos:
                                # EftShotChar_Init reserva 0x2000 para qualquer variante da classe do slot.
                                # EftEmit_LoadSet usa a vista completa, maior que a vista parcial desses headers.
                                exigir(passo == 0x2000, 'capacidade do trabalho do slot diferente de 0x2000')
                                sub = bytearray(self.tipos[tipo])
                                o = grupos[fx]
                                fim = max(len(sub), o+len(self.tipos['EftSet']))
                                sub += b'\x00'*(fim-len(sub))
                                sub[o:o+len(self.tipos['EftSet'])] = self.tipos['EftSet']
                                if fx == 'EftMultiMgr_Init': sub[8:0x210] = self.tipos['EftAeTexSet32']
                                sub += b'\xff'*(passo-len(sub))
                            else:
                                sub = self.mapa(tipo, passo)
                        # Enchimento copiado dos argumentos, nunca consultado pelos callbacks destas classes.
                        sem_leitor = {'EftAnimPart_Init':(0x8c,), 'EftPtcl_Init':(0x3c,), 'EftBill_Init':(0x3c,)}
                        sub = bytearray(sub)
                        for o in sem_leitor.get(fx, ()):
                            sub[o:o+4] = b'\xff'*4
                        if fx == 'EftRibbon_Init':
                            # O helper EftRibbon_SetTexPair lê EftAbTexEntry, dois u64, não os Vec4 da outra vista.
                            sub[0x130:0x1b0] = self.tipos['EftAbTexEntry']*8
                        wm[i*passo:(i+1)*passo] = sub
                        self.trabalhos_vivos.append((workbase+i*passo,sub,a,fx))
                        self.rotulos.append(f'tarefa viva: {fx}, trabalho {tipo or "sem leitor"}, {passo:#x} bytes')

    def script(self):
        ax, ap = self.x.simbolos['gBtlScript'], self.p.simbolos['gBtlScript']
        # gBtlScript é o objeto, não um ponteiro.
        m = self.mapa('BtlScriptWork', 0xbe0)
        itens = self.tipos['BtlScriptCmdWork']
        m[:len(itens)] = itens
        m[0x354:0x3f4] = self.mapa('BtlScriptCmdWindow',0xa0)
        n = self.par(ax, ap, 0xbc4)
        exigir(n <= 50, 'requestCount fora da capacidade')
        for i in range(50):
            o = 0x8a4 + i*0x10
            if i < n:
                kind = self.par(ax, ap, o)
                exigir(kind in (1,2), 'kind vivo inválido em BtlScriptRequest')
                nome = 'BtlScriptLine' if kind == 1 else 'BtlScriptStart'
                m[o+8:o+16] = self.mapa(nome,8)
                if kind == 2:
                    acao = self.par(ax,ap,o+8)
                    exigir(10000<=acao<10052,'ação de requisição fora da tabela de eventos')
                    exigir(self.x.u32(ax+o+12)==ax+0x14c+acao-10000 and
                           self.p.u32(ap+o+12)==ap+0x14c+acao-10000,'dono do evento não corresponde à ação')
                self.rotulos.append(f'BtlScriptRequest[{i}]: kind {kind}, {nome}')
            else:
                m[o:o+16] = b'\xff'*16  # só requestCount elementos são percorridos
        for o,desl,kind in [(0x350,0,2)]+[(0xbc8+i*4,8,1) for i in range(4)]:
            px,pp=self.x.u32(ax+o),self.p.u32(ap+o)
            exigir(bool(px)==bool(pp),'requisição apontada só numa arquitetura')
            if px:
                idx=(px-ax-0x8a4-desl)//16
                exigir(0<=idx<n and px==ax+0x8a4+idx*16+desl and pp==ap+0x8a4+idx*16+desl,
                       'ponteiro de requisição fora dos registros vivos')
                exigir(self.par(ax,ap,0x8a4+idx*16)==kind,'ponteiro de requisição com kind errado')
        acao=self.par(ax,ap,0x180)
        px,pp=self.x.u32(ax+0x184),self.p.u32(ap+0x184)
        exigir((acao==0 and px==pp==0) or
               (10000<=acao<10052 and px==ax+0x14c+acao-10000 and pp==ap+0x14c+acao-10000),
               'evento corrente fora da ação dona')
        ativa=self.par(ax,ap,0x190)
        indice=self.par(ax,ap,0x198)
        exigir(ativa in (0,1) and indice<9,'estado da câmera do script inválido')
        for i in range(9):
            o=0x1a0+i*0x30
            flags=self.par(ax,ap,o)
            exigir(flags in (0,1),'flags da chave de câmera inválidas')
            if flags:
                # MoveCamera preenche só xyz; DemoCam_SetFixedPose remonta os w como 1.
                m[o+0x1c:o+0x20]=b'\xff'*4
                m[o+0x2c:o+0x30]=b'\xff'*4
        if ativa:exigir(self.par(ax,ap,0x1a0+indice*0x30)==1,'segmento da câmera sem chave viva')
        self.mapas[ax] = m

    def cameras(self):
        cam=self.x.global_('gBtlCam')
        demo=self.x.global_('gDemoCam')
        exigir(cam==self.p.global_('gBtlCam') and demo==self.p.global_('gDemoCam'),'donos das câmeras diferentes')
        if not cam:
            # (F5, etapa 9) A cena do Shenlong (ShenScene_Init) cria só a câmera demo, sem o BtlCam da luta
            # (BtlCam_Init chama DemoCam_Init junto): a demo é tipada igual, o gerente de câmeras segue nulo.
            if demo:
                self.camera_demo(demo)
                self.rotulos.append('câmeras: gerente da luta nulo, câmera demo da cena tipada')
            return
        cur=self.par(cam,cam,0xc40)
        exigir(cur in (cam+0x720,cam+0x9b0),'vista corrente fora das câmeras dos dois lados')
        for i in range(2):
            exigir(self.par(cam,cam,0x720+i*0x290+0x284)==i,'índice da vista não corresponde ao lado')
        m=self.mapa('BtlCam',0xc50)
        self.mapas[cam]=m
        exigir(demo!=0,'gerente de câmeras sem câmera demo')
        self.camera_demo(demo)
        self.rotulos.append('câmeras: três layouts, duas vistas e câmera demo tipados, donos conferidos')

    def camera_demo(self,demo):
        m=self.mapa('DemoCam',0x340)
        m[0x2b8:0x2bc]=b'\x04\x80\x80\x80'  # DemoCamPose.f.frame é s32 na união.
        flags=self.par(demo,demo,0x308)
        exigir(flags & ~15 == 0 and self.par(demo,demo,0x32c) in (0,1),'modo da câmera demo inválido')
        self.mapas[demo]=m

    def gsc(self):
        ativo = self.x.global_('gGscActive')
        exigir(ativo == self.p.global_('gGscActive'), 'GSC: ativo diferente')
        if not ativo: return
        arquivos = {}
        for raiz, livre, pool, maximo, passo, tipo in (
                ('gGscFileList','gGscFileFree','gGscFilePool','gGscFileMax',0x24,'GscFile'),
                ('gGscTaskList','gGscTaskFree','gGscTaskPool','gGscTaskMax',0x50,'GscTask')):
            base = self.x.global_(pool)
            exigir(base == self.p.global_(pool), 'GSC: base diferente')
            if not base: continue
            n = self.x.global_(maximo)
            exigir(n == self.p.global_(maximo) and 0 < n <= 1000, 'GSC: capacidade diferente ou inválida')
            m = self.mapa(tipo,n*passo)
            vistos = set()
            for nome, vivo in ((raiz,True),(livre,False)):
                ax, ap = self.x.simbolos[nome],self.p.simbolos[nome]
                cur, ant, cont = self.par(ax,ap),0,0
                while cur:
                    exigir(cur not in vistos and base <= cur < base+n*passo and (cur-base)%passo == 0,
                           'GSC: nó fora do pool, duplicado ou ciclo')
                    vistos.add(cur);cont+=1
                    exigir(self.par(cur,cur) == ant, 'GSC: anterior diferente')
                    off = cur-base
                    if not vivo: m[off+8:off+passo] = b'\xff'*(passo-8)
                    else:
                        m[off+8:off+12] = b'\xff'*4  # unk8 nunca escrito nem lido (gsc.c)
                        if tipo == 'GscFile':
                            handle = self.par(cur,cur,0xc)
                            exigir(handle not in arquivos, 'GSC: handle de arquivo duplicado')
                            code = self.par(cur,cur,0x1c)
                            data = self.par(cur,cur,0x20)
                            head = self.par(code,code,4)
                            tam = self.par(code,code,8)
                            exigir(head >= 0x10 and tam > 0, 'GSC: limites do código inválidos')
                            arquivos[handle] = (code+head,code+head+tam,data)
                        if tipo == 'GscTask':
                            handle = self.par(cur,cur,0xc) & 0xffff0000
                            exigir(handle in arquivos, 'GSC: tarefa sem arquivo dono carregado')
                            ini,fim,data = arquivos[handle]
                            exigir(self.par(cur,cur,0x14) == data, 'GSC: tarefa aponta dados de outro arquivo')
                            pc = self.par(cur,cur,0x18)
                            exigir(ini <= pc < fim and pc % 4 == 0, 'GSC: pc fora do código do arquivo dono')
                            cmd = self.par(pc,pc)
                            kind = cmd & 15
                            exigir(kind in (1,3), 'GSC: tarefa suspensa fora de comando ou chamada')
                            flags = self.par(cur,cur,0x20)
                            m[off+0x30:off+0x50] = b'\xff'*0x20
                            if kind == 1 and flags & 3:
                                cmdid = cmd >> 16
                                tblx,tblp = self.x.global_('gGscCommands'),self.p.global_('gGscCommands')
                                handler = None
                                for j in range(1000):
                                    hx,hp = self.x.u32(tblx+j*8+4),self.p.u32(tblp+j*8+4)
                                    exigir(bool(hx)==bool(hp),'GSC: tabelas de comandos diferentes')
                                    if not hx: break
                                    if self.par(tblx+j*8,tblp+j*8) == cmdid:
                                        handler = self.x.funcao(hx+1)
                                        exigir(handler == self.p.funcao(hp+1),'GSC: handler diferente')
                                        break
                                gt = self.schema.get('gsc_trabalhos',{})
                                exigir(handler in gt, f'GSC: comando vivo sem tipo: {handler}')
                                wt = gt[handler]
                                if wt:
                                    sub = self.tipos[wt]
                                    exigir(len(sub)<=0x20,'GSC: trabalho maior que a capacidade')
                                    m[off+0x30:off+0x30+len(sub)] = sub
                                self.rotulos.append(f'GSC comando: {handler}, trabalho {wt or "sem leitor"}')
                        self.rotulos.append(f'GSC vivo: {tipo}[{off//passo}]')
                    ant,cur=cur,self.par(cur,cur,4)
                exigir(self.par(ax,ap,4)==ant and self.par(ax,ap,8)==cont,'GSC: cauda ou contagem incorreta')
            exigir(len(vistos)==n,'GSC: elemento fora das listas')
            self.mapas[base]=m
