#!/usr/bin/env python3
"""Executa a main real nas duas builds e exige a recusa de políticas adulteradas."""
from pathlib import Path
import subprocess
import tempfile
import teste_queda360 as queda

RAIZ = queda.RAIZ


def fonte_teste():
    s = (RAIZ/'port/x360/tools/teste_queda/teste.cpp').read_text()
    a = s.index('    Exigir(relatorio.find(', s.index('DWORD XShowMessageBoxUI'))
    b = s.index('    if(caso=="caixa-negada"', a)
    s = s[:a] + '''    Exigir(wcscmp(titulo,L"Unable to continue")==0 && n==1 && wcscmp(b[0],L"OK")==0,"título ou botão");
    Exigir(wcsstr(texto,L"Press OK to return to the dashboard.")!=NULL,"saída sem instrução");
    const wchar_t *motivo = caso=="memoria" ? L"prepare its memory" :
        caso=="video" ? L"graphics renderer" : caso=="abortar" ? L"consistency check" :
        caso=="dados" ? L"required data" : L"fatal error";
    Exigir(wcsstr(texto,motivo)!=NULL && wcsstr(texto,L"Phase: ")!=NULL,"motivo ou fase ausente");
''' + s[b:]
    s = s.replace('if(!BT3_TESTE_FALHAS) Exigir(cor==0xFF010101 || cor==0xFF000000,"cor de fase no release");', '''#if BT3_TESTE_FALHAS
    if(cor==0xFFC02020) Evento("vermelha");
#else
    Exigir(cor==0xFF010101 || cor==0xFF000000,"cor de fase no release");
#endif''')
    s = s.replace('void Port_MapMemory() {}', 'void Port_MapMemory() { if(caso=="memoria") Port_Sair(2); }')
    a = s.index('    CONTEXT c={};', s.index('int __real_main'))
    b = s.index('\n}\n}', a)
    s = s[:a] + '''    gPortStage="battle";
    if(caso=="video") Port_FalhaVideo360();
    if(caso=="abortar") Port_Abortar();
    Port_Sair(caso=="dados" ? 2 : 9);
    return 0;''' + s[b:]
    a = s.index('        Exigir(Pos("criou relatório")')
    b = s.index('        printf(', a)
    s = s[:a] + '''        Exigir(Pos("cargas")<Pos("dashboard") && Pos("som")<Pos("dashboard"),"saída sem desmontagem");
#if BT3_TESTE_FALHAS
        Exigir(caixas==0 && Pos("vermelha")<Pos("dashboard"),"teste sem tela vermelha");
#else
        bool caixa=caso!="caixa-negada" && caso!="caixa-falhou";
        Exigir((caixas==1)==caixa,"caixa pública ausente");
        if(caixa) Exigir(Pos("OK")<Pos("dashboard") && presents>=4,"saída antes do OK ou apresentação parada");
#endif
''' + s[b:]
    return s


def main():
    with tempfile.TemporaryDirectory(prefix='release-falhas-', dir=Path.home()) as tmp:
        p = Path(tmp)
        arquivos = {'main360.cpp': 'f2/main360.cpp', 'falha360.h': 'falha360.h',
                    'queda360.h': 'queda360.h', 'saida360.h': 'saida360.h'}
        originais = {n: (RAIZ/'port/x360/src'/f).read_text() for n, f in arquivos.items()}
        variantes = [('íntegro', None, None, None),
                     ('cor pública', 'falha360.h', 'return 0xFF010101;', 'return fase;'),
                     ('sem cor teste', 'falha360.h', 'return fase;', 'return 0xFF010101;'),
                     ('sem apresentação', 'falha360.h', 'while (!XHasOverlappedIoCompleted(&ov)) apresentar();',
                      'while (!XHasOverlappedIoCompleted(&ov)) {}'),
                     ('sem caixa', 'main360.cpp', 'if (gCodigo != 0 || sQuedaTomada) Falha_Mostrar(Saida_Apresentar);',
                      '(void)&Falha_Mostrar;'),
                     ('sem motivo', 'falha360.h', 'const wchar_t *p = motivo;', 'const wchar_t *p = L""; (void)motivo;')]
        for nome, arquivo, antes, depois in variantes:
            for n, s in originais.items():
                if n == arquivo:
                    assert s.count(antes)==1, nome
                    s = s.replace(antes, depois)
                (p/n).write_text(s)
            (p/'teste.cpp').write_text(fonte_teste())
            resultados = []
            for teste in (0, 1):
                cmd = ['g++', '-std=c++11', '-O2', '-Wno-unknown-pragmas', '-Wno-int-to-pointer-cast',
                       '-DBT3_TESTE_FALHAS='+str(teste), '-I'+str(p),
                       '-I'+str(RAIZ/'port/x360/tools/teste_queda'), '-I'+str(RAIZ/'port/x360/src'),
                       '-I'+str(RAIZ/'port/x360/src/f2'), str(p/'teste.cpp'), '-o', str(p/'teste')]
                subprocess.run(cmd, check=True)
                for caso in ('memoria','video','abortar','dados','outro','log-ligado','caixa-ocupada','caixa-negada','caixa-falhou','resultado-falhou'):
                    try:
                        r = subprocess.run([p/'teste',caso],cwd=p,capture_output=True,text=True,timeout=3)
                        resultados.append(r.returncode)
                        if nome=='íntegro':
                            assert r.returncode==0, r.stdout+r.stderr
                    except subprocess.TimeoutExpired:
                        resultados.append(124)
            assert any(resultados)==(nome!='íntegro'), (nome,resultados)
            print(nome+(': 20 caminhos aprovados' if nome=='íntegro' else ': cópia recusada'),flush=True)
    print('Falhas públicas e de teste: tudo certo')


if __name__ == '__main__':
    main()
