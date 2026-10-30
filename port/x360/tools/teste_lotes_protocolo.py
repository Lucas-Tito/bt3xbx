#!/usr/bin/env python3
"""Confere as fronteiras que o falso não pode provar sobre o D3D, com sabotagens.
É auditoria de fonte; não substitui execução do D3D nem imagem no console.
"""
from pathlib import Path
import re
from teste_lotes_fonte import funcao

RAIZ = Path(__file__).resolve().parents[3]
# Métodos de D3DDevice que o port chama, cada um conferido no d3d9.h do XDK 2.0.21256 (D3DDevice::Nome). O GCC do
# WSL não compila o gs_d3d.cpp nem as outras fontes do 360: um método que não existe (o KickOff, que só o D3DXps tem)
# só aparecia no cl.exe. Método novo entra aqui depois de conferido no d3d9.h.
XDK_D3DDEVICE = {
    'AcquireThreadOwnership', 'BlockOnFence', 'BlockUntilIdle', 'Clear', 'CreateDepthStencilSurface',
    'CreateIndexBuffer', 'CreatePixelShader', 'CreateRenderTarget', 'CreateTexture', 'CreateVertexBuffer',
    'CreateVertexDeclaration', 'CreateVertexShader', 'DrawIndexedVertices', 'DrawPrimitiveUP', 'GetRenderTarget',
    'InsertCallback', 'InsertFence', 'IsFencePending', 'Present', 'QuerySwapStatus', 'QueryThreadOwnership',
    'Release', 'ReleaseThreadOwnership', 'Resolve', 'SetDepthStencilSurface', 'SetIndices', 'SetPixelShader',
    'SetPixelShaderConstantF', 'SetRenderState', 'SetRenderTarget', 'SetSamplerState', 'SetScissorRect',
    'SetStreamSource', 'SetTexture', 'SetThreadOwnership', 'SetVertexDeclaration', 'SetVertexShader',
    'SetVertexShaderConstantF', 'SetVerticalBlankCallback', 'SetViewport',
}
CHAMADA = re.compile(r'\b(?:sDev|gDev|dev|gX360Dev)->(\w+)\(')


def fontes_360():
    return [p.read_text(errors='replace') for d in ('port/x360/src', 'port/x360/include')
            for p in sorted((RAIZ / d).rglob('*')) if p.suffix in ('.c', '.cpp', '.h')]


def metodos(textos):
    usados = {m for t in textos for m in CHAMADA.findall(t)}
    assert 'InsertFence' in usados and 'SetRenderState' in usados, 'a busca de chamadas de D3DDevice não achou nada'
    fora = sorted(usados - XDK_D3DDEVICE)
    assert not fora, f'métodos de D3DDevice fora do d3d9.h do XDK: {fora}'


def confere(d3d, grav, desenho, posix):
    publica = grav[grav.index('static void gs_lotes_passo'):]
    assert '__sync_synchronize();' in publica, 'barreira antes de publicar'
    assert publica.index('sBackend->lotesPrepara') < publica.index('__sync_synchronize();') < publica.index('sDesTem = 4;'), 'barreira antes de publicar'
    assert 'void __sync_synchronize(void) {\n    __sync();\n}' in posix, 'a ponte precisa de sync completo'
    prepara = funcao(d3d, 'lotes_prepara')
    assert 'vu_plano(q);' in prepara and 'prof_garante(q);' in prepara and 'sincroniza_alvos();' in prepara, 'recursos antes de publicar'
    assert not any(x in prepara for x in ('sProfFilaG ^=', 'sPendGrav = 0', 'limpa[', 'cleared =')), 'preparo preserva filas e gravação'
    parcial = d3d[d3d.index('    if (!final) {'):d3d.index('    sConta.msVuDes += msVu;')]
    assert 'sDev->InsertFence();' in parcial, 'despacho do lote obrigatório'
    assert parcial.index('ed_fim();') < parcial.index('prof_entrega();') < parcial.index('sDev->InsertFence();') < parcial.index('return;'), 'despacho e EDRAM livre'
    assert 'Present(' not in parcial and 'InsertCallback(' not in parcial, 'apresentação e cerca só no fecho'
    prontos = funcao(d3d, 'lotes_prontos')
    assert 'q->vuJogo < 0' in prontos and 'sMostradoTex != sFxaaTex' in prontos, 'buffers sem cópia e imagem completa para Guide'
    assert '!sAlvos[d->target].tex' in prontos and '!sProf[d->target].tex' in prontos and '!sModoZ' in prontos, 'cor e z preserváveis antes do corte'
    assert 'sLtExec.ver[i] == sAlvoD[i].ver' in d3d and 'q->targets[i].cleared |= sLtExec.cleared[i];' in d3d, 'cleared do desenho conserva versão'
    inicio = desenho.index('        if (sDesTem == 4) {')
    consumo = desenho[inicio:desenho.index('\n#endif', inicio)]
    assert 'lotesDesenha(&sLote, sLoteAte);' in consumo and 'continue;' in consumo, 'consumidor retorna ao laço sem espera de lote'


def main():
    d = (RAIZ / 'port/x360/src/gs/gs_d3d.cpp').read_text()
    g = (RAIZ / 'port/x360/include/gs_lotes_grav.h').read_text()
    s = (RAIZ / 'port/src/gs/gs_draw.c').read_text()
    p = (RAIZ / 'port/x360/src/f2/posix360.c').read_text()
    confere(d, g, s, p)
    textos = [d, g, s, p]
    for k, a, b in (
        (1, '__sync_synchronize();', '(void)0;'),
        (3, 'void __sync_synchronize(void) {\n    __sync();', 'void __sync_synchronize(void) {\n    __lwsync();'),
        (0, '    vu_plano(q);\n    prof_garante(q);', '    prof_garante(q);'),
        (0, 'static void lotes_prepara(const GsQuadro *q) {', 'static void lotes_prepara(const GsQuadro *q) { sPendGrav = 0;'),
        (0, '\n        sDev->InsertFence();', '\n        sDev->Present(NULL, NULL, NULL, NULL);'),
        (0, '!sAlvos[d->target].tex', '0'),
        (0, '!sProf[d->target].tex', '0'),
        (0, 'sMostradoTex != sFxaaTex', 'sMostradoTex == NULL'),
        (0, 'q->targets[i].cleared |= sLtExec.cleared[i];', 'q->targets[i].cleared = 0;'),
        (2, 'sBackend->lotesDesenha(&sLote, sLoteAte);', '(void)0;'),
    ):
        x = textos.copy()
        assert a in x[k]
        x[k] = x[k].replace(a, b, 1)
        try: confere(*x)
        except AssertionError as e: print('sabotagem recusada:', e)
        else: raise AssertionError('sabotagem aceita: ' + a)
    fontes = fontes_360()
    metodos(fontes)
    try: metodos(fontes + [d.replace('\n        sDev->InsertFence();', '\n        sDev->KickOff();', 1)])
    except AssertionError as e: print('sabotagem recusada:', e)
    else: raise AssertionError('sabotagem aceita: KickOff no D3DDevice')
    print('tudo certo na auditoria do protocolo e nos métodos de D3DDevice do XDK, onze sabotagens recusadas')


if __name__ == '__main__':
    main()
