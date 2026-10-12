#ifndef X360_SHIM_STDIO_H
#define X360_SHIM_STDIO_H
#include <stddef.h>
struct _iobuf { char *_ptr; int _cnt; char *_base; int _flag; int _file; int _charbuf; int _bufsiz; char *_tmpfname; };
typedef struct _iobuf FILE;
FILE *__iob_func(void); /* a CRT do Xbox (crt/stdio.h): stdin, stdout e stderr são as três primeiras */
#define stdin (&__iob_func()[0])
#define stdout (&__iob_func()[1])
#define stderr (&__iob_func()[2])
FILE *fopen(const char *, const char *);
int fclose(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
int fflush(FILE *);
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
