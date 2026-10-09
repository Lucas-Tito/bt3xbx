#!/usr/bin/env python3
"""Gera o SPA (arquivo XDBF) que vai numa seção do .xex com o nome do Title ID: é dele que o Aurora,
a FreeStyle e o dashboard leem o nome e o ícone do título.

O XDK faz isso com spac.exe, que é .NET e não roda no wine da imagem. Aqui sai só o mínimo: cabeçalho
do título (XTHD), idioma padrão (XSTC), a tabela de textos em inglês com o nome (id 0x8000) e o ícone
64x64 em PNG (imagem 0x8000). Layout conferido com o SPA de um título homebrew feito pelo XDK.
Tudo big-endian."""
import argparse, struct

NS_META, NS_IMAGE, NS_STRING = 1, 2, 3
LOCALE_EN = 1
ID_TITLE = 0x8000

def section(magic, payload, size=None):
    # cabeçalho de seção: magia, versão 1, tamanho (o XDK grava o total menos 4; no XTHD grava 0)
    body = magic + struct.pack(">I", 1)
    total = len(body) + 4 + len(payload)
    return body + struct.pack(">I", total - 4 if size is None else size) + payload

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--title-id", required=True, type=lambda s: int(s, 0))
    ap.add_argument("--name", required=True)
    ap.add_argument("--icon", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    icon = open(a.icon, "rb").read()
    assert icon[:8] == b"\x89PNG\r\n\x1a\n", "o ícone tem de ser PNG"
    w, h = struct.unpack(">II", icon[16:24])
    assert (w, h) == (64, 64), f"o ícone tem de ser 64x64 (é {w}x{h})"
    name = a.name.encode("utf-8")

    xthd = section(b"XTHD", struct.pack(">II4H", a.title_id, 1, 1, 0, 0, 0) + bytes(16), size=0)
    xstc = section(b"XSTC", struct.pack(">I", LOCALE_EN))
    xstr = section(b"XSTR", struct.pack(">H", 1) + struct.pack(">HH", ID_TITLE, len(name)) + name)

    # (namespace, id) em ordem crescente, como o XDK grava
    entries = sorted([
        (NS_META, int.from_bytes(b"XSTC", "big"), xstc),
        (NS_META, int.from_bytes(b"XTHD", "big"), xthd),
        (NS_IMAGE, ID_TITLE, icon),
        (NS_STRING, LOCALE_EN, xstr),
    ], key=lambda e: (e[0], e[1]))

    table, data = b"", b""
    for ns, ident, blob in entries:
        table += struct.pack(">HQII", ns, ident, len(data), len(blob))
        data += blob
    free = struct.pack(">II", len(data), 0)  # como no SPA de referência: fim dos dados, tamanho 0
    header = b"XDBF" + struct.pack(">IIIII", 0x10000, len(entries), len(entries), 1, 1)
    open(a.out, "wb").write(header + table + free + data)

main()
