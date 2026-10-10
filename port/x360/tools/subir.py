#!/usr/bin/env python3
"""Envia arquivos ao console pelo FTP da FreeStyle e confere o md5 de volta.

Uso: subir.py [--ip IP] [--destino /Hdd1/HOMEBREW/bt3xbx] ARQUIVO...
     subir.py [--ip IP] --baixar NOME [--destino ...]       (ex.: o log bt3xbx.log)
     subir.py --achar                                       (só procura o console)

Regras do servidor da FreeStyle (hiddriver/docs/ftp-no-xbox.md): só responde com o console parado
na FreeStyle; uma conexão por vez; não responde ao QUIT (fechar com close()); sem REST; RETR só
depois de CWD; ler arquivo sendo gravado trava o servidor. Por isso: envia com nome temporário,
apaga o antigo, renomeia, e baixa de volta para conferir.
"""
import argparse, concurrent.futures, ftplib, hashlib, io, os, socket, subprocess, sys, time

USUARIO, SENHA = "xbox", "xbox"
DESTINO = "/Hdd1/HOMEBREW/bt3xbx"


def porta_aberta(ip, porta=21, espera=2.0):
    try:
        with socket.create_connection((ip, porta), timeout=espera):
            return True
    except OSError:
        return False


def redes_locais():
    """Prefixos /24 das interfaces IPv4 da máquina (ex.: 192.168.0)."""
    saida = subprocess.run(["ip", "-4", "-o", "addr"], capture_output=True, text=True).stdout
    prefixos = []
    for linha in saida.splitlines():
        partes = linha.split()
        ip = partes[3].split("/")[0]
        if partes[1].startswith(("docker", "br-", "veth", "lo")):
            continue  # pontes do docker e loopback
        prefixos.append(ip.rsplit(".", 1)[0])
    return prefixos


def avisar(texto):
    print(texto, file=sys.stderr)  # stdout fica só para o arquivo baixado


def mac_microsoft(ip):
    """O console tem placa de rede da Microsoft (MAC 4c:0b:be...)."""
    vizinhos = subprocess.run(["ip", "neigh"], capture_output=True, text=True).stdout
    return any(l.split()[0] == ip and "4c:0b:be" in l.lower() for l in vizinhos.splitlines() if l.split())


def achar():
    if os.environ.get("XBOX_IP"):
        return os.environ["XBOX_IP"]
    # Atalho: quem já está na tabela de vizinhos com MAC da Microsoft. A primeira conexão ao
    # console demora; uma varredura com espera curta não o vê.
    vizinhos = subprocess.run(["ip", "neigh"], capture_output=True, text=True).stdout
    for l in vizinhos.splitlines():
        if "4c:0b:be" in l.lower() and porta_aberta(l.split()[0], espera=5):
            return l.split()[0]
    for prefixo in redes_locais():
        candidatos = [f"{prefixo}.{i}" for i in range(1, 255)]
        with concurrent.futures.ThreadPoolExecutor(64) as ex:
            abertos = [ip for ip, ok in zip(candidatos, ex.map(porta_aberta, candidatos)) if ok]
        # Primeiro pelo MAC, sem abrir sessão FTP: o servidor atende uma conexão por vez e a
        # sondagem de porta acima pode deixar a próxima saudação atrasada.
        for ip in abertos:
            if mac_microsoft(ip):
                return ip
        for ip in abertos:
            for tentativa in range(3):
                try:
                    f = ftplib.FTP(ip, timeout=10)
                    boas_vindas = f.getwelcome()
                    f.close()
                except Exception:
                    time.sleep(3)
                    continue
                if "F3" in boas_vindas or "FTPD" in boas_vindas.upper() or "xbox" in boas_vindas.lower():
                    return ip
                avisar(f"porta 21 aberta em {ip}, mas não parece o console: {boas_vindas!r}")
                break
    return None


def conectar(ip):
    for tentativa in range(4):
        try:
            f = ftplib.FTP(ip, timeout=20)
            f.login(USUARIO, SENHA)
            return f
        except Exception as e:
            avisar(f"conexão falhou ({e}); tentando de novo em 10 s")
            time.sleep(10)
    sys.exit("o console não aceitou a conexão. Ele está parado na FreeStyle com o FTP ligado?")


def garantir_pasta(f, destino):
    f.cwd("/")
    for parte in destino.strip("/").split("/"):
        try:
            f.cwd(parte)
        except ftplib.error_perm:
            f.mkd(parte)
            f.cwd(parte)


def subir(f, caminho):
    nome = os.path.basename(caminho)
    dados = open(caminho, "rb").read()
    md5 = hashlib.md5(dados).hexdigest()
    # nome único: um envio abortado tranca o temporário até a sessão cair
    temporario = f"{nome}.{os.getpid()}.{int(time.time())}.tmp"
    f.storbinary(f"STOR {temporario}", io.BytesIO(dados))
    try:
        f.sendcmd(f"DELE {nome}")
    except ftplib.error_perm:
        pass  # não existia
    f.sendcmd(f"RNFR {temporario}")
    f.sendcmd(f"RNTO {nome}")
    volta = io.BytesIO()
    f.retrbinary(f"RETR {nome}", volta.write)
    if hashlib.md5(volta.getvalue()).hexdigest() != md5:
        sys.exit(f"ERRO: {nome} chegou diferente no console")
    avisar(f"ok  {nome}  {len(dados)} bytes  md5 {md5}")


def existentes(f):
    """Nomes da pasta atual, em minúsculas (o FATX não diferencia caixa). Pelo LIST, que o
    servidor da FreeStyle atende."""
    linhas = []
    f.retrlines("LIST", linhas.append)
    return {l.split(None, 8)[-1].lower() for l in linhas if l.strip()}


def baixar(f, nome):
    # renomeia antes: ler arquivo que alguém está gravando trava o servidor
    copia = nome + ".lido"
    nomes = existentes(f)
    if nome.lower() in nomes:  # só apaga a cópia anterior se houver um novo para pôr no lugar
        if copia.lower() in nomes:
            f.sendcmd(f"DELE {copia}")
        f.sendcmd(f"RNFR {nome}")
        f.sendcmd(f"RNTO {copia}")
    elif copia.lower() in nomes:
        avisar(f"{nome} não existe; lendo a cópia anterior {copia}")
    else:
        sys.exit(f"{nome} não existe no console")
    buf = io.BytesIO()
    f.retrbinary(f"RETR {copia}", buf.write)
    return buf.getvalue()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip")
    ap.add_argument("--destino", default=DESTINO)
    ap.add_argument("--achar", action="store_true")
    ap.add_argument("--baixar")
    ap.add_argument("--listar", action="store_true")
    ap.add_argument("arquivos", nargs="*")
    a = ap.parse_args()

    ip = a.ip or achar()
    if not ip:
        sys.exit("console não encontrado na rede (porta 21). Ligue o FTP na FreeStyle ou passe --ip.")
    avisar(f"console em {ip}")
    if a.achar:
        return
    f = conectar(ip)
    try:
        garantir_pasta(f, a.destino)
        if a.listar:
            f.retrlines("LIST")
        if a.baixar:
            sys.stdout.buffer.write(baixar(f, a.baixar))
        for caminho in a.arquivos:
            subir(f, caminho)
    finally:
        f.close()  # nunca quit(): o servidor não responde ao QUIT


main()
