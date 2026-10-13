/*
 * Memory card library (Sony libmc) on PC: slot 1 is a formatted card whose contents are ordinary files in a folder;
 * slot 2 is empty.
 *
 *     saves/card1/BASLUS-21678DBZT3/...     (the folder is `saves` in the current directory, or $BT3_SAVES)
 *
 * The game's paths ("/BASLUS-21678DBZT3/icon.sys") are used as they are below that folder, so a save made here
 * has the same files as one on a console card (icon.sys, the icon, the data file).
 *
 * The library is asynchronous: a call only starts a request and returns 0; sceMcSync reports the request's number
 * and its result. Here every request is carried out at once and sceMcSync hands the stored result over.
 * Results follow libmc (what src/battle/stgm_a_b.c and src/sys/mcflow_a.c test):
 *     sceMcGetInfo   0 = the card seen last time, -1 = a formatted card seen for the first time, -10 = no card;
 *                    type 2 = a PS2 card, free = free clusters of 1 KB, format 1 = formatted
 *     sceMcOpen      a file handle >= 0, or -4 = no such file      (mode: 1 read, 2 write, 0x200 create)
 *     sceMcMkdir     0, or -4 = it exists already
 *     sceMcGetDir    the number of entries that match (the game only asks "is this directory there": 1 or 0)
 *     sceMcRead / sceMcWrite   bytes transferred;   sceMcSeek   the new position;   sceMcClose / sceMcFormat   0
 * The names are the library's addresses in the PS2 executable; the game sources #define the sce names onto them.
 *
 * Big-endian builds (Xbox 360 and the PowerPC test build, PORT_MC_ATOMIC): a power cut never leaves a half-written
 * save (bt3xbx, docs/x360/decisoes.md 22). A file opened for writing is held in memory; on sceMcClose the whole file
 * goes to NAME.tmp (every write, the flush to the disk and the close checked), NAME.tmp is renamed NAME.new (the
 * commit point: a .new is always complete), NAME is deleted and NAME.new renamed NAME (FATX cannot rename over an
 * existing file). Any card path that is used first finishes or undoes an interrupted save: a .new replaces NAME,
 * a .tmp is deleted. A failure is the card's own error (-5), which the game shows as "could not save".
 */
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path) /* Windows: no permission bits */
#endif

#if defined(PORT_BIG_ENDIAN) && !defined(PORT_MC_ATOMIC)
#define PORT_MC_ATOMIC
#endif
#if defined(PORT_MC_ATOMIC) && !defined(PORT_X360) && !defined(_WIN32)
#include <unistd.h> /* fsync, _exit */
#endif

#define MC_FILES 8
#define MC_FREE_CLUSTERS 8000 /* an empty 8 MB card */

static int sLastCmd, sLastResult;
static int sSeen;             /* slot 1 was reported once: later sceMcGetInfo calls answer "same card" */
/* An open file as the game knows it (part of the game's state): open or not, how it was opened, which host file,
   the position. The host's own file object is beside it (not state) and is brought in line when it is used, so a
   frame that is run again after the state was restored finds the files as that state says. */
#include "port_host.h"
extern void Port_StateTouch(void *p, unsigned long n); /* gs/state.c */
static struct { int open, mode; long pos; char path[600]; } sMc[MC_FILES];
PORT_HOST static FILE *sHost[MC_FILES] = {NULL};
PORT_HOST static char sHostPath[MC_FILES][600] = {{0}};

static FILE *host_file(int fd) {
    if (sHost[fd] != NULL && strcmp(sHostPath[fd], sMc[fd].path) != 0) {
        fclose(sHost[fd]);
        sHost[fd] = NULL;
    }
    if (sHost[fd] == NULL) {
        sHost[fd] = fopen(sMc[fd].path, (sMc[fd].mode & 2) ? "r+b" : "rb");
        snprintf(sHostPath[fd], sizeof(sHostPath[fd]), "%s", sMc[fd].path);
    }
    return sHost[fd];
}

static int request(int cmd, int result) {
    sLastCmd = cmd;
    sLastResult = result;
    return 0;
}

#ifdef PORT_MC_ATOMIC
/* The contents of a file open for writing (host memory, like sHost; a state restored while a save is open is not
   supported, and the big-endian builds never restore one). */
PORT_HOST static struct { unsigned char *data; long size, cap; int dirty; } sBuf[MC_FILES];

#ifdef PORT_X360
void Diario_Escrever(const char *formato, ...);
#define MC_LOG(line) Diario_Escrever("%s", line)  /* the console's log, game:\bt3xbx.log */
#define MC_COMMIT_MODE "wbc"                      /* "c": fflush goes on to the disk (FlushFileBuffers) */
#elif defined(_WIN32)
#define MC_LOG(line) fprintf(stderr, "%s\n", line)
#define MC_COMMIT_MODE "wbc"
#else
#define MC_LOG(line) fprintf(stderr, "%s\n", line)
#define MC_COMMIT_MODE "wb"                       /* fsync after the fflush */
#endif

/* The card path of a host path, for the log. */
static const char *mc_name(const char *path) {
    const char *c = strstr(path, "card1");
    return c != NULL ? c + 6 : path;
}

static int mc_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Test only (never set by default): BT3_MC_FALHA=escrita makes the write of the temporary file short, so the save
   fails as a full card would; BT3_MC_FALHA=queda1|queda2|queda3 ends the program (as a power cut) half-way through
   the temporary file, after NAME.new exists, after NAME was deleted; BT3_MC_FALHA=apaga makes the delete of NAME
   fail (and the rename onto it, as the MoveFile of the 360 CRT, which does not replace an existing file). ":SUFFIX"
   limits it to the paths that end with SUFFIX. The log lines are in Portuguese, like the rest of the console log (bt3xbx). */
static int mc_fail(const char *what, const char *path) {
    const char *v = getenv("BT3_MC_FALHA");
    size_t n = strlen(what), lp = strlen(path), ls;
    if (v == NULL || strncmp(v, what, n) != 0 || (v[n] != '\0' && v[n] != ':')) {
        return 0;
    }
    if (v[n] == '\0') {
        return 1;
    }
    ls = strlen(v + n + 1);
    return lp >= ls && strcmp(path + lp - ls, v + n + 1) == 0;
}

static void mc_cut(const char *what, const char *path) {
#if !defined(PORT_X360) && !defined(_WIN32)
    if (mc_fail(what, path)) {
        char line[700];
        snprintf(line, sizeof(line), "mc: queda simulada (%s) gravando %s", what, mc_name(path));
        MC_LOG(line);
        _exit(86);
    }
#else
    (void)what; (void)path;
#endif
}

/* remove(NAME) and rename(NAME.new, NAME), with the delete failure of the test (BT3_MC_FALHA=apaga) */
static int mc_remove(const char *path) {
    return mc_fail("apaga", path) ? -1 : remove(path);
}

static int mc_rename(const char *from, const char *to) {
    return mc_fail("apaga", to) && mc_exists(to) ? -1 : rename(from, to);
}

/* Finishes or undoes a save that a power cut interrupted: NAME.new is complete (it is only named so after the
   commit), NAME.tmp may be anything. */
static void mc_recover(const char *path) {
    char tmp[620], fresh[620], line[700];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    snprintf(fresh, sizeof(fresh), "%s.new", path);
    if (mc_exists(tmp)) {
        remove(tmp);
        snprintf(line, sizeof(line), "mc: %s.tmp de uma gravação interrompida apagado (vale o anterior, se houver)",
                 mc_name(path));
        MC_LOG(line);
    }
    if (mc_exists(fresh)) {
        mc_remove(path);
        snprintf(line, sizeof(line), "mc: %s.new de uma gravação interrompida %s", mc_name(path),
                 mc_rename(fresh, path) == 0 ? "toma o lugar do arquivo" : "não pôde ser renomeado");
        MC_LOG(line);
    }
}

/* The whole file to the disk under its name, or nothing changed: 0, or -5 (libmc: the card refused the write). */
static int mc_commit(const char *path, const unsigned char *data, long size) {
    char tmp[620], fresh[620], line[700];
    const char *step = NULL;
    long n = mc_fail("escrita", path) ? size / 2 : size;
    FILE *f;

    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    snprintf(fresh, sizeof(fresh), "%s.new", path);
    remove(tmp);
    f = fopen(tmp, MC_COMMIT_MODE);
    if (f == NULL) {
        step = "abertura do temporário";
    } else {
        if (n > 0 && fwrite(data, 1, (size_t)n, f) != (size_t)n) {
            step = "escrita";
        } else if (n != size) {
            step = "escrita (teste: BT3_MC_FALHA)";
        }
        if (fflush(f) != 0 && step == NULL) {
            step = "descarga (fflush)";
        }
        mc_cut("queda1", path);
#if !defined(PORT_X360) && !defined(_WIN32)
        if (step == NULL && fsync(fileno(f)) != 0) {
            step = "fsync";
        }
#endif
        if (fclose(f) != 0 && step == NULL) {
            step = "fechamento (fclose)";
        }
    }
    if (step == NULL) {
        remove(fresh);
        if (rename(tmp, fresh) != 0) {
            step = "troca de nome para .new";
        }
    }
    if (step != NULL) {
        remove(tmp);
        snprintf(line, sizeof(line), "mc: gravação de %s (%ld bytes) FALHOU (%s); o arquivo anterior fica",
                 mc_name(path), size, step);
        MC_LOG(line);
        return -5;
    }
    mc_cut("queda2", path);
    mc_remove(path); /* (not checked here: the rename below is, and it fails while the old file is there) */
    mc_cut("queda3", path);
    if (mc_rename(fresh, path) != 0) {
        /* once more; when that fails too, the next use of the path tries again. Success only when the .new took the
           place of the file: an old file that could not be deleted is still there, and it is not the save */
        mc_recover(path);
        if (mc_exists(fresh) || !mc_exists(path)) {
            snprintf(line, sizeof(line), "mc: gravação de %s (%ld bytes) FALHOU na troca de nome; fica como .new",
                     mc_name(path), size);
            MC_LOG(line);
            return -5;
        }
    }
    snprintf(line, sizeof(line), "mc: gravou %s (%ld bytes): ok", mc_name(path), size);
    MC_LOG(line);
    return 0;
}

static void mc_drop(int fd) {
    free(sBuf[fd].data);
    sBuf[fd].data = NULL;
    sBuf[fd].size = sBuf[fd].cap = 0;
    sBuf[fd].dirty = 0;
}

/* sceMcOpen for writing: the file's present contents into memory. A file that the open creates is written at the
   close even when nothing is written into it. Returns the libmc result. */
static int mc_open_write(int fd, const char *path, int mode) {
    FILE *f = fopen(path, "rb");
    long n = 0;

    mc_drop(fd);
    if (f == NULL) {
        if (!(mode & 0x200)) {
            return -4;
        }
        sBuf[fd].dirty = 1;
        return fd;
    }
    if (fseek(f, 0, SEEK_END) == 0) {
        n = ftell(f);
    }
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        sBuf[fd].data = (unsigned char *)malloc((size_t)n);
        if (sBuf[fd].data == NULL || fread(sBuf[fd].data, 1, (size_t)n, f) != (size_t)n) {
            fclose(f);
            mc_drop(fd);
            return -5;
        }
        sBuf[fd].size = sBuf[fd].cap = n;
    }
    fclose(f);
    return fd;
}

static int mc_write(int fd, const void *buf, int size) {
    long end = sMc[fd].pos + size;
    if (size < 0) {
        return -5;
    }
    if (end > sBuf[fd].cap) {
        long cap = sBuf[fd].cap * 2 > end ? sBuf[fd].cap * 2 : end;
        unsigned char *p = (unsigned char *)realloc(sBuf[fd].data, (size_t)cap);
        if (p == NULL) {
            return -5;
        }
        sBuf[fd].data = p;
        sBuf[fd].cap = cap;
    }
    if (sMc[fd].pos > sBuf[fd].size) {
        memset(sBuf[fd].data + sBuf[fd].size, 0, (size_t)(sMc[fd].pos - sBuf[fd].size));
    }
    memcpy(sBuf[fd].data + sMc[fd].pos, buf, (size_t)size);
    sMc[fd].pos = end;
    if (end > sBuf[fd].size) {
        sBuf[fd].size = end;
    }
    sBuf[fd].dirty = 1;
    return size;
}

static int mc_read(int fd, void *buf, int size) {
    long n = sBuf[fd].size - sMc[fd].pos;
    if (n > size) {
        n = size;
    }
    if (n <= 0) {
        return 0;
    }
    Port_StateTouch(buf, (size_t)n);
    memcpy(buf, sBuf[fd].data + sMc[fd].pos, (size_t)n);
    sMc[fd].pos += n;
    return (int)n;
}

static int mc_seek(int fd, int offset, int origin) {
    long pos = (origin == 0 ? 0 : origin == 1 ? sMc[fd].pos : sBuf[fd].size) + offset;
    sMc[fd].pos = pos < 0 ? 0 : pos;
    return (int)sMc[fd].pos;
}

static int mc_close(int fd) {
    int r = sBuf[fd].dirty ? mc_commit(sMc[fd].path, sBuf[fd].data, sBuf[fd].size) : 0;
    mc_drop(fd);
    return r;
}

#define MC_BUFFERED(fd) ((sMc[fd].mode & 2) != 0)
#endif

/* The host path of a card path; creates the card's folder on first use. Returns 0 for slot 2 (no card). */
static int host_path(int port, const char *name, char *out, size_t size) {
    const char *root = getenv("BT3_SAVES") != NULL ? getenv("BT3_SAVES") : "saves";
    char dir[512];

    if (port != 0) {
        return 0;
    }
    mkdir(root, 0755);
    snprintf(dir, sizeof(dir), "%s/card1", root);
    mkdir(dir, 0755);
    while (*name == '/') {
        name++;
    }
    if (strstr(name, "..") != NULL) {
        return 0;
    }
    snprintf(out, size, "%s/%s", dir, name);
#ifdef PORT_MC_ATOMIC
    if (*name != '\0') {
        mc_recover(out);
    }
#endif
    return 1;
}

int func_002A1A48(void) { return 0; } /* sceMcInit */

/* sceMcGetInfo: the outputs are filled when the request completes. */
int func_002A25B8(int port, int slot, int *type, int *freeClusters, int *format) {
    char path[600];
    int card = slot == 0 && host_path(port, "", path, sizeof(path));

    if (type != NULL) { *type = card ? 2 : 0; }
    if (freeClusters != NULL) { *freeClusters = card ? MC_FREE_CLUSTERS : 0; }
    if (format != NULL) { *format = card ? 1 : 0; }
    if (!card) {
        return request(1, -10);
    }
    if (!sSeen) {
        sSeen = 1;
        return request(1, -1);
    }
    return request(1, 0);
}

/* sceMcOpen */
int func_002A1E58(int port, int slot, char *name, int mode) {
    char path[600];
    FILE *f;
    int fd;

    if (slot != 0 || !host_path(port, name, path, sizeof(path))) {
        return request(2, -10);
    }
    for (fd = 0; fd < MC_FILES && sMc[fd].open; fd++) {
    }
    if (fd == MC_FILES) {
        return request(2, -7); /* too many open files */
    }
#ifdef PORT_MC_ATOMIC
    if (mode & 2) {
        int r = mc_open_write(fd, path, mode);
        if (r < 0) {
            return request(2, r);
        }
        if (sHost[fd] != NULL) {
            fclose(sHost[fd]);
            sHost[fd] = NULL;
        }
        sHostPath[fd][0] = '\0';
        sMc[fd].open = 1;
        sMc[fd].mode = mode;
        sMc[fd].pos = 0;
        snprintf(sMc[fd].path, sizeof(sMc[fd].path), "%s", path);
        return request(2, fd);
    }
#endif
    f = fopen(path, (mode & 2) ? "r+b" : "rb");
    if (f == NULL && (mode & 0x200)) {
        f = fopen(path, "w+b");
    }
    if (f == NULL) {
        return request(2, -4);
    }
    if (sHost[fd] != NULL) {
        fclose(sHost[fd]);
    }
    sHost[fd] = f;
    snprintf(sHostPath[fd], sizeof(sHostPath[fd]), "%s", path);
    sMc[fd].open = 1;
    sMc[fd].mode = mode;
    sMc[fd].pos = 0;
    snprintf(sMc[fd].path, sizeof(sMc[fd].path), "%s", path);
    return request(2, fd);
}

/* sceMcMkdir */
int func_002A1F80(int port, int slot, char *name) {
    char path[600];

    if (slot != 0 || !host_path(port, name, path, sizeof(path))) {
        return request(11, -10);
    }
    return request(11, mkdir(path, 0755) == 0 ? 0 : -4);
}

/* sceMcClose */
int func_002A1FB8(int fd) {
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open) {
        return request(3, -4);
    }
#ifdef PORT_MC_ATOMIC
    if (MC_BUFFERED(fd)) {
        sMc[fd].open = 0;
        return request(3, mc_close(fd));
    }
#endif
    if (sHost[fd] != NULL) {
        fclose(sHost[fd]);
        sHost[fd] = NULL;
    }
    sMc[fd].open = 0;
    return request(3, 0);
}

/* sceMcSeek: origin 0 = start, 1 = current, 2 = end */
int func_002A2078(int fd, int offset, int origin) {
    FILE *f;
#ifdef PORT_MC_ATOMIC
    if (fd >= 0 && fd < MC_FILES && sMc[fd].open && MC_BUFFERED(fd)) {
        return request(4, mc_seek(fd, offset, origin));
    }
#endif
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(4, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    fseek(f, offset, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END);
    sMc[fd].pos = ftell(f);
    return request(4, (int)sMc[fd].pos);
}

/* sceMcRead */
int func_002A2208(int fd, void *buf, int size) {
    FILE *f;
    int n;
#ifdef PORT_MC_ATOMIC
    if (fd >= 0 && fd < MC_FILES && sMc[fd].open && MC_BUFFERED(fd)) {
        return request(5, mc_read(fd, buf, size));
    }
#endif
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(5, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    Port_StateTouch(buf, (size_t)size); /* (gs/state.c: file data straight into watched memory) */
    n = (int)fread(buf, 1, (size_t)size, f);
#ifdef PORT_MC_ATOMIC
    {
        char line[700];
        snprintf(line, sizeof(line), "mc: leu %s em %ld: %d de %d bytes", mc_name(sMc[fd].path), sMc[fd].pos, n, size);
        MC_LOG(line);
    }
#endif
    sMc[fd].pos += n;
    return request(5, n);
}

/* sceMcWrite */
int func_002A2320(int fd, void *buf, int size) {
    int n;

    FILE *f;
#ifdef PORT_MC_ATOMIC
    if (fd >= 0 && fd < MC_FILES && sMc[fd].open && MC_BUFFERED(fd)) {
        return request(6, mc_write(fd, buf, size));
    }
#endif
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(6, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    n = (int)fwrite(buf, 1, (size_t)size, f);
    fflush(f);
    sMc[fd].pos += n;
    return request(6, n);
}

/* sceMcGetDir: the number of entries matching `name` ("*" = everything in the root, otherwise one name). The
   table is not filled: nothing in the game reads it. */
int func_002A27A8(int port, int slot, char *name, int mode, int maxent, void *table) {
    char path[600];
    struct stat st;
    int n = 0;

    (void)mode; (void)maxent; (void)table;
    if (slot != 0 || !host_path(port, strcmp(name, "*") == 0 ? "" : name, path, sizeof(path))) {
        return request(13, -10);
    }
    if (strcmp(name, "*") == 0) {
        DIR *d = opendir(path);
        struct dirent *e;
        while (d != NULL && (e = readdir(d)) != NULL) {
            if (e->d_name[0] != '.') {
                n++;
            }
        }
        if (d != NULL) {
            closedir(d);
        }
    } else {
        n = stat(path, &st) == 0 ? 1 : 0;
    }
    return request(13, n);
}

/* sceMcFormat: the folder is always "formatted"; nothing is erased. */
int func_002A2AC8(int port, int slot) {
    (void)slot;
    return request(16, port == 0 ? 0 : -10);
}

/* sceMcSync: 1 = the request has finished (its number and result are returned), -1 = nothing was pending. */
int func_002A2498(int mode, int *cmd, int *result) {
    (void)mode;
    if (sLastCmd == 0) {
        return -1;
    }
    if (cmd != NULL) { *cmd = sLastCmd; }
    if (result != NULL) { *result = sLastResult; }
    sLastCmd = 0;
    return 1;
}
