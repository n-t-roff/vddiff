#define PATHSIZ	(1024*16)

#define PWD (*pwd == '/' ? pwd + 1 : pwd)

#define PTHCAT(p, l, n, a) \
	do { \
		size_t ln = strlen(n); \
		\
		if (p[l-1] != '/') \
			p[l++] = '/'; \
		\
		memcpy(p + l, n, ln + 1); \
		\
		if (a) \
			l += ln; \
	} while (0)

extern struct stat stat1, stat2;
extern size_t llen, rlen;
extern char *pwd, *arg[];
extern char lpath[PATHSIZ], rpath[PATHSIZ], lbuf[PATHSIZ], rbuf[PATHSIZ];
extern short recursive, scan;
