/* A main e o filtro reais são executados com escalonamento determinístico. */
#include "xtl.h"
extern "C" int gX360Log;
DWORD agora, atual = 1, hardware = 0;
std::string caso, registro, relatorio;
std::vector<std::string> eventos;
bool filtroAtivo;
static unsigned presents, caixas, recusas;
static DWORD proximo = 10;
static std::vector<HANDLE> alcas;
void Exigir(bool ok, const char *m) { if (!ok) throw std::runtime_error(m); }
void Evento(const char *m) { eventos.push_back(m); }
HANDLE GetCurrentThread() { return NULL; }
DWORD GetCurrentThreadId() { return atual; }
DWORD GetTickCount() { return agora; }
DWORD GetLastError() { return 8; }
void Sleep(unsigned n) { agora += n; }
void ExitThread(DWORD) { throw ThreadExit(); }
void XLaunchNewImage(const void *, unsigned) { Evento("dashboard"); throw Dashboard(); }
void *VirtualAlloc(void *, unsigned, unsigned, unsigned) { return NULL; }
LONG InterlockedExchange(volatile LONG *p, LONG v) { LONG a=*p; *p=v; return a; }
LONG InterlockedCompareExchange(volatile LONG *p, LONG v, LONG c) { LONG a=*p; if(a==c)*p=v; return a; }
void XSetThreadProcessor(HANDLE h, unsigned n) { if(h)h->hw=n; else hardware=n; }
HANDLE CreateThread(void *, unsigned, DWORD (*fn)(LPVOID), LPVOID arg, unsigned flags, DWORD *id) {
    HANDLE h=new Alca{proximo++, hardware, fn, arg, false, NULL}; alcas.push_back(h);
    if(id)*id=h->id;
    if (!(flags & CREATE_SUSPENDED)) ResumeThread(h);
    return h;
}
DWORD ResumeThread(HANDLE h) {
    DWORD ant=atual, hw=hardware; bool filtro=filtroAtivo;
    atual=h->id; hardware=h->hw; filtroAtivo=false;
    try { h->fn(h->arg); } catch(const ThreadExit &) {}
    h->fim=true; atual=ant; hardware=hw; filtroAtivo=filtro; return 0;
}
DWORD WaitForSingleObject(HANDLE h, unsigned) { return h->fim ? WAIT_OBJECT_0 : WAIT_TIMEOUT; }
int CloseHandle(HANDLE h) { if(h->arquivo) { Exigir(!filtroAtivo && hardware==1,"fechamento na thread errada"); fclose(h->arquivo); h->arquivo=NULL; Evento("fechou relatório"); } return 1; }
HANDLE CreateFile(const char *p, unsigned, unsigned, void *, unsigned, unsigned, void *) {
    Exigir(!filtroAtivo && hardware==1,"E/S no filtro ou fora da thread 1");
    Exigir(strcmp(p,"game:\\crash.txt")==0,"nome público errado");
    Evento("criou relatório");
    if(caso=="disco-falhou")return NULL;
    HANDLE h=new Alca{0,1,NULL,NULL,true,fopen("crash.txt","wb")}; alcas.push_back(h); return h;
}
int WriteFile(HANDLE h, const void *p, DWORD n, DWORD *escritos, void *) {
    Exigir(!filtroAtivo && hardware==1,"escrita na thread errada");
    relatorio.assign((const char *)p,n);
    *escritos=(DWORD)fwrite(p,1,n,h->arquivo);
    if(caso=="escrita-curta")--*escritos;
    return 1;
}
int FlushFileBuffers(HANDLE h) { Evento("flush relatório"); fflush(h->arquivo); return caso!="flush-falhou"; }
void D3DDevice::Clear(unsigned, void *, unsigned, DWORD cor, float, unsigned) {
    if(!gX360Log) Exigir(cor==0xFF010101 || cor==0xFF000000,"cor de fase no release");
}
void D3DDevice::Present(void *, void *, void *, void *) { ++presents; ++agora; }
int IDirect3D9::CreateDevice(unsigned,unsigned,void *,unsigned,D3DPRESENT_PARAMETERS *,D3DDevice **d) { *d=new D3DDevice; return 0; }
IDirect3D9 *Direct3DCreate9(unsigned) { return new IDirect3D9; }
DWORD XInputGetState(unsigned, XINPUT_STATE *s) { static unsigned consultas; s->Gamepad.wButtons=consultas++>=4 ? 1 : 0; return 0; }
void SetUnhandledExceptionFilter(LONG (*)(LPEXCEPTION_POINTERS)) {}
DWORD XShowMessageBoxUI(unsigned,LPCWSTR titulo,LPCWSTR texto,unsigned n,const wchar_t **b,unsigned,unsigned,MESSAGEBOX_RESULT *,XOVERLAPPED *) {
    Exigir(presents>0,"caixa sem apresentação");
    Exigir(relatorio.find("Build: teste-123 diff:limpo")!=std::string::npos,"caixa antes do relatório");
    Exigir(wcscmp(titulo,L"Unexpected error")==0 && n==1 && wcscmp(b[0],L"OK")==0,"título ou botão errado");
    Exigir(wcscmp(texto,L"The game ran into an unexpected error and needs to close. A report was saved to crash.txt in the game folder. Press OK to return to the dashboard.")==0,"texto errado");
    if(caso=="caixa-negada" || (caso=="caixa-ocupada" && recusas++<3))return ERROR_ACCESS_DENIED;
    if(caso=="caixa-falhou")return 8;
    ++caixas; Evento("caixa"); return ERROR_IO_PENDING;
}
int XHasOverlappedIoCompleted(XOVERLAPPED *o) { if(++o->consultas<4)return 0; Evento("OK"); return 1; }
DWORD XGetOverlappedResult(XOVERLAPPED *,DWORD *,int) { return caso=="resultado-falhou" ? 8 : 0; }
#define main TituloMain
#include "main360.cpp"
#undef main
extern "C" {
int gX360Log;
void Diario_Abrir(const char *) { Evento("diário aberto"); }
void Diario_Escrever(const char *f,...) { if(!gX360Log)return; char b[1024]; va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);registro+=b; }
void Diario_Descarregar() {}
void Diario_Fechar() { Evento("diário fechado"); }
void Port_X360FecharSaida() {}
const char *Port_Carimbo360() { return "teste-123 diff:limpo"; }
void Port_MapMemory() {}
int MpegMede_Rodar() { return 0; }
void Ambiente_Carregar(const char *caminho) {
    Exigir(strcmp(caminho,"game:\\environment.txt")==0,"nome do arquivo de ambiente");
    gX360Log=caso=="log-ligado";
}
void Estado_Resumo() {}
const char *volatile gPortStage="starting";
void GsD3d_Soltar() {}
void Port_GsPararJa() { Evento("parar gravação"); }
volatile int gGsThreadEstado, gGsDesenhoEstado;
void GsDraw_DesenhoPararJa() { Evento("parar desenho"); }
void GsDraw_DesenhoCaiu() { Evento("desenho caiu"); }
volatile const char *gGsD3dEtapa="desenho", *gGsD3dEtapaGrav="gravação";
volatile unsigned gGsD3dQuadros, gGsVu1Instr, gGsVu1Prog;
unsigned gGsFrame=180, gPortVBlanks=360;
volatile unsigned gX360Luta=1;
void *Port_GsThread360() { return NULL; }
void *GsDraw_Thread360() { return NULL; }
void Blocos_Iniciar360() {}
int Blocos_PararTodos360() { Evento("cargas"); return 1; }
void Som360_Iniciar() {}
void Som360_Parar() { Evento("som"); }
void Som360_MarcarQueda() {}
void Iso_Iniciar() {}
int Iso_Recusada() { return 0; }
int __real_main(int,char **) {
    CONTEXT c={}; EXCEPTION_RECORD er={0xC0000005,2,{1,0x12345678}};
    c.Iar=0x82001234;c.Lr=0x82005678;c.Gpr1=1;c.Gpr3=0x123456789ABCDEF0ull;
    EXCEPTION_POINTERS p={&er,&c};
    gPortStage="battle";
    if(caso=="desenho")gThreadDesenho=atual;
    if(caso=="audio")hardware=4;
    filtroAtivo=true;
    AoMorrer(&p);
    return 0;
}
}
static size_t Pos(const char *s) { for(size_t i=0;i<eventos.size();++i)if(eventos[i]==s)return i; throw std::runtime_error(std::string("faltou ")+s); }
int main(int argc,char **argv) {
    try {
        Exigir(argc==2,"informe o caso");caso=argv[1];
        setenv("BT3_GS","gpu",1);
        try { TituloMain(); } catch(const Dashboard &) {}
        Exigir(Pos("criou relatório")<Pos("dashboard"),"painel antes do relatório");
        bool disco=caso=="disco-falhou" || caso=="escrita-curta" || caso=="flush-falhou";
        bool caixa= !disco && caso!="caixa-falhou" && caso!="caixa-negada";
        Exigir((caixas==1)==caixa,"resultado da caixa");
        if(caixa) { Exigir(Pos("fechou relatório")<Pos("caixa"),"caixa antes de fechar relatório"); Exigir(Pos("OK")<Pos("cargas") && Pos("OK")<Pos("som"),"desmontagem antes do OK"); Exigir(presents>=4,"main parou durante a caixa"); }
        if(!disco) {
            Exigir(relatorio.find("123456789ABCDEF0")!=std::string::npos,"registro truncado");
            Exigir(relatorio.find("C0000005")!=std::string::npos && relatorio.find("12345678")!=std::string::npos,"exceção ou endereço ausente");
            Exigir(relatorio.find("Phase: battle")!=std::string::npos && relatorio.find("Call chain")!=std::string::npos,"fase ou cadeia ausente");
        }
        if(caso!="log-ligado")Exigir(registro.empty(),"diário ativo sem chave");
        printf("%s: tudo certo (%u apresentações, %u caixas)\n",caso.c_str(),presents,caixas);
        for(const auto &e:eventos)printf("  %s\n",e.c_str());
        for(HANDLE h:alcas)delete h;
        return 0;
    } catch(const std::exception &e) { fprintf(stderr,"FALHA: %s\n",e.what());return 1; }
}
