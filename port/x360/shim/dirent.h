#ifndef X360_SHIM_DIRENT_H
#define X360_SHIM_DIRENT_H
/* listagem de pasta pelo FindFirstFile do 360 (port/x360/src/posix360.c) */
typedef struct x360_dir DIR;
struct dirent { char d_name[256]; };
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);
#endif
