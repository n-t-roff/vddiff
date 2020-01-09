#ifndef MAIN_H
#define MAIN_H

#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include "compat.h"

#define EXIT_STATUS_DIFF  1
#define EXIT_STATUS_ERROR 2

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
# define LOCFMT "%s:%u: "
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

extern const char *prog;
extern struct stat gstat[2];
extern size_t pthlen[2];
extern const char *pwd, *rpwd, *arg[];
extern char syspth[2][PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
extern char *printwd;
extern struct MoveCursorToFile *moveCursorToFileInst;
extern short recursive, scan;
extern short nosingle;
#ifdef TRACE
extern FILE *debug;
extern char trcpth[2][PATHSIZ];
#endif
extern bool bmode;
extern bool qdiff;
extern bool dontcmp;
extern bool force_exec, force_fs, force_multi;
extern bool readonly;
extern bool nofkeys;
extern bool summary;
extern bool verbose;
extern bool cli_rm;
extern bool cli_mode;
extern bool dont_overwrite; /* -O: Like `cp -n` */
extern bool overwrite_if_old; /* -U: Like `cp -u` */
extern bool nodialog;
extern bool find_dir_name_only;
extern bool exit_on_error;
extern bool moveCursorToFile;

char *add_home_pth(const char *);
/*
 * Input:
 *   Global:
 *     zipfile[i]
 *     zipdir[i]
 *     lstat_args
 *     fmode
 *   Arguments:
 *     s: CLI argument
 *     i: 0: Left side, 1: Right Side
 *
 * Output:
 *   Global:
 *     arg[i]
 *     gstat[i]
 *     pthlen[i]
 *     syspth[i]
 *
 * Terminates using exit() in case of an error.
 */
void get_arg(const char *s, int i);
void sig_term(int);
void remove_tmp_dirs(void);

#endif /* MAIN_H */
