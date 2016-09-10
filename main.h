#define PATHSIZ	(1024*16)

#define PWD  (*pwd  == '/' ? pwd  + 1 : pwd )
#define RPWD (*rpwd == '/' ? rpwd + 1 : rpwd)

extern struct stat stat1, stat2;
extern size_t llen, rlen;
extern char *pwd, *rpwd, *arg[];
extern char lpath[PATHSIZ], rpath[PATHSIZ], lbuf[BUFSIZ], rbuf[BUFSIZ];
extern short recursive, scan;
