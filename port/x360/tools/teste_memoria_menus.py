#!/usr/bin/env python3
"""Exercita a identificação de sufixos sem aceitar bytes crus ou alterados."""
from random import Random
from memoria_menus import fragmentos_antecipados, restos_antecipados


def main():
    antigo = Random(741).randbytes(0x3100)
    convertido = bytes(b ^ 0x5a for b in antigo)
    atual = b'\x33' * 0x1800
    dados = atual + antigo[len(atual):]
    refs = [('anterior', antigo, convertido)]
    trechos = restos_antecipados(dados, len(atual), refs)
    assert [(a, len(b)) for a, b, _ in trechos] == [(0x1800, 0x1900)]
    def aceita(ppc, trechos=None):
        return all(ppc[a:a + len(b)] == b for a, b, _ in (trechos if trechos is not None else ultimos))
    ultimos = trechos
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
    # Duas referências com o mesmo original e saídas diferentes: nenhuma escolhe a saída.
    avisos = []
    assert not restos_antecipados(dados, len(atual), refs + [('outra saída', antigo, antigo)], avisos)
    assert avisos and all(a.startswith('ambíguo') for a in avisos), avisos
    print('estrago referências conflitantes: recusado e registrado')
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
        assert aceita(ppc, trechos)
        assert not aceita(dados, trechos), rotulo
        ppc[trechos[0][0] + 64] ^= 1
        assert not aceita(ppc, trechos), rotulo
        print(f'estragos {rotulo}, cru e byte: reprovados')
    assert not fragmentos_antecipados(antigo, len(antigo), refs), 'conteúdo vivo coberto'
    assert not fragmentos_antecipados(antigo[:127], 0, refs), 'âncora curta aceita'
    assert not fragmentos_antecipados(antigo, 0, []), 'fragmento sem origem aceito'
    avisos = []
    assert not fragmentos_antecipados(antigo, 0, refs + [('outra saída', antigo, antigo)], avisos)
    assert avisos, 'fragmentos conflitantes sem aviso'
    print('estrago fragmentos conflitantes: recusado e registrado')

    # (mutante h) A expansão para trás não entra no conteúdo vivo, mesmo quando ele é igual ao arquivo velho.
    trechos = fragmentos_antecipados(antigo, 0x1800, refs)
    assert trechos and min(a for a, _, _ in trechos) >= 0x1800, 'expansão entrou no conteúdo vivo'
    print('estrago expansão sobre o conteúdo vivo: recusado')
    # (mutante i) Original e convertido de tamanhos diferentes: a referência sai, com aviso.
    avisos = []
    assert not fragmentos_antecipados(antigo, 0, [('curto', antigo, convertido[:-4])], avisos), \
        'referência de tamanhos diferentes usada'
    assert avisos and avisos[0].startswith('referência excluída'), avisos
    print('estrago tamanhos diferentes: referência excluída e registrada')

    # Pacote de seções como referência (risco 1 da revisão): resto cru reprova, convertido passa.
    pacote = Random(451).randbytes(0x20000)
    pacote_conv = bytes(b ^ 0x3c for b in pacote)
    retrato = Random(760).randbytes(0x4000)
    retrato_conv = bytes(b ^ 0xa5 for b in retrato)
    refs2 = [('pacote', pacote, pacote_conv), ('retrato', retrato, retrato_conv)]
    vivo = b'\x44' * 0x1000
    dados = vivo + pacote[0x8000:0xa000]
    trechos = fragmentos_antecipados(dados, 0x1000, refs2)
    assert [(a, bytes(b)) for a, b, _ in trechos] == [(0x1000, pacote_conv[0x8000:0xa000])], 'pacote sem referência'
    assert aceita(vivo + pacote_conv[0x8000:0xa000], trechos)
    assert not aceita(dados, trechos), 'resto de pacote cru aceito'
    print('estrago resto de pacote cru: reprovado; convertido aceito')
    # Conteúdo de um arquivo repetido dentro de outro (risco 4): o trecho curto não escolhe o retrato; o longo,
    # único, fica com o pacote, inclusive sobre a parte repetida.
    pacote_rep = bytearray(pacote)
    pacote_rep[0x9000:0x9200] = retrato[0x1000:0x1200]
    pacote_rep = bytes(pacote_rep)
    refs3 = [('pacote', pacote_rep, bytes(b ^ 0x3c for b in pacote_rep)), ('retrato', retrato, retrato_conv)]
    avisos = []
    dados = vivo + pacote_rep[0x8000:0xa000]
    trechos = fragmentos_antecipados(dados, 0x1000, refs3, avisos)
    assert [(a, bytes(b)) for a, b, _ in trechos] == [(0x1000, refs3[0][2][0x8000:0xa000])], 'repetido mal atribuído'
    assert any('retrato' in a for a in avisos), avisos
    print('estrago conteúdo repetido entre arquivos: atribuído ao trecho único e registrado')
    # Conteúdo repetido dentro do mesmo pacote com saídas diferentes (o Disc Fusion): nenhuma referência.
    bloco = Random(29).randbytes(0x200)
    pacote_dup = bytearray(pacote)
    pacote_dup[0x4000:0x4200] = bloco
    pacote_dup[0x6000:0x6200] = bloco
    conv_dup = bytearray(b ^ 0x3c for b in pacote_dup)
    conv_dup[0x6000:0x6200] = bytes(b ^ 0x77 for b in bloco)
    avisos = []
    dados = vivo + b'\x55' * 0x40 + bloco + b'\x66' * 0x40
    assert not fragmentos_antecipados(dados, 0x1000, [('pacote', bytes(pacote_dup), bytes(conv_dup))], avisos), \
        'repetido com saídas diferentes aceito'
    assert avisos, 'repetido sem aviso'
    # Repetido com a mesma saída (arquivo duplicado): a saída é única e vale.
    conv_dup[0x6000:0x6200] = conv_dup[0x4000:0x4200]
    trechos = fragmentos_antecipados(dados, 0x1000, [('pacote', bytes(pacote_dup), bytes(conv_dup))])
    assert [(a, bytes(b)) for a, b, _ in trechos] == [(0x1040, bytes(conv_dup[0x4000:0x4200]))], trechos
    print('estrago repetido com saídas diferentes: recusado; com a mesma saída: aceito')
    # Âncora com menos de 64 bytes não nulos não identifica origem, mesmo única.
    esparso = bytearray(0x1000)
    for i in range(0x800, 0x880, 4):
        esparso[i] = (i >> 2) & 0xff or 1
    esparso = bytes(esparso)
    dados = vivo + b'\x77' * 0x40 + esparso[0x800:0x880] + b'\x77' * 0x40
    assert not fragmentos_antecipados(dados, 0x1000, [('esparso', esparso, bytes(b ^ 0x11 for b in esparso))]), \
        'âncora esparsa aceita'
    print('estrago âncora esparsa: sem referência aceita')
    # Dois trechos únicos que se sobrepõem com saídas diferentes: a sobreposição não escolhe nenhuma.
    s1, s2, s3 = (Random(k).randbytes(0x200) for k in (1, 2, 3))
    x = bytearray(Random(4).randbytes(0x1000)); x[0x400:0x800] = s1 + s2
    y = bytearray(Random(5).randbytes(0x1000)); y[0x800:0xc00] = s2 + s3
    x, y = bytes(x), bytes(y)
    xc, yc = bytes(b ^ 0x3c for b in x), bytes(b ^ 0xa5 for b in y)
    avisos = []
    trechos = fragmentos_antecipados(vivo + s1 + s2 + s3, 0x1000, [('x', x, xc), ('y', y, yc)], avisos)
    assert [(a, bytes(b)) for a, b, _ in trechos] == [(0x1000, xc[0x400:0x600]), (0x1400, yc[0xa00:0xc00])], \
        'sobreposição conflitante aceita'
    assert any(a.startswith('ambíguo:') for a in avisos), avisos
    print('estrago sobreposição com saídas diferentes: recusada e registrada')
    print('teste_memoria_menus: tudo certo')


if __name__ == '__main__':
    main()
