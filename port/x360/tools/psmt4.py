"""PSMT4 enviado em CT32: endereçamento do GS, sem dependências externas."""
import struct
import zlib
B32=[[0,1,4,5,16,17,20,21],[2,3,6,7,18,19,22,23],[8,9,12,13,24,25,28,29],[10,11,14,15,26,27,30,31]]
B16=[[0,2,8,10],[1,3,9,11],[4,6,12,14],[5,7,13,15],[16,18,24,26],[17,19,25,27],[20,22,28,30],[21,23,29,31]]
C32=[[0,1,4,5,8,9,12,13],[2,3,6,7,10,11,14,15],[16,17,20,21,24,25,28,29],[18,19,22,23,26,27,30,31],
     [32,33,36,37,40,41,44,45],[34,35,38,39,42,43,46,47],[48,49,52,53,56,57,60,61],[50,51,54,55,58,59,62,63]]
C4F=[[0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110],
 [16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126],
 [65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47],
 [81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63]]
C4=[[C4F[y&3][(x^4) if ((y>>2)&1) else x]+(y>>2)*128 for x in range(32)] for y in range(16)]
def a32(x,y,bw):
    bw=bw or 1
    return ((((y&~31)*bw+((x>>1)&~31)+B32[(y>>3)&3][(x>>3)&7])<<6)+C32[y&7][x&7])
def a4(x,y,bw):
    bw=bw or 1
    return (((((y>>2)&~31)*(bw>>1)+((x>>2)&~31)+B16[(y>>4)&7][(x>>5)&3])<<9)+C4[y&15][x&31])


def decodificar(dados, largura):
    """Desfaz o envio CT32 de 16x8 ou 32x16 palavras para índices PSMT4."""
    envio = largura // 2
    palavras = struct.unpack('<%dI' % (len(dados) // 4), dados)
    memoria = {a32(k % envio, k // envio, 1): v for k, v in enumerate(palavras)}
    return [[(memoria[a4(x, y, 1) >> 3] >> (4 * (a4(x, y, 1) & 7))) & 15
             for x in range(largura)] for y in range(largura)]


def codificar(pixels):
    largura = len(pixels)
    memoria = {}
    for y, linha in enumerate(pixels):
        for x, v in enumerate(linha):
            a = a4(x, y, 1)
            memoria[a >> 3] = memoria.get(a >> 3, 0) | (v << (4 * (a & 7)))
    envio = largura // 2
    return struct.pack('<%dI' % (largura * largura // 8),
                       *[memoria[a32(k % envio, k // envio, 1)]
                         for k in range(largura * largura // 8)])


def png(caminho, linhas):
    altura, largura = len(linhas), len(linhas[0])
    bruto = b''.join(b'\0' + bytes(v for p in linha for v in p) for linha in linhas)
    def bloco(tipo, dados):
        return struct.pack('>I', len(dados)) + tipo + dados + struct.pack('>I', zlib.crc32(tipo + dados))
    caminho.write_bytes(b'\x89PNG\r\n\x1a\n' + bloco(b'IHDR', struct.pack('>IIBBBBB', largura, altura, 8, 6, 0, 0, 0))
                        + bloco(b'IDAT', zlib.compress(bruto)) + bloco(b'IEND', b''))
