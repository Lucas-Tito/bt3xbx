#!/usr/bin/env python3
"""Leituras limitadas do conteúdo, sem relocação nem execução do jogo.

Tabelas: Res/MPACK_AT; BPE: src/sys/bpe.c; GSCF: src/sys/gsc.c.
Aceita a árvore extraída ou uma ISO (original ou convertida). Nunca grava dados.
"""
from functools import lru_cache
from pathlib import Path
import struct

from convert_iso import inventario, ler, MARCA_POS, MARCA_TAM, MAGIA


def exigir(condicao, mensagem):
    if not condicao:
        raise ValueError(mensagem)


class Bloco(bytes):
    """A ISO convertida contém cabeçalhos nativos e BPE cujo miolo continua PS2."""
    def __new__(cls, b, ordem):
        obj = super().__new__(cls, b)
        obj.ordem = ordem
        return obj

    def __getitem__(self, chave):
        b = super().__getitem__(chave)
        return Bloco(b, self.ordem) if isinstance(chave, slice) else b


class Dados:
    def __init__(self, origem):
        self.origem = Path(origem).resolve()
        self.iso = self.origem.is_file()
        self.entradas = None
        self.convertida = False
        if self.iso:
            with self.origem.open('rb') as f:
                self.entradas = inventario(f)
                self.convertida = ler(f, MARCA_POS, MARCA_TAM).startswith(MAGIA)
        cab = self.arquivo(3)
        # comum3 tem duas seções; no disco convertido só os números mudam.
        self.ordem = '<' if struct.unpack_from('<I', cab)[0] == 2 else '>'
        exigir(self.u32(cab, 0) == 2, 'comum3: tabela incompatível')

    @lru_cache(maxsize=16)
    def arquivo(self, indice):
        nome = f'pzs3us1/{indice:05d}.bin'
        if self.iso:
            exigir(nome in self.entradas, f'falta {nome}')
            with self.origem.open('rb') as f:
                return ler(f, *self.entradas[nome])
        return (self.origem / nome).read_bytes()

    def u32(self, b, pos=0):
        return struct.unpack_from(getattr(b, 'ordem', self.ordem) + 'I', b, pos)[0]

    def s32(self, b, pos=0):
        return struct.unpack_from(getattr(b, 'ordem', self.ordem) + 'i', b, pos)[0]

    def u16(self, b, pos=0):
        return struct.unpack_from(getattr(b, 'ordem', self.ordem) + 'H', b, pos)[0]

    def secao(self, b, n):
        total = self.u32(b)
        exigir(0 < total < 10000 and 1 <= n <= total, f'seção {n}: tabela inválida ({total})')
        ini, fim = self.u32(b, n * 4) & ~3, self.u32(b, (n + 1) * 4) & ~3
        exigir((total + 2) * 4 <= ini <= fim <= len(b), f'seção {n}: limites inválidos')
        return Bloco(b[ini:fim], getattr(b, 'ordem', self.ordem))

    def pacote(self, indice, secao=None):
        b = self.arquivo(indice)
        return self.bpe(self.secao(b, secao) if secao else b)

    def bpe(self, b):
        tamanho, comprimido = self.u32(b), self.u32(b, 4)
        exigir(0 < tamanho <= 16 * 1024 * 1024 and 8 + comprimido <= len(b), 'BPE: limites inválidos')
        entrada = memoryview(b)[8:8 + comprimido]
        pos, saida = 0, bytearray()

        def proximo():
            nonlocal pos
            exigir(pos < len(entrada), 'BPE truncado')
            valor = entrada[pos]
            pos += 1
            return valor

        while pos < len(entrada):
            esquerda, direita = list(range(256)), [0] * 256
            contador, c = proximo(), 0
            while True:
                if contador > 127:
                    c += contador - 127
                    contador = 0
                exigir(c <= 256, 'BPE: tabela excedida')
                if c == 256:
                    break
                for _ in range(contador + 1):
                    exigir(c < 256, 'BPE: tabela excedida')
                    esquerda[c] = proximo()
                    if c != esquerda[c]:
                        direita[c] = proximo()
                    c += 1
                if c == 256:
                    break
                contador = proximo()
            bloco = proximo() * 256 + proximo()
            for _ in range(bloco):
                pilha = [proximo()]
                passos = 0
                while pilha:
                    passos += 1
                    exigir(passos <= tamanho * 2 + 256, 'BPE: expansão cíclica')
                    c = pilha.pop()
                    if esquerda[c] == c:
                        saida.append(c)
                        exigir(len(saida) <= tamanho, 'BPE: saída excedida')
                    else:
                        pilha.extend((direita[c], esquerda[c]))
                        exigir(len(pilha) <= 256, 'BPE: pilha excedida')
        exigir(len(saida) == tamanho, 'BPE: tamanho final incorreto')
        return Bloco(saida, '<')

    def gsc(self, indice):
        """Ações e comandos com operandos/ opções, sem executar saltos ou condições."""
        b = self.arquivo(indice)

        def bloco(pos, etiqueta):
            # As etiquetas são números invertidos por GSC_SWAP no jogo.
            tag = b[pos:pos + 4]
            exigir(tag in (etiqueta, etiqueta[::-1]), f'GSC: falta {etiqueta!r}')
            cab, tam = self.u32(b, pos + 4), self.u32(b, pos + 8)
            exigir(cab == 16 and pos + cab + tam + 16 <= len(b), 'GSC: limites inválidos')
            return pos + cab, pos + cab + tam, pos + cab + tam + 16

        ini, fim, _ = bloco(0, b'GSCF')
        _, _, codigo = bloco(ini, b'GSHD')
        acini, acfim, constantes = bloco(codigo, b'GSCD')
        vini, vfim, _ = bloco(constantes, b'GSDT')
        valores = b[vini:vfim]

        def valor(palavra):
            exigir(palavra & 15 == 10, 'GSC: operando inválido')
            n = palavra >> 8
            exigir(n * 4 + 4 <= len(valores), 'GSC: constante fora da tabela')
            return self.s32(valores, n * 4)

        acoes = []
        while acini < acfim:
            cini, cfim, prox = bloco(acini, b'GSAC')
            acao = self.s32(b, acini + 12)
            comandos = []
            while cini < cfim:
                w = self.u32(b, cini)
                cini += 4
                tipo = w & 15
                if tipo == 0:
                    break
                if tipo in (2, 3):
                    continue
                exigir(tipo == 1, f'GSC: palavra inesperada {tipo}')
                args = []
                for _ in range((w >> 8) & 255):
                    args.append(valor(self.u32(b, cini)))
                    cini += 4
                opcoes = {}
                while cini < cfim and self.u32(b, cini) & 8:
                    ow = self.u32(b, cini)
                    cini += 4
                    exigir(ow & 15 == 8, 'GSC: opção inesperada')
                    letra, num = chr((ow >> 8) & 255), (ow >> 16) & 255
                    opcoes[letra] = [valor(self.u32(b, cini + k * 4)) for k in range(num)]
                    cini += num * 4
                comandos.append((w >> 16, args, opcoes))
            acoes.append((acao, comandos))
            acini = prox
        exigir(acini == acfim, 'GSC: fim das ações incorreto')
        return acoes
