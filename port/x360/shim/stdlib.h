#ifndef X360_SHIM_STDLIB_H
#define X360_SHIM_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
void exit(int);
void abort(void);
int abs(int);
int atoi(const char *);
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
char *getenv(const char *);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int rand(void);
void srand(unsigned int);
#endif
