#!/usr/bin/env python3
"""Auxiliar de FTP para sessões de console com várias aberturas em série (gravorig2 e as que vierem).

Subcomandos:
  lista [CAMINHO]                           lista a pasta (padrão: a pasta do jogo) pelo LIST
  poe ARQUIVO_LOCAL [--como NOME]           envia à pasta do jogo (temporário, DELE, RNFR/RNTO, md5 de volta)
  coleta NOME --destino PASTA [--capturas PREFIXO_OU_PASTA]
                                            baixa bt3xbx.log e output.txt como NOME_bt3xbx.log e NOME_output.txt;
                                            com --capturas, as pastas de captura para PASTA/capturas/<pasta>
  proxima ATUAL PROXIMO --pasta RODADA      coleta ATUAL em RODADA/logs, põe RODADA/environment_PROXIMO.txt como
                                            environment.txt e confere o log baixado (só avisa; quem julga é você)
  confere REMOTO LOCAL                      compara o md5 de um arquivo da pasta do jogo com o local
  --autoteste                               roda teste_abertura_console.py (servidor FTP local, falhas de propósito)

O IP do console não fica no arquivo: vem de --ip ou da chave BT3_CONSOLE_IP de ~/.config/bt3xbx/maquinas.env
(modelo em port/x360/maquinas.env.example; variável de ambiente BT3_CONSOLE_IP também vale).

Regras do FTP da FreeStyle (CLAUDE.md, guia de port 1.3): xbox/xbox, modo passivo, uma conexão por vez, nunca QUIT
(fecha com close()), nunca REST, RETR só depois de CWD na pasta do arquivo (nunca com caminho). O servidor perde
conexões de dados: todo LIST e toda transferência recomeçam do zero (conexão nova, CWD de novo), com limite de
tentativas e pausa curta, e o número de tentativas usado é dito na saída.
Desenho do envio igual ao de subir.py (nome temporário, DELE, RNFR/RNTO, md5 conferido baixando).
"""
import argparse, ftplib, hashlib, io, os, re, socket, subprocess, sys, time

USUARIO, SENHA = "xbox", "xbox"
PASTA_JOGO = "/Hdd1/HOMEBREW/bt3xbx"  # a mesma de subir.py e do guia de port (1.3)
TENTATIVAS = 5
PAUSA = 2.0
ARQUIVOS_COLETA = ("bt3xbx.log", "output.txt")


def avisar(texto):
    print(texto, file=sys.stderr)


def ler_maquinas_env():
    valores = {}
    for caminho in (os.path.expanduser("~/.config/bt3xbx/maquinas.env"),):
        try:
            with open(caminho, encoding="utf-8") as f:
                for linha in f:
                    linha = linha.strip()
                    if linha and not linha.startswith("#") and "=" in linha:
                        k, v = linha.split("=", 1)
                        valores[k.strip()] = v.split("#", 1)[0].strip().strip('"\'')
        except OSError:
            pass
    return valores


class Sessao:
    """Parâmetros de conexão e a repetição de tentativas. Cada tentativa abre uma conexão nova."""

    def __init__(self, ip, porta=21, pasta=PASTA_JOGO, tentativas=TENTATIVAS, pausa=PAUSA):
        self.ip, self.porta, self.pasta = ip, porta, pasta
        self.tentativas, self.pausa = tentativas, pausa
        self.usadas = []  # tentativas gastas por operação, para o relatório

    def conecta(self, pasta=None):
        f = ftplib.FTP()
        f.connect(self.ip, self.porta, timeout=20)
        f.login(USUARIO, SENHA)
        f.set_pasv(True)
        f.sock.settimeout(180)  # a FreeStyle para por dezenas de segundos no meio de arquivo grande
        f.timeout = 180
        f.cwd(pasta or self.pasta)
        return f

    def tenta(self, descricao, operacao, pasta=None):
        """operacao(f) -> valor. Em falha de rede ou 4xx, fecha, espera e refaz do zero. 5xx não se repete."""
        ultimo = None
        for n in range(1, self.tentativas + 1):
            f = None
            try:
                f = self.conecta(pasta)
                valor = operacao(f)
                self.usadas.append((descricao, n))
                avisar(f"{descricao}: ok em {n} tentativa(s)")
                return valor
            except ftplib.error_perm:
                raise
            except (ftplib.Error, OSError, EOFError) as e:
                ultimo = e
                avisar(f"{descricao}: tentativa {n}/{self.tentativas} falhou ({type(e).__name__}: {e})")
                time.sleep(self.pausa)
            finally:
                if f is not None:
                    try:
                        f.close()  # nunca quit()
                    except Exception:
                        pass
        raise SystemExit(f"ERRO: {descricao}: {self.tentativas} tentativas sem sucesso (último erro: {ultimo})")


def entradas(f):
    """[(nome, é_pasta)] do LIST da pasta atual. O LIST da FreeStyle traz '.' e '..' sem o 'd': descartados."""
    linhas = []
    f.retrlines("LIST", linhas.append)
    saida = []
    for l in linhas:
        if not l.strip():
            continue
        nome = l.split(None, 8)[-1] if len(l.split(None, 8)) >= 9 else l.split()[-1]
        if nome in (".", ".."):
            continue
        saida.append((nome, l.startswith("d")))
    return saida


def baixa_bytes(f, nome):
    """RETR só com o nome simples, na pasta atual (o CWD já foi feito)."""
    buf = io.BytesIO()
    f.retrbinary(f"RETR {nome}", buf.write, blocksize=1 << 16)
    return buf.getvalue()


def md5(b):
    return hashlib.md5(b).hexdigest()


# ---------------------------------------------------------------- subcomandos

def cmd_lista(s, caminho=None):
    def op(f):
        return entradas(f)
    itens = s.tenta("lista", op, pasta=caminho)
    for nome, pasta in sorted(itens, key=lambda x: x[0].lower()):
        print(nome + ("/" if pasta else ""))
    return 0


def poe(s, local, como=None):
    nome = como or os.path.basename(local)
    if len(nome) > 42:
        raise SystemExit(f"ERRO: {nome}: o FATX aceita nome de até 42 caracteres")
    # Em blocos, sem o arquivo inteiro na memória: as partes da imagem do BT4 chegam a 4 GB, e o WSL tem uns 7 GB.
    h = hashlib.md5()
    tamanho = 0
    with open(local, "rb") as a:
        for bloco in iter(lambda: a.read(1 << 20), b""):
            h.update(bloco)
            tamanho += len(bloco)
    esperado = h.hexdigest()

    def op(f):
        # nome único por tentativa: um envio abortado tranca o temporário até a sessão cair
        temporario = f"{nome[:15]}.{os.getpid() % 100000}.{int(time.time() * 1000) % 10**9}.tmp"
        t0 = time.time()
        with open(local, "rb") as a:
            f.storbinary(f"STOR {temporario}", a, blocksize=1 << 16)
        existentes = {n.lower() for n, _ in entradas(f)}
        if nome.lower() in existentes:
            f.sendcmd(f"DELE {nome}")
        f.sendcmd(f"RNFR {temporario}")
        f.sendcmd(f"RNTO {nome}")
        t1 = time.time()
        volta = hashlib.md5()
        f.retrbinary(f"RETR {nome}", volta.update, blocksize=1 << 16)
        if tamanho > 1 << 26:
            avisar(f"{nome}: envio {tamanho / max(t1 - t0, 1e-3) / 1e6:.1f} MB/s, releitura "
                   f"{tamanho / max(time.time() - t1, 1e-3) / 1e6:.1f} MB/s")
        return volta.hexdigest()

    volta = s.tenta(f"poe {nome}", op)
    if volta != esperado:
        avisar(f"ERRO: {nome} chegou diferente no console (local {esperado}, de volta {volta})")
        return 1
    avisar(f"ok  {nome}  {tamanho} bytes  md5 {esperado}")
    return 0


def cmd_poe(s, local, como=None):
    return poe(s, local, como)


def coleta(s, nome, destino, capturas=None, sobrescreve=False):
    os.makedirs(destino, exist_ok=True)
    alvos = {r: os.path.join(destino, f"{nome}_{r}") for r in ARQUIVOS_COLETA}
    if not sobrescreve:
        ja = [p for p in alvos.values() if os.path.exists(p)]
        if ja:
            raise SystemExit("ERRO: já existe (use --sobrescreve): " + ", ".join(ja))
    baixados = {}
    for remoto, local in alvos.items():
        baixados[remoto] = s.tenta(f"coleta {remoto}", lambda f, r=remoto: baixa_bytes(f, r))
        with open(local, "wb") as a:
            a.write(baixados[remoto])
        avisar(f"ok  {local}  {len(baixados[remoto])} bytes  md5 {md5(baixados[remoto])}")
    if capturas:
        itens = s.tenta("coleta lista de capturas", entradas)
        pastas = sorted(n for n, p in itens if p and (n == capturas or n.startswith(capturas)))
        if not pastas:
            avisar(f"AVISO: nenhuma pasta de captura '{capturas}' na pasta do jogo")
        for pasta in pastas:
            base = os.path.join(destino, "capturas", pasta)
            if os.path.isdir(base) and os.listdir(base) and not sobrescreve:
                raise SystemExit(f"ERRO: já existe (use --sobrescreve): {base}")
            os.makedirs(base, exist_ok=True)

            def op(f, pasta=pasta):
                f.cwd(pasta)  # RETR sempre com o nome simples depois do CWD
                return {n: baixa_bytes(f, n) for n, p in entradas(f) if not p}

            arquivos = s.tenta(f"coleta capturas {pasta}", op)
            for n, b in arquivos.items():
                with open(os.path.join(base, n), "wb") as a:
                    a.write(b)
            avisar(f"ok  {len(arquivos)} arquivos de {pasta} em {base}")
    return baixados


def cmd_coleta(s, a):
    coleta(s, a.nome, a.destino, a.capturas, a.sobrescreve)
    return 0


def confere_log(texto, atual):
    """Devolve [(rotulo, ok, detalhe)]. Só informa; quem julga é o chamador."""
    linhas = texto.splitlines()
    res = []
    amb = [l for l in linhas if re.search(r"ambiente: BT3_SAVES=", l)]
    esperado = "sv_g2" + atual
    if not amb:
        res.append(("BT3_SAVES", False, "sem linha 'ambiente: BT3_SAVES='"))
    else:
        valor = amb[0].split("BT3_SAVES=", 1)[1].strip()
        res.append(("BT3_SAVES", valor.endswith(esperado), f"{valor} (esperado terminar em {esperado})"))
    ult = atual[-1:]
    if ult in ("c", "o"):
        og = [l for l in linhas if "ambiente: BT3_GRAVACAO_ORIGEM=" in l]
        quer = "0" if ult == "c" else "1"
        if not og:
            res.append(("BT3_GRAVACAO_ORIGEM", False, "sem a linha 'ambiente: BT3_GRAVACAO_ORIGEM='"))
        else:
            v = og[0].split("BT3_GRAVACAO_ORIGEM=", 1)[1].strip()
            res.append(("BT3_GRAVACAO_ORIGEM", v == quer, f"{v} (esperado {quer} para '{ult}')"))
    fim = [l for l in linhas if re.search(r"roteiro:.*\bfim\b", l)]
    res.append(("roteiro fim", bool(fim), fim[0].strip() if fim else "nenhuma linha 'roteiro: ... fim'"))
    falha = [l for l in linhas if "roteiro: FALHA" in l]
    res.append(("sem roteiro: FALHA", not falha, f"{len(falha)} linha(s)" + (f", a primeira: {falha[0].strip()}" if falha else "")))
    erro = [l for l in linhas if "ERRO" in l]
    res.append(("sem ERRO", not erro, f"{len(erro)} linha(s)" + (f", a primeira: {erro[0].strip()}" if erro else "")))
    ultima = next((l for l in reversed(linhas) if l.strip()), "")
    res.append(("termina em 'gs: VU1 direto'", "gs: VU1 direto" in ultima, f"última linha: {ultima.strip()[:100]}"))
    return res


def cmd_proxima(s, a):
    env = os.path.join(a.pasta, f"environment_{a.proximo}.txt")
    if not os.path.isfile(env):
        raise SystemExit(f"ERRO: não existe {env}")
    logs = os.path.join(a.pasta, "logs")
    baixados = coleta(s, a.atual, logs, a.capturas, a.sobrescreve)
    texto = baixados["bt3xbx.log"].decode("utf-8", "replace")
    avisos = 0
    print(f"checagem do log de {a.atual} (só avisa):")
    for rotulo, ok, detalhe in confere_log(texto, a.atual):
        print(f"  {'ok   ' if ok else 'AVISO'} {rotulo}: {detalhe}")
        avisos += (not ok)
    rc = poe(s, env, "environment.txt")
    print(f"{avisos} aviso(s) na checagem; ambiente {a.proximo} {'posto' if rc == 0 else 'NÃO posto (md5)'}")
    return rc


def cmd_confere(s, remoto, local):
    if "/" in remoto:
        raise SystemExit("ERRO: ARQUIVO_REMOTO é um nome da pasta do jogo, sem caminho (RETR só depois de CWD)")
    dados = s.tenta(f"confere {remoto}", lambda f: baixa_bytes(f, remoto))
    with open(local, "rb") as a:
        l = md5(a.read())
    r = md5(dados)
    print(f"remoto {r}\nlocal  {l}\n{'IGUAIS' if r == l else 'DIFERENTES'}")
    return 0 if r == l else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--autoteste", action="store_true")
    ap.add_argument("--ip")
    ap.add_argument("--porta", type=int, default=21)
    ap.add_argument("--pasta-jogo", default=PASTA_JOGO)
    ap.add_argument("--tentativas", type=int, default=TENTATIVAS)
    ap.add_argument("--pausa", type=float, default=PAUSA)
    sub = ap.add_subparsers(dest="cmd")
    p = sub.add_parser("lista"); p.add_argument("caminho", nargs="?")
    p = sub.add_parser("poe"); p.add_argument("arquivo"); p.add_argument("--como")
    p = sub.add_parser("coleta"); p.add_argument("nome"); p.add_argument("--destino", required=True)
    p.add_argument("--capturas"); p.add_argument("--sobrescreve", action="store_true")
    p = sub.add_parser("proxima"); p.add_argument("atual"); p.add_argument("proximo")
    p.add_argument("--pasta", required=True); p.add_argument("--capturas"); p.add_argument("--sobrescreve", action="store_true")
    p = sub.add_parser("confere"); p.add_argument("remoto"); p.add_argument("local")
    a = ap.parse_args(argv)
    if a.autoteste:
        aqui = os.path.dirname(os.path.abspath(__file__))
        return subprocess.call([sys.executable, os.path.join(aqui, "teste_abertura_console.py")])
    if not a.cmd:
        ap.error("diga um subcomando (lista, poe, coleta, proxima, confere) ou --autoteste")
    ip = a.ip or os.environ.get("BT3_CONSOLE_IP") or ler_maquinas_env().get("BT3_CONSOLE_IP")
    if not ip:
        raise SystemExit("ERRO: sem IP do console: passe --ip ou ponha BT3_CONSOLE_IP em ~/.config/bt3xbx/maquinas.env")
    s = Sessao(ip, a.porta, a.pasta_jogo, a.tentativas, a.pausa)
    if a.cmd == "lista": return cmd_lista(s, a.caminho)
    if a.cmd == "poe": return cmd_poe(s, a.arquivo, a.como)
    if a.cmd == "coleta": return cmd_coleta(s, a)
    if a.cmd == "proxima": return cmd_proxima(s, a)
    if a.cmd == "confere": return cmd_confere(s, a.remoto, a.local)


if __name__ == "__main__":
    sys.exit(main())
