#!/usr/bin/env python3
"""Exercita a identificação de sufixos sem aceitar bytes crus ou alterados."""
from memoria_menus import restos_antecipados


def main():
    antigo = bytes((i * 13 + i // 251) % 256 for i in range(0x3100))
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
    print('teste_memoria_menus: tudo certo')


if __name__ == '__main__':
    main()
