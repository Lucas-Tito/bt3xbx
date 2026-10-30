"""Referências exatas dos arquivos BPE que uma leitura ou alocação deixou no buffer."""

import struct


def bpe_inteiro(original, tamanho_conteudo):
    """Exige o conteúdo registrado em offset zero, além do cabeçalho plausível.

    Um pacote de seções também começa com dois inteiros pequenos. Isso não
    o torna BPE nem autoriza usar seus bytes como restos de um retrato.
    """
    if tamanho_conteudo is None or len(original) < 8:
        return False
    bruto, comprimido = struct.unpack_from('<II', original)
    return (0 < bruto <= 0x900000 and bruto == tamanho_conteudo
            and 0 < comprimido <= len(original) - 8)


def fragmentos_antecipados(dados, inicio_minimo, referencias):
    """Identifica fragmentos pela x86, inclusive após a divisão de um bloco.

    Só usa âncoras de 128 bytes com ao menos 64 não nulos. Expande a igualdade
    exata até os limites do fragmento e conserva palavras completas, sem tocar
    no conteúdo vivo anterior a inicio_minimo. Uma referência nunca depende
    dos bytes PPC. Saídas conflitantes interrompem a comparação.
    """
    palavras, fontes = {}, {}
    for nome, original, convertido in referencias:
        if len(original) != len(convertido):
            raise ValueError(f'tamanhos antecipados diferentes: {nome}')
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
                    for p in range(inicio, fim, 4):
                        esperado = convertido[p - deslocamento:p - deslocamento + 4]
                        if p in palavras and palavras[p] != esperado:
                            raise ValueError(f'fragmento antecipado ambíguo em {p:#x}: {nome}')
                        palavras[p] = esperado
                        fontes.setdefault(p, set()).add(nome)
                pos = dados.find(ancora, pos + 1)
    trechos = []
    for pos in sorted(palavras):
        if trechos and trechos[-1][0] + len(trechos[-1][1]) == pos:
            trechos[-1][1].extend(palavras[pos])
            trechos[-1][2].update(fontes[pos])
        else:
            trechos.append((pos, bytearray(palavras[pos]), set(fontes[pos])))
    return trechos


def restos_antecipados(dados, tamanho_atual, referencias):
    """Protege todos os setores da leitura atual antes de procurar os restos."""
    return fragmentos_antecipados(dados, (tamanho_atual + 0x7ff) & ~0x7ff, referencias)
