"""Referências exatas dos arquivos antecipados que uma leitura ou alocação deixou no buffer."""

# Ocorrências do mesmo trecho que ainda se comparam; acima disso o mapeamento é ambíguo.
MAX_OCORRENCIAS = 64


def ocorrencias(trecho, referencias, limite=MAX_OCORRENCIAS):
    """[(nome, posição)] de cada cópia exata de trecho nos originais, até limite + 1."""
    achadas = []
    for nome, original, _ in referencias:
        pos = original.find(trecho)
        while pos >= 0:
            achadas.append((nome, pos))
            if len(achadas) > limite:
                return achadas
            pos = original.find(trecho, pos + 1)
    return achadas


def fragmentos_antecipados(dados, inicio_minimo, referencias, avisos=None):
    """Identifica fragmentos pela x86, inclusive após a divisão de um bloco.

    Só usa âncoras de 128 bytes com ao menos 64 não nulos. Expande a igualdade
    exata até os limites do fragmento e conserva palavras completas, sem tocar
    no conteúdo vivo anterior a inicio_minimo. Uma referência nunca depende
    dos bytes PPC.

    O mapeamento tem de ser único: o trecho da x86 é procurado em todos os
    originais, em qualquer posição, e só vale se cada cópia der a mesma saída
    convertida. Conteúdo repetido dentro de um pacote de seções, ou de um
    arquivo dentro de outro, com saídas diferentes, não escolhe referência:
    o trecho fica com a comparação por palavra e o motivo vai para avisos.
    Referência com original e convertido de tamanhos diferentes é excluída.
    """
    avisos = [] if avisos is None else avisos
    validas = []
    for nome, original, convertido in referencias:
        if len(original) != len(convertido):
            avisos.append(f'referência excluída, original {len(original):#x} e convertido '
                          f'{len(convertido):#x} bytes: {nome}')
            continue
        validas.append((nome, original, convertido))
    saidas = {nome: convertido for nome, _, convertido in validas}
    candidatos = []  # (início, fim, saída esperada)
    for nome, original, convertido in validas:
        cobertos = {}
        for origem in range(0, len(original) - 127, 64):
            ancora = original[origem:origem + 128]
            if sum(b != 0 for b in ancora) < 64:
                continue
            pos = dados.find(ancora, inicio_minimo)
            while pos >= 0:
                deslocamento = pos - origem
                if deslocamento % 16 == 0 and not any(
                        a <= pos and pos + 128 <= b for a, b in cobertos.get(deslocamento, [])):
                    inicio, fim = pos, pos + 128
                    while inicio > inicio_minimo and inicio > deslocamento and (
                            dados[inicio - 1] == original[inicio - deslocamento - 1]):
                        inicio -= 1
                    while fim < len(dados) and fim - deslocamento < len(original) and (
                            dados[fim] == original[fim - deslocamento]):
                        fim += 1
                    inicio, fim = (inicio + 3) & ~3, fim & ~3
                    cobertos.setdefault(deslocamento, []).append((inicio, fim))
                    if fim > inicio:
                        trecho = dados[inicio:fim]
                        copias = ocorrencias(trecho, validas)
                        esperadas = {saidas[n][p:p + len(trecho)] for n, p in copias}
                        if len(copias) > MAX_OCORRENCIAS or len(esperadas) != 1:
                            nomes = sorted({n for n, _ in copias})
                            aviso = (f'ambíguo dados+{inicio:#x} +{fim - inicio:#x}: {len(copias)} cópias com '
                                     f'{len(esperadas)} saídas em ' + ', '.join(nomes))
                            if aviso not in avisos:
                                avisos.append(aviso)
                        else:
                            candidatos.append((inicio, fim, esperadas.pop(), {n for n, _ in copias}))
                pos = dados.find(ancora, pos + 1)
    palavras, fontes, conflito = {}, {}, set()
    for inicio, fim, esperado, nomes in candidatos:
        for p in range(inicio, fim, 4):
            palavra = esperado[p - inicio:p - inicio + 4]
            if p in palavras and palavras[p] != palavra:
                conflito.add(p)
            palavras.setdefault(p, palavra)
            fontes.setdefault(p, set()).update(nomes)
    if conflito:
        avisos.append(f'ambíguo: {len(conflito)} palavras com saídas diferentes a partir de dados+{min(conflito):#x} em '
                      + ', '.join(sorted(set().union(*(fontes[p] for p in conflito)))))
    trechos = []
    for pos in sorted(palavras):
        if pos in conflito:
            continue
        if trechos and trechos[-1][0] + len(trechos[-1][1]) == pos:
            trechos[-1][1].extend(palavras[pos])
            trechos[-1][2].update(fontes[pos])
        else:
            trechos.append((pos, bytearray(palavras[pos]), set(fontes[pos])))
    return trechos


def restos_antecipados(dados, tamanho_atual, referencias, avisos=None):
    """Protege todos os setores da leitura atual antes de procurar os restos."""
    return fragmentos_antecipados(dados, (tamanho_atual + 0x7ff) & ~0x7ff, referencias, avisos)
