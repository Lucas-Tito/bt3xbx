#ifndef X360_SHIM_SYS_STAT_H
#define X360_SHIM_SYS_STAT_H
struct stat { long st_size; int st_mode; long st_mtime; };
#define S_IFDIR 0x4000
#define S_IFMT 0xF000
#define S_IFREG 0x8000
#define S_ISDIR(m) (((m) & S_IFDIR) != 0)
int stat(const char *, struct stat *);
int mkdir(const char *, int);
#endif
