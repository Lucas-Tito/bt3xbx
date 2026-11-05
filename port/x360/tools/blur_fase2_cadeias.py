#!/usr/bin/env python3
"""Repete só o critério final e as referências de PC afetadas pelos marcadores.

Executar sob ~/vez.sh com tee persistente. Conserva a rodada extensa anterior.
"""
import pathlib
import subprocess

import blur_fase2 as fase
import blur_rodada as rodada

ANTERIOR = pathlib.Path.home() / 'blur-analise/fase2'
SAIDA = pathlib.Path.home() / 'blur-analise/fase2-despacho'
QUADROS = ['2969', '3362', '4590', '4977', '5357', '5418']


def mesmos(caso, anterior, escala):
    for q in QUADROS:
        nome = 'gpu_%05d.ppm' % int(q)
        if rodada.ler_ppm(caso / nome) != rodada.ler_ppm(anterior / nome):
            raise RuntimeError('controle mudou no GS ' + q)
    print('controle preservado:', caso.name, 'escala', escala, '6/6', flush=True)


def sharp_exercitado(caso, base):
    alterados = []
    for q in QUADROS:
        nome = 'gpu_%05d.ppm' % int(q)
        alterados.append(rodada.ler_ppm(caso / nome) != rodada.ler_ppm(base / nome))
    if any(alterados[:2]) or not any(alterados[2:]):
        raise RuntimeError('Sharp sem diferença na luta ou alterando menu/seleção')
    print(caso.name + ': diferença na luta, menu e seleção iguais', flush=True)


def media_exercitada(caso, sem_media):
    try:
        mesmos(caso, sem_media, 2)
    except RuntimeError:
        print('média exercitada: c04 difere de c03', flush=True)
    else:
        raise RuntimeError('média não contribuiu nas capturas')


def main():
    if subprocess.check_output(['git', 'status', '--porcelain'], cwd=rodada.RAIZ).strip():
        raise RuntimeError('árvore suja antes da rodada')
    SAIDA.mkdir(parents=True, exist_ok=True)
    if (SAIDA / 'INICIO').exists():
        raise RuntimeError('preserve a rodada anterior antes de repetir')
    (SAIDA / 'INICIO').write_bytes(subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=rodada.RAIZ))
    # A referência anterior da variante Sharp é a entrada estragada que este
    # controle deve recusar: seus marcadores tinham desaparecido com Native=0.
    try:
        sharp_exercitado(ANTERIOR / 'pc/2/c02_blocos_sharp', ANTERIOR / 'pc/2/c00_base')
    except RuntimeError as erro:
        print('negativo real da coleta anterior recusado:', erro, flush=True)
    else:
        raise RuntimeError('o negativo real deixou de reproduzir a falha')
    fase.SAIDA = rodada.SAIDA = SAIDA
    (SAIDA / 'base-102').symlink_to(ANTERIOR.parent / 'base-102', target_is_directory=True)
    rodada.rodar('teste-cadeias', ['python3', 'port/x360/tools/teste_blur_pc.py'])
    rodada.rodar('pc-build', ['python3', 'port/x360/tools/blur_pc_build.py', fase.PC])
    controle = ANTERIOR.parent / 'imagens/base/x86/controle.bin'
    for escala, nome, ajustes in (
        (2, 'c00_base', {}),
        (2, 'c01_blocos_macio', {'BT3_GLOW_BLOCK': '1'}),
        (1, 'c02_blocos_sharp', {'BT3_GLOW_BLOCK': '1', 'BT3_SHARP_BLUR': '1'}),
        (2, 'c02_blocos_sharp', {'BT3_GLOW_BLOCK': '1', 'BT3_SHARP_BLUR': '1'}),
        (2, 'c03_distancia_nativa', {'BT3_DISTANCIA_NATIVA': '1'}),
        (2, 'c04_distancia_media', {'BT3_DISTANCIA_NATIVA': '1', 'BT3_DISTANCIA_MEDIA': '1'}),
        (1, 'c05_pc_completo', {k: '1' for k in fase.CHAVES}),
        (2, 'c05_pc_completo', {k: '1' for k in fase.CHAVES}),
    ):
        fase.pc_caso(escala, nome, ajustes, controle, QUADROS)
        nova = SAIDA / 'pc' / str(escala) / nome
        if nome != 'c02_blocos_sharp' or escala == 1:
            mesmos(nova, ANTERIOR / 'pc' / str(escala) / nome, escala)
        if escala == 2 and nome not in ('c00_base', 'c01_blocos_macio'):
            sharp_exercitado(nova, SAIDA / 'pc/2/c00_base')
    # Preservar a evidência anterior; as opções de distância precisam de novo
    # julgamento, pois os códigos antigos colidiam com os menus do PC.
    media_exercitada(SAIDA / 'pc/2/c04_distancia_media', SAIDA / 'pc/2/c03_distancia_nativa')
    for alvo, extra in (('x86', []), ('x86', ['--rastreio']), ('ppc', [])):
        rodada.rodar('final-' + alvo + ('r' if extra else ''),
                     ['python3', 'port/x360/tools/headless.py', '--alvo', alvo, *extra],
                     {'BT3_TESTE_DESFOQUE': '0'})
    rodada.objetos()
    subprocess.run(['cmp', str(SAIDA / 'text-final.bin'), str(SAIDA / 'base-102/text.bin')], check=True)
    rodada.rodar('oficial', ['bash', 'port/x360/tools/roteiros.sh', 'f5_duelo'],
                 {'SEMMAPA': '1', 'IMG': '1', 'O': str(SAIDA / 'oficial'), 'TP': '3600'})
    rodada.conferir_ancoras(SAIDA / 'oficial/f5_duelo/telas_a.txt')
    (SAIDA / 'COLETA_CONCLUIDA').write_text('Julgar Sharp, distância e conjunto completo; console ainda separado.\n')


if __name__ == '__main__':
    try:
        main()
    except Exception as erro:
        SAIDA.mkdir(parents=True, exist_ok=True)
        (SAIDA / 'FALHA').write_text(str(erro) + '\n')
        raise
