#!/usr/bin/env python3
"""F6, etapa 8: deriva relativa do BGM no console (decisão 25.1, limite de 100 ms).

    python3 port/x360/tools/som_deriva.py bt3xbx.log [--geracao N]
    python3 port/x360/tools/som_deriva.py --autoteste

Exige BT3_SOM_DERIVA=1, dez minutos contínuos da mesma geração do BGM e dados a cada segundo.
SamplesPlayed conta quadros estéreo de 48 kHz, não amostras dos dois canais nem bytes. Vblanks: 60000/1001 Hz.
A origem é a primeira observação do fluxo, depois da primeira fila cheia. Não mede a latência do início do ADX,
nem o fim ouvido de uma voz curta, nem substitui a escuta. O laço não zera nenhum dos dois relógios.
"""
import argparse
import pathlib
import re
import subprocess
import sys
import tempfile

MARCA = "som (deriva):"
CAMPOS = set("ms vb vb_fim quadro luta tocadas enviadas mixadas geracao curtos fila faltas travas silencios "
             "erro_voz erro_motor guide janela_us".split())
CAMPO_VB_MS = 1001 / 60
FILA_MAX = 5 * 512  # quatro buffers, mais um em mistura/envio durante a olhada do vigia
LIMITE_MS = 100
MINIMO_MS = 600_000


def ler(caminho):
    registros = []
    with caminho.open(encoding="utf-8", errors="replace") as f:
        for numero, linha in enumerate(f, 1):
            if "CRASH" in linha or "linhas perdidas:" in linha:
                raise ValueError(f"linha {numero}: queda ou perda de linhas no diário")
            if MARCA not in linha:
                continue
            if not linha.endswith("\n"):
                raise ValueError(f"linha {numero}: registro cortado")
            texto = linha.split(MARCA, 1)[1].strip()
            pares = re.findall(r"([a-z_]+)=([0-9]+)", texto)
            if " ".join(f"{k}={v}" for k, v in pares) != texto:
                raise ValueError(f"linha {numero}: formato inválido")
            r = {k: int(v) for k, v in pares}
            if set(r) != CAMPOS or len(pares) != len(CAMPOS):
                raise ValueError(f"linha {numero}: campos ausentes, duplicados ou desconhecidos")
            r["linha"] = numero
            if registros:
                p = registros[-1]
                if any(r[k] < p[k] for k in ("ms", "vb", "quadro", "tocadas", "enviadas")):
                    raise ValueError(f"linha {numero}: relógio voltou; analisar cada abertura separadamente")
                if r["ms"] == p["ms"]:
                    raise ValueError(f"linha {numero}: registro duplicado")
            registros.append(r)
    if not registros:
        raise ValueError("faltam os dados de SamplesPlayed; usar BT3_SOM_DERIVA=1 no console")
    return registros


def avaliar(trecho):
    # A primeira observação pode pegar o fluxo antes de ter áudio bastante para cobrir a fila.
    while trecho and trecho[0]["mixadas"] < FILA_MAX:
        trecho = trecho[1:]
    if len(trecho) < 2:
        raise ValueError("sem música com fila cheia")
    origem = trecho[0]
    maximo, final = 0.0, 0.0
    pior_linha = origem["linha"]
    p = origem
    for r in trecho:
        if any(r[k] for k in ("curtos", "faltas", "travas", "silencios", "erro_voz", "erro_motor")):
            raise ValueError(f"linha {r['linha']}: falta, silêncio ou erro de áudio; fluxo descontínuo")
        if not 1 <= r["fila"] <= 4 or not 0 <= r["enviadas"] - r["tocadas"] <= FILA_MAX:
            raise ValueError(f"linha {r['linha']}: fila incompatível com SamplesPlayed")
        if r["guide"] not in (0, 1) or not 0 <= r["vb_fim"] - r["vb"] <= 1 or r["janela_us"] > 1000:
            raise ValueError(f"linha {r['linha']}: observação ambígua ou lenta")
        if r is not origem:
            if not 0 < r["ms"] - p["ms"] <= 2500:
                raise ValueError(f"linha {r['linha']}: lacuna acima de 2,5 s")
            if any(r[k] <= p[k] for k in ("vb", "quadro", "tocadas", "mixadas")):
                raise ValueError(f"linha {r['linha']}: jogo, desenho ou música não avançou")
        audio = (r["tocadas"] - origem["tocadas"]) / 48
        mix = (r["mixadas"] - origem["mixadas"]) / 48
        if abs(mix - audio) > FILA_MAX / 48:
            raise ValueError(f"linha {r['linha']}: o relógio do BGM não acompanha o PCM tocado")
        # A origem tem quantização de um vblank; a leitura pode atravessar mais um vblank.
        # Aprovar só se o intervalo inteiro de incerteza ficar dentro dos 100 ms.
        jogo_min = (r["vb"] - origem["vb_fim"] - 1) * CAMPO_VB_MS
        jogo_max = (r["vb_fim"] - origem["vb"] + 1) * CAMPO_VB_MS
        erro_leitura = (r["janela_us"] + origem["janela_us"]) / 1000
        limite = max(abs(audio - jogo_min), abs(audio - jogo_max)) + erro_leitura
        if limite > maximo:
            maximo, pior_linha = limite, r["linha"]
        final = audio - (r["vb"] - origem["vb"]) * CAMPO_VB_MS
        p = r
    jogo = (p["vb"] - origem["vb"]) * CAMPO_VB_MS
    audio = (p["tocadas"] - origem["tocadas"]) / 48
    parede = p["ms"] - origem["ms"]
    if min(jogo, audio, parede) < MINIMO_MS:
        raise ValueError(f"trecho curto: jogo {jogo / 1000:.3f} s, áudio {audio / 1000:.3f} s, parede {parede / 1000:.3f} s; exigir 600 s")
    if maximo > LIMITE_MS:
        raise ValueError(f"deriva além de 100 ms: limite com incerteza {maximo:.3f} ms na linha {pior_linha}; final {final:+.3f} ms")
    return (f"geração {origem['geracao']}, luta desde o vblank {origem['luta']}, {len(trecho)} observações, jogo {jogo / 1000:.3f} s, "
            f"áudio {audio / 1000:.3f} s, parede {parede / 1000:.3f} s; deriva final {final:+.3f} ms "
            f"({final / jogo * 1e6:+.1f} ppm), maior limite com incerteza {maximo:.3f} ms "
            f"(linha {pior_linha}); Guide em {sum(r['guide'] for r in trecho)} observações")


def conferir(caminho, geracao=None):
    registros = ler(caminho)
    trechos = []
    for r in registros:
        if not trechos or any(r[k] != trechos[-1][-1][k] for k in ("geracao", "luta")):
            trechos.append([])
        trechos[-1].append(r)
    candidatos = [t for t in trechos if t[0]["geracao"] and t[0]["luta"] and
                  (geracao is None or t[0]["geracao"] == geracao)]
    if not candidatos:
        raise ValueError("geração solicitada ausente ou nenhum BGM na luta")
    longos = [t for t in candidatos if t[-1]["ms"] - t[0]["ms"] >= MINIMO_MS]
    resultados = []
    for trecho in longos or [max(candidatos, key=lambda t: t[-1]["ms"] - t[0]["ms"])]:
        g = trecho[0]["geracao"]
        try:
            resultados.append(avaliar(trecho))
        except ValueError as e:
            raise ValueError(f"geração {g}: {e}") from e
    return resultados


def autoteste():
    # Dados sintéticos, sem bytes do jogo. A parede e o PCM avançam 1 s; os vblanks seguem a razão exata NTSC.
    linhas = []
    for segundo in range(612):
        ms = (segundo + 1) * 1000
        vb = ms * 60 // 1001
        tocadas = ms * 48
        linhas.append(dict(ms=ms, vb=vb + 1000, vb_fim=vb + 1000, quadro=vb // 2, luta=1000, tocadas=tocadas,
                           enviadas=tocadas + 1536, mixadas=tocadas + 1536, geracao=7, curtos=0,
                           fila=3, faltas=0, travas=0, silencios=0, erro_voz=0, erro_motor=0,
                           guide=int(100 <= segundo < 110), janela_us=20))

    def gravar(caminho, registros):
        caminho.write_text("".join(MARCA + " " + " ".join(f"{k}={v}" for k, v in r.items()) + "\n"
                                   for r in registros), encoding="utf-8")

    with tempfile.TemporaryDirectory(prefix="som-deriva-") as temp:
        bom = pathlib.Path(temp) / "bom.log"
        ruim = pathlib.Path(temp) / "estragado.log"
        gravar(bom, linhas)
        print("log sintético: " + conferir(bom)[0])
        controles = {}
        for sinal in (-1, 1):
            copia = [r.copy() for r in linhas]
            for i, r in enumerate(copia):
                for k in ("tocadas", "enviadas", "mixadas"):
                    r[k] += sinal * i * 24  # 305,5 ms de deriva em 611 s, fila e PCM coerentes
            controles[f"deriva {sinal:+d}"] = copia
        copia = [r.copy() for r in linhas]
        for i, r in enumerate(copia):
            # Deriva transitória de 250 ms, voltando a zero no fim.
            extra = max(0, 12000 - abs(i - 300) * 120)
            for k in ("tocadas", "enviadas", "mixadas"):
                r[k] += extra
        controles["deriva no meio, final certo"] = copia
        controles["trecho curto"] = linhas[:300]
        controles["lacuna"] = linhas[:200] + linhas[204:]
        for campo, valor in (("curtos", 1), ("faltas", 1), ("travas", 1), ("silencios", 1),
                             ("erro_voz", 1), ("erro_motor", 1), ("tocadas", 0), ("quadro", 0),
                             ("geracao", 8), ("mixadas", 0), ("vb_fim", 999999), ("fila", 0), ("luta", 0)):
            copia = [r.copy() for r in linhas]
            copia[300][campo] = valor
            controles[campo] = copia
        for nome, copia in controles.items():
            gravar(ruim, copia)
            try:
                conferir(ruim)
            except ValueError as e:
                print(f"controle {nome}: acusado ({e})")
            else:
                raise ValueError(f"controle {nome}: NÃO acusado")
        # Também provar o contrato da linha de comando: log válido sai com 0, cópia com deriva sai com 1.
        gravar(ruim, controles["deriva +1"])
        for caminho, esperado in ((bom, 0), (ruim, 1)):
            resultado = subprocess.run([sys.executable, __file__, str(caminho)], capture_output=True, text=True)
            if resultado.returncode != esperado:
                raise ValueError(f"CLI em {caminho.name}: saiu com {resultado.returncode}, esperado {esperado}")
            print(f"CLI {caminho.name}: saída {resultado.returncode}; {(resultado.stderr or resultado.stdout).splitlines()[0]}")
        for nome, texto in (("log antigo", "som (vigia): blocos 60000\n"),
                            ("registro cortado", bom.read_text()[:-1]),
                            ("campo ausente", bom.read_text().replace(" curtos=0", "")),
                            ("campo duplicado", bom.read_text() + MARCA + " ms=2 ms=3\n")):
            ruim.write_text(texto)
            try:
                conferir(ruim)
            except ValueError:
                print(f"controle {nome}: acusado")
            else:
                raise ValueError(f"controle {nome}: NÃO acusado")
    print("som_deriva: tudo certo, log válido e 22 controles negativos acusados")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("log", nargs="?", type=pathlib.Path)
    p.add_argument("--geracao", type=int, help="examinar só uma criação do BGM, identificada no log")
    p.add_argument("--autoteste", action="store_true", help="log sintético de dez minutos e cópias estragadas")
    a = p.parse_args()
    try:
        if a.autoteste:
            autoteste()
        elif a.log:
            for resultado in conferir(a.log, a.geracao):
                print(resultado)
            print("som_deriva: tudo certo (deriva relativa até 100 ms nas observações)")
        else:
            p.error("informar o log ou --autoteste")
    except (OSError, ValueError) as e:
        print(f"FALHA: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
