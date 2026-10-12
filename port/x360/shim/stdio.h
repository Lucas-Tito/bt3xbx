#ifndef X360_SHIM_STDIO_H
#define X360_SHIM_STDIO_H
#include <stddef.h>
struct _iobuf { char *_ptr; int _cnt; char *_base; int _flag; int _file; int _charbuf; int _bufsiz; char *_tmpfname; };
typedef struct _iobuf FILE;
/* no 360 a saída de texto do port vai para game:\saida.txt, e os caminhos passam pela tradução de
   port/x360/src/f2/posix360.c ('/' para '\', relativo à pasta do .xex) */
FILE *Port_X360Saida(void);
FILE *Port_X360Fopen(const char *, const char *);
int Port_Printf(const char *, ...);
#define stdout (Port_X360Saida())
#define stderr (Port_X360Saida())
#define fopen Port_X360Fopen
#define printf Port_Printf
FILE *fopen(const char *, const char *);
int fclose(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
int fflush(FILE *);
int fgetc(FILE *);
char *fgets(char *, int, FILE *);
int fputc(int, FILE *);
int fputs(const char *, FILE *);
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int sprintf(char *, const char *, ...);
int sscanf(const char *, const char *, ...);
int _snprintf(char *, size_t, const char *, ...);
#define snprintf _snprintf
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)
#endif
