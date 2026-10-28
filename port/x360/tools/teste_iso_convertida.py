#!/usr/bin/env python3
"""Critério completo da ISO convertida no PC, com recusas e estragos provocados.

python3 port/x360/tools/teste_iso_convertida.py ORIGINAL --dados gamedata --trabalho ~/f5-iso/rodada
Recompila o conv_tool, guarda logs e uma ISO válida em --trabalho. As cópias estragadas são apagadas.
"""

import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

import convert_iso as iso


def rodar(cmd, log, esperado=0, trecho=None):
    with log.open("w") as f:
        r = subprocess.run([str(c) for c in cmd], stdout=f, stderr=subprocess.STDOUT)
    texto = log.read_text()
    iso.exigir(r.returncode == esperado, f"{log.name}: saída {r.returncode}, esperada {esperado}")
    iso.exigir(trecho is None or trecho in texto, f"{log.name}: mensagem esperada ausente: {trecho}")
    print(f"{log.name}: saída {r.returncode}, conforme", flush=True)
    return texto


def alterar(p, pos):
    with p.open("r+b") as f:
        b = iso.ler(f, pos, 1)
        f.seek(pos)
        f.write(bytes([b[0] ^ 1]))


def executar(args):
    trabalho = args.trabalho.resolve()
    # Sem apagar uma saída anterior: cada rodada recebe uma pasta nova.
    trabalho.mkdir(parents=True, exist_ok=False)
    tool = trabalho / "conv_tool"
    fontes = sorted((iso.RAIZ / "port/x360/conv").glob("*.c"))
    fontes = [f for f in fontes if f.name != "conv_jogo.c"]
    rodar(["gcc", "-std=c89", "-O2", "-Wall", "-o", tool, *fontes], trabalho / "build.log")
    rodar([sys.executable, iso.RAIZ / "port/x360/tools/icones360.py", "--conferir"],
          trabalho / "arte.log", trecho="tudo certo")
    rodar([sys.executable, iso.RAIZ / "port/x360/tools/teste_icones360.py",
           "--dados", args.dados, "--conv-tool", tool], trabalho / "icones.log", trecho="tudo certo")
    (trabalho / "commit.txt").write_text(subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=iso.RAIZ, text=True))
    (trabalho / "diff.txt").write_text(subprocess.check_output(
        ["git", "diff", "HEAD", "--"], cwd=iso.RAIZ, text=True))
    with args.origem.open("rb") as f:
        antes = iso.hashes(f)
        entradas = iso.inventario(f)
    iso.exigir(antes == (iso.MD5, iso.SHA256), "origem dos testes incompatível")
    comando = [sys.executable, iso.RAIZ / "port/x360/tools/convert_iso.py"]
    convertida = trabalho / "bt3-x360.iso"
    base = ["--conv-tool", tool]
    rodar([*comando, args.origem, convertida, *base], trabalho / "conversao.log", trecho="tudo certo")
    verifica = [*comando, args.origem, convertida, *base, "--conferir", "--dados", args.dados]
    rodar(verifica, trabalho / "reabertura.log", trecho="tudo certo")

    def recusar(entrada, saida, nome, mensagem):
        # A recusa não pode criar nem temporários; uma saída preexistente também fica intacta.
        conteudo = saida.read_bytes() if saida.exists() else None
        lista = sorted(p.name for p in trabalho.iterdir())
        log = trabalho / f"recusa-{nome}.log"
        rodar([*comando, entrada, saida, *base], log, 1, mensagem)
        iso.exigir(sorted(p.name for p in trabalho.iterdir()) == sorted(lista + [log.name]),
                   f"{nome}: a recusa criou arquivos")
        iso.exigir((saida.read_bytes() if saida.exists() else None) == conteudo,
                   f"{nome}: a recusa alterou a saída")

    saida = trabalho / "recusada.iso"
    with tempfile.TemporaryDirectory(prefix="estragos-", dir=trabalho) as t:
        copia = Path(t) / "copia.iso"
        # Cópias esparsas ou por reflink evitam reservar desnecessariamente os zeros da mídia.
        subprocess.run(["cp", "--reflink=auto", "--sparse=always", args.origem, copia], check=True)
        alterar(copia, 0)
        recusar(copia, saida, "byte", "hashes incompatíveis")
        rodar([sys.executable, Path(__file__), copia, "--dados", args.dados,
               "--trabalho", trabalho / "criterio-invalido"],
              trabalho / "estrago-criterio.log", 1, "origem dos testes incompatível")
        alterar(copia, 0)
        with copia.open("r+b") as f:
            f.truncate(iso.TAMANHO - 1)
        recusar(copia, saida, "truncada", "ISO truncada")
        copia.unlink()
        # A já convertida é recusada como origem, sem qualquer escrita nela.
        # Uma única convertida ocupa o disco: estraga e restaura no lugar, sem segunda ISO.
        copia = convertida
        recusar(copia, saida, "convertida", "ISO já convertida")
        existente = trabalho / "existente.iso"
        existente.write_text("saída anterior que deve permanecer intacta\n")
        recusar(args.origem, existente, "existente", "saída já existe")
        existente.unlink()
        enlace = trabalho / "enlace.iso"
        enlace.symlink_to(trabalho / "ausente.iso")
        rodar([*comando, args.origem, enlace, *base], trabalho / "recusa-enlace.log", 1, "saída já existe")
        iso.exigir(enlace.is_symlink() and not enlace.exists(), "recusa alterou o enlace pendente")
        enlace.unlink()
        # O verificador precisa detectar tanto conversão errada quanto região preservada alterada.
        nomes = iso.selecionados(str(tool), entradas)
        preservado = next(n for n, (_, tam) in entradas.items() if n not in nomes and tam > 0)
        casos = {
            "convertido": (entradas[nomes[0]][0], "conversão diferente"),
            "preservado": (entradas[preservado][0], "bytes diferentes"),
            "fora-arquivos": (0, "área preservada"),
            "tabela-afs": (0x3DF000 + 11, "fora dos limites"),
            "tabela-iso": (16 * iso.SETOR + 84, "ordens de bytes discordam"),
            "folga-afs": (0x3DF000 + 24, "folga inesperada"),
            "marca": (iso.MARCA_POS + 16, "marca, versão ou conversor incompatível"),
            "arte": (entradas["pzs3us1/00004.bin"][0] + 0x19A40 + 0x9E8 * 4 + 96,
                     "conversão diferente em pzs3us1/00004.bin"),
        }
        for nome, (pos, mensagem) in casos.items():
            alterar(copia, pos)
            try:
                rodar([*comando, args.origem, copia, *base, "--conferir", "--dados", args.dados],
                      trabalho / f"estrago-{nome}.log", 1, mensagem)
            finally:
                alterar(copia, pos)

        with copia.open("r+b") as f:
            f.seek(iso.MARCA_POS + 16)
            f.write(struct.pack("<I", 1))
        try:
            rodar([*comando, args.origem, copia, *base, "--conferir", "--dados", args.dados],
                  trabalho / "estrago-marca-antiga.log", 1, "marca, versão ou conversor incompatível")
        finally:
            with copia.open("r+b") as f:
                f.seek(iso.MARCA_POS + 16)
                f.write(struct.pack("<I", iso.VERSAO))
        arte = Path(t) / "arte"
        shutil.copytree(iso.RAIZ / "port/x360/arte/kenney", arte)
        png = arte / "xbox_button_color_a.png"
        alterar(png, 50)
        rodar([sys.executable, iso.RAIZ / "port/x360/tools/icones360.py", "--arte", arte, "--conferir"],
              trabalho / "estrago-png.log", 1, "CRC PNG inválido")
        cab = Path(t) / "arte.h"
        cab.write_text((iso.RAIZ / "port/x360/conv/icones360_arte.h").read_text().replace("0x00", "0x01", 1))
        rodar([sys.executable, iso.RAIZ / "port/x360/tools/icones360.py", "--cabecalho", cab, "--conferir"],
              trabalho / "estrago-gerador.log", 1, "arte gerada diverge")

        # Uma saída que aparece na última hora nunca é substituída pelo rename.
        temporario, ocupado = Path(t) / "temporario", Path(t) / "ocupado"
        temporario.write_bytes(b"novo")
        ocupado.write_bytes(b"anterior")
        try:
            iso.publicar(temporario, ocupado)
        except FileExistsError:
            pass
        else:
            raise ValueError("publicação substituiu o destino concorrente")
        iso.exigir(temporario.read_bytes() == b"novo" and ocupado.read_bytes() == b"anterior",
                   "publicação concorrente alterou arquivos")
        print("publicação concorrente: recusada, ambos intactos", flush=True)

        # Prova também a guarda nova que exige cobertura inteira do --saida.
        falso = Path(t) / "conv-cobertura.py"
        falso.write_text("#!" + sys.executable + "\nimport subprocess, sys\n"
                         f"r = subprocess.run([{str(tool)!r}, *sys.argv[1:]], capture_output=True, text=True)\n"
                         "sys.stdout.write(r.stdout.replace('100.0%', '99.0%'))\n"
                         "sys.exit(r.returncode)\n")
        falso.chmod(0o700)
        try:
            iso.converter(str(falso), args.dados, Path(t) / "cobertura", nomes[:1])
        except ValueError as e:
            iso.exigir("conversão incompleta" in str(e), "guarda de cobertura falhou por outro motivo")
            print("cobertura estragada: recusada", flush=True)
        else:
            raise ValueError("guarda aceitou cobertura estragada")

    texto = rodar([tool, "--disco", args.dados], trabalho / "disco.log")
    linhas = re.findall(r"cobertura (\d+) de (\d+) bytes", texto)
    iso.exigir(linhas and all(a == b for a, b in linhas), "--disco: cobertura incompleta")
    resumos = re.findall(r"disco(?: \(descomprimidos\))?: \d+ (?:arquivos )?convertidos sem erro, 0 com erro", texto)
    iso.exigir(len(resumos) == 2, "--disco: faltam os dois resumos sem erro")
    print("\n".join(resumos), flush=True)
    print(f"--disco: {len(linhas)} conversões com cobertura integral", flush=True)
    with args.origem.open("rb") as f:
        iso.exigir(iso.hashes(f) == antes, "original mudou no critério")
    with convertida.open("rb") as f:
        h = iso.hashes(f)
    (trabalho / "hashes.txt").write_text(
        f"original md5 {antes[0]}\noriginal sha256 {antes[1]}\n"
        f"convertida md5 {h[0]}\nconvertida sha256 {h[1]}\n")
    print(f"original intacta: md5 {antes[0]}\nISO convertida: md5 {h[0]}\ntudo certo", flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("origem", type=Path)
    p.add_argument("--dados", required=True, type=Path)
    p.add_argument("--trabalho", required=True, type=Path)
    args = p.parse_args()
    args.origem = args.origem.resolve()
    args.dados = args.dados.resolve()
    try:
        executar(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as e:
        print(f"teste_iso_convertida: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
