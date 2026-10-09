/*
 * Log em arquivo ao lado do .xex. É o único canal de diagnóstico sem xbdm (o OutputDebugString só
 * sai por ele). Cada linha vai ao disco com FlushFileBuffers: num crash de verdade, o que ficou no
 * cache do sistema de arquivos se perde, e a última linha é a que mais importa.
 * Seguro entre threads. Callable de C e de C++.
 */
#ifndef X360_DIARIO_H
#define X360_DIARIO_H

#ifdef __cplusplus
extern "C" {
#endif

void Diario_Abrir(const char *caminho); /* trunca: cada execução começa limpa */
void Diario_Escrever(const char *formato, ...);
void Diario_Fechar(void);

#ifdef __cplusplus
}
#endif

#endif
