#define PATHSIZ  (1024*16)
#define BUF_SIZE PATHSIZ

#define PWD  ((pwd  != lpath && *pwd  == '/') ? pwd  + 1 : pwd )
#define RPWD ((rpwd != rpath && *rpwd == '/') ? rpwd + 1 : rpwd)

#ifndef CTRL
# define CTRL(c) ((c) & 037)
#endif

#ifndef CERASE
# define CERASE 0177
#endif

#ifdef DEBUG
# define LOCFMT "%s %u "
# define LOCVAR , __FILE__, __LINE__
#else
# define LOCFMT
# define LOCVAR
#endif

struct strlst {
	char *str;
	struct strlst *next;
};

extern struct stat stat1, stat2;
extern size_t llen, rlen;
extern char *pwd, *rpwd, *arg[];
extern char lpath[PATHSIZ], rpath[PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
extern regex_t fn_re;
extern short recursive, scan;
extern short nosingle;
#ifdef TRACE
extern FILE *debug;
#endif
extern bool bmode;
extern bool qdiff;
extern bool find_name;
extern bool dontcmp;
extern bool force_exec, force_fs, force_multi;
