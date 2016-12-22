#define PATHSIZ  (1024*16)
#define BUF_SIZE PATHSIZ

#define PWD  ((pwd  != syspth[0] && *pwd  == '/') ? pwd  + 1 : pwd )
#define RPWD ((rpwd != syspth[1] && *rpwd == '/') ? rpwd + 1 : rpwd)

#ifndef CTRL
# define CTRL(c) ((c) & 037)
#endif

#ifndef CERASE
# define CERASE 0177
#endif

#ifdef DEBUG
# define LOCFMT "%s %u: "
# define LOCVAR , __FILE__, __LINE__
#else
# define LOCFMT
# define LOCVAR
#endif

#ifdef TRACE
# define TPTH \
	do { \
		memcpy(tlpth, syspth[0], pthlen[0]); tlpth[pthlen[0]] = 0; \
		memcpy(trpth, syspth[1], pthlen[1]); trpth[pthlen[1]] = 0; \
	} while (0)
#endif

struct strlst {
	char *str;
	struct strlst *next;
};

extern struct stat stat1, stat2;
extern size_t pthlen[2];
extern char *pwd, *rpwd, *arg[];
extern char syspth[2][PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
extern regex_t fn_re;
extern short recursive, scan;
extern short nosingle;
#ifdef TRACE
extern FILE *debug;
extern char tlpth[PATHSIZ], trpth[PATHSIZ];
#endif
extern bool bmode;
extern bool qdiff;
extern bool find_name;
extern bool dontcmp;
extern bool force_exec, force_fs, force_multi;
extern bool readonly;
extern bool nofkeys;

char *add_home_pth(char *);
void sig_term(int);
