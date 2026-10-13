#!/usr/bin/env python3
"""F4, etapa 1: confere que toda chamada de Sprite_Unpack no código do jogo tem o gancho de conversão logo depois.

    unpacks.py [--lista]        (em qualquer máquina, na raiz; só lê src/ e port/x360/conv/)
    (roteiros.py importa levantar(): a lista das chamadas, para classificar as que um roteiro alcança)

Cada Sprite_Unpack (src/sys/sprite.c) descomprime um formato que só quem chama conhece (docs/x360/formatos/menus.md 4),
então o gancho fica em cada chamador, sob #ifdef PORT_BIG_ENDIAN, logo depois do comando da chamada:
  - Port_ConvertMenuPack(buffer, ((s32 *)origem)[0], "formato"): o formato tem de existir na tabela de formatos do
    conversor (sFormatos em port/x360/conv/conv.c);
  - Port_ConvertLoadSheet(buffer, ((s32 *)origem)[0]): a folha da tela de carregamento (formato "folha");
  - Port_UnpackSemNumeros(buffer): a marca explícita de que o conteúdo só tem bytes.
O buffer do gancho tem de ser o resultado do Sprite_Unpack (quem recebe o retorno ou o dst) e o tamanho, o rawSize da
origem; é a mesma conferência que a guarda faz em tempo de jogo (port/x360/conv/conv_jogo.c), só que antes de rodar.
Confere também que a tabela de chamadores da guarda (CHAMADORES em conv_jogo.c, de onde sai o nome na mensagem de
falha) tem exatamente as funções que chamam Sprite_Unpack, e lista os Bpe_Decode chamados direto (fora do sprite.c).

A conferência é pelo texto do fonte, como o pacotes64.py faz na linha que o AST aponta: a chamada, o comando até o
';' e as linhas seguintes. Sai com 1 se alguma chamada estiver sem gancho (as pendentes, listadas) ou com gancho
errado, ou se a tabela de chamadores divergir.
"""
import collections, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
GANCHOS = ("Port_ConvertMenuPack", "Port_ConvertLoadSheet", "Port_UnpackSemNumeros")
FUNCAO = re.compile(r"^(?:static\s+)?[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;]*\)\s*\{?\s*$")


def formatos_do_conversor():
    """Os nomes da tabela sFormatos de conv.c."""
    texto = (ROOT / "port/x360/conv/conv.c").read_text()
    corpo = texto[texto.index("static const Formato sFormatos[]"):]
    corpo = corpo[:corpo.index("};")]
    return set(re.findall(r'\{"(\w+)",\s*Conv_\w+\}', corpo))


def chamadores_da_guarda():
    texto = (ROOT / "port/x360/conv/conv_jogo.c").read_text()
    m = re.search(r"#define CHAMADORES\(X\)((?:.*\\\n)*.*)", texto)
    return set(re.findall(r"X\((\w+)\)", m.group(1))) if m else set()


def norm(s):
    return re.sub(r"\s+", "", s)


def argumentos(s, ini):
    """Os argumentos da chamada cujo '(' está em s[ini], separados no nível de cima; e o fim."""
    nivel, atual, args, i = 0, "", [], ini
    while i < len(s):
        c = s[i]
        if c == "(":
            nivel += 1
            if nivel == 1:
                i += 1
                continue
        elif c == ")":
            nivel -= 1
            if nivel == 0:
                args.append(atual.strip())
                return args, i
        elif c == "," and nivel == 1:
            args.append(atual.strip())
            atual = ""
            i += 1
            continue
        atual += c
        i += 1
    return args, i


def chamadas(arq):
    """[(linha, função, comando, linhas depois do comando)] de cada Sprite_Unpack/Bpe_Decode do arquivo."""
    linhas = arq.read_text(errors="replace").split("\n")
    func, out = None, []
    for n, l in enumerate(linhas, 1):
        m = FUNCAO.match(l)
        if m and not l.startswith(("typedef", "#", "extern")):
            func = m.group(1)
        for nome in ("Sprite_Unpack", "Bpe_Decode"):
            if re.search(r"\b" + nome + r"\s*\(", l) and "extern" not in l and \
                    not re.match(r"^\s*void\s*\*\s*" + nome, l) and not l.lstrip().startswith(("/*", "*", "//")):
                # o comando inteiro, até o ';'
                k, cmd = n - 1, ""
                while k < len(linhas):
                    cmd += linhas[k] + "\n"
                    if ";" in linhas[k]:
                        break
                    k += 1
                out.append((n, func, nome, cmd, linhas[k + 1:k + 16]))
    return out


def gancho(depois):
    """O bloco #ifdef PORT_BIG_ENDIAN logo depois do comando (linhas em branco à parte), ou None."""
    i = 0
    while i < len(depois) and not depois[i].strip():
        i += 1
    if i >= len(depois) or not depois[i].strip().startswith("#ifdef PORT_BIG_ENDIAN"):
        return None
    bloco = []
    for l in depois[i + 1:]:
        if l.strip().startswith("#endif"):
            return "\n".join(bloco)
        if "extern" not in l:  # (a declaração do gancho, dentro do bloco)
            bloco.append(l)
    return None


def levantar():
    """(res, erros, funcs): res é [(situação, "arquivo:linha função", formato)] de cada chamada, na ordem do fonte
    (situação: "com gancho", "sem números", "pendente" ou "bpe-direto"). Também usado por roteiros.py."""
    formatos = formatos_do_conversor()
    res, erros, funcs = [], [], set()
    for arq in sorted((ROOT / "src").glob("**/*.c")):
        rel = str(arq.relative_to(ROOT))
        for n, func, nome, cmd, depois in chamadas(arq):
            if rel == "src/sys/sprite.c":
                continue  # o próprio Sprite_Unpack
            onde = f"{rel}:{n} {func}"
            p = cmd.index(nome)
            args, _ = argumentos(cmd, cmd.index("(", p))
            m = re.match(r"\s*([^;=]*?)\s*=\s*(?:\([^)]*\)\s*)?" + nome, cmd)
            buf = m.group(1).strip() if m and m.group(1).strip() else (args[1] if len(args) > 1 else "?")
            if nome == "Bpe_Decode":
                b = gancho(depois) or ""
                res.append(("bpe-direto", onde, "Port_ConvertAnim" if "Port_ConvertAnim" in b else "sem gancho"))
                continue
            funcs.add(func)
            b = gancho(depois)
            hs = [(h, i.start()) for h in GANCHOS for i in re.finditer(r"\b" + h + r"\s*\(", b or "")]
            if not hs:
                res.append(("pendente", onde, ""))
                continue
            h, ini = hs[0]
            hargs, _ = argumentos(b, b.index("(", ini))
            problemas = []
            if norm(hargs[0]) != norm(buf):
                problemas.append(f"gancho sobre {hargs[0]}, e o Sprite_Unpack dá {buf}")
            if h != "Port_UnpackSemNumeros":
                tam = norm(hargs[1]) if len(hargs) > 1 else ""
                if tam != norm(f"((s32 *){args[0]})[0]") and tam != norm(f"((s32 *)({args[0]}))[0]"):
                    problemas.append(f"tamanho {hargs[1] if len(hargs) > 1 else '?'} não é o rawSize de {args[0]}")
            if h == "Port_ConvertMenuPack":
                fm = re.match(r'\s*"(\w+)"\s*$', hargs[2] if len(hargs) > 2 else "")
                f = fm.group(1) if fm else "?"
                if f not in formatos:
                    problemas.append(f'formato "{f}" fora da tabela do conversor (conv.c)')
            else:
                f = "folha" if h == "Port_ConvertLoadSheet" else "sem números"
            if problemas:
                erros.append((onde, "; ".join(problemas)))
            res.append(("com gancho" if h != "Port_UnpackSemNumeros" else "sem números", onde, f))
    return res, erros, funcs


def main():
    lista = "--lista" in sys.argv
    res, erros, funcs = levantar()
    tabela = chamadores_da_guarda()
    for f in sorted(funcs - tabela):
        erros.append((f, "chama Sprite_Unpack e falta na tabela CHAMADORES de port/x360/conv/conv_jogo.c"))
    for f in sorted(tabela - funcs):
        erros.append((f, "está na tabela CHAMADORES de conv_jogo.c e não chama Sprite_Unpack"))

    cont = collections.Counter(s for s, _, _ in res)
    sp = sum(1 for s, _, _ in res if s != "bpe-direto")
    print(f"unpacks: {sp} chamadas de Sprite_Unpack em {len(funcs)} funções: " +
          ", ".join(f"{s} {n}" for s, n in sorted(cont.items())) + f"; {len(erros)} com erro")
    for s, onde, f in res:
        if s == "pendente" or lista:
            print(f"  {s}: {onde}{' ' + f if f else ''}")
    for onde, msg in erros:
        print(f"  ERRO: {onde}: {msg}")
    sys.exit(1 if erros or cont["pendente"] or any(s == "bpe-direto" and f == "sem gancho" for s, _, f in res)
             else 0)


if __name__ == "__main__":
    main()
