"""Referências exatas dos sufixos BPE que uma leitura menor deixou no buffer."""


def restos_antecipados(dados, tamanho_atual, referencias):
    """Retorna [(início, bytes esperados, origens)] sem consultar a memória PPC.

    File_Request substitui setores de 0x800 bytes. Só reconhece um sufixo
    inteiro de arquivo anteriormente lido, na mesma posição, após o último
    setor do arquivo atual. Exige 64 bytes não nulos para identificar a origem.
    Não reconhece conteúdo descomprimido nem dispensa bytes pelo tamanho.
    """
    inicio_minimo = (tamanho_atual + 0x7ff) & ~0x7ff
    palavras, fontes = {}, {}
    for nome, original, convertido in referencias:
        fim = len(original)
        if fim > len(dados) or fim <= inicio_minimo or fim % 4 or len(convertido) != fim:
            continue
        inicio = fim
        while inicio > inicio_minimo and dados[inicio - 1] == original[inicio - 1]:
            inicio -= 1
        inicio = (inicio + 0x7ff) & ~0x7ff
        if sum(b != 0 for b in original[inicio:fim]) < 64:
            continue
        for pos in range(inicio, fim, 4):
            esperado = convertido[pos:pos + 4]
            if pos in palavras and palavras[pos] != esperado:
                raise ValueError(f'sufixo antecipado ambíguo em {pos:#x}: {nome}')
            palavras[pos] = esperado
            fontes.setdefault(pos, set()).add(nome)
    trechos = []
    for pos in sorted(palavras):
        if trechos and trechos[-1][0] + len(trechos[-1][1]) == pos:
            trechos[-1][1].extend(palavras[pos])
            trechos[-1][2].update(fontes[pos])
        else:
            trechos.append((pos, bytearray(palavras[pos]), set(fontes[pos])))
    return trechos
