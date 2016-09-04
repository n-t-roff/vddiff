#define PATHSIZ	(1024*16)

#define PWD (*pwd == '/' ? pwd + 1 : pwd)
#define PTHSEP(p, l) \
	if (p[l-1] != '/') \
		p[l++] = '/'

extern struct stat stat1, stat2;
extern size_t llen, rlen;
extern char *pwd, *arg[];
extern char lpath[PATHSIZ], rpath[PATHSIZ], lbuf[PATHSIZ], rbuf[PATHSIZ];
extern short recursive, scan;
