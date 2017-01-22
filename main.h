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
# define TRCPTH \
	do { \
		memcpy(trcpth[0], syspth[0], pthlen[0]); \
		trcpth[0][pthlen[0]] = 0; \
		memcpy(trcpth[1], syspth[1], pthlen[1]); \
		trcpth[1][pthlen[1]] = 0; \
	} while (0)
#endif

struct strlst {
	char *str;
	struct strlst *next;
};

extern const char rc_name[];
extern struct stat stat1, stat2;
extern size_t pthlen[2];
extern char *pwd, *rpwd, *arg[];
extern char syspth[2][PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
extern regex_t fn_re;
extern short recursive, scan;
extern short nosingle;
#ifdef TRACE
extern FILE *debug;
extern char trcpth[2][PATHSIZ];
#endif
extern bool bmode;
extern bool qdiff;
extern bool find_name;
extern bool dontcmp;
extern bool force_exec, force_fs, force_multi;
extern bool readonly;
extern bool nofkeys;

char *add_home_pth(const char *);
void sig_term(int);
