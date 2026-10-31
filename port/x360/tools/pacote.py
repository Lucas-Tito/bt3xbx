#!/usr/bin/env python3
"""Monta o pacote da release do 360 (F8, etapa 5): bt3-x360-<versão>.zip, por lista branca.

    pacote.py [--build DIR] [--fontes RAIZ] [--saida DIR] [--estrito]

  --build   a pasta da build do build_f2.sh com default.xex, default.dat e default.swap (padrão:
            build/x360/f2 desta árvore). Os três vêm sempre da mesma pasta: andam juntos.
  --fontes  a árvore de onde vêm o convert_iso.py e os fontes do conversor (padrão: a árvore dona da pasta da
            build, se ela tiver port/x360/conv; senão esta). Os textos (README, licenças) vêm desta árvore.
  --saida   onde gravar o zip (padrão: build/x360/pacote). Nunca dentro do git.
  --estrito para a release: a build, os fontes e os textos da mesma árvore, sem diferença para o commit.

O conteúdo, com a pasta raiz bt3-x360-<versão>/:

  README.txt, LICENSE.txt, THIRD-PARTY.txt, licenses/   textos (o README com a versão de versao.env)
  bt3-x360/default.xex, default.dat, default.swap        a pasta que vai para o console
  pc-tools/port/x360/tools/convert_iso.py                a conversão no PC, com os fontes do conv_tool em
  pc-tools/port/x360/conv/*.c, *.h                       pc-tools/port/x360/conv (o mesmo leiaute do
                                                         repositório: o script acha os fontes por ele)
  SHA256SUMS.txt                                         de todos os outros, no formato do sha256sum

Zip reprodutível: entradas em ordem, data fixa, permissões fixas. Depois de gravar, o pacote_confere.py confere o
zip; qualquer falha apaga o zip e sai com 1. Não lê nenhum dado do jogo.
"""
import argparse
import ast
import hashlib
from pathlib import Path
import subprocess
import sys
import zipfile

AQUI = Path(__file__).resolve().parent
RAIZ = AQUI.parents[2]
sys.path.insert(0, str(AQUI))
import pacote_confere  # noqa: E402

DATA_FIXA = (2026, 1, 1, 0, 0, 0)


def git(raiz, *args):
    r = subprocess.run(["git", "-C", str(raiz), *args], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def importa_so_biblioteca_padrao(script):
    """O convert_iso.py vai sozinho para o PC do usuário: nada além da biblioteca padrão do Python."""
    fora = set()
    for no in ast.walk(ast.parse(script.read_text(), str(script))):
        nomes = [a.name for a in no.names] if isinstance(no, ast.Import) else \
                [no.module] if isinstance(no, ast.ImportFrom) and no.level == 0 and no.module else []
        fora |= {n.split(".")[0] for n in nomes} - set(sys.stdlib_module_names)
    if fora:
        raise SystemExit(f"{script}: importa fora da biblioteca padrão: {sorted(fora)}; incluir no pacote antes")


def montar(build, fontes, versao):
    pk = RAIZ / "port/x360/package"
    readme = (pk / "README.txt").read_text()
    if "{{VERSION}}" not in readme:
        raise SystemExit("README.txt sem o marcador {{VERSION}}")
    script = fontes / "port/x360/tools/convert_iso.py"
    importa_so_biblioteca_padrao(script)
    itens = {
        "README.txt": readme.replace("{{VERSION}}", versao).encode(),
        "LICENSE.txt": (RAIZ / "LICENSE").read_bytes(),
        "THIRD-PARTY.txt": (pk / "THIRD-PARTY.txt").read_bytes(),
        "licenses/COPYING.NEWLIB.txt": (RAIZ / "port/third_party/newlib_libm/COPYING.NEWLIB").read_bytes(),
        "licenses/Kenney-Input-Prompts-License.txt": (RAIZ / "port/x360/arte/kenney/License.txt").read_bytes(),
        "pc-tools/port/x360/tools/convert_iso.py": script.read_bytes(),
    }
    for n in ("default.xex", "default.dat", "default.swap"):
        itens[f"bt3-x360/{n}"] = (build / n).read_bytes()
    # todos os fontes do conversor: o convert_iso.py guarda na marca o hash de port/x360/conv/*.[ch]
    for c in sorted((fontes / "port/x360/conv").glob("*.[ch]")):
        itens[f"pc-tools/port/x360/conv/{c.name}"] = c.read_bytes()
    somas = "".join(f"{hashlib.sha256(b).hexdigest()}  {rel}\n" for rel, b in sorted(itens.items()))
    itens["SHA256SUMS.txt"] = somas.encode()
    return itens


def gravar(itens, destino, raiz_zip):
    temporario = destino.with_suffix(".zip.tmp")
    with zipfile.ZipFile(temporario, "w") as z:
        for rel in sorted(itens):
            info = zipfile.ZipInfo(raiz_zip + rel, DATA_FIXA)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3  # Unix, para as permissões abaixo valerem igual em toda máquina
            info.external_attr = 0o100644 << 16
            z.writestr(info, itens[rel], compresslevel=9)
    temporario.replace(destino)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build", type=Path, default=RAIZ / "build/x360/f2")
    p.add_argument("--fontes", type=Path)
    p.add_argument("--saida", type=Path, default=RAIZ / "build/x360/pacote")
    p.add_argument("--estrito", action="store_true")
    a = p.parse_args()
    build = a.build.resolve()
    dona = build.parents[2] if len(build.parents) > 2 else None
    fontes = (a.fontes or (dona if dona and (dona / "port/x360/conv").is_dir() else RAIZ)).resolve()
    versao = pacote_confere.ler_env(RAIZ / "port/x360/package/versao.env", "VERSION")

    commit_build, commit_fontes, commit_textos = (git(r, "rev-parse", "--short", "HEAD") for r in (dona, fontes, RAIZ))
    print(f"versão {versao}; build {build} (árvore {commit_build}); conversor de {fontes} ({commit_fontes}); "
          f"textos de {RAIZ} ({commit_textos})")
    if a.estrito:
        problemas = []
        if not (dona == fontes == RAIZ):
            problemas.append("a build, os fontes e os textos têm de vir desta mesma árvore")
        if git(RAIZ, "status", "--porcelain", "--untracked-files=no"):
            problemas.append("a árvore tem diferença para o commit")
        if problemas:
            raise SystemExit("--estrito: " + "; ".join(problemas))
    elif not (commit_build == commit_fontes == commit_textos):
        print("aviso: commits diferentes entre build, conversor e textos; serve para ensaio, não para a release")

    itens = montar(build, fontes, versao)
    a.saida.mkdir(parents=True, exist_ok=True)
    raiz_zip = f"bt3-x360-{versao}/"
    destino = a.saida.resolve() / f"bt3-x360-{versao}.zip"
    gravar(itens, destino, raiz_zip)

    falhas, arquivos, total = pacote_confere.conferir(destino, versao, fontes=RAIZ if a.estrito else None)
    if falhas:
        destino.unlink()
        for f in falhas:
            print(f"FALHA: {f}")
        raise SystemExit("pacote recusado pelo pacote_confere.py; zip apagado")
    for rel in sorted(itens):
        print(f"  {len(itens[rel]):>9}  {raiz_zip}{rel}")
    print(f"{destino}: {destino.stat().st_size} bytes, {len(arquivos)} arquivos ({total} bytes abertos), "
          f"sha256 {hashlib.sha256(destino.read_bytes()).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
