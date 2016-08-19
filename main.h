#define PATHSIZ	(1024*16)

enum sorting { FILESFIRST, DIRSFIRST, SORTMIXED };

extern enum sorting sorting;
extern char *difftool;
extern struct stat stat1, stat2;
extern size_t llen, rlen;
extern char lpath[PATHSIZ], rpath[PATHSIZ], lbuf[PATHSIZ], rbuf[PATHSIZ];
