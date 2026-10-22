#!/usr/bin/env python3
"""F6, etapa 2, rede 3: decodificadores de referência do som, escritos de novo a partir do formato, sem o código do port.

Fontes do formato (descrições públicas, não o C do port):
  - ADX da CRI (tipo 3, 4 bits em quadros de 18 bytes): o cabeçalho big-endian (0x8000, tamanho do cabeçalho menos 4,
    tipo, tamanho do quadro, bits, canais, taxa, amostras, frequência do passa-alta, versão, bandeiras; na versão 4 o
    laço em 0x24: bandeira, amostra e byte do começo, amostra e byte do fim), os coeficientes da predição pela fórmula
    do passa-alta (a = sqrt(2) - cos(2 pi f / taxa), b = sqrt(2) - 1, c = (a - sqrt((a + b)(a - b))) / b, c1 = c * 8192,
    c2 = -c^2 * 4096, em inteiro) e o quadro (escala de 16 bits, 32 nibbles com sinal, o mais alto primeiro; amostra =
    nibble * escala + (c1 * anterior + c2 * a de antes) >> 12, saturada em 16 bits). Duas dúvidas do formato ficam
    como variantes, para o relatório medir o quanto cada uma muda: a escala lida com +1 ou sem (o vgmstream soma 1, o
    artigo da Wikipédia e o ffmpeg não), e o inteiro do coeficiente truncado ou arredondado (o ffmpeg arredonda). O laço:
    toca [0, fim do laço) e volta ao começo do laço com o estado do decodificador de quando passou por lá (o que o
    vgmstream faz); as variantes "continua" (segue com o histórico do fim) e "zera" (histórico zerado na volta) servem
    para dizer qual o port segue.
  - ADPCM do chip de som do PS2 (o do PS1, "SPU ADPCM" do psx-spx): blocos de 16 bytes; byte 0 com o deslocamento no
    nibble baixo e o filtro no alto (bits 4 a 6), byte 1 com as bandeiras (1 fim do trecho, 2 repete, 4 começo do
    laço), 14 bytes com 28 nibbles, o baixo primeiro; amostra = (nibble << 12) >> deslocamento + (anterior * f0 + a de
    antes * f1 + 32) >> 6, saturada; f0, f1 = (0, 0), (60, 0), (115, -52), (98, -55), (122, -60); deslocamento acima
    de 12 vale 9. Filtro 5 a 7 não tem coeficiente na tabela: aqui vale o 0, e o censo conta quantos blocos o usam.
  - Banco de efeitos (os formatos da Sony, little-endian, nomes dos pedaços invertidos, "IECSigaV" = SCEI Vagi): o
    arquivo começa com a contagem de partes (3) e os deslocamentos de cada parte e do fim; a parte com o "Vagi" é a
    das tabelas, a com o "Sesq" a das sequências, a outra a das amostras. Vagi: +0x0C o maior número de amostra,
    +0x10 os deslocamentos (do começo do pedaço) dos registros de 8 bytes {u32 deslocamento na parte das amostras, u16
    taxa, u8 laço, u8}.
Nada aqui lê o port; nenhum byte do jogo vai para o repositório (os testes leem os dados do WSL).
"""
import array, math, struct, sys


# ------------------------------------------------------------------------------------------------------------ ADX

class Adx:
    def __init__(self, b):
        if len(b) < 0x40 or b[0] != 0x80 or b[1] != 0x00:
            raise ValueError("não é ADX (sem 0x8000)")
        self.dados = struct.unpack_from(">H", b, 2)[0] + 4
        (self.tipo, self.quadro, self.bits, self.canais, self.taxa, self.amostras, self.corte, self.versao,
         self.bandeiras) = struct.unpack_from(">BBBBIIHBB", b, 4)
        self.laco = False
        self.laco_ini = self.laco_fim = 0
        if self.versao == 4 and self.dados >= 0x38:
            bandeira, ini, _, fim, _ = struct.unpack_from(">IIIII", b, 0x24)
            if bandeira:
                self.laco, self.laco_ini, self.laco_fim = True, ini, fim
        elif self.versao == 3 and self.dados >= 0x2C:
            bandeira, ini, _, fim, _ = struct.unpack_from(">IIIII", b, 0x18)
            if bandeira:
                raise ValueError("ADX versão 3 com laço: fora do que o jogo usa")
        self.b = b

    def coefs(self, arredonda=False):
        a = math.sqrt(2.0) - math.cos(2.0 * math.pi * self.corte / self.taxa)
        b = math.sqrt(2.0) - 1.0
        c = (a - math.sqrt((a + b) * (a - b))) / b
        if arredonda:
            return int(round(c * 8192.0)), int(round(-(c * c) * 4096.0))
        return int(c * 8192.0), int(c * c * -4096.0)

    def decodifica(self, ate, escala_mais_um=True, arredonda=False, hist=None, quadro_ini=0):
        """As amostras de cada canal dos quadros quadro_ini.. até cobrir a amostra `ate` (exclusiva, contada do
        quadro_ini * 32), ou até onde o arquivo tiver quadros inteiros. Devolve (lista de array('h') por canal,
        histórico no fim)."""
        if self.tipo != 3 or self.quadro != 18 or self.bits != 4 or self.canais not in (1, 2):
            raise ValueError(f"ADX de outro tipo: tipo {self.tipo}, quadro {self.quadro}, bits {self.bits}, {self.canais} canais")
        c1, c2 = self.coefs(arredonda)
        b, nc = self.b, self.canais
        mais = 1 if escala_mais_um else 0
        sinal = [n - 16 if n & 8 else n for n in range(16)]
        saida = [array.array("h") for _ in range(nc)]
        h = [list(x) for x in (hist or [[0, 0]] * nc)]
        quadros = (ate + 31) // 32
        for q in range(quadro_ini, quadro_ini + quadros):
            for ch in range(nc):
                o = self.dados + (q * nc + ch) * 18
                if o + 18 > len(b):
                    return saida, h, True
                escala = (b[o] << 8 | b[o + 1]) + mais
                h1, h2 = h[ch]
                out = saida[ch]
                for k in range(o + 2, o + 18):
                    byte = b[k]
                    for nib in (sinal[byte >> 4], sinal[byte & 15]):
                        s = nib * escala + ((c1 * h1 + c2 * h2) >> 12)
                        if s > 32767:
                            s = 32767
                        elif s < -32768:
                            s = -32768
                        out.append(s)
                        h2, h1 = h1, s
                h[ch] = [h1, h2]
        return saida, h, False

    def toca(self, depois=0, escala_mais_um=True, arredonda=False, laco="restaura"):
        """O que um player toca, em estéreo intercalado (array('h')): o arquivo inteiro, ou com laço [0, fim do laço)
        e mais `depois` amostras a partir da volta. laco: "restaura" (a referência), "continua" ou "zera"."""
        fim = self.laco_fim if self.laco else self.amostras
        chave = (escala_mais_um, arredonda)
        if getattr(self, "_passada", (None,))[0] != chave:  # (a primeira passada serve às três variantes do laço)
            self._passada = (chave, self.decodifica(fim, escala_mais_um, arredonda))
        canais, h, curto = self._passada[1]
        n = min(fim, len(canais[0]))
        partes = [array.array("h", c[:n]) for c in canais]
        if self.laco and depois > 0 and not curto:
            ini = self.laco_ini
            if laco == "restaura":
                # o estado de quando passou pelo começo do laço: as amostras da primeira passada
                if ini + depois > n:
                    raise ValueError("laço curto demais para a janela pedida")
                for ch in range(self.canais):
                    partes[ch].extend(canais[ch][ini:ini + depois])
            else:
                q0 = ini // 32
                hist = h if laco == "continua" else None
                novos, _, _ = self.decodifica((ini - q0 * 32) + depois, escala_mais_um, arredonda, hist, q0)
                for ch in range(self.canais):
                    partes[ch].extend(novos[ch][ini - q0 * 32:ini - q0 * 32 + depois])
        out = array.array("h", bytes(2 * 2 * len(partes[0])))
        if self.canais == 1:
            out[0::2] = partes[0]
            out[1::2] = partes[0]
        else:
            out[0::2] = partes[0]
            out[1::2] = partes[1]
        return out, n


# ------------------------------------------------------------------------------------------- ADPCM do chip de som

F0 = (0, 60, 115, 98, 122)
F1 = (0, 0, -52, -55, -60)


def vag_decodifica(parte, ini, conta=None):
    """Uma amostra da parte das amostras de um banco, de `ini` até o bloco com a bandeira 1 (inclusive) ou o fim da
    parte. Devolve array('h'). `conta`: dicionário para o censo (filtros e deslocamentos fora da tabela)."""
    out = array.array("h")
    h1 = h2 = 0
    sinal = [n - 16 if n & 8 else n for n in range(16)]
    o = ini
    while o + 16 <= len(parte):
        desl = parte[o] & 15
        filtro = (parte[o] >> 4) & 7
        flags = parte[o + 1]
        if conta is not None:
            if filtro > 4:
                conta["filtro>4"] = conta.get("filtro>4", 0) + 1
            if desl > 12:
                conta["desl>12"] = conta.get("desl>12", 0) + 1
        if filtro > 4:
            filtro = 0
        if desl > 12:
            desl = 9
        f0, f1 = F0[filtro], F1[filtro]
        for k in range(o + 2, o + 16):
            byte = parte[k]
            for nib in (sinal[byte & 15], sinal[byte >> 4]):
                s = ((nib << 12) >> desl) + ((h1 * f0 + h2 * f1 + 32) >> 6)
                if s > 32767:
                    s = 32767
                elif s < -32768:
                    s = -32768
                out.append(s)
                h2, h1 = h1, s
        o += 16
        if flags & 1:
            break
    return out


def _pedaco(b, ini, fim, nome, limite):
    tag = b"IECS" + nome[::-1].encode()
    for o in range(ini, min(fim, ini + limite) - 15, 4):
        if b[o:o + 8] == tag:
            return o
    return None


def banco_partes(b):
    """(tabelas, sequências, amostras) como (início, fim) no arquivo, ou None se não for um banco."""
    if len(b) < 16:
        return None
    n = struct.unpack_from("<I", b, 0)[0]
    if n != 3:
        return None
    offs = struct.unpack_from("<4I", b, 4)
    partes = {}
    for i in range(3):
        o, e = offs[i], offs[i + 1]
        if not (o < e <= len(b)):
            return None
        if _pedaco(b, o, e, "Vagi", 0x8000) is not None:
            partes["hd"] = (o, e)
        elif _pedaco(b, o, e, "Sesq", 0x4000) is not None:
            partes["sq"] = (o, e)
        else:
            partes["smp"] = (o, e)
    if len(partes) != 3:
        return None
    return partes["hd"], partes["sq"], partes["smp"]


def banco_amostras(b):
    """Os inícios (na parte das amostras) de cada amostra do Vagi, sem repetir, em ordem, e as taxas; None se não é
    um banco."""
    p = banco_partes(b)
    if p is None:
        return None
    (h0, h1), _, (s0, s1) = p
    v = _pedaco(b, h0, h1, "Vagi", 0x8000)
    maior = struct.unpack_from("<I", b, v + 0x0C)[0]
    inis = {}
    for i in range(maior + 1):
        if v + 0x10 + 4 * i + 4 > h1:
            break
        o = struct.unpack_from("<I", b, v + 0x10 + 4 * i)[0]
        if o == 0xFFFFFFFF or v + o + 8 > h1:
            continue
        ini, taxa = struct.unpack_from("<IH", b, v + o)
        if ini < s1 - s0:
            inis.setdefault(ini, taxa)
    return sorted(inis.items())


if __name__ == "__main__":
    # uso direto: som_ref.py adx ARQUIVO [DEPOIS]  (o PCM estéreo s16 little-endian na saída padrão)
    if len(sys.argv) >= 3 and sys.argv[1] == "adx":
        a = Adx(open(sys.argv[2], "rb").read())
        pcm, _ = a.toca(int(sys.argv[3]) if len(sys.argv) > 3 else 0)
        if sys.byteorder != "little":
            pcm.byteswap()
        sys.stdout.buffer.write(pcm.tobytes())
    else:
        sys.exit(__doc__)
