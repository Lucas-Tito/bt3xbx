/*
 * Ponte entre o C do jogo (gnu89 para GCC) e o cl.exe do XDK. Forçado antes do port_compat.h.
 * Versão de levantamento (F0): só tira o que o cl.exe não entende, para medir o que sobra.
 */
#ifndef MSVC_COMPAT_H
#define MSVC_COMPAT_H

#define __attribute__(x)
#define inline __inline
#define __inline__ __inline
#define __asm__(x)

/* A CRT do Xbox não tem variáveis de ambiente; o port lê chaves por elas (port/x360/src/ambiente.c). */
char *getenv(const char *nome);

#endif
