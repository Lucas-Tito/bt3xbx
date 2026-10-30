#!/usr/bin/env python3
"""Exercita a identificação de sufixos sem aceitar bytes crus ou alterados."""
from random import Random
import struct
from memoria_menus import bpe_inteiro, fragmentos_antecipados, restos_antecipados


def main():
    pacote = struct.pack('<II', 6, 64) + Random(451).randbytes(512)
    assert not bpe_inteiro(pacote, None), 'pacote de seções aceito como BPE'
    bpe = struct.pack('<II', 1024, 512) + pacote
    assert bpe_inteiro(bpe, 1024)
    assert not bpe_inteiro(bpe, None), 'BPE sem conteúdo registrado aceito'
    assert not bpe_inteiro(bpe, 1023), 'tamanho do conteúdo incorreto aceito'
    assert not bpe_inteiro(bpe[:64], 1024), 'fluxo truncado aceito'
    print('estragos pacote de seções, origem ausente, tamanho e truncamento: recusados')
    antigo = Random(741).randbytes(0x3100)
    convertido = bytes(b ^ 0x5a for b in antigo)
    atual = b'\x33' * 0x1800
    dados = atual + antigo[len(atual):]
    refs = [('anterior', antigo, convertido)]
    trechos = restos_antecipados(dados, len(atual), refs)
    assert [(a, len(b)) for a, b, _ in trechos] == [(0x1800, 0x1900)]
    def aceita(ppc):
        return all(ppc[a:a + len(b)] == b for a, b, _ in trechos)
    assert aceita(atual + convertido[len(atual):])
    assert not aceita(dados), 'sufixo cru aceito'
    ruim = bytearray(atual + convertido[len(atual):]); ruim[0x2000] ^= 1
    assert not aceita(ruim), 'byte alterado aceito'
    print('estragos sufixo cru e byte: reprovados')
    # O setor inteiro substituído pela carga atual não pertence ao arquivo velho.
    dados = atual + b'\x33' * 0x800 + antigo[0x2000:]
    assert restos_antecipados(dados, 0x1801, refs)[0][0] == 0x2000
    for rotulo, x, tamanho, referencias in [
        ('posição', b'\x00' * 4 + dados, len(atual), refs),
        ('sem origem', dados, len(atual), []),
        ('conteúdo curto', antigo, len(antigo) - 32, refs),
        ('só zeros', bytes(len(antigo)), len(atual), [('zeros', bytes(len(antigo)), convertido)]),
        ('arquivo atual', antigo, len(antigo), refs),
    ]:
        assert not restos_antecipados(x, tamanho, referencias), rotulo
        print(f'estrago {rotulo}: sem referência aceita')
    try:
        restos_antecipados(dados, len(atual), refs + [('outra saída', antigo, antigo)])
    except ValueError:
        print('estrago referências conflitantes: recusado')
    else:
        raise AssertionError('referências conflitantes aceitas')
    # O alocador dividiu o bloco: um pedaço termina no limite do buffer e
    # outro começa no meio do arquivo, sem o cabeçalho BPE original.
    for rotulo, dados, esperado in [
        ('cabeça deslocada', b'\x33' * 0x1800 + antigo[:0x1000],
         [(0x1800, convertido[:0x1000])]),
        ('cauda cortada', antigo[0x1000:0x2100], [(0, convertido[0x1000:0x2100])]),
        ('dois fragmentos', antigo[:0x1000] + b'\x33' * 0x800 + antigo[0x2000:],
         [(0, convertido[:0x1000]), (0x1800, convertido[0x2000:])]),
    ]:
        trechos = fragmentos_antecipados(dados, 0, refs)
        assert [(a, bytes(b)) for a, b, _ in trechos] == esperado, rotulo
        ppc = bytearray(dados)
        for a, b in esperado:
            ppc[a:a + len(b)] = b
        assert all(ppc[a:a + len(b)] == b for a, b, _ in trechos)
        assert not all(dados[a:a + len(b)] == b for a, b, _ in trechos), rotulo
        ppc[trechos[0][0] + 64] ^= 1
        assert not all(ppc[a:a + len(b)] == b for a, b, _ in trechos), rotulo
        print(f'estragos {rotulo}, cru e byte: reprovados')
    assert not fragmentos_antecipados(antigo, len(antigo), refs), 'conteúdo vivo coberto'
    assert not fragmentos_antecipados(antigo[:127], 0, refs), 'âncora curta aceita'
    assert not fragmentos_antecipados(antigo, 0, []), 'fragmento sem origem aceito'
    try:
        fragmentos_antecipados(antigo, 0, refs + [('outra saída', antigo, antigo)])
    except ValueError:
        print('estrago fragmentos conflitantes: recusado')
    else:
        raise AssertionError('fragmentos conflitantes aceitos')
    print('teste_memoria_menus: tudo certo')


if __name__ == '__main__':
    main()
