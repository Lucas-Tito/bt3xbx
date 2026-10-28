#!/usr/bin/env python3
"""F8: identificação, recusa e busca do iso360.c real em imagens sintéticas esparsas.

Não usa dados do jogo, console nem emulador. Compila também com a restrição C89 de declarações.
Os controles negativos exigem falha em execução, com compilação bem-sucedida.
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
SETOR = 2048
TAMANHO = 3020816384
MARCA = 0x3DF400
CNF = b"BOOT2 = cdrom0:\\SLUS_216.78;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n"


def duplo(v, n=4):
    return v.to_bytes(n, "little") + v.to_bytes(n, "big")


def registro(nome, lba, tam, pasta=False):
    r = bytearray(33 + len(nome) + ((33 + len(nome)) % 2))
    r[0] = len(r)
    r[2:10] = duplo(lba)
    r[10:18] = duplo(tam)
    r[25] = 2 if pasta else 0
    r[28:32] = duplo(1, 2)
    r[32] = len(nome)
    r[33:33 + len(nome)] = nome
    return r


def imagem(p, marca=True, volume=b"SLUS_21678", cnf=CNF, setores=TAMANHO // SETOR, base=24):
    """Cabeçalho e três entradas inventadas. Só os identificadores públicos são do título esperado."""
    pvd = bytearray(SETOR)
    pvd[:7] = b"\1CD001\1"
    pvd[40:72] = volume.ljust(32, b" ")
    pvd[80:88] = duplo(setores)
    pvd[128:132] = duplo(SETOR, 2)
    pvd[156:190] = registro(b"\0", 18, SETOR, True)
    raiz = registro(b"\0", 18, SETOR, True) + registro(b"\1", 18, SETOR, True)
    raiz += registro(b"SYSTEM.CNF;1", 20, len(cnf)) + registro(b"DATA", 19, SETOR, True)
    dados = b"".join(registro(f"PZS3US{i}.AFS;1".encode(), base + i * 2, 2 * SETOR) for i in range(3))
    with p.open("wb") as f:
        f.truncate(setores * SETOR)
        for pos, b in [(16 * SETOR, pvd), (18 * SETOR, raiz), (19 * SETOR, dados), (20 * SETOR, cnf)]:
            f.seek(pos)
            f.write(b)
        for i in range(3):
            f.seek((base + i * 2) * SETOR)
            f.write(b"AFS\0" + struct.pack("<III", 1, SETOR, 9))
            f.seek((base + i * 2 + 1) * SETOR)
            f.write(b"TESTE-ISO")
        if marca:
            f.seek(MARCA)
            f.write(struct.pack("<16sIIQ", b"BT3X360-ISO", 2, 512, TAMANHO).ljust(512, b"\0"))


def troca(p, pos, b):
    with p.open("r+b") as f:
        f.seek(pos)
        f.write(b)


def compilar(raiz, pasta, fonte=None):
    exe = pasta / "teste_iso"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu89", "-Wdeclaration-after-statement",
                    "-Wall", "-Wextra", "-Werror", "-Wno-stringop-truncation", "-O2",
                    "-I" + str(raiz / "port/x360/src/som/teste/xtl"),
                    "-I" + str(raiz / "port/x360/src"), "-I" + str(raiz / "port/x360/conv"),
                    str(fonte or raiz / "port/x360/src/iso360.c"),
                    str(raiz / "port/x360/src/teste_iso/teste_iso.c"), "-lpthread", "-o", str(exe)], check=True)
    return exe


def conferir(exe, pasta, motivo, nome, explicita=None):
    env = os.environ.copy()
    env.pop("BT3_ISO", None)
    env["TESTE_GAME"] = str(pasta)
    if explicita is not None:
        env["BT3_ISO"] = str(explicita)
    return subprocess.run([str(exe), str(motivo), str(nome)], env=env, capture_output=True, text=True, timeout=10)


def exigir(r, caso):
    if r.returncode:
        raise ValueError(f"{caso}:\n{r.stdout}\n{r.stderr}")
    print(f"ok: {caso}", flush=True)


def rodada(exe, pasta, anunciar=True):
    casos = []
    p = pasta / "teste.iso"

    def caso(nome, motivo, **kw):
        imagem(p, **kw)
        casos.append((nome, conferir(exe, pasta, motivo, p, p)))

    caso("USA convertida", 0)
    caso("USA original", 1, marca=False)
    caso("outra região sem marca", 3, volume=b"SLES_54945", marca=False,
         cnf=CNF.replace(b"SLUS_216.78", b"SLES_549.45"))
    caso("marca em outra região", 3, volume=b"SLES_54945")
    caso("volume USA com BOOT2 de outra região", 3, cnf=CNF.replace(b"SLUS_216.78", b"SLES_549.45"))
    caso("BOOT2 USA com volume diferente", 3, volume=b"OUTRO_DISCO")
    caso("revisão diferente", 3, cnf=CNF.replace(b"1.00", b"2.00"))
    caso("volume de outra revisão", 3, setores=TAMANHO // SETOR + 1)
    caso("SYSTEM.CNF com espaços e LF", 0,
         cnf=b"\tBOOT2= cdrom0:\\SLUS_216.78;1 \nVER=1.00\nVMODE=NTSC\n")
    caso("SYSTEM.CNF com chave repetida", 3, cnf=CNF + b"VER = 1.00\n")
    caso("valor de BOOT2 incompleto", 3, cnf=CNF.replace(b";1", b""))
    for nome, pos, b in [
        ("PVD inválido", 16 * SETOR, b"X"),
        ("versão do PVD inválida", 16 * SETOR + 6, b"\2"),
        ("ordens do volume diferentes", 16 * SETOR + 84, b"\1"),
        ("setor inválido", 16 * SETOR + 129, b"\4"),
        ("raiz fora do volume", 16 * SETOR + 158, duplo(0xFFFFFFFF)),
        ("registro curto", 18 * SETOR, b"\1"),
        ("nome cruza registro", 18 * SETOR + 32, b"\xff"),
        ("SYSTEM.CNF fora do volume", 18 * SETOR + 68 + 2, duplo(0xFFFFFFFF)),
        ("SYSTEM.CNF acima do limite", 18 * SETOR + 68 + 10, duplo(1025)),
        ("SYSTEM.CNF ausente", 18 * SETOR + 68 + 33, b"X"),
        ("SYSTEM.CNF com nulo", 20 * SETOR + 5, b"\0"),
        ("marca desconhecida", MARCA, b"X"),
        ("tamanho da marca inválido", MARCA + 20, struct.pack("<I", 1)),
        ("tamanho da origem inválido", MARCA + 24, struct.pack("<Q", 1)),
    ]:
        imagem(p)
        troca(p, pos, b)
        casos.append((nome, conferir(exe, pasta, 4, p, p)))
    imagem(p)
    prefixo = registro(b"X" * 220, 20, 1) * 8
    assert len(prefixo) == 2032
    troca(p, 18 * SETOR, prefixo + b"\xff" + b"\0" * 15)
    casos.append(("registro cruza setor", conferir(exe, pasta, 4, p, p)))
    for tam in (0, 16 * SETOR + 6, MARCA + 128, TAMANHO - 1):
        imagem(p)
        with p.open("r+b") as f:
            f.truncate(tam)
        casos.append((f"truncada em {tam}", conferir(exe, pasta, 4, p, p)))
    for versao in (0, 1, 3, 0xFFFFFFFF):
        imagem(p)
        troca(p, MARCA + 16, struct.pack("<I", versao))
        casos.append((f"conversão versão {versao} recusada", conferir(exe, pasta, 1, p, p)))
    p.unlink()
    casos.append(("caminho explícito ausente", conferir(exe, pasta, 2, p, p)))
    casos.append(("sem ISO mostra caixa com nome esperado", conferir(exe, pasta, 2, "game:\\bt3-x360.iso")))
    a, b, c = (pasta / n for n in ("a.iso", "b.iso", "c.iso"))
    imagem(a, marca=False)
    imagem(b, base=36)
    casos.append(("original antes da convertida", conferir(exe, pasta, 0, "game:\\b.iso")))
    casos.append(("BT3_ISO original não busca alternativa", conferir(exe, pasta, 1, a, a)))
    casos.append(("BT3_ISO ausente não busca alternativa", conferir(exe, pasta, 2, p, p)))
    casos.append(("BT3_ISO convertida", conferir(exe, pasta, 0, b, b)))
    imagem(a, base=40)
    imagem(b, marca=False)
    casos.append(("convertida antes da original", conferir(exe, pasta, 0, "game:\\a.iso")))
    troca(a, 16 * SETOR, b"X")
    imagem(c, base=44)
    casos.append(("inválida e original antes da convertida", conferir(exe, pasta, 0, "game:\\c.iso")))
    c.unlink()
    casos.append(("sem convertida prefere orientar USA original", conferir(exe, pasta, 1, "game:\\b.iso")))
    b.unlink()
    casos.append(("busca só com imagem inválida", conferir(exe, pasta, 4, "game:\\a.iso")))
    imagem(a, volume=b"OUTRA_REGIAO")
    casos.append(("busca só com região incompatível", conferir(exe, pasta, 3, "game:\\a.iso")))
    imagem(b)
    nome_publico = pasta / "bt3-x360.iso"
    imagem(nome_publico, base=52)
    casos.append(("nome público preferido à outra convertida", conferir(exe, pasta, 0, "game:\\bt3-x360.iso")))
    nome_publico.unlink()
    b.unlink()
    a.unlink()
    for nome, r in casos:
        if anunciar:
            exigir(r, nome)
        elif r.returncode:
            return r
    return None


def textos(raiz, pasta):
    fonte = (raiz / "port/x360/src/iso_avisos.h").read_text()
    principal = (raiz / "port/x360/src/f2/main360.cpp").read_text()
    if 'Iso_Aviso(motivo, &kTitulo, &kTexto);' not in principal:
        raise ValueError("interface não usa os textos conferidos")
    programa = pasta / "textos.c"
    programa.write_text(r'''#include <wchar.h>
#include <stdio.h>
#include "iso_avisos.h"
int main(void) {
    const wchar_t *titulo, *texto;
    int i;
    for (i = 1; i <= 4; i++) {
        Iso_Aviso(i, &titulo, &texto);
        if (!wcsstr(texto, L"bt3-x360.iso") || !wcsstr(texto, L"root of the game folder") ||
            (i != 2 && !wcsstr(texto, L"convert_iso.py")) || wcsstr(texto, L"ambiente.txt") ||
            wcsstr(texto, L"BT3_ISO")) return 1;
    }
    Iso_Aviso(2, &titulo, &texto);
    if (wcscmp(titulo, L"ISO not found") || !wcsstr(texto, L"expected name") ||
        wcscmp(wcsstr(texto, L"bt3-x360.iso"), L"bt3-x360.iso.")) return 1;
    Iso_Aviso(1, &titulo, &texto);
    if (wcscmp(titulo, L"ISO conversion required")) return 1;
    puts("textos: tudo certo");
    return 0;
}
''')
    def executar(d):
        exe = d / "textos"
        subprocess.run([os.environ.get("CC", "gcc"), "-std=c89", "-Wall", "-Wextra", "-Werror",
                        "-I"+str(d), "-I"+str(raiz / "port/x360/src"), str(programa), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], capture_output=True)
    if executar(pasta).returncode:
        raise ValueError("textos públicos incompatíveis")
    for j, (de, para) in enumerate([("root of the game folder", "ambiente.txt"),
                                  ("expected name", "nome esperado"),
                                  ("ISO conversion required", "Unsupported converted ISO version"),
                                  ("convert_iso.py", "iso_convertida.py"),
                                  ('ISO_LARGO(ISO_NOME) L".";', 'ISO_LARGO(ISO_NOME) L". Copy it there.";')]):
        d = pasta / f"texto{j}"
        d.mkdir()
        (d / "iso_avisos.h").write_text(fonte.replace(de, para))
        if executar(d).returncode != 1:
            raise ValueError("verificador dos textos aceitou mutante: "+de)
        print("ok: texto estragado recusado: "+de, flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raiz", type=Path, default=RAIZ)
    ap.add_argument("--conferir", type=Path, help="confere uma imagem sintética já criada, esperando convertida")
    args = ap.parse_args()
    with tempfile.TemporaryDirectory(prefix="f8-avisos-") as t:
        pasta = Path(t)
        exe = compilar(args.raiz, pasta)
        if args.conferir:
            p = args.conferir.resolve()
            exigir(conferir(exe, pasta, 0, p, p), "imagem fornecida")
            return
        rodada(exe, pasta)
        textos(args.raiz, pasta)
        # Entrada estragada: o mesmo verificador externo precisa recusar, com código 1, uma cópia sintética.
        p = pasta / "estragada.iso"
        imagem(p)
        troca(p, 16 * SETOR, b"X")
        r = subprocess.run([os.sys.executable, __file__, "--raiz", str(args.raiz), "--conferir", str(p)],
                           capture_output=True, text=True, timeout=30)
        if r.returncode != 1 or "FALHA:" not in r.stderr:
            raise ValueError("o verificador não recusou a cópia estragada em execução")
        print("ok: verificador externo visto falhando na cópia estragada", flush=True)
        p.unlink()
        fonte = (args.raiz / "port/x360/src/iso360.c").read_text()
        mutantes = [
            ("volume ignorado", 'usa = memcmp(pvd + 40, "SLUS_21678", 10) == 0;', 'usa = 1;'),
            ("BOOT2 ignorado", '!cnf_valor(cnf, "BOOT2", "cdrom0:\\\\SLUS_216.78;1")', '0'),
            ("revisão ignorada", '!cnf_valor(cnf, "VER", "1.00")', '0'),
            ("tamanho ignorado", 'sIsoTam != sVolumeTam', 'sIsoTam < 0'),
            ("versão dispensada", 'le32(m + 16) != ISO_MARCA_VERSAO', '0'),
            ("nome público ignorado", 'tentar("game:\\\\" ISO_NOME)', 'ISO_AUSENTE'),
            ("marca dispensada", 'marca > 0 ? ISO_OK', 'marca >= 0 ? ISO_OK'),
            ("busca para na primeira", 'while (FindNextFile(h, &d))', 'while (0 && FindNextFile(h, &d))'),
        ]
        for i, (nome, de, para) in enumerate(mutantes):
            if fonte.count(de) != 1:
                raise ValueError(f"mutação {nome}: trecho não é único")
            d = pasta / f"mutante{i}"
            d.mkdir()
            f = d / "iso360.c"
            f.write_text(fonte.replace(de, para))
            prog = compilar(args.raiz, d, f)
            r = rodada(prog, d, anunciar=False)
            if r is None or r.returncode != 1 or "FALHA:" not in r.stdout:
                raise ValueError(f"mutante não acusado em execução: {nome}")
            print(f"ok: mutante acusado: {nome}; " + next(l for l in r.stdout.splitlines() if "FALHA:" in l), flush=True)
    print("teste_iso_avisos: tudo certo", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, subprocess.SubprocessError) as e:
        print(f"FALHA: {e}", file=os.sys.stderr)
        raise SystemExit(1)
