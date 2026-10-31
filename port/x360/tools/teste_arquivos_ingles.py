#!/usr/bin/env python3
"""Confere nomes de arquivos, exceções históricas e FTP simulado; inventaria antes/depois."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
BASE = '78dbb29c1187937d820ac15e634f5453f2bcdeee'
EXCECOES = 'docs/x360/plano/F8/05-excecoes.json'
# Fragmentos também detectam o prefixo montado por concatenação num gerador.
ANTIGOS = re.compile(
    r'(?<!\w)(?:' + 'ambiente' + r'(?:_[^\s`\"\'/\\<>(),;]*|\*)?\\?\.txt|'
    + 'saida' + r'\\?\.txt)|(?<=[\"\'])' + 'ambiente' + r'_(?=[\"\'])', re.I)
ATUAIS = re.compile(r'(?<!\w)(?:environment(?:_[^\s`\"\'/\\<>(),;]*|\*)?\.txt|output\.txt|bt3xbx\.log)', re.I)


def exigir(condicao, mensagem):
    if not condicao:
        raise ValueError(mensagem)


def versao_base():
    dados = subprocess.check_output(['git', 'archive', BASE], cwd=RAIZ)
    with tarfile.open(fileobj=io.BytesIO(dados)) as tar:
        return {m.name: tar.extractfile(m).read() for m in tar if m.isfile()}


def versao_atual():
    nomes = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=RAIZ)
    return {n: (RAIZ/n).read_bytes() for n in nomes.decode().split('\0') if n and (RAIZ/n).is_file()}


def levantar(arquivos, padrao):
    linhas = []
    for nome, dados in sorted(arquivos.items()):
        for m in padrao.finditer(Path(nome).name):
            linhas.append(dict(arquivo=nome, linha=0, nome=m[0], texto=nome))
        try:
            texto = dados.decode('utf-8')
        except UnicodeError:
            continue
        for n, linha in enumerate(texto.splitlines(), 1):
            for m in padrao.finditer(linha):
                linhas.append(dict(arquivo=nome, linha=n, nome=m[0], texto=linha))
    return linhas


def chave(item):
    return item['arquivo'], hashlib.sha256(item['texto'].encode()).hexdigest()


def conferir(arquivos, excecoes):
    permitidas = { (e['arquivo'], e['sha256']): e['motivo'] for e in excecoes }
    antigas = levantar(arquivos, ANTIGOS)
    vistas = Counter()
    for item in antigas:
        k = chave(item)
        exigir(item['linha'] and k in permitidas,
               f"nome antigo: {item['arquivo']}:{item['linha']}: {item['nome']}")
        item['motivo'] = permitidas[k]
        vistas[k] += 1
    # Uma linha permitida não pode autorizar uma cópia adicional em outro ponto.
    for e in excecoes:
        k = e['arquivo'], e['sha256']
        if 'linha_atual' in e:  # exceção de arquivo posterior à base: confere na árvore atual
            original = arquivos[e['arquivo']].decode().splitlines()
            linha = original[e['linha_atual']-1]
        else:
            original = subprocess.check_output(['git', 'show', BASE+':'+e['arquivo']], cwd=RAIZ).decode().splitlines()
            linha = original[e['linha_na_base']-1]
        exigir(hashlib.sha256(linha.encode()).hexdigest() == e['sha256'], 'exceção não corresponde à base')
        exigir(vistas[k] == len(ANTIGOS.findall(linha)), 'exceção ausente ou duplicada: '+e['arquivo'])
    print(f"inventário: {len(antigas)} ocorrências antigas justificadas em {len(vistas)} linhas; nenhum nome antigo ativo")
    return antigas


def ftp_simulado():
    spec = importlib.util.spec_from_file_location('subir', RAIZ/'port/x360/tools/subir.py')
    subir = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(subir)  # importar não deve procurar o console

    class Ftp:
        def __init__(self):
            self.arquivos = {}
            self.comandos = []

        def storbinary(self, comando, fonte, blocksize):
            self.comandos.append(comando)
            self.arquivos[comando[5:]] = fonte.read(blocksize)

        def sendcmd(self, comando):
            self.comandos.append(comando)
            verbo, nome = comando.split(' ', 1)
            if verbo == 'DELE':
                self.arquivos.pop(nome, None)
            elif verbo == 'RNFR':
                self.origem = nome
            elif verbo == 'RNTO':
                self.arquivos[nome] = self.arquivos.pop(self.origem)

        def retrbinary(self, comando, receber, blocksize=None):
            self.comandos.append(comando)
            receber(self.arquivos[comando[5:]])

        def retrlines(self, comando, receber):
            for nome in self.arquivos:
                receber('-rw-r--r-- 1 xbox xbox 1 Jan 1 00:00 '+nome)

    with tempfile.TemporaryDirectory(prefix='ingles-ftp-') as d:
        p = Path(d)/'environment_teste.txt'
        p.write_text('BT3_LOG=1\n')
        f = Ftp()
        subir.subir(f, str(p), 'environment.txt')
        exigir(f.arquivos == {'environment.txt': p.read_bytes()}, 'destino do ambiente incorreto')
        subir.subir(f, str(p))
        exigir(f.arquivos[p.name] == p.read_bytes(), 'nome do modelo incorreto')
        f.arquivos['output.txt'] = b'teste\n'
        exigir(subir.baixar(f, 'output.txt') == b'teste\n', 'download da saída incorreto')
        exigir(subir.baixar(f, 'output.txt') == b'teste\n', 'releitura da cópia incorreta')
        exigir('output.txt' not in f.arquivos and 'output.txt.lido' in f.arquivos, 'renomeação da saída incorreta')
        f.arquivos['bt3xbx.log'] = b'diario\n'
        exigir(subir.baixar(f, 'bt3xbx.log') == b'diario\n', 'nome do diário incorreto')
        for comando in f.comandos:
            exigir(not ANTIGOS.search(comando), 'FTP usou nome antigo: '+comando)
    print('FTP simulado: envio por nome e por basename, md5 e downloads com os nomes atuais: tudo certo')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--inventario', type=Path, help='JSON de evidências fora do repositório')
    ap.add_argument('--mutantes', action='store_true')
    a = ap.parse_args()
    arquivos = versao_atual()
    excecoes = json.loads(arquivos[EXCECOES])
    antigas = conferir(arquivos, excecoes)
    atuais = levantar(arquivos, ATUAIS)
    # Todos os componentes de nomes dos modelos cabem no FATX.
    for item in atuais:
        if item['linha'] == 0:
            exigir(len(Path(item['arquivo']).name) <= 42, 'nome excede FATX: '+item['arquivo'])
    ftp_simulado()
    if a.mutantes:
        casos = (
            ('gravador', 'port/x360/src/f2/posix360.c', b'sSaida = fopen(', b'output.txt', b'saida'+b'.txt'),
            ('leitor', 'port/x360/src/f2/main360.cpp', b'    Ambiente_Carregar(', b'environment.txt', b'ambiente'+b'.txt'),
            ('gerador', 'port/x360/tools/cortes_console.py', b'write_text(', b"'environment_'", b"'ambiente"+b"_'"),
        )
        for nome, caminho, marca, de, para in casos:
            copia = arquivos.copy()
            linhas = copia[caminho].splitlines(keepends=True)
            alvos = [i for i, l in enumerate(linhas) if marca in l and de in l]
            exigir(len(alvos) == 1, 'alvo da sabotagem ausente ou ambíguo')
            i = alvos[0]
            linhas[i] = linhas[i].replace(de, para)
            copia[caminho] = b''.join(linhas)
            try:
                conferir(copia, excecoes)
            except ValueError as e:
                exigir('nome antigo:' in str(e), 'mutante falhou por outro motivo')
                print(f'mutante {nome}: acusado: {e}')
            else:
                raise ValueError('mutante aceito: '+nome)
        copia = arquivos.copy()
        copia['port/x360/ambientes/environment_teste.txt'.replace('environment', 'ambiente')] = b'BT3_LOG=1\n'
        try:
            conferir(copia, excecoes)
        except ValueError as e:
            exigir('nome antigo:' in str(e), 'mutante de caminho falhou por outro motivo')
            print('mutante nome do modelo: acusado:', e)
        else:
            raise ValueError('modelo com nome antigo aceito')
    if a.inventario:
        exigir(not a.inventario.resolve().is_relative_to(RAIZ), 'inventário deve ficar fora do repositório')
        antes = levantar(versao_base(), ANTIGOS)
        motivos = {(e['arquivo'], e['sha256']): e['motivo'] for e in excecoes}
        for item in antes:
            item['destino'] = motivos.get(chave(item), 'substituída pelo nome em inglês')
        a.inventario.write_text(json.dumps(dict(base=BASE, antes=antes, depois_antigas=antigas,
                                              depois_atuais=atuais), ensure_ascii=False, indent=2)+'\n')
        print(f'inventário completo salvo: {len(antes)} antes; {len(antigas)} antigas e {len(atuais)} atuais depois')
    print('arquivos em inglês: tudo certo')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as e:
        raise SystemExit('FALHA: '+str(e))
