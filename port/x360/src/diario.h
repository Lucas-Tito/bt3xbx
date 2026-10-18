/*
 * Log em arquivo ao lado do .xex. É o único canal de diagnóstico sem xbdm (o OutputDebugString só
 * sai por ele). As linhas vão para um anel em memória e uma thread de baixa prioridade as grava em lote (ver o
 * cabeçalho do diario.c): quem chama Diario_Escrever não espera pelo disco. A garantia de queda é do
 * Diario_Descarregar (síncrono, com FlushFileBuffers), que o filtro de exceção e a saída do jogo chamam, e do flush
 * periódico da thread. Seguro entre threads. Callable de C e de C++.
 */
#ifndef X360_DIARIO_H
#define X360_DIARIO_H

#ifdef __cplusplus
extern "C" {
#endif

void Diario_Abrir(const char *caminho); /* trunca: cada execução começa limpa; liga a thread escritora */
void Diario_Escrever(const char *formato, ...);
void Diario_Fechar(void); /* grava o que falta, com flush, para a thread e fecha */

/* Grava o anel no disco e dá o flush antes de voltar (a regra do CLAUDE.md numa queda). Barato quando o anel está vazio
   (um flush); pode custar de 30 a 40 ms no HD: só para o filtro de exceção, a saída do jogo e os pontos sem volta. */
void Diario_Descarregar(void);

/* O ambiente.txt: sincrono != 0 volta a gravar e dar flush em cada linha, na thread de quem escreve (BT3_LOG_SINCRONO,
   para comparar com o anel); flushMs é a idade máxima de uma linha gravada sem flush (BT3_LOG_FLUSH_MS, 0 = padrão). */
void Diario_Configurar(int sincrono, int flushMs);

/* Uma função chamada pela thread escritora a cada lote (a cada 50 ms): trabalho de disco de outro módulo que não
   deve ficar na thread do jogo (posix360.c a usa para o game:\saida.txt). */
void Diario_Gancho(void (*funcao)(void));

/* 1 se a thread escritora trabalha (o diário não grava na thread de quem escreve); 0 no jeito síncrono. Quem depende
   de a linha chegar ao disco sem a escritora (o vblank do estado360.c) usa isto. */
int Diario_Assincrono(void);

#ifdef __cplusplus
}
#endif

#endif
