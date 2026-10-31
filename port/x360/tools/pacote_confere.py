#!/usr/bin/env python3
"""Confere o pacote da release do 360 (F8, etapa 5): sai com 1 em qualquer coisa fora da regra.

    pacote_confere.py PACOTE [--versao V] [--title-id 0xFFFF4254] [--fontes RAIZ]

PACOTE é o bt3-x360-<versão>.zip do pacote.py ou uma pasta com o conteúdo dele já aberto (a pasta raiz
bt3-x360-<versão> dentro). Regras (docs/x360/plano/F8/06-publicacao.md):

  - tudo sob a pasta raiz bt3-x360-<versão>/, sem caminho absoluto, `..`, link simbólico, pasta explícita
    ou nome repetido;
  - nomes proibidos: ISO, AFS e .bin, ELF, filmes e sons do jogo, environment.txt, ambiente*.txt, logs,
    output.txt, crash.txt, capturas, saves, .map, .pdb, temporários;
  - lista branca: só os arquivos de LISTA e os fontes do conversor em pc-tools/port/x360/conv;
  - teto de tamanho por arquivo e no total;
  - assinaturas de ISO 9660, AFS, ELF, MPEG (PSS), ADX, imagens e arquivos compactados, em qualquer arquivo e
    nos inícios de setor de 2.048 bytes;
  - formato: default.xex é XEX2 com o Title ID de titulo.env e sem mídia de disco; default.dat e default.swap
    têm o cabeçalho e o tamanho do dat360.py; os textos são UTF-8 sem NUL; o README tem a versão e nenhum
    nome de desenvolvimento;
  - SHA256SUMS.txt lista exatamente os outros arquivos, com o hash certo;
  - com --fontes, os fontes do conversor, o convert_iso.py, as licenças e o README iguais aos da árvore.

Não abre nenhum dado do jogo. O teste das regras, com um estrago de cada tipo, é teste_pacote.py.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import stat
import struct
import sys
import zipfile

RAIZ = Path(__file__).resolve().parents[3]
SETOR = 2048
KIB, MIB = 1024, 1024 * 1024

# Caminho dentro da pasta raiz: teto de tamanho em bytes e tipo (xex, dat, swap ou texto).
LISTA = {
    "README.txt": (256 * KIB, "texto"),
    "LICENSE.txt": (64 * KIB, "texto"),
    "THIRD-PARTY.txt": (128 * KIB, "texto"),
    "SHA256SUMS.txt": (64 * KIB, "texto"),
    "licenses/COPYING.NEWLIB.txt": (128 * KIB, "texto"),
    "licenses/Kenney-Input-Prompts-License.txt": (16 * KIB, "texto"),
    "bt3-x360/default.xex": (16 * MIB, "xex"),
    "bt3-x360/default.dat": (1 * MIB, "dat"),
    "bt3-x360/default.swap": (1 * MIB, "swap"),
    "pc-tools/port/x360/tools/convert_iso.py": (256 * KIB, "texto"),
}
CONV = re.compile(r"pc-tools/port/x360/conv/[a-z0-9_]{1,40}\.[ch]")
TETO_CONV = 512 * KIB
TETO_TOTAL = 32 * MIB
# Os fontes mínimos do conversor no PC (o resto entra pelo padrão CONV, conferido com --fontes).
CONV_MINIMO = ("conv.c", "conv.h", "conv_tool.c", "icones360.c", "icones360_arte.h")

# Nomes proibidos, conferidos antes da lista branca para o motivo sair claro. Comparação sem caixa.
PROIBIDOS = [
    (r"\.(iso|cso|chd|img|mdf|nrg|cue)$", "imagem de disco"),
    (r"\.(bin|afs)$", "arquivo .bin ou AFS do jogo"),
    (r"(\.elf|\.irx|(^|/)slus_\d{3}\.\d{2})$", "executável do PS2 (ELF)"),
    (r"\.(pss|adx|ahx|vag|xma|wav|ogg|unk)$", "filme ou som do jogo"),
    (r"(^|/)environment\.txt$", "environment.txt (configuração de desenvolvimento)"),
    (r"(^|/)ambiente[^/]*\.txt$", "ambiente*.txt (configuração de desenvolvimento)"),
    (r"\.log$", "log"),
    (r"(^|/)(output|saida|bt3xbx)\.txt$", "saída de diagnóstico"),
    (r"(^|/)crash\.txt$", "crash.txt"),
    (r"(^|/)(shots|capturas)/|\.(png|ppm|bmp|jpe?g|tga|dds|gif|raw)$", "captura ou imagem"),
    (r"(^|/)saves/|(^|/)baslus-|(^|/)icon\.sys$|\.(sav|mcd|ps2|max|psu)$", "save"),
    (r"\.(map|pdb|xdb|exe|obj|lib)$", "artefato de build fora do pacote (.map, .pdb, .exe...)"),
    (r"\.(tmp|new|part)$", "temporário"),
    (r"\.(zip|7z|rar|gz|xz|bz2|tar)$", "arquivo compactado dentro do pacote"),
]

# Assinaturas no início do arquivo.
INICIOS = [
    (b"\x7fELF", "ELF"),
    (b"AFS\x00", "AFS"),
    (b"\x00\x00\x01\xba", "MPEG (PSS)"),
    (b"\x00\x00\x01\xb3", "MPEG (vídeo)"),
    (b"\x89PNG", "PNG"),
    (b"\xff\xd8\xff", "JPEG"),
    (b"P6\n", "PPM"),
    (b"GIF8", "GIF"),
    (b"PK\x03\x04", "ZIP"),
    (b"7z\xbc\xaf", "7z"),
    (b"Rar!", "RAR"),
    (b"\x1f\x8b", "gzip"),
    (b"RIFF", "RIFF (WAV)"),
]
# Assinaturas procuradas em todo início de setor (um pedaço de ISO ou AFS embutido noutro arquivo).
SETORES = [(b"\x01CD001", "ISO 9660"), (b"\x02CD001", "ISO 9660"), (b"\xffCD001", "ISO 9660"),
           (b"AFS\x00", "AFS"), (b"\x7fELF", "ELF"), (b"\x00\x00\x01\xba", "MPEG (PSS)")]
# Nomes do desenvolvimento que nenhum texto do usuário cita.
TEXTO_DEV = re.compile(r"environment\.txt|ambiente|BT3_[A-Z]|bt3xbx\.log|output\.txt|/home/|wsl\.sh", re.I)
TRAVESSAO = re.compile("[\\u2013\\u2014]")


def ler_env(arquivo, chave):
    for linha in arquivo.read_text().splitlines():
        if linha.startswith(chave + "="):
            return linha.split("=", 1)[1].strip()
    raise SystemExit(f"{arquivo}: falta {chave}")


class Conferencia:
    def __init__(self, versao, title_id, fontes):
        self.versao, self.title_id, self.fontes = versao, title_id, fontes
        self.raiz = f"bt3-x360-{versao}/"
        self.falhas = []

    def falha(self, nome, motivo):
        self.falhas.append(f"{nome}: {motivo}")

    # ---------- leitura: zip ou pasta, sempre com o tamanho conferido antes de ler ----------
    def entradas_zip(self, caminho):
        arquivos = {}
        with zipfile.ZipFile(caminho) as z:
            vistos = set()
            for info in z.infolist():
                nome = info.filename
                if nome in vistos:
                    self.falha(nome, "nome repetido no zip")
                    continue
                vistos.add(nome)
                modo = info.external_attr >> 16
                if stat.S_ISLNK(modo):
                    self.falha(nome, "link simbólico")
                    continue
                if nome.endswith("/") or stat.S_ISDIR(modo):
                    self.falha(nome, "pasta explícita no zip (o pacote.py não grava pastas)")
                    continue
                if info.file_size > TETO_TOTAL:
                    self.falha(nome, f"acima do teto total ({info.file_size} bytes)")
                    continue
                arquivos[nome] = (info.file_size, lambda i=info: z.read(i))
            return {n: (t, f()) for n, (t, f) in arquivos.items()}

    def entradas_pasta(self, caminho):
        base = caminho.parent
        arquivos = {}
        for atual, pastas, nomes in os.walk(caminho, followlinks=False):
            for n in pastas + nomes:
                p = Path(atual) / n
                rel = p.relative_to(base).as_posix()
                if p.is_symlink():
                    self.falha(rel, "link simbólico")
                elif p.is_file():
                    t = p.stat().st_size
                    if t > TETO_TOTAL:
                        self.falha(rel, f"acima do teto total ({t} bytes)")
                    else:
                        arquivos[rel] = (t, p.read_bytes())
                elif not p.is_dir():
                    self.falha(rel, "não é arquivo comum")
        return arquivos

    # ---------- regras ----------
    def caminho(self, nome):
        if nome.startswith("/") or "\\" in nome or re.match(r"[A-Za-z]:", nome):
            return self.falha(nome, "caminho absoluto ou com barra invertida")
        if any(p in ("", ".", "..") for p in nome.split("/")):
            return self.falha(nome, "caminho com '..', '.' ou parte vazia")
        if not nome.startswith(self.raiz):
            return self.falha(nome, f"fora da pasta raiz {self.raiz}")
        return nome[len(self.raiz):]

    def nome(self, rel, nome):
        for padrao, motivo in PROIBIDOS:
            if re.search(padrao, rel, re.I):
                self.falha(nome, f"proibido: {motivo}")
                return None
        if rel in LISTA:
            return LISTA[rel]
        if CONV.fullmatch(rel):
            return (TETO_CONV, "texto")
        self.falha(nome, "fora da lista branca")
        return None

    def assinaturas(self, nome, rel, b):
        bmp = b[:2] == b"BM" and len(b) >= 14 and struct.unpack("<I", b[2:6])[0] == len(b)
        for sig, tipo in INICIOS + ([(b"BM", "BMP")] if bmp else []):
            if b.startswith(sig):
                self.falha(nome, f"assinatura de {tipo} no início")
                return
        for pos in range(0, len(b), SETOR):
            for sig, tipo in SETORES:
                if b.startswith(sig, pos):
                    self.falha(nome, f"assinatura de {tipo} no setor {pos // SETOR}")
                    return
        if b.find(b"(c)CRI") >= 0:
            self.falha(nome, "assinatura de ADX/AFS da CRI")

    def xex(self, nome, b):
        if len(b) < 24 or b[:4] != b"XEX2":
            return self.falha(nome, "não é XEX2")
        _, _, _, seguranca, n = struct.unpack(">IIIII", b[4:24])
        exec_info = None
        for i in range(n):
            if 32 + 8 * i > len(b):
                return self.falha(nome, "cabeçalhos opcionais truncados")
            chave, valor = struct.unpack(">II", b[24 + 8 * i:32 + 8 * i])
            if chave == 0x00040006:
                exec_info = valor
        if exec_info is None or exec_info + 24 > len(b):
            return self.falha(nome, "sem as informações de execução (Title ID)")
        tid = struct.unpack(">I", b[exec_info + 12:exec_info + 16])[0]
        if tid != self.title_id:
            self.falha(nome, f"Title ID 0x{tid:08X}, esperado 0x{self.title_id:08X}")
        if seguranca + 0x180 > len(b):
            return self.falha(nome, "informações de segurança truncadas")
        midias = struct.unpack(">I", b[seguranca + 0x17C:seguranca + 0x180])[0]
        if not midias & 0x1 or midias & 0x1E:
            self.falha(nome, f"mídias 0x{midias:08X}: precisa do HD e de nenhuma mídia de disco (sem <dvdxgd2/>)")

    def tabela(self, nome, b, magia, cab, item):
        if len(b) < cab or b[:4] != magia:
            return self.falha(nome, f"sem o cabeçalho {magia.decode()}")
        n = struct.unpack("<I", b[4:8])[0]
        if n == 0 or len(b) != cab + n * item:
            self.falha(nome, f"tamanho {len(b)} incompatível com {n} itens do cabeçalho {magia.decode()}")

    def texto(self, nome, rel, b):
        if b"\0" in b:
            return self.falha(nome, "texto com byte NUL (binário disfarçado)")
        try:
            t = b.decode("utf-8")
        except UnicodeDecodeError:
            return self.falha(nome, "texto que não é UTF-8")
        if rel in ("README.txt", "THIRD-PARTY.txt"):
            m = TEXTO_DEV.search(t)
            if m:
                self.falha(nome, f"texto do usuário cita nome de desenvolvimento: {m.group(0)!r}")
            if TRAVESSAO.search(t):
                self.falha(nome, "texto do usuário com travessão")
        if rel == "README.txt" and self.versao not in t:
            self.falha(nome, f"README sem a versão {self.versao}")

    def somas(self, arquivos):
        chave = "SHA256SUMS.txt"
        if chave not in arquivos:
            return
        esperado = {}
        for i, linha in enumerate(arquivos[chave].decode("utf-8", "replace").splitlines(), 1):
            m = re.fullmatch(r"([0-9a-f]{64}) [ *](\S.*)", linha)
            if not m:
                self.falha(chave, f"linha {i} fora do formato do sha256sum")
                continue
            if m[2] in esperado:
                self.falha(chave, f"{m[2]} repetido")
            esperado[m[2]] = m[1]
        for rel, b in arquivos.items():
            if rel == chave:
                continue
            if rel not in esperado:
                self.falha(rel, "fora do SHA256SUMS.txt")
            elif hashlib.sha256(b).hexdigest() != esperado[rel]:
                self.falha(rel, "SHA256 não bate com o SHA256SUMS.txt")
        for rel in esperado.keys() - arquivos.keys():
            self.falha(rel, "listado no SHA256SUMS.txt mas ausente")

    def contra_fontes(self, arquivos):
        f = self.fontes
        iguais = {"LICENSE.txt": f / "LICENSE",
                  "THIRD-PARTY.txt": f / "port/x360/package/THIRD-PARTY.txt",
                  "licenses/COPYING.NEWLIB.txt": f / "port/third_party/newlib_libm/COPYING.NEWLIB",
                  "licenses/Kenney-Input-Prompts-License.txt": f / "port/x360/arte/kenney/License.txt",
                  "pc-tools/port/x360/tools/convert_iso.py": f / "port/x360/tools/convert_iso.py"}
        for c in sorted((f / "port/x360/conv").glob("*.[ch]")):
            iguais[f"pc-tools/port/x360/conv/{c.name}"] = c
        for rel, origem in iguais.items():
            if rel not in arquivos:
                self.falha(rel, f"falta (existe em {origem})")
            elif arquivos[rel] != origem.read_bytes():
                self.falha(rel, f"diferente de {origem}")
        for rel in arquivos:
            if rel.startswith("pc-tools/port/x360/conv/") and rel not in iguais:
                self.falha(rel, "fonte do conversor que não existe na árvore")
        readme = (f / "port/x360/package/README.txt").read_text().replace("{{VERSION}}", self.versao).encode()
        if arquivos.get("README.txt") != readme:
            self.falha("README.txt", "diferente do modelo port/x360/package/README.txt com a versão")

    def conferir(self, pacote):
        pacote = Path(pacote)
        if pacote.is_dir():
            if pacote.name + "/" != self.raiz:
                self.falha(str(pacote), f"a pasta deve se chamar {self.raiz[:-1]}")
            brutos = self.entradas_pasta(pacote)
        else:
            if pacote.name != self.raiz[:-1] + ".zip":
                self.falha(pacote.name, f"o zip deve se chamar {self.raiz[:-1]}.zip")
            brutos = self.entradas_zip(pacote)
        arquivos, total = {}, 0
        for nome, (tam, b) in sorted(brutos.items()):
            rel = self.caminho(nome)
            if rel is None:
                continue
            regra = self.nome(rel, nome)
            total += tam
            self.assinaturas(nome, rel, b)
            if regra is None:
                continue
            teto, tipo = regra
            if tam > teto:
                self.falha(nome, f"acima do teto de tamanho ({tam} > {teto} bytes)")
                continue
            if tipo == "xex":
                self.xex(nome, b)
            elif tipo == "dat":
                self.tabela(nome, b, b"BT3D", 16, 16)
            elif tipo == "swap":
                self.tabela(nome, b, b"BT3S", 8, 8)
            else:
                self.texto(nome, rel, b)
            arquivos[rel] = b
        if total > TETO_TOTAL:
            self.falha("(pacote)", f"acima do teto total ({total} > {TETO_TOTAL} bytes)")
        for rel in LISTA:
            if rel not in arquivos and not any(f.startswith(self.raiz + rel + ":") for f in self.falhas):
                self.falha(self.raiz + rel, "falta no pacote")
        for c in CONV_MINIMO:
            if f"pc-tools/port/x360/conv/{c}" not in arquivos:
                self.falha(f"{self.raiz}pc-tools/port/x360/conv/{c}", "falta no pacote")
        self.somas(arquivos)
        if self.fontes:
            self.contra_fontes(arquivos)
        return arquivos, total


def conferir(pacote, versao=None, title_id=None, fontes=None):
    """Devolve (falhas, arquivos, total). Usado pelo pacote.py depois de montar o zip."""
    versao = versao or ler_env(RAIZ / "port/x360/package/versao.env", "VERSION")
    title_id = title_id if title_id is not None else int(ler_env(RAIZ / "port/x360/package/titulo.env", "TITLE_ID"), 16)
    c = Conferencia(versao, title_id, Path(fontes) if fontes else None)
    arquivos, total = c.conferir(pacote)
    return c.falhas, arquivos, total


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("pacote", type=Path)
    p.add_argument("--versao")
    p.add_argument("--title-id", type=lambda s: int(s, 16))
    p.add_argument("--fontes", type=Path, help="raiz da árvore do commit da build, para conferir fontes e textos")
    a = p.parse_args()
    try:
        falhas, arquivos, total = conferir(a.pacote, a.versao, a.title_id, a.fontes)
    except (OSError, zipfile.BadZipFile, ValueError) as e:
        print(f"FALHA: {a.pacote}: {e}")
        return 1
    for f in falhas:
        print(f"FALHA: {f}")
    if falhas:
        print(f"pacote recusado: {len(falhas)} falha(s)")
        return 1
    print(f"pacote conferido: {len(arquivos)} arquivos, {total} bytes, nenhum fora da regra")
    return 0


if __name__ == "__main__":
    sys.exit(main())
