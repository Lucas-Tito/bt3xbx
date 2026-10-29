#!/usr/bin/env python3
"""Compara a instalação C real do console com o instalador PC e sabota cópias.

Sem console/emulador. Trabalho novo fora do git, original somente leitura.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import convert_iso as pc
from preparar_iso_console import preparar


def exigir(ok, msg):
    if not ok:
        raise ValueError(msg)


def hashes(p):
    with p.open("rb") as f:
        return pc.hashes(f)


def comparar(a, b):
    exigir(a.stat().st_size == b.stat().st_size == pc.TAMANHO, "tamanhos diferentes")
    with a.open("rb") as x, b.open("rb") as y:
        while z := x.read(pc.BLOCO):
            exigir(z == y.read(len(z)), f"bytes diferentes perto de {x.tell()}")


def compilar(pasta):
    raiz = pc.RAIZ
    preparar(pasta / "selo")
    exe = pasta / "teste_instalar"
    fontes = sorted((raiz / "port/x360/conv").glob("*.c"))
    subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu89", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wdeclaration-after-statement", "-Wno-unused-function", "-Wno-stringop-truncation",
                    "-I" + str(raiz / "port/x360/src/teste_instalar"),
                    "-I" + str(raiz / "port/x360/src"), "-I" + str(raiz / "port/x360/conv"),
                    "-I" + str(pasta / "selo"), str(raiz / "port/x360/src/iso_instalar.c"),
                    str(raiz / "port/x360/src/teste_instalar/teste_instalar.c"),
                    *(str(f) for f in fontes if f.name not in ("conv_tool.c", "conv_jogo.c")),
                    "-o", str(exe)], check=True)
    return exe


def rodar(exe, origem, pasta, esperado=0, falha=None, corromper=None):
    env = os.environ.copy()
    env.pop("TESTE_FALHA", None)
    env.pop("TESTE_CORROMPER", None)
    env["TESTE_GAME"] = str(pasta)
    if falha:
        env["TESTE_FALHA"] = falha
    if corromper is not None:
        env["TESTE_CORROMPER"] = str(corromper)
    r = subprocess.run([str(exe), str(origem)], env=env, capture_output=True, text=True)
    print(r.stdout, end="", flush=True)
    exigir(r.returncode == esperado, f"esperado {esperado}, obtido {r.returncode}: {r.stderr}")
    return r


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("origem", type=Path)
    p.add_argument("--trabalho", type=Path, required=True)
    args = p.parse_args()
    d = args.trabalho.resolve()
    d.mkdir()  # Nunca reutiliza nem apaga rodada alheia.
    (d / "commit.txt").write_text(subprocess.check_output(["git", "rev-parse", "HEAD"], text=True))
    (d / "diff.patch").write_bytes(subprocess.check_output(["git", "diff", "HEAD"]))
    exe = compilar(d)
    # A rotina de SHA é vista falhando no comparador com um digest adulterado.
    for b in (b"", b"abc", b"a" * 55, b"a" * 56, b"a" * 64, b"a" * 1000000):
        obtido = subprocess.check_output([str(exe), "--sha"], input=b).decode().strip()
        esperado = hashlib.sha256(b).hexdigest()
        exigir(obtido == esperado, "SHA256 diferente da hashlib")
    exigir(obtido != "0" + esperado[1:], "digest adulterado não foi acusado")
    print("ok: vetores SHA256 e controle negativo", flush=True)
    # O verificador, aplicado à origem estragada, precisa sair com falha real.
    invalida = d / "origem-invalida.iso"
    with invalida.open("wb") as f:
        f.truncate(pc.TAMANHO)
    rodar(exe, invalida, d, 1)
    exigir(not (d / "bt3-x360.tmp").exists(), "criou temporário antes de autenticar")
    invalida.unlink()
    print("ok: origem estragada recusada antes de escrever", flush=True)
    antes = hashes(args.origem)
    exigir(antes == (pc.MD5, pc.SHA256), "a ISO não é a original do Lucas")
    convertido = d / "bt3-x360.iso"
    for falha, codigo in (("espaco",2),("leitura",1),("escrever",3),("sincronizar",3),
                           ("fechar",3),("reabrir",3),("truncar",5),("publicar",6)):
        rodar(exe, args.origem, d, codigo, falha)
        exigir(not (d / "bt3-x360.tmp").exists(), f"temporário sobrou: {falha}")
        if falha == "publicar":
            exigir(convertido.read_bytes() == b"preservado", "destino concorrente alterado")
            convertido.unlink()
        else:
            exigir(not convertido.exists(), f"publicou falha: {falha}")
        print(f"ok: sabotagem {falha}", flush=True)
    # Byte convertido, preservado, índices ISO/AFS, folga/UDF, selo e cauda.
    with args.origem.open("rb") as f:
        entradas = pc.inventario(f)
    for nome, pos in (("convertido",entradas["pzs3us1/00004.bin"][0]),
                      ("preservado",entradas["disc/SYSTEM.CNF"][0]), ("índice ISO",16*2048+158),
                      ("índice AFS",0x3df008), ("folga AFS",0x3df018),
                      ("área UDF",256*2048), ("selo",pc.MARCA_POS+16), ("cauda",pc.TAMANHO-1)):
        rodar(exe, args.origem, d, 5, corromper=pos)
        exigir(not convertido.exists() and not (d / "bt3-x360.tmp").exists(), "falha publicada")
        print(f"ok: byte sabotado em {nome}", flush=True)
    # Interrupção real de processo. A próxima abertura descarta o parcial e recomeça.
    rodar(exe,args.origem,d,77,"parar")
    exigir((d / "bt3-x360.tmp").stat().st_size > 0 and not convertido.exists(), "interrupção não deixou só parcial")
    rodar(exe,args.origem,d)
    exigir(not (d / "bt3-x360.tmp").exists(), "parcial não descartado")
    # Destino já existente preservado, inclusive se ele for a própria origem.
    h = hashes(convertido)
    rodar(exe,args.origem,d,6)
    exigir(hashes(convertido) == h, "destino existente mudou")
    rodar(exe,convertido,d,1)
    print("ok: retomada do zero, saída existente e reconversão recusada", flush=True)
    referencia = d / "pc.iso"
    subprocess.run([sys.executable,str(pc.RAIZ / "port/x360/tools/convert_iso.py"),str(args.origem),str(referencia),
                    "--conv-tool",str(d / "selo/conv_tool")], check=True)
    comparar(convertido,referencia)
    exigir(hashes(referencia) == h, "hashes diferentes entre C e PC")
    # Vê o próprio comparador falhar com byte adulterado numa cópia.
    with referencia.open("r+b") as f:
        pos=entradas["pzs3us1/00004.bin"][0];f.seek(pos);b=f.read(1);f.seek(pos);f.write(bytes([b[0]^1]))
    try:
        comparar(convertido,referencia)
    except ValueError:
        print("ok: comparador visto falhando com byte estragado",flush=True)
    else:
        raise ValueError("comparador aceitou sabotagem")
    with referencia.open("r+b") as f:
        f.seek(pos);f.write(b)
    comparar(convertido,referencia)
    depois=hashes(args.origem)
    exigir(depois == antes, "original mudou")
    (d / "hashes.txt").write_text(f"tamanho {convertido.stat().st_size}\nMD5 {h[0]}\nSHA256 {h[1]}\noriginal {depois}\n")
    print(f"igualdade C = PC: {pc.TAMANHO} bytes, MD5 {h[0]}, SHA256 {h[1]}",flush=True)
    print("teste_iso_console: tudo certo",flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError,ValueError,subprocess.SubprocessError) as e:
        print(f"FALHA: {e}",file=sys.stderr)
        raise SystemExit(1)
