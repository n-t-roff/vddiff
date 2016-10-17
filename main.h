#define PATHSIZ  (1024*16)
#define BUF_SIZE PATHSIZ

#define PWD  (*pwd  == '/' ? pwd  + 1 : pwd )
#define RPWD (*rpwd == '/' ? rpwd + 1 : rpwd)
#ifndef CTRL
# define CTRL(c) ((c) & 037)
#endif
#ifndef CERASE
# define CERASE 0177
#endif

struct strlst {
	char *str;
	struct strlst *next;
};

extern struct stat stat1, stat2;
extern size_t llen, rlen;
extern char *pwd, *rpwd, *arg[];
extern char lpath[PATHSIZ], rpath[PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
extern short recursive, scan;
extern short bmode;
extern short nosingle;
#ifdef TRACE
extern FILE *debug;
#endif
