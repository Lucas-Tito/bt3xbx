#!/usr/bin/env python3
"""Prova do pacote_confere.py (F8, etapa 5): passa no pacote intacto e sai com 1 em cada estrago plantado.

    teste_pacote.py ZIP --trabalho DIR [--verificador pacote_confere.py] [--mutantes]

ZIP é um bt3-x360-<versão>.zip do pacote.py. Para cada caso, grava em DIR uma cópia do zip com um estrago (um
arquivo proibido de cada tipo, um fora da lista, um hash errado, um acima do teto...), refaz o SHA256SUMS.txt
quando o caso não é justamente ele (para só a regra do caso poder acusar), roda o verificador e exige a saída 1 e
o motivo esperado na resposta. O intacto tem de sair com 0. Os arquivos plantados são inventados aqui: cabeçalhos
mínimos de ISO 9660, AFS, ELF, MPEG e PNG sobre zeros, nenhum byte do jogo.

--mutantes roda o mesmo teste contra cópias estragadas do próprio verificador (sem saída 1, sem conferir os
hashes, sem os nomes proibidos, sem as assinaturas) e exige que cada uma seja acusada: o teste não aceita um
verificador que não verifica. Sai com 1 se qualquer expectativa falhar.
"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys
import zipfile

AQUI = Path(__file__).resolve().parent


def iso_falsa():
    b = bytearray(0x8800)
    b[0x8000:0x8006] = b"\x01CD001"
    b[0x8028:0x8032] = b"SLUS_21678"
    return bytes(b)


def afs_falso():
    return b"AFS\x00" + struct.pack("<III", 1, 0x800, 16) + bytes(0x800 - 16) + bytes(16)


ELF = b"\x7fELF\x01\x01\x01" + bytes(57)
PSS = b"\x00\x00\x01\xba" + bytes(2044)
PNG = b"\x89PNG\r\n\x1a\n" + bytes(64)


def ler(zipe):
    with zipfile.ZipFile(zipe) as z:
        return {i.filename: z.read(i) for i in z.infolist()}


def gravar(arquivos, destino, simbolicos=()):
    with zipfile.ZipFile(destino, "w", zipfile.ZIP_DEFLATED) as z:
        for nome, b in arquivos.items():
            info = zipfile.ZipInfo(nome, (2026, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (0o120777 if nome in simbolicos else 0o100644) << 16
            z.writestr(info, b)


def refazer_somas(arquivos, raiz):
    somas = "".join(f"{hashlib.sha256(b).hexdigest()}  {n[len(raiz):]}\n" for n, b in sorted(arquivos.items())
                    if n != raiz + "SHA256SUMS.txt" and n.startswith(raiz))
    arquivos[raiz + "SHA256SUMS.txt"] = somas.encode()


def casos(raiz):
    """(nome, função que estraga o dicionário de arquivos, refazer as somas?, motivo esperado, links simbólicos)."""
    def plantar(rel, b):
        return lambda a: a.__setitem__(raiz + rel, b)

    def trocar(rel, f):
        return lambda a: a.__setitem__(raiz + rel, f(a[raiz + rel]))

    def tirar(rel):
        return lambda a: a.pop(raiz + rel)

    def xex_title(b):
        n = struct.unpack(">I", b[20:24])[0]
        for i in range(n):
            k, v = struct.unpack(">II", b[24 + 8 * i:32 + 8 * i])
            if k == 0x00040006:
                return b[:v + 12] + struct.pack(">I", 0x41560001) + b[v + 16:]
        raise ValueError("xex sem informações de execução")

    def sem_linha_licenca(a):
        s = a[raiz + "SHA256SUMS.txt"].decode()
        a[raiz + "SHA256SUMS.txt"] = "".join(l + "\n" for l in s.splitlines() if not l.endswith("  LICENSE.txt")).encode()

    return [
        ("iso na pasta do jogo", plantar("bt3-x360/bt3.iso", iso_falsa()), True, "imagem de disco", ()),
        ("iso convertida na pasta", plantar("bt3-x360/bt3-x360.iso", iso_falsa()), True, "imagem de disco", ()),
        ("iso disfarçada de fonte", plantar("pc-tools/port/x360/conv/tabela.h", iso_falsa()), True,
         "assinatura de ISO 9660", ()),
        ("AFS do jogo (.bin)", plantar("bt3-x360/pzs3us0/00004.bin", afs_falso()), True, "arquivo .bin ou AFS", ()),
        ("AFS no lugar do default.swap", trocar("bt3-x360/default.swap", lambda b: afs_falso()), True,
         "assinatura de AFS", ()),
        ("ELF do PS2 (SLUS)", plantar("bt3-x360/SLUS_216.78", ELF), True, "executável do PS2", ()),
        ("ELF (.elf)", plantar("pc-tools/slus.elf", ELF), True, "executável do PS2", ()),
        ("filme do jogo (.pss)", plantar("bt3-x360/ZS3USOP.PSS", PSS), True, "filme ou som do jogo", ()),
        ("environment.txt", plantar("bt3-x360/environment.txt", b"BT3_LOG=1\n"), True, "environment.txt", ()),
        ("ambiente*.txt", plantar("bt3-x360/ambiente_brilho.txt", b"BT3_GLOW=60\n"), True, "ambiente*.txt", ()),
        ("log do diário", plantar("bt3-x360/bt3xbx.log", b"passos: origem\n"), True, "proibido: log", ()),
        ("output.txt", plantar("bt3-x360/output.txt", b"saida\n"), True, "saída de diagnóstico", ()),
        ("crash.txt", plantar("bt3-x360/crash.txt", b"Iar 82000000\n"), True, "crash.txt", ()),
        ("captura", plantar("bt3-x360/shots/0001.png", PNG), True, "captura ou imagem", ()),
        ("save do cartão", plantar("bt3-x360/saves/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3", bytes(0x4000)), True,
         "proibido: save", ()),
        (".map da build", plantar("bt3-x360/default.map", b" 0004:00000050  D_002C3380\n"), True,
         "artefato de build", ()),
        (".pdb da build", plantar("bt3-x360/default.pdb", bytes(64)), True, "artefato de build", ()),
        ("arquivo fora da lista", plantar("NOTES.txt", b"hello\n"), True, "fora da lista branca", ()),
        ("hash que não bate", trocar("README.txt", lambda b: b.replace(b"Version", b"Versiom", 1)), False,
         "SHA256 não bate", ()),
        ("arquivo fora do SHA256SUMS", sem_linha_licenca, False, "fora do SHA256SUMS", ()),
        ("xex acima do teto", trocar("bt3-x360/default.xex", lambda b: b + bytes(20 * 1024 * 1024)), True,
         "acima do teto", ()),
        ("Title ID trocado", trocar("bt3-x360/default.xex", xex_title), True, "Title ID", ()),
        ("default.swap ausente", tirar("bt3-x360/default.swap"), True, "falta no pacote", ()),
        ("README com nome de desenvolvimento", trocar("README.txt", lambda b: b + b"Put environment.txt here.\n"),
         True, "nome de desenvolvimento", ()),
        ("README com travessão", trocar("README.txt", lambda b: b + "A \u2014 B\n".encode()), True, "travessão", ()),
        ("link simbólico", plantar("bt3-x360/link.txt", b"/etc/passwd"), True, "link simbólico",
         ("bt3-x360/link.txt",)),
        ("caminho com ..", lambda a: a.__setitem__(raiz + "../fora.txt", b"x\n"), True, "'..'", ()),
    ]


def rodar(verificador, zipe, extra):
    r = subprocess.run([sys.executable, str(verificador), str(zipe), *extra], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def testar(zipe, trabalho, verificador, extra, mostrar=True):
    intacto = ler(zipe)
    raiz = next(iter(intacto)).split("/")[0] + "/"
    ok = True
    codigo, saida = rodar(verificador, zipe, extra)
    linhas = [("intacto", codigo, 0, saida.strip().splitlines()[-1] if saida.strip() else "")]
    ok &= codigo == 0
    for nome, estrago, somas, motivo, simbolicos in casos(raiz):
        arquivos = dict(intacto)
        estrago(arquivos)
        if somas:
            refazer_somas(arquivos, raiz)
        pasta = trabalho / re.sub(r"[^a-z0-9]+", "_", nome.lower()).strip("_")
        pasta.mkdir(parents=True, exist_ok=True)
        copia = pasta / zipe.name
        gravar(arquivos, copia, {raiz + s for s in simbolicos})
        codigo, saida = rodar(verificador, copia, extra)
        achado = next((l for l in saida.splitlines() if motivo in l), None)
        ok &= codigo == 1 and achado is not None
        linhas.append((nome, codigo, 1, achado or f"MOTIVO AUSENTE ({motivo!r}): " + saida.strip()[-200:]))
    if mostrar:
        for nome, codigo, esperado, texto in linhas:
            marca = "ok" if codigo == esperado and not texto.startswith("MOTIVO AUSENTE") else "ERRO"
            print(f"{marca:4} | {nome:36} | saída {codigo} (esperada {esperado}) | {texto}")
    return ok


MUTANTES = [
    ("sempre sai com 0", 'if falhas:\n        print(f"pacote recusado', 'if False:\n        print(f"pacote recusado'),
    ("não confere os hashes", "        self.somas(arquivos)\n", "        pass\n"),
    ("sem os nomes proibidos", "        for padrao, motivo in PROIBIDOS:", "        for padrao, motivo in []:"),
    ("sem as assinaturas", "            self.assinaturas(nome, rel, b)\n", "            pass\n"),
    ("sem o teto", "            if tam > teto:", "            if False:"),
]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("zip", type=Path)
    p.add_argument("--trabalho", type=Path, required=True)
    p.add_argument("--verificador", type=Path, default=AQUI / "pacote_confere.py")
    p.add_argument("--mutantes", action="store_true")
    a = p.parse_args()
    # versão e Title ID explícitos: as cópias mutantes do verificador moram fora da árvore
    versao = re.fullmatch(r"bt3-x360-(.+)\.zip", a.zip.name)[1]
    tid = next(l.split("=", 1)[1] for l in (AQUI.parent / "package/titulo.env").read_text().splitlines()
               if l.startswith("TITLE_ID="))
    extra = ["--versao", versao, "--title-id", tid]
    ok = testar(a.zip.resolve(), a.trabalho.resolve(), a.verificador.resolve(), extra)
    print("verificador:", "passa no intacto e acusa todos os estragos" if ok else "FALHOU")
    if a.mutantes:
        fonte = a.verificador.read_text()
        for nome, antes, depois in MUTANTES:
            if antes not in fonte:
                print(f"ERRO | mutante {nome!r}: trecho não encontrado no verificador")
                ok = False
                continue
            m = a.trabalho / "mutantes" / re.sub(r"[^a-z0-9]+", "_", nome) / "pacote_confere.py"
            m.parent.mkdir(parents=True, exist_ok=True)
            m.write_text(fonte.replace(antes, depois, 1))
            acusado = not testar(a.zip.resolve(), m.parent / "casos", m, extra, mostrar=False)
            print(f"{'ok' if acusado else 'ERRO':4} | mutante {nome!r}: {'acusado' if acusado else 'PASSOU'}")
            ok &= acusado
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
