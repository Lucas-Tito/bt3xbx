"""Instrumentação apenas das cópias de teste, sem alterar os fontes upstream.

BT3_VU0_CONTA=1 na compilação de headless.py cria x86vc/ppcvc.
As chamadas contam inclusive funções inline. Nomes repetidos em ramos de
pré-processador usam a mesma entrada. Não somar auxiliares como trabalho exclusivo.
"""
import re
PADRAO = re.compile(r'(?m)^(?:static\s+)?(?:VN_SEM_INLINE\s+)?(?:inline\s+)?(?:unsigned\s+)?(?:void|int|float|double|uint32_t|uint64_t|int32_t|RefVec4f|RefVec4i|u32|VnBits)\s+\*?\s*(\w+)\s*\([^;{}]*\)\s*\{')

class Contador:
    def __init__(self, raiz, pasta):
        self.raiz, self.pasta, self.nomes = raiz, pasta / 'conta', []
        self.pasta.mkdir(parents=True, exist_ok=True)

    def preparar(self, fonte, grupo):
        texto = fonte.read_text()
        if grupo == "ponte":
            original = (self.raiz / "port/src/plat_fastvec.c").read_text()
            original = original.replace("Port_FastMtxApply", "Vn_MatrizReferencia")
            texto = texto.replace('#include "../../src/plat_fastvec.c"', original)
        def entrada(m):
            nome = grupo + '/' + m[1]
            if nome not in self.nomes:
                self.nomes.append(nome)
            i = self.nomes.index(nome)
            return m[0] + f'\n    ++gVcChamadas[{i}];'
        texto = PADRAO.sub(entrada, texto)
        # Retornos efetivos, motivos classificados só no recuo. A ponte e Bc4
        # herdam o motivo publicado pelo núcleo, depois da chamada terminar.
        for nome in ['Vn_Op4', 'Vn_MtxApply', 'Vn_Bc4', 'Port_FastMtxApply']:
            m = next((m for m in PADRAO.finditer(texto) if m[1] == nome), None)
            if not m: continue
            inicio, fim, nivel = m.end(), m.end(), 1
            while nivel:
                if texto[fim] == '{': nivel += 1
                if texto[fim] == '}': nivel -= 1
                fim += 1
            corpo = texto[inicio:fim]
            i = self.nomes.index(grupo + '/' + nome)
            if nome == 'Vn_Op4':
                motivo = 'Vc_Motivo(a,4,b,4,acc,acc ? 4 : 0)'
            elif nome == 'Vn_MtxApply':
                motivo = 'Vc_Motivo(r,16,v,4,0,0)'
            else: motivo = '-1'
            corpo = re.sub(r'\breturn\s+([^;]+);', lambda r: f'return Vc_Resultado({i}, ({r[1]}), {motivo if r[1].strip() == "0" else "-1"});', corpo)
            texto = texto[:inicio] + corpo + texto[fim:]
        # Includes relativos continuam resolvidos no diretório do fonte original.
        texto = re.sub(r'#include "([^"\n]+)"', lambda m: '#include "' + str((fonte.parent / m[1]).resolve()) + '"' if (fonte.parent / m[1]).exists() else m[0], texto)
        copia = self.pasta / (grupo + '.c')
        copia.write_text('#include "vu0_conta.h"\n' + texto)
        return copia

    def escrever(self):
        # Inventário inclui funções que o headless não liga: zero explícito.
        for m in PADRAO.finditer((self.raiz / 'port/x360/src/f2/float360.c').read_text()):
            n = 'float360/' + m[1]
            if n not in self.nomes: self.nomes.append(n)
        (self.pasta / 'nomes.inc').write_text('\n'.join('"' + n + '",' for n in self.nomes))
