#ifndef X360_SHIM_STDLIB_H
#define X360_SHIM_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
/* no 360 o fim do jogo encerra só a thread dele (port/x360/src/f2/main360.cpp) */
void Port_Sair(int);
void Port_Abortar(void);
#define exit Port_Sair
#define abort Port_Abortar
int abs(int);
int atoi(const char *);
double atof(const char *); /* (o gs_draw.c: sem a declaração o cl assume int, e o valor vem do registrador errado) */
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
char *getenv(const char *);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int rand(void);
void srand(unsigned int);
#endif
