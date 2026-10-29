#!/usr/bin/env python3
"""Converts and verifies your own USA ISO of the game (SLUS-21678) for the Xbox 360 port, on a PC.

Usage: convert_iso.py ORIGINAL.iso bt3-x360.iso [--conv-tool PATH]. Requires Python 3 and conv_tool.
Temporary files are created next to the output, never next to the original, which is never modified.
(Contract of the format, in Portuguese: docs/x360/formatos/iso-convertida.md.)
"""

import argparse
import ctypes
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
TAMANHO = 3020816384
MD5 = "350404a81de912347c8b262542fed970"
SHA256 = "93f1f911e9a2bbdf92e5794fc3075ddf3f87db3df4b1cb8cfbb0448721c91172"
SETOR = 2048
MARCA_POS = 0x3DF400
MARCA_TAM = 512
VERSAO = 2
MAGIA = b"BT3X360-ISO".ljust(16, b"\0")
BLOCO = 4 * 1024 * 1024


def exigir(ok, mensagem):
    if not ok:
        raise ValueError(mensagem)


def ler(f, pos, n):
    f.seek(pos)
    b = f.read(n)
    exigir(len(b) == n, f"short read at 0x{pos:X}, expected {n} bytes")
    return b


def hashes(f):
    f.seek(0)
    md5, sha = hashlib.md5(), hashlib.sha256()
    while b := f.read(BLOCO):
        md5.update(b)
        sha.update(b)
    return md5.hexdigest(), sha.hexdigest()


def original(f):
    exigir(os.fstat(f.fileno()).st_size == TAMANHO, "the ISO is truncated or has an unexpected size")
    exigir(not ler(f, MARCA_POS, MARCA_TAM).startswith(MAGIA), "this ISO is already converted")
    h = hashes(f)
    exigir(h == (MD5, SHA256), "this is not the original USA ISO (SLUS-21678): the checksums do not match")
    print(f"original: md5 {h[0]}, sha256 {h[1]}", flush=True)
    return h


def sem_sobreposicao(trechos, limite):
    fim = 0
    for pos, n, nome in sorted(trechos):
        exigir(0 <= pos <= limite and 0 <= n <= limite - pos, f"{nome}: out of bounds")
        if n:
            exigir(pos >= fim, f"{nome}: overlapping regions")
            fim = pos + n


def inventario(f):
    """ISO 9660 e AFS originais: limites, duplicidade e ambas as ordens da ISO conferidos.

    A imagem também tem UDF. Nenhum metadado dele é reescrito: a marca ocupa folga interna
    do cabeçalho AFS0, e a comparação integral cobre tudo fora das entradas e da marca.
    """
    def duplo(b, pos, n):
        a = int.from_bytes(b[pos:pos + n], "little")
        exigir(a == int.from_bytes(b[pos + n:pos + 2 * n], "big"), "ISO: both-endian fields disagree")
        return a

    pvd = ler(f, 16 * SETOR, SETOR)
    exigir(pvd[:7] == b"\1CD001\1", "ISO: invalid primary volume descriptor")
    exigir(duplo(pvd, 80, 4) * SETOR == TAMANHO and duplo(pvd, 128, 2) == SETOR,
           "ISO: unexpected volume or sector size")
    exigir(ler(f, 17 * SETOR, 7) == b"\xffCD001\1", "ISO: unexpected volume descriptors")
    arquivos, regioes, visitados = {}, [], set()

    def registro(b):
        exigir(len(b) >= 34 and b[0] == len(b) and 33 + b[32] <= len(b), "ISO: invalid directory record")
        exigir(b[1] == 0 and b[26:28] == b"\0\0" and duplo(b, 28, 2) == 1,
               "ISO: unsupported attributes, interleaving or volume")
        exigir(b[25] in (0, 2), "ISO: unsupported file flags")
        return duplo(b, 2, 4) * SETOR, duplo(b, 10, 4)

    def pasta(pos, n, prefixo):
        exigir((pos, n) not in visitados, "ISO: directory loop or repeated directory")
        visitados.add((pos, n))
        exigir(0 < n <= 1024 * 1024, "ISO: invalid directory size")
        regioes.append((pos, (n + SETOR - 1) // SETOR * SETOR, prefixo or "root"))
        b = ler(f, pos, n)
        k = 0
        while k < n:
            z = b[k]
            if z == 0:
                k = (k // SETOR + 1) * SETOR
                continue
            exigir(k % SETOR + z <= SETOR and k + z <= n, "ISO: directory record crosses a sector boundary")
            r = b[k:k + z]
            a, t = registro(r)
            k += z
            nome = r[33:33 + r[32]]
            if nome in (b"\0", b"\1"):
                continue
            nome = nome.decode("ascii").removesuffix(";1").rstrip(".")
            exigir(re.fullmatch(r"[A-Z0-9_.]+", nome) and nome not in (".", ".."), "ISO: invalid file name")
            rel = prefixo + nome
            if r[25] & 2:
                pasta(a, t, rel + "/")
            else:
                exigir(rel not in arquivos, "ISO: duplicate file name")
                arquivos[rel] = (a, t)
                regioes.append((a, (t + SETOR - 1) // SETOR * SETOR, rel))

    pasta(*registro(pvd[156:156 + pvd[156]]), "")
    sem_sobreposicao(regioes, TAMANHO)
    entradas = {}
    afs_nomes = {f"DATA/PZS3US{p}.AFS" for p in range(3)}
    for p in range(3):
        nome = f"DATA/PZS3US{p}.AFS"
        exigir(nome in arquivos, f"ISO: {nome} is missing")
        ini, tam = arquivos[nome]
        cab = ler(f, ini, 8)
        exigir(cab[:4] == b"AFS\0", f"{nome}: invalid signature")
        n = struct.unpack_from("<I", cab, 4)[0]
        exigir(n == (1, 3399, 65201)[p], f"{nome}: unexpected file count")
        tabela = ler(f, ini + 8, n * 8 + 8)
        meta, meta_tam = struct.unpack_from("<II", tabela, n * 8)
        exigir(meta_tam == n * 48, f"{nome}: unexpected name table")
        ocupados = [(0, 8 + n * 8 + 8, "header"), (meta, meta_tam, "names")]
        for i in range(n):
            a, t = struct.unpack_from("<II", tabela, i * 8)
            rel = f"pzs3us{p}/{i:05d}.bin"
            entradas[rel] = (ini + a, t)
            ocupados.append((a, t, rel))
        sem_sobreposicao(ocupados, tam)
        if p == 0:
            exigir(ini == 0x3DF000 and tam == 14336, "AFS0: unexpected position or size")
            # Inclui o ponteiro opcional aos nomes; a marca não toca nenhum campo AFS.
            sem_sobreposicao(ocupados + [(MARCA_POS - ini, MARCA_TAM, "marca")], tam)
            exigir(ler(f, ini + 24, 1000) == bytes(1000) and
                   ler(f, MARCA_POS + MARCA_TAM, 512) == bytes(512), "AFS0: unexpected data in the free area")
    for nome, regiao in arquivos.items():
        if nome not in afs_nomes:
            entradas["disc/" + nome] = regiao
    sem_sobreposicao([(a, t, r) for r, (a, t) in entradas.items()], TAMANHO)
    return entradas


def selecionados(tool, entradas):
    r = subprocess.run([tool, "--lista-conversores"], check=True, capture_output=True, text=True)
    nomes = r.stdout.splitlines()
    exigir(nomes and len(nomes) == len(set(nomes)), "conv_tool: empty or duplicated converter list")
    exigir(all(re.fullmatch(r"pzs3us[0-2]/[0-9]{5}\.bin", n) for n in nomes), "conv_tool: invalid file name")
    return sorted(set(nomes) & entradas.keys())


def converter(tool, dados, saida, nomes):
    """O mesmo --saida público usado pela referência, com cobertura integral obrigatória."""
    saida.mkdir()
    for i in range(0, len(nomes), 200):
        lote = nomes[i:i + 200]
        r = subprocess.run([tool, "--icones360", "--saida", str(saida), str(dados), *lote],
                           check=True, capture_output=True, text=True)
        linhas = r.stdout.splitlines()
        exigir(len(linhas) == len(lote), "conv_tool: missing results for a batch")
        for nome, linha in zip(lote, linhas):
            m = re.fullmatch(re.escape(nome) + r": convertido; cobertura (\d+) de (\d+) bytes \(100\.0%\), "
                             r"\d+ em números trocados", linha)
            exigir(m and m[1] == m[2], f"conv_tool: incomplete conversion: {linha}")
    lista = (saida / "convertidos.txt").read_text().splitlines()
    exigir(lista == nomes, "conv_tool: output list does not match")
    print(f"converted {len(nomes)} files, full coverage", flush=True)


def marca(tool):
    # Identifica o programa efetivamente usado e seu código, sem depender do git do usuário.
    h = hashlib.sha256()
    fontes = sorted((RAIZ / "port/x360/conv").glob("*.[ch]")) + [Path(__file__)]
    for f in fontes:
        h.update(f.name.encode("ascii") + b"\0" + f.read_bytes())
    executavel = hashlib.sha256(Path(tool).read_bytes()).digest()
    b = struct.pack("<16sIIQ16s32s32s32s", MAGIA, VERSAO, MARCA_TAM, TAMANHO,
                    bytes.fromhex(MD5), bytes.fromhex(SHA256), h.digest(), executavel)
    return b.ljust(MARCA_TAM, b"\0")


def comparar(a, b, pos, n, nome):
    while n:
        z = min(n, BLOCO)
        exigir(ler(a, pos, z) == ler(b, pos, z), f"{nome}: bytes differ at 0x{pos:X}")
        pos += z
        n -= z


def conferir(origem, destino, entradas, nomes, ref, selo):
    """Reabre, relê os índices e compara todos os bytes, inclusive folgas e UDF."""
    with destino.open("rb") as f:
        exigir(os.fstat(f.fileno()).st_size == TAMANHO, "output: unexpected size")
        exigir(ler(f, MARCA_POS, MARCA_TAM) == selo, "output: mark, version or converter does not match")
        exigir(inventario(f) == entradas, "output: ISO/AFS indexes differ")
        escolhidos = set(nomes)
        for rel, (a, t) in entradas.items():
            if rel in escolhidos:
                b = (ref / rel).read_bytes()
                exigir(len(b) == t and ler(f, a, t) == b, f"output: conversion differs in {rel}")
            else:
                comparar(origem, f, a, t, rel)
        regioes = sorted([(entradas[n][0], entradas[n][1], n) for n in nomes] +
                         [(MARCA_POS, MARCA_TAM, "marca")])
        sem_sobreposicao(regioes, TAMANHO)
        fim = 0
        preservados = 0
        for a, t, _ in regioes + [(TAMANHO, 0, "fim")]:
            comparar(origem, f, fim, a - fim, "preserved area")
            preservados += a - fim
            fim = a + t
    print(f"verified: {len(entradas)} files, {len(nomes)} converted, "
          f"{len(entradas) - len(nomes)} kept; {preservados} untouched bytes identical", flush=True)


def publicar(temporario, destino):
    """Renomeia atomicamente sem substituir uma saída criada enquanto se convertia."""
    if sys.platform == "linux":
        libc = ctypes.CDLL(None, use_errno=True)
        rename = libc.renameat2
        rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        rename.restype = ctypes.c_int
        if rename(-100, os.fsencode(temporario), -100, os.fsencode(destino), 1):
            erro = ctypes.get_errno()
            raise OSError(erro, os.strerror(erro), str(destino))
    elif os.name == "nt":
        os.rename(temporario, destino)  # No Windows, recusa destino existente.
    else:
        raise ValueError("this script runs only on Linux, WSL or Windows")


def executar(args):
    tool = str(args.conv_tool.resolve(strict=True))
    saida = args.saida.absolute()
    if not args.conferir:
        exigir(not os.path.lexists(saida), "the output file already exists; nothing was written")
    with args.origem.open("rb") as origem:
        antes = original(origem)
        entradas = inventario(origem)
        exigir(ler(origem, MARCA_POS, MARCA_TAM) == bytes(MARCA_TAM), "the mark area of the ISO is not free")
        nomes = selecionados(tool, entradas)
        exigir(nomes, "no convertible file found in the ISO")
        selo = marca(tool)
        # Somente depois de todas as recusas de entrada se cria qualquer temporário.
        with tempfile.TemporaryDirectory(prefix="bt3-iso-", dir=saida.parent) as pasta:
            trabalho = Path(pasta)
            dados = args.dados
            if not args.conferir:
                dados = trabalho / "originais"
                for nome in nomes:
                    a, t = entradas[nome]
                    arq = dados / nome
                    arq.parent.mkdir(parents=True, exist_ok=True)
                    arq.write_bytes(ler(origem, a, t))
            ref = trabalho / "convertidos"
            converter(tool, dados, ref, nomes)
            destino = saida if args.conferir else trabalho / "convertida.iso"
            if not args.conferir:
                origem.seek(0)
                with destino.open("xb") as f:
                    shutil.copyfileobj(origem, f, BLOCO)
                    for nome in nomes:
                        a, t = entradas[nome]
                        b = (ref / nome).read_bytes()
                        exigir(len(b) == t, f"{nome}: the converter changed the file size")
                        f.seek(a)
                        f.write(b)
                    f.seek(MARCA_POS)
                    f.write(selo)
                    f.flush()
                    os.fsync(f.fileno())
            conferir(origem, destino, entradas, nomes, ref, selo)
            exigir(hashes(origem) == antes, "the original changed during the conversion")
            print(f"original untouched: md5 {antes[0]}", flush=True)
            if not args.conferir:
                publicar(destino, saida)
    print("all done", flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("origem", type=Path, metavar="ORIGINAL", help="your original USA ISO")
    p.add_argument("saida", type=Path, metavar="OUTPUT", help="the converted ISO to create (bt3-x360.iso)")
    p.add_argument("--conv-tool", type=Path, default=RAIZ / "port/build/conv_tool")
    p.add_argument("--conferir", action="store_true", help="reopen the output and compare it with the converter output of extracted data (developer check)")
    p.add_argument("--dados", type=Path, help="extracted original data, required with --conferir")
    args = p.parse_args()
    if args.conferir and args.dados is None:
        p.error("--conferir requires --dados")
    if not args.conferir and args.dados is not None:
        p.error("--dados is only valid with --conferir; the conversion reads the ISO itself")
    try:
        executar(args)
    except (OSError, ValueError, subprocess.CalledProcessError, UnicodeError) as e:
        print(f"convert_iso: {e}", file=sys.stderr)
        if isinstance(e, subprocess.CalledProcessError):
            print(e.stdout or "", end="", file=sys.stderr)
            print(e.stderr or "", end="", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
