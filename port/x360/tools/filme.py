#!/usr/bin/env python3
"""F6, etapa 7: comparação estrita das quatro capturas e análise da deriva por imagem mostrada.

filme.py compara CONSOLE REFERENCIA [--tipo op|ed]
filme.py log bt3xbx.log [--tipo op|ed]
filme.py controles

Capturas: escala 1, antes do fade e do FXAA; filtro no ponto, igual ao decodificador. Todos os arquivos são
obrigatórios. Aceite por imagem: >=98% até 8 por canal E PSNR >=30 dB. A aprovação visual ainda cabe ao Lucas.
Log: usa o vblank que mostrou a imagem e PCM consumido, incluindo o atraso inicial e o intervalo até a próxima
imagem. Não transforma a primeira amostra em zero. Falha em lacunas, silêncio sem relógio e rodada incompleta.
"""
import argparse
import math
from pathlib import Path
import re
import tempfile

from compara_quadros import medir

TOTAIS = {"op": 2810, "ed": 4200}
PERIODO = 1001 / 30  # ritmo pedido pelo jogo
VIDEO_MS = 1000 / 30  # frame_rate_code 5 nos dois MPEG; os ADX têm exatamente N/30 segundos
AMOSTRAS = {"op": 4496000, "ed": 6720000}


def ppm(caminho, console=False):
    d = Path(caminho).read_bytes()
    m = re.match(rb"P6\n(512|640) 448\n255\n", d)
    if not m:
        raise ValueError(f"{caminho}: cabeçalho fora do contrato P6, 512/640 x 448, 255")
    w = int(m[1])
    px = d[m.end():]
    if len(px) != w * 448 * 3 or (not console and w != 512):
        raise ValueError(f"{caminho}: tamanho incorreto")
    if w == 640:
        if any(any(px[(y * w + 512) * 3:(y + 1) * w * 3]) for y in range(448)):
            raise ValueError(f"{caminho}: borda fora dos 512 pixels não é preta")
        px = b"".join(px[y * w * 3:(y * w + 512) * 3] for y in range(448))
    return px


def compara(console, referencia, tipo):
    falhas = []
    for i in (0, 1000, 2000, TOTAIS[tipo] - 1):
        nome = f"filme_{tipo}_{i:05d}.ppm"
        a, b = ppm(Path(console) / nome, True), ppm(Path(referencia) / nome)
        n, _, _, ate8, pior, _, quadrados, _ = medir(a, b, 8)
        psnr = math.inf if not quadrados else 10 * math.log10(255 ** 2 / (quadrados / (3 * n)))
        ok = ate8 / n >= .98 and psnr >= 30
        print(f"{nome}: {100 * ate8 / n:.4f}% até 8, PSNR {psnr:.3f} dB, pior {pior}: {'ok' if ok else 'FALHA'}")
        if not ok:
            falhas.append(nome)
    if falhas:
        raise ValueError("capturas recusadas: " + ", ".join(falhas))


def campos(linha):
    return {k: float(v) if "." in v else int(v) for k, v in re.findall(r"(\w+)=([0-9]+(?:\.[0-9]+)?)", linha)}


def confere_trecho(linhas, tipo, fim):
    total = TOTAIS[tipo]
    obrig = {"tipo", "global", "imagem", "quadro", "swap", "vb", "tb", "tb_ms", "valido", "geracao", "audio",
             "janela_us", "faltas", "travas", "silencios", "erro", "livre_kb"}
    if not linhas or any(set(r) != obrig for r in linhas):
        raise ValueError("trecho vazio ou registro incompleto")
    # A imagem 0 pode ficar sob o fade de entrada e não chegar à tela (etapa 6). Nenhuma outra lacuna é aceita.
    primeira = linhas[0]["imagem"]
    if primeira not in (0, 1) or [r["imagem"] for r in linhas] != list(range(primeira, total)):
        raise ValueError("filme incompleto, repetido ou com lacuna na sequência de imagens")
    if (fim is None or set(fim) != {"tipo", "global", "imagem", "tb", "tb_ms"} or
            fim["global"] != linhas[-1]["global"] or fim["imagem"] != total - 1 or fim["tb_ms"] <= 0):
        raise ValueError("sem a troca de saída depois da última imagem")
    geracao = linhas[0]["geracao"]
    if not geracao:
        raise ValueError("BGM sem geração")
    erros, intervalos, extremos = [], [], []
    for i, r in enumerate(linhas):
        if not r["valido"] or r["geracao"] != geracao or any(r[k] for k in ("faltas", "travas", "silencios", "erro")):
            raise ValueError(f"áudio sem medida válida na imagem {r['imagem']}")
        if r["janela_us"] >= 250000 or r["audio"] > AMOSTRAS[tipo] or r["livre_kb"] < 32768 or r["tb_ms"] <= 0:
            raise ValueError("janela de leitura excessiva, relógio inválido ou piso de 32 MiB violado")
        deriva = r["audio"] / 48 - r["imagem"] * VIDEO_MS
        extremos.append(deriva)
        if i:
            a = linhas[i - 1]
            if (r["tb"] <= a["tb"] or r["vb"] <= a["vb"] or r["swap"] <= a["swap"] or
                    r["audio"] < a["audio"] or r["global"] != a["global"] + 1 or r["quadro"] <= a["quadro"]):
                raise ValueError("relógio, geração ou ordem dos quadros regrediu")
            dt = (r["tb"] - a["tb"]) / r["tb_ms"]
            intervalos.append(dt)
            # Até a próxima troca ainda é a imagem anterior na tela. A incerteza da consulta PCM é de um bloco.
            extremos.append(r["audio"] / 48 - a["imagem"] * VIDEO_MS)
            esperado_audio = min(dt, max(0, (AMOSTRAS[tipo] - a["audio"]) / 48))
            if abs((r["audio"] - a["audio"]) / 48 - esperado_audio) > 12:
                erros.append("PCM não acompanha o tempo de parede (falta, pausa ou extrapolação inválida)")
    bons = sum(x <= PERIODO + 1 for x in intervalos)
    ultimo_periodo = (fim["tb"] - linhas[-1]["tb"]) / fim["tb_ms"]
    if not 0 < ultimo_periodo <= 50.05:
        erros.append("última imagem ficou tempo excessivo ou inválido na tela")
    extremos.append(min(AMOSTRAS[tipo] / 48, linhas[-1]["audio"] / 48 + ultimo_periodo) - linhas[-1]["imagem"] * VIDEO_MS)
    intervalos.append(ultimo_periodo)
    bons += ultimo_periodo <= PERIODO + 1
    duracao = (fim["tb"] - linhas[0]["tb"]) / fim["tb_ms"]
    esperado = len(linhas) * PERIODO
    # Não se aprova uma medida encostada em 100 ms: um bloco de 512/48 kHz cobre a incerteza da consulta.
    margem = 512 / 48
    if max(map(abs, extremos)) + margem > 100:
        erros.append("deriva, com margem de um bloco PCM, excede 100 ms")
    if bons / len(intervalos) < .99 or max(intervalos) > 50.05 or abs(duracao / esperado - 1) > .01:
        erros.append("ritmo fora do aceite")
    print(f"{tipo}: {len(linhas)} imagens na tela, deriva inicial {extremos[0]:.3f} ms, "
          f"final {linhas[-1]['audio'] / 48 - linhas[-1]['imagem'] * VIDEO_MS:.3f} ms; "
          f"pior |deriva| {max(map(abs, extremos)):.3f} + {margem:.3f} ms; "
          f"intervalos no prazo {100 * bons / len(intervalos):.3f}%, máximo {max(intervalos):.3f} ms; "
          f"duração da primeira até a saída {duracao / 1000:.3f} s; mínimo livre {min(r['livre_kb'] for r in linhas)} KiB")
    if erros:
        raise ValueError("; ".join(sorted(set(erros))))


def log(caminho, tipo):
    texto = Path(caminho).read_text(errors="replace")
    if "filme_av: LACUNA" in texto or "CRASH" in texto:
        raise ValueError("lacuna de vídeo ou queda no log")
    linhas, trechos, fins = [], [], {}
    n = 1 if tipo == "op" else 2
    for l in texto.splitlines():
        if "filme_av_fim:" in l:
            f = campos(l)
            fins[(f.get("tipo"), f.get("global"))] = f
        if "filme_av:" not in l:
            continue
        r = campos(l)
        if r.get("tipo") not in (1, 2):
            raise ValueError("registro de vídeo sem tipo válido")
        if r.get("tipo") != n:
            continue
        if linhas and r.get("imagem", -1) <= linhas[-1].get("imagem", -1):
            trechos.append(linhas)
            linhas = []
        linhas.append(r)
    if linhas:
        trechos.append(linhas)
    if not trechos:
        raise ValueError("nenhum filme do tipo pedido no log")
    for trecho in trechos:
        confere_trecho(trecho, tipo, fins.get((n, trecho[-1].get("global"))))


def controles():
    """Dados sintéticos e cópias estragadas: não são imagens nem logs do jogo."""
    import contextlib
    import copy
    import io
    import subprocess
    import sys
    recusas = 0

    def recusa(nome, fn):
        nonlocal recusas
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                fn()
        except (ValueError, OSError):
            recusas += 1
            print(f"controle {nome}: recusado")
        else:
            raise ValueError(f"controle {nome}: NÃO recusado")

    def recusa_cli(nome, argumentos):
        nonlocal recusas
        r = subprocess.run([sys.executable, __file__, *map(str, argumentos)], capture_output=True, text=True)
        if r.returncode != 1 or "FALHA:" not in r.stderr:
            raise ValueError(f"controle CLI {nome}: saída {r.returncode}, esperada 1 com FALHA")
        recusas += 1
        print(f"controle CLI {nome}: saída 1 confirmada")

    with tempfile.TemporaryDirectory() as t:
        a, b = Path(t) / "console", Path(t) / "ref"
        a.mkdir(); b.mkdir()
        # Padrão espacial sintético, com valores de todos os canais.
        px = bytes((x * 13 + y * 7 + c * 53) % 256 for y in range(448) for x in range(512) for c in range(3))
        cab = b"P6\n512 448\n255\n"
        for i in (0, 1000, 2000, 2809):
            nome = f"filme_op_{i:05d}.ppm"
            (a / nome).write_bytes(cab + px); (b / nome).write_bytes(cab + px)
        with contextlib.redirect_stdout(io.StringIO()): compara(a, b, "op")
        p = a / "filme_op_01000.ppm"
        original = p.read_bytes()
        p.write_bytes(cab + bytes(255 - v for v in px))
        recusa("cor trocada", lambda: compara(a, b, "op"))
        p.write_bytes(original[:-1]); recusa("PPM truncado", lambda: compara(a, b, "op"))
        p.unlink(); recusa("captura ausente", lambda: compara(a, b, "op"))
        recusa_cli("captura ausente", ["compara", a, b])
        p.write_bytes(original)
        # Um erro difuso de 9 excede os 2% de pixels, mesmo com PSNR acima de 30.
        difuso = bytearray(px)
        for j in range(len(px) * 3 // 100): difuso[j] = min(255, difuso[j] + 9)
        p.write_bytes(cab + difuso)
        recusa("percentual de pixels", lambda: compara(a, b, "op"))
        corrompido = bytearray(px)
        for j in range(0, len(px), 300): corrompido[j:j + 3] = bytes(255 - v for v in px[j:j + 3])
        p.write_bytes(cab + corrompido)
        recusa("PSNR com poucos pixels ruins", lambda: compara(a, b, "op"))
        p.write_bytes(original)
        for tipo, total in TOTAIS.items():
            rows = [dict(tipo=1 if tipo == "op" else 2, global_=i + 1, imagem=i, quadro=i + 1, swap=i + 2,
                         vb=2 * i + 4, tb=round((i + 4) * VIDEO_MS * 49875), tb_ms=49875., valido=1, geracao=7,
                         audio=round(i * VIDEO_MS * 48), janela_us=20000, faltas=0, travas=0, silencios=0, erro=0,
                         livre_kb=64000) for i in range(total)]
            for r in rows: r["global"] = r.pop("global_")
            caminho = Path(t) / "log.txt"
            def grava_confere(dados, com_fim=True):
                caminho.write_text("\n".join("filme_av: " + " ".join(f"{k}={v}" for k, v in r.items()) for r in dados))
                if com_fim:
                    with caminho.open("a") as f:
                        f.write(f"\nfilme_av_fim: tipo={rows[-1]['tipo']} global={total} imagem={total - 1} "
                                f"tb={round((total + 4) * VIDEO_MS * 49875)} tb_ms=49875.0\n")
                log(caminho, tipo)
            with contextlib.redirect_stdout(io.StringIO()): grava_confere(rows)
            recusa(f"{tipo}: saída ausente", lambda: grava_confere(rows, False))
            for nome, muda in (
                ("atraso inicial", lambda rs: [r.update(audio=r["audio"] + 6000) for r in rs]),
                ("deriva acumulada", lambda rs: [r.update(audio=r["audio"] + i * 3) for i, r in enumerate(rs)]),
                ("silêncio", lambda rs: rs[10].update(valido=0)),
                ("troca do BGM", lambda rs: rs[10].update(geracao=8)),
                ("piso de memória", lambda rs: rs[10].update(livre_kb=32000)),
                ("trava perdida", lambda rs: rs[10].update(travas=1)),
                ("imagem ausente", lambda rs: rs.pop(10)),
                ("última ausente", lambda rs: rs.pop()),
                ("relógio parado", lambda rs: rs[10].update(audio=0)),
                ("vídeo lento", lambda rs: [r.update(tb=r["tb"] * 2) for r in rs]),
            ):
                copia = copy.deepcopy(rows); muda(copia)
                recusa(f"{tipo}: {nome}", lambda: grava_confere(copia))
            recusa_cli(f"{tipo}: vídeo lento", ["log", caminho, "--tipo", tipo])
    print(f"tudo certo: comparação sintética e {recusas} controles recusados")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="acao", required=True)
    c = sub.add_parser("compara"); c.add_argument("console"); c.add_argument("referencia")
    l = sub.add_parser("log"); l.add_argument("arquivo")
    for s in (c, l): s.add_argument("--tipo", choices=TOTAIS, default="op")
    sub.add_parser("controles")
    a = p.parse_args()
    try:
        if a.acao == "compara": compara(a.console, a.referencia, a.tipo)
        elif a.acao == "log": log(a.arquivo, a.tipo)
        else: controles()
    except (ValueError, OSError) as e:
        p.exit(1, f"FALHA: {e}\n")
    print("tudo certo")


if __name__ == "__main__":
    main()
