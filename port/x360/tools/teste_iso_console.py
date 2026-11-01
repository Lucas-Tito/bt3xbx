#!/usr/bin/env python3
"""Compara a instalação C real do console com o instalador PC e sabota cópias.

Sem console/emulador. Trabalho novo fora do git, original somente leitura.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import resource
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


def validar_fluxo(texto, sha=None):
    # Cada byte da original lido uma vez na passagem; só os índices do inventário
    # (menos de 1 MiB) são lidos de novo, para conferir o cache com o lote inteiro.
    m = re.search(r"origem=(\d+) escrita=(\d+) relidos=(\d+) duplicados=(\d+)", texto)
    exigir(m and int(m[2]) == pc.TAMANHO and int(m[1]) - int(m[4]) == pc.TAMANHO and int(m[4]) < 1024 * 1024,
           "E/S repetiu bytes da origem além dos índices ou não escreveu exatamente uma ISO")
    exigir(re.search(rf"conversão: \d+ lotes alinhados a 2048 bytes", texto), "lotes ausentes do registro")
    # Releitura integral: a ISO inteira mais índices e selo, nada a menos.
    m = re.search(r"relidos=(\d+) duplicados=", texto)
    exigir(m and pc.TAMANHO <= int(m[1]) < pc.TAMANHO + 2 * 1024 * 1024, "releitura não cobriu a ISO inteira")
    for etapa in ("leitura da original", "gravação", "releitura"):
        exigir(re.search(rf"conversão: {etapa} {pc.TAMANHO} bytes em \d+ ms, \d+\.\d MB/s", texto),
               f"{etapa}: bytes ou velocidade ausentes do registro")
    for etapa in ("identificar", "converter", "verificar", "publicar"):
        exigir(re.search(rf"conversão: etapa {etapa} em \d+ ms", texto), f"tempo da etapa {etapa} ausente")
    exigir("progresso: 101 valores distintos, de 0 a 100, fase final 4, retrocessos=0" in texto,
           "porcentagem não foi única e monotônica de 0 a 100")
    # Categorias da tela: as contas fecham no total do inventário e cada entrada convertida bate uma vez.
    m = re.search(r"categorias: feitos (\d+) de (\d+)", texto)
    b = re.search(r"batidas=(\d+)", texto)
    exigir(m and b and int(m[1]) == int(m[2]) == int(b[1]) == 3366,
           "categorias não fecham em 3366 entradas ou as batidas não são uma por entrada")
    for nome in re.findall(r"(\w+) (\d+)/(\d+)", re.search(r"categorias: personagens.*", texto)[0]):
        exigir(nome[1] == nome[2] and int(nome[2]) > 0, f"categoria {nome[0]} não terminou ou está vazia")
    sobrepostas = re.search(r"pedidos sobrepostos=(\d+)", texto)
    exigir(sobrepostas and int(sobrepostas[1]) > 0, "nenhuma E/S sobreposta foi pedida")
    gravacoes = re.search(r"gravações=(\d+)", texto)
    exigir(gravacoes and 360 < int(gravacoes[1]) < 1000, "escritas pequenas não foram agrupadas")
    if sha:
        exigir(f"SHA256 saída {sha}" in texto, "hash em fluxo diferente da saída reaberta")


def compilar(pasta, adaptador=None, nucleo=None):
    raiz = pc.RAIZ
    preparar(pasta / "selo")
    exe = pasta / "teste_instalar"
    fontes = sorted((raiz / "port/x360/conv").glob("*.c"))
    subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu89", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wdeclaration-after-statement", "-pthread", "-Wno-unused-function", "-Wno-stringop-truncation",
                    "-I" + str(raiz / "port/x360/src/teste_instalar"),
                    "-I" + str(raiz / "port/x360/src"), "-I" + str(raiz / "port/x360/conv"),
                    "-I" + str(pasta / "selo"), str(adaptador or raiz / "port/x360/src/iso_instalar.c"),
                    str(raiz / "port/x360/src/teste_instalar/teste_instalar.c"),
                    *(str(nucleo if nucleo and f.name == "iso_conv.c" else f) for f in fontes
                      if f.name not in ("conv_tool.c", "conv_jogo.c")),
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
    r = subprocess.run([str(exe), str(origem)], env=env, capture_output=True, text=True, timeout=600)
    print(r.stdout, end="", flush=True)
    exigir(r.returncode == esperado, f"esperado {esperado}, obtido {r.returncode}: {r.stderr}")
    if esperado == 0:
        validar_fluxo(r.stdout)
    return r


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("origem", type=Path)
    p.add_argument("--dados", type=Path, default=Path("/srv/bt3/dados/gamedata"))
    p.add_argument("--trabalho", type=Path, required=True)
    args = p.parse_args()
    d = args.trabalho.resolve()
    d.mkdir()  # Nunca reutiliza nem apaga rodada alheia.
    (d / "commit.txt").write_text(subprocess.check_output(["git", "rev-parse", "HEAD"], text=True))
    (d / "diff.patch").write_bytes(subprocess.check_output(["git", "diff", "HEAD"]))
    exe = compilar(d)
    # Guardas das threads e dos buffers vistas falhando em cópias compiladas.
    fonte = pc.RAIZ / "port/x360/src/iso_instalar.c"
    texto = fonte.read_text()
    for nome, de, para, acusacao in (
        ("afinidade", "escrita ? 1 : 3", "escrita ? 4 : 3", "hw == 1 || hw == 3"),
        ("alinhamento", "return _aligned_malloc(n, 128);",
         "return (conv_u8 *)_aligned_malloc(n + 1, 128) + 1;", "((uintptr_t)p & 127) == 0"),
    ):
        mutante = d / nome
        mutante.mkdir()
        exigir(de in texto, f"mutação de {nome} não encontrou alvo")
        adulterado = mutante / "iso_instalar.c"
        adulterado.write_text(texto.replace(de, para))
        ruim = compilar(mutante, adulterado)
        env = dict(os.environ, TESTE_GAME=str(d))
        env.pop("TESTE_FALHA", None)
        env.pop("TESTE_CORROMPER", None)
        r = subprocess.run([str(ruim), str(args.origem)], env=env, capture_output=True, text=True, timeout=30)
        exigir(r.returncode != 0 and acusacao in r.stderr, f"{nome} proibido não foi acusado: {r.stderr}")
        # O mutante do buffer já criou o temporário antes da primeira escrita.
        (d / "bt3-x360.tmp").unlink(missing_ok=True)
        print(f"ok: mutante de {nome} recusado em execução", flush=True)
    # Lotes fora do setor (o plano da primeira passagem no console, 29/10): o
    # contrato do DMA na leitora simulada precisa recusar o núcleo sem a regra.
    nucleo = (pc.RAIZ / "port/x360/conv/iso_conv.c").read_text()
    regras = (("if (atual->tam >= BLOCO && (atual->pos + atual->tam) % SETOR == 0)", "if (atual->tam >= BLOCO)"),
              ("if (l[i].pos % SETOR || l[i].tam % SETOR || l[i].tam > MAX_ARQUIVO)", "if (l[i].tam > MAX_ARQUIVO)"))
    for de_, para_ in regras:
        exigir(de_ in nucleo, "mutação do setor não encontrou alvo")
        nucleo = nucleo.replace(de_, para_)
    mutante = d / "setor"
    mutante.mkdir()
    (mutante / "iso_conv.c").write_text(nucleo)
    ruim = compilar(mutante, nucleo=mutante / "iso_conv.c")
    env = dict(os.environ, TESTE_GAME=str(d))
    env.pop("TESTE_FALHA", None)
    env.pop("TESTE_CORROMPER", None)
    r = subprocess.run([str(ruim), str(args.origem)], env=env, capture_output=True, text=True, timeout=600)
    (d / "bt3-x360.tmp").unlink(missing_ok=True)
    exigir(r.returncode != 0 and "atual % 2048 == 0" in r.stderr, f"lote fora do setor não foi acusado: {r.stderr}")
    print("ok: mutante com lotes fora do setor recusado em execução", flush=True)
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
    exigir(not (d / "bt3-x360.tmp").exists(), "temporário não foi descartado")
    invalida.unlink()
    print("ok: índices estragados recusados antes de escrever", flush=True)
    antes = hashes(args.origem)
    exigir(antes == (pc.MD5, pc.SHA256), "a ISO não é a original do Lucas")
    convertido = d / "bt3-x360.iso"
    for falha, codigo in (("espaco",2),("leitura",1),("escrever",3),("sincronizar",3),
                           ("alocar",3),("evento1",3),("evento2",3),("evento3",3),("evento4",3),
                           ("thread1",3),("thread2",3),("origem",1),
                           ("fechar",3),("reabrir",3),("truncar",5),("publicar",6)):
        rodar(exe, args.origem, d, codigo, falha)
        exigir(not (d / "bt3-x360.tmp").exists(), f"temporário sobrou: {falha}")
        if falha == "publicar":
            exigir(convertido.read_bytes() == b"preservado", "destino concorrente alterado")
            convertido.unlink()
        else:
            exigir(not convertido.exists(), f"publicou falha: {falha}")
        print(f"ok: sabotagem {falha}", flush=True)
    # Porcentagem de etapa que volta a zero na releitura (o comportamento antigo)
    # num núcleo mutante: converte certo, mas o verificador do registro recusa.
    nucleo = (pc.RAIZ / "port/x360/conv/iso_conv.c").read_text()
    de = "progresso(io, IC_VERIFICAR, IsoContas_Verificacao(&contas, iso, pos >> 10), total);"
    exigir(de in nucleo, "mutação do progresso não encontrou alvo")
    mutante = d / "progresso"
    mutante.mkdir()
    (mutante / "iso_conv.c").write_text(nucleo.replace(
        de, "progresso(io, IC_VERIFICAR, (conv_u32)((double)pos * total / IC_TAMANHO), total);"))
    ruim = compilar(mutante, nucleo=mutante / "iso_conv.c")
    env = dict(os.environ, TESTE_GAME=str(d))
    env.pop("TESTE_FALHA", None)
    env.pop("TESTE_CORROMPER", None)
    r = subprocess.run([str(ruim), str(args.origem)], env=env, capture_output=True, text=True, timeout=600)
    convertido.unlink(missing_ok=True)
    exigir(r.returncode == 0, f"mutante do progresso não converteu: {r.stderr}")
    try:
        validar_fluxo(r.stdout)
    except ValueError as e:
        exigir("porcentagem" in str(e), f"mutante do progresso recusado por outro motivo: {e}")
    else:
        raise ValueError("verificador aceitou porcentagem que volta a zero")
    print("ok: mutante do progresso por etapa recusado", flush=True)
    # Byte convertido, preservado (inclusive no meio da cópia, que a amostragem
    # antiga não relia), índices ISO/AFS, folga/UDF, selo e cauda.
    with args.origem.open("rb") as f:
        entradas = pc.inventario(f)
        f.seek(0)
        prefixo = f.read(1024 * 1024)
        registro = prefixo.index(b"PZS3US2.AFS;1") - 33
        exigir(prefixo[registro + 32] == len(b"PZS3US2.AFS;1"), "registro AFS2 inválido")
        afs2 = int.from_bytes(prefixo[registro + 2:registro + 6], "little") * 2048
        exigir(pc.ler(f, afs2, 4) == b"AFS\0", "base do AFS2 inválida")
    for nome, pos in (("convertido",entradas["pzs3us1/00004.bin"][0]),
                      ("preservado",entradas["disc/SYSTEM.CNF"][0]), ("índice ISO",16*2048+158),
                      ("índice AFS",0x3df008), ("meio do índice AFS2",afs2+8+40000*8), ("folga AFS",0x3df018),
                      ("área UDF",256*2048), ("selo",pc.MARCA_POS+16), ("cauda",pc.TAMANHO-1),
                      ("preservado no meio da cópia", 2 * 1024 * 1024), ("meio da ISO", pc.TAMANHO // 2)):
        rodar(exe, args.origem, d, 5, corromper=pos)
        exigir(not convertido.exists() and not (d / "bt3-x360.tmp").exists(), "falha publicada")
        print(f"ok: byte sabotado em {nome}", flush=True)
    # Interrupção real de processo. A próxima abertura descarta o parcial e recomeça.
    rodar(exe,args.origem,d,77,"parar")
    exigir((d / "bt3-x360.tmp").stat().st_size > 0 and not convertido.exists(), "interrupção não deixou só parcial")
    sucesso = rodar(exe,args.origem,d)
    exigir(not (d / "bt3-x360.tmp").exists(), "parcial não descartado")
    # Destino já existente preservado, inclusive se ele for a própria origem.
    h = hashes(convertido)
    rodar(exe,args.origem,d,6)
    exigir(hashes(convertido) == h, "destino existente mudou")
    rodar(exe,convertido,d,6)
    print("ok: retomada do zero, saída existente e reconversão recusada", flush=True)
    validar_fluxo(sucesso.stdout, h[1])
    # O verificador das métricas precisa rejeitar cópias de um registro real.
    origem = re.search(r"origem=(\d+)", sucesso.stdout)
    duplicados = re.search(r"duplicados=(\d+)", sucesso.stdout)
    for de, para in ((origem[0], f"origem={int(origem[1]) + 1}"),
                     (duplicados[0], f"duplicados={int(duplicados[1]) + 1}"),
                     (re.search(r"gravações=\d+", sucesso.stdout)[0], "gravações=10000"),
                     (re.search(r"relidos=\d+", sucesso.stdout)[0], f"relidos={pc.TAMANHO - 1}"),
                     (re.search(r"relidos=\d+", sucesso.stdout)[0], "relidos=1451394452"),
                     (f"releitura {pc.TAMANHO} bytes", "releitura 1451394452 bytes"),
                     (re.search(r"gravação \d+ bytes em \d+ ms, [0-9.]+ MB/s", sucesso.stdout)[0], "gravação"),
                     ("conversão: etapa verificar em", "conversão: em"),
                     ("retrocessos=0", "retrocessos=1"), ("de 0 a 100", "de 0 a 99"),
                     (h[1], "0" * 64)):
        exigir(de in sucesso.stdout, f"mutação de log não encontrou {de}")
        try:
            validar_fluxo(sucesso.stdout.replace(de, para), h[1])
        except ValueError:
            print(f"ok: verificador visto recusando registro adulterado: {de}", flush=True)
        else:
            raise ValueError("verificador aceitou registro adulterado")
    try:
        validar_fluxo(re.sub(r"pedidos sobrepostos=\d+", "pedidos sobrepostos=0", sucesso.stdout), h[1])
    except ValueError:
        print("ok: verificador visto recusando ausência de sobreposição", flush=True)
    else:
        raise ValueError("verificador aceitou ausência de sobreposição")
    # A conferência Python cobre cada byte contra a conversão comum e a origem.
    # Só uma ISO em disco.
    comando = [sys.executable, str(pc.RAIZ / "port/x360/tools/convert_iso.py"), str(args.origem),
               str(convertido), "--conv-tool", str(d / "selo/conv_tool")]
    conferir = [*comando, "--conferir", "--dados", str(args.dados)]
    subprocess.run(conferir, check=True)
    # Verificador integral visto falhando num byte preservado no meio da cópia.
    pos = 2 * 1024 * 1024
    with convertido.open("r+b") as f:
        f.seek(pos); b = f.read(1); f.seek(pos); f.write(bytes([b[0] ^ 1]))
    r = subprocess.run(conferir, capture_output=True, text=True)
    exigir(r.returncode == 1 and "bytes differ" in r.stderr,
           "conferência Python integral aceitou byte estragado")
    with convertido.open("r+b") as f:
        f.seek(pos); f.write(b)
    print("ok: conferência integral Python vista falhando no meio da cópia", flush=True)
    convertido.unlink()
    # Produção real Python depois de remover a C. A igualdade integral acima já
    # comparou todos os bytes C à receita Python; os dois hashes também devem bater.
    subprocess.run(comando, check=True)
    exigir(hashes(convertido) == h, "hashes diferentes entre C e Python")
    # Reconversão sem destino existente: a marca antiga na origem é recusada.
    outra = d / "original.iso"
    convertido.rename(outra)
    try:
        rodar(exe, outra, d, 1)
        exigir(not convertido.exists() and not (d / "bt3-x360.tmp").exists(), "reconversão publicada")
    finally:
        outra.rename(convertido)
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
