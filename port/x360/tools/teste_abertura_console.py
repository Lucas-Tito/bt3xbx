#!/usr/bin/env python3
"""Prova do abertura_console.py contra um servidor FTP local mínimo (nada do console, nada do jogo).

O servidor imita o que importa da FreeStyle: LIST com '.' e '..' sem o 'd', RETR só com nome simples (caminho = 550),
sem REST (502), e registra todo comando recebido. Defeitos de propósito: 'corrompe' (STOR grava um byte a menos) e
'derruba' (a primeira conexão de dados de cada tipo cai). Falhar aqui sai com 1.
Uso: teste_abertura_console.py
"""
import importlib.util, os, socket, sys, tempfile, threading, shutil, hashlib, io, contextlib

AQUI = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("abertura_console", os.path.join(AQUI, "abertura_console.py"))
ac = importlib.util.module_from_spec(spec); spec.loader.exec_module(ac)

PASTA = "/Hdd1/HOMEBREW/bt3xbx"


class Servidor:
    def __init__(self, raiz, corrompe=False, derruba=False):
        self.raiz, self.corrompe, self.derruba = raiz, corrompe, derruba
        self.comandos, self.ja_derrubou, self.conexoes = [], set(), 0
        self.ativas = 0; self.max_ativas = 0
        self.sock = socket.socket(); self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", 0)); self.sock.listen(5)
        self.porta = self.sock.getsockname()[1]
        self.lock = threading.Lock()
        threading.Thread(target=self.aceita, daemon=True).start()

    def aceita(self):
        while True:
            try:
                c, _ = self.sock.accept()
            except OSError:
                return
            threading.Thread(target=self.atende, args=(c,), daemon=True).start()

    def real(self, cwd, nome):
        p = nome if nome.startswith("/") else cwd.rstrip("/") + "/" + nome
        return os.path.normpath(self.raiz + "/" + p.strip("/"))

    def atende(self, c):
        with self.lock:
            self.conexoes += 1; self.ativas += 1; self.max_ativas = max(self.max_ativas, self.ativas)
        f = c.makefile("rwb", buffering=0)
        def r(t): f.write((t + "\r\n").encode())
        r("220 teste")
        cwd, pasv, rnfr = "/", None, None
        try:
            while True:
                linha = f.readline().decode("utf-8", "replace").rstrip("\r\n")
                if not linha:
                    break
                cmd, _, arg = linha.partition(" ")
                cmd = cmd.upper()
                with self.lock:
                    self.comandos.append((cmd, arg))
                if cmd == "USER": r("331 senha")
                elif cmd == "PASS": r("230 ok")
                elif cmd == "TYPE": r("200 ok")
                elif cmd == "PWD": r(f'257 "{cwd}"')
                elif cmd == "QUIT": r("221 tchau"); break
                elif cmd == "REST": r("502 sem REST")
                elif cmd == "PASV":
                    pasv = socket.socket(); pasv.bind(("127.0.0.1", 0)); pasv.listen(1)
                    p = pasv.getsockname()[1]
                    r(f"227 Entering Passive Mode (127,0,0,1,{p >> 8},{p & 255})")
                elif cmd == "CWD":
                    d = self.real(cwd, arg)
                    if os.path.isdir(d):
                        cwd = "/" + os.path.relpath(d, self.raiz) if d != self.raiz else "/"
                        r("250 ok")
                    else: r("550 sem pasta")
                elif cmd == "MKD": os.makedirs(self.real(cwd, arg), exist_ok=True); r("257 ok")
                elif cmd == "DELE":
                    p = self.real(cwd, arg)
                    if os.path.isfile(p): os.remove(p); r("250 ok")
                    else: r("550 não existe")
                elif cmd == "RNFR":
                    p = self.real(cwd, arg)
                    if os.path.exists(p): rnfr = p; r("350 ok")
                    else: r("550 não existe")
                elif cmd == "RNTO":
                    os.rename(rnfr, self.real(cwd, arg)); r("250 ok")
                elif cmd in ("LIST", "RETR", "STOR"):
                    if pasv is None: r("425 sem PASV"); continue
                    if cmd in ("RETR", "STOR") and "/" in arg:
                        r("550 só nome simples depois de CWD"); pasv.close(); pasv = None; continue
                    alvo = self.real(cwd, arg) if arg else self.real(cwd, ".")
                    if cmd == "RETR" and not os.path.isfile(alvo):
                        r("550 não existe"); pasv.close(); pasv = None; continue
                    r("150 abrindo")
                    d, _ = pasv.accept(); pasv.close(); pasv = None
                    if self.derruba and cmd not in self.ja_derrubou:
                        self.ja_derrubou.add(cmd)
                        d.close(); r("426 conexão de dados perdida"); continue
                    if cmd == "LIST":
                        linhas = ["-rw-r--r-- 1 xbox xbox 0 Jan 1 1980 .",
                                  "-rw-r--r-- 1 xbox xbox 0 Jan 1 1980 .."]
                        for n in sorted(os.listdir(alvo)):
                            p = os.path.join(alvo, n)
                            linhas.append(("d" if os.path.isdir(p) else "-") + f"rw-r--r-- 1 xbox xbox {os.path.getsize(p) if os.path.isfile(p) else 0} Jan 1 1980 {n}")
                        d.sendall(("\r\n".join(linhas) + "\r\n").encode()); d.close()
                    elif cmd == "RETR":
                        d.sendall(open(alvo, "rb").read()); d.close()
                    else:
                        dados = b""
                        while True:
                            b = d.recv(65536)
                            if not b: break
                            dados += b
                        d.close()
                        if self.corrompe: dados = dados[:-1]
                        open(alvo, "wb").write(dados)
                    r("226 ok")
                else: r("502 não sei")
        except OSError:
            pass
        finally:
            with self.lock: self.ativas -= 1
            c.close()

    def fecha(self): self.sock.close()


falhas = []
def confere(cond, texto):
    print(("  ok   " if cond else "  FALHA ") + texto)
    if not cond: falhas.append(texto)


def roda(srv, args, saida=None):
    """Roda main() capturando stdout/stderr; devolve (código, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            rc = ac.main(["--ip", "127.0.0.1", "--porta", str(srv.porta), "--pausa", "0.05"] + args)
        except SystemExit as e:
            rc = e.code if isinstance(e.code, int) else 1
            if isinstance(e.code, str): err.write(e.code + "\n")
    return rc, out.getvalue(), err.getvalue()


def novo(tmp, nome, **kw):
    raiz = os.path.join(tmp, nome)
    os.makedirs(raiz + PASTA)
    return Servidor(raiz, **kw), raiz + PASTA


LOG_BOM = ("versão: build teste\nambiente: BT3_SAVES=game:\\sv_g2cgdu1o\nambiente: BT3_GRAVACAO_ORIGEM=1\n"
           "roteiro: f5_duelo fim\ngs: VU1 direto (BT3_VU1_DIRETO): ok\n")
LOG_RUIM = ("versão: build teste\nambiente: BT3_SAVES=game:\\sv_g2outro\nambiente: BT3_GRAVACAO_ORIGEM=1\n"
            "roteiro: FALHA no quadro 5\nERRO: algo\n")


def main():
    tmp = tempfile.mkdtemp(prefix="abertura_console_")
    try:
        # 1. poe, coleta e proxima numa cópia fictícia
        print("1. poe, coleta e proxima")
        srv, jogo = novo(tmp, "s1")
        os.makedirs(jogo + "/shg2_cgdu1o"); open(jogo + "/shg2_cgdu1o/gpu_00001.ppm", "wb").write(b"P6 falso")
        open(jogo + "/bt3xbx.log", "w").write(LOG_BOM); open(jogo + "/output.txt", "w").write("saida falsa\n")
        open(jogo + "/environment.txt", "w").write("antigo\n")
        rodada = os.path.join(tmp, "rodada"); os.makedirs(rodada)
        open(rodada + "/environment_cgdi1o.txt", "w").write("BT3_SAVES=game:\\sv_g2cgdi1o\n")
        rc, out, err = roda(srv, ["lista"]); confere(rc == 0 and "bt3xbx.log" in out and "shg2_cgdu1o/" in out and "." not in out.split(), "lista sem '.' e '..'")
        rc, out, err = roda(srv, ["poe", rodada + "/environment_cgdi1o.txt", "--como", "environment.txt"])
        confere(rc == 0 and open(jogo + "/environment.txt").read().startswith("BT3_SAVES=game:\\sv_g2cgdi1o"), "poe: arquivo novo no lugar do antigo")
        confere(not [f for f in os.listdir(jogo) if f.endswith(".tmp")], "poe: nenhum temporário sobrou")
        rc, out, err = roda(srv, ["coleta", "cgdu1o", "--destino", rodada + "/logs", "--capturas", "shg2_cgdu1o"])
        confere(rc == 0 and open(rodada + "/logs/cgdu1o_bt3xbx.log").read() == LOG_BOM and open(rodada + "/logs/cgdu1o_output.txt").read() == "saida falsa\n"
                and os.path.exists(rodada + "/logs/capturas/shg2_cgdu1o/gpu_00001.ppm"), "coleta: log, output e capturas")
        rc, out, err = roda(srv, ["coleta", "cgdu1o", "--destino", rodada + "/logs"])
        confere(rc != 0 and "--sobrescreve" in err, "coleta: não sobrescreve sem --sobrescreve")
        rc, out, err = roda(srv, ["coleta", "cgdu1o", "--destino", rodada + "/logs", "--sobrescreve"]); confere(rc == 0, "coleta: com --sobrescreve")
        rodada2 = os.path.join(tmp, "rodada2"); os.makedirs(rodada2); open(rodada2 + "/environment_cgdi1o.txt", "w").write("novo ambiente\n")
        rc, out, err = roda(srv, ["proxima", "cgdu1o", "cgdi1o", "--pasta", rodada2]); print(out.rstrip())
        confere(rc == 0 and "0 aviso(s)" in out and open(jogo + "/environment.txt").read() == "novo ambiente\n"
                and os.path.exists(rodada2 + "/logs/cgdu1o_bt3xbx.log"), "proxima: coleta, checagem limpa e ambiente posto")
        rc, out, err = roda(srv, ["confere", "environment.txt", rodada2 + "/environment_cgdi1o.txt"]); confere(rc == 0 and "IGUAIS" in out, "confere: iguais")
        open(rodada2 + "/outro.txt", "w").write("x")
        rc, out, err = roda(srv, ["confere", "environment.txt", rodada2 + "/outro.txt"]); confere(rc == 1 and "DIFERENTES" in out, "confere: diferentes sai 1")

        # 5. comandos recebidos
        print("5. comandos recebidos pelo servidor (sessões 1 a 4)")
        cmds = [c for c, _ in srv.comandos]
        print("   distintos:", sorted(set(cmds)))
        confere("QUIT" not in cmds and "REST" not in cmds, "nenhum QUIT nem REST")
        confere(all("/" not in a for c, a in srv.comandos if c == "RETR"), "nenhum RETR com caminho")
        # RETR sempre depois de CWD na mesma conexão: a sequência dentro de cada conexão não é rastreada; ao menos todo CWD precede os RETR
        confere(srv.max_ativas == 1, f"uma conexão por vez (máximo simultâneo {srv.max_ativas})")
        srv.fecha()

        # 2. md5 errado
        print("2. md5 errado (servidor que corrompe o recebido)")
        srv, jogo = novo(tmp, "s2", corrompe=True)
        rc, out, err = roda(srv, ["poe", rodada + "/environment_cgdi1o.txt", "--como", "environment.txt"]); print("  ", err.strip().splitlines()[-1])
        confere(rc == 1 and "diferente" in err, "md5 diferente: erro com saída 1")
        srv.fecha()

        # 3. nova tentativa
        print("3. nova tentativa (a primeira conexão de dados de cada tipo cai)")
        srv, jogo = novo(tmp, "s3", derruba=True)
        open(jogo + "/bt3xbx.log", "w").write(LOG_BOM); open(jogo + "/output.txt", "w").write("saida falsa\n")
        rc, out, err = roda(srv, ["lista"]); print("  ", [l for l in err.splitlines()])
        confere(rc == 0 and "2 tentativa(s)" in err and "bt3xbx.log" in out, "lista refeita")
        rc, out, err = roda(srv, ["poe", rodada + "/environment_cgdi1o.txt", "--como", "environment.txt"]); print("  ", [l for l in err.splitlines()])
        confere(rc == 0 and "poe environment.txt: tentativa 1/5 falhou" in err and "poe environment.txt: ok em " in err, "poe refeito (STOR caiu na primeira; o RETR da conferência também caiu, 3 tentativas)")
        confere(os.listdir(jogo).count("environment.txt") == 1, "poe refeito: arquivo íntegro no destino")
        srv.ja_derrubou.discard("RETR")
        rc, out, err = roda(srv, ["coleta", "n", "--destino", os.path.join(tmp, "c3")]); print("  ", [l for l in err.splitlines()])
        confere(rc == 0 and "ok em 2 tentativa(s)" in err and open(os.path.join(tmp, "c3/n_bt3xbx.log")).read() == LOG_BOM, "coleta refeita (RETR caiu na primeira)")
        cmds = [c for c, _ in srv.comandos]
        confere("QUIT" not in cmds and "REST" not in cmds and srv.max_ativas == 1, "sem QUIT/REST e uma conexão por vez também com quedas")
        srv.fecha()
        srv, jogo = novo(tmp, "s3b", derruba=True)
        # esgota o limite: servidor que sempre derruba
        srv.ja_derrubou = type("Nunca", (set,), {"add": lambda self, x: None})()
        rc, out, err = roda(srv, ["--tentativas", "3", "lista"])
        confere(rc != 0 and "3 tentativas sem sucesso" in err, "limite de tentativas respeitado")
        srv.fecha()

        # 4. checagem avisando
        print("4. checagem do proxima com log ruim e log sem fim")
        for rotulo, log in (("log com FALHA/ERRO/SAVES errado", LOG_RUIM), ("log sem fim", LOG_BOM.replace("roteiro: f5_duelo fim\n", "roteiro: f5_duelo passo 10\n"))):
            srv, jogo = novo(tmp, "s4" + rotulo[:6])
            open(jogo + "/bt3xbx.log", "w").write(log); open(jogo + "/output.txt", "w").write("x")
            rod = os.path.join(tmp, "r4" + str(abs(hash(rotulo)))); os.makedirs(rod); open(rod + "/environment_cgdi1o.txt", "w").write("a\n")
            rc, out, err = roda(srv, ["proxima", "cgdu1o", "cgdi1o", "--pasta", rod]); print(f"  [{rotulo}]"); print("\n".join("  " + l for l in out.splitlines()))
            confere(rc == 0 and "AVISO" in out and "ambiente cgdi1o posto" in out, f"{rotulo}: avisa e segue")
            if "FALHA" in log: confere("AVISO roteiro fim" in out and "AVISO sem roteiro: FALHA" in out and "AVISO sem ERRO" in out and "AVISO BT3_SAVES" in out, "avisos de fim, FALHA, ERRO e SAVES")
            else: confere("AVISO roteiro fim" in out and "AVISO sem roteiro: FALHA" not in out, "só o aviso de fim")
            srv.fecha()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("\nFALHARAM: " + "; ".join(falhas) if falhas else "\ntodos os casos passaram")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
