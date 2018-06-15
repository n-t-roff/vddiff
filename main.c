/*
Copyright (c) 2016-2018, Carsten Kunze <carsten.kunze@arcor.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/

#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <regex.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include "compat.h"
#include "main.h"
#include "y.tab.h"
#include "ui.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "diff.h"
#include "ver.h"
#include "ui2.h"
#include "gq.h"
#include "tc.h"
#include "info.h"
#include "lex.h"

int yyparse(void);

static char *prog;
char *pwd, *rpwd, *arg[2];
size_t pthlen[2];
char syspth[2][PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
struct stat gstat[2];
regex_t fn_re;
short recursive, scan;
short nosingle;
#ifdef TRACE
FILE *debug;
char trcpth[2][PATHSIZ];
#endif
static struct filediff *zipfile[2];
static char *zipdir[2];

static void arg_diff(int);
static void check_args(int, char **);
static void cmp_inodes(void);
static void get_arg(char *, int);
static int read_rc(char *);
static void ttcharoff(void);
static void usage(void);
static void runs2x(void);

const char rc_name[] = "." BIN "rc";
static const char *const usage_txt =
"Usage: %s [-u [<RC file>]] [-BbCcdEefgIiklMmNnoqRrVWXy] [-F <pattern>]\n"
"	[-G <pattern>] [-P <last_wd_file>] [-t <diff_tool>] [-v <view_tool>]\n"
"	[<file or directory 1> [<file or directory 2>]]\n";
static const char *const getopt_arg = "BbCcdEeF:fG:gIiklMmNnoP:qRrt:Vv:WXy";

char *printwd;
bool bmode;
bool qdiff;
bool find_name;
static bool dontdiff;
bool dontcmp;
bool force_exec, force_fs, force_multi;
bool readonly;
bool nofkeys;
static bool run2x;

int
main(int argc, char **argv)
{
	int opt;
	int i;

	prog = *argv;
	setlocale(LC_ALL, "");

#ifdef TRACE
	{
		size_t l;
		static const char * const s = TRACE;

		l = strlen(s);
		memcpy(lbuf, s, l);
#if 1
		snprintf(lbuf + l, BUF_SIZE - l, "%lu",
		    (unsigned long)getuid());
#else
		snprintf(lbuf + l, BUF_SIZE - l, "%lu_%lu",
		    (unsigned long)getuid(), (unsigned long)getpid());
#endif

		if (!(debug = fopen(lbuf, "w"))) {
			printf("fopen \"%s\": %s\n", lbuf, strerror(errno));
			return 1;
		}

		setbuf(debug, NULL);
	}
#endif

#ifdef HAVE_LIBAVLBST
	db_init();
#endif
	if (uz_init()) {
		return 1;
	}

	set_tool(&difftool, strdup(vimdiff), 0);
	set_tool(&viewtool, strdup("less -Q --"), 0);

	if (argc < 2 || argv[1][0] != '-' || argv[1][1] != 'u') {
		if (read_rc(NULL))
			return 1;
	} else {
		argc--; argv++;

		if (argc > 1 && argv[1][0] != '-' &&
		    stat(argv[1], &gstat[0]) == 0 && S_ISREG(gstat[0].st_mode)) {
			if (read_rc(argv[1]))
				return 1;

			argc--; argv++;
		}
	}

	while ((opt = getopt(argc, argv, getopt_arg)) != -1) {
		switch (opt) {
		case 'B':
			dontdiff = TRUE;
			break;
		case 'b':
			color = 0;
			break;

		case 'C':
			dontcmp = TRUE;
			break;

		case 'c':
			real_diff = 1;
			break;
		case 'd':
			set_tool(&difftool, strdup(diffless), 0);
			break;
		case 'E':
			magic = 1;
			break;
		case 'e':
			magic = 0;
			break;

		case 'F':
			if (fn_init(optarg)) {
				return 1;
			}

			break;

		case 'f':
			sorting = FILESFIRST;
			break;

		case 'G':
			if (gq_init(optarg)) {
				return 1;
			}

			break;

		case 'g':
			set_tool(&difftool, strdup("gvim -dR"), 0);
			set_tool(&viewtool, strdup("gvim -R"), 0);
			break;
		case 'I':
			noic = 1; /* don't ignore case */
			break;
		case 'i':
			noic = 0; /* ignore case */
			break;
		case 'k':
			set_tool(&difftool, strdup("tkdiff"), TOOL_BG);
			break;
		case 'l':
			followlinks = 1;
			break;

		case 'M':
			force_multi = TRUE;
			break;

		case 'm':
			sorting = SORTMIXED;
			break;

		case 'N':
			run2x = TRUE;
			break;

		case 'n':
			noequal = 1;
			break;
		case 'o':
			nosingle = 3;
			break;

		case 'P':
			printwd = optarg;
			break;

		case 'q':
			qdiff = TRUE;
			break;

		case 'R':
			readonly = TRUE;
			nofkeys = TRUE;
			break;

		case 'r':
			recursive = 1;
			break;
		case 't':
			set_tool(&difftool, strdup(optarg), 0);
			break;
		case 'V':
			printf(BIN " %s\n\tCompile option(s): "
#if defined HAVE_NCURSESW_CURSES_H
			    "ncursesw"
#elif defined HAVE_NCURSES_CURSES_H
			    "ncurses"
#else
			    "curses"
#endif

#if !defined(NCURSES_MOUSE_VERSION)
			    " (currently no mouse support in " BIN ")"
#elif NCURSES_MOUSE_VERSION < 2
			    " (currently no mouse scroll wheel support"
			    " in " BIN ")"
#endif
			    ", "
#ifdef HAVE_LIBAVLBST
			    "libavlbst"
#else
			    "tsearch"
#endif
			    "\n", version);
			exit(0);
		case 'v':
			set_tool(&viewtool, strdup(optarg), 0);
			break;

		case 'W':
			force_fs = TRUE;
			break;

		case 'X':
			force_exec = TRUE;
			break;

		case 'y':
			twocols = TRUE;
			break;

		default:
			usage();
		}
	}

	if (!run2x) {
		runs2x();
	}

	argc -= optind;
	argv += optind;

	if (argc > 2 || (qdiff && argc != 2)) {
		printf("Two arguments expected\n");
		usage();
		/* not reached */
	}

	if (argc < 2) {
		if (twocols)
			fmode = TRUE;
		else
			bmode = TRUE;
	} else if (dontdiff) { /* Exactly 2 args */
		twocols = TRUE;
		fmode = TRUE;
	}

	inst_sighdl(SIGCHLD, sig_child);
	inst_sighdl(SIGINT , sig_term);
	inst_sighdl(SIGTERM, sig_term);
	ttcharoff();

	if (argc || fmode) {
		check_args(argc, argv);

		if (zipfile[0]) {
			setpthofs(bmode ? 5 : 4, arg[0], zipfile[0]->name);
		}

		if (zipfile[1]) {
			/* 2: don't set vpath[0] */
			setpthofs(6, arg[1], zipfile[1]->name);
		}

		if (!S_ISDIR(gstat[0].st_mode)) {
			if (argc < 2) {
				tool(syspth[0], NULL, 1, 0);
			/* check_args() uses stat(), hence type can't
			 * be symlink */
			} else if (S_ISREG(gstat[0].st_mode)
			        && S_ISREG(gstat[1].st_mode))
			{
				tool("", "", 3, 0);
			} else {
				/* get_arg() already checks for supported
				 * file types */
				arg_diff(1);
			}

			goto rmtmp;

		} else if (argc > 1 && !S_ISDIR(gstat[1].st_mode)) {
			/* get_arg() already checks for supported
			 * file types */
			arg_diff(0);
			goto rmtmp;
		}
	} else { /* bmode only */
		/* Since bmode does not work with paths it need to
		 * resolve the absolute path. */

		if (!getcwd(syspth[0], sizeof syspth[0])) {
			printf("getcwd failed: %s\n",
			    strerror(errno));
			exit(1);
		}

		pthlen[0] = strlen(syspth[0]);
	}

	pwd  = syspth[0] + pthlen[0];
	rpwd = syspth[1] + pthlen[1];
	info_load();
	build_ui();

	if (printwd) {
		wr_last_path();
	}

rmtmp:
	for (i = 0; i < 2; i++) {
		if (zipdir[i]) {
			rmtmpdirs(zipdir[i], TOOL_NOCURS);
		}
#if defined(TRACE)
		else {
			fprintf(debug, "  zipdir[%d]==0\n", i);
		}
#endif
	}

	return 0;
}

/* according POSIX diff(1) */

static void
arg_diff(int i)
{
	char *s, *s2;

	s = strdup(syspth[i ? 0 : 1]);
	s2 = basename(s);

	pthlen[i] = pthcat(syspth[i], pthlen[i], s2);

	if (stat(syspth[i], &gstat[i]) == -1) {
		printf(LOCFMT "stat \"%s\": %s\n" LOCVAR,
		    syspth[i], strerror(errno));
		exit(1);
	}

	cmp_inodes();
	tool("", "", 3, 0);
	free(s);
}

static int
read_rc(char *upath)
{
	char *rc_path;
	int rv = 0;
	extern FILE *yyin;

	if (upath) {
		rc_path = upath;
	} else if (!(rc_path = add_home_pth(rc_name))) {
		return 1;
	}

	if (stat(rc_path, &gstat[0]) == -1) {
		if (errno == ENOENT)
			goto free;
		printf("stat \"%s\": %s\n", rc_path,
		    strerror(errno));
		rv = 1;
		goto free;
	}

	if (!(yyin = fopen(rc_path, "r"))) {
		printf("fopen \"%s\": %s\n", rc_path,
		    strerror(errno));
		rv = 1;
		goto free;
	}

	cur_rc_filenam = rc_path;
	rv = yyparse();

	if (fclose(yyin) == EOF) {
		printf("fclose \"%s\": %s\n", rc_path,
		    strerror(errno));
	}
free:
	if (!upath)
		free(rc_path);

	return rv;
}

char *
add_home_pth(const char *s)
{
	char *h, *m = NULL;
	size_t lh, ls;

#if defined(TRACE)
	fprintf(debug, "->add_home_pth(%s)\n", s);
#endif
	if (!(h = getenv("HOME"))) {
		printf("HOME not set\n");
		goto ret;
	}

	lh = strlen(h);
	ls = strlen(s);
	m = malloc(lh + 1 + ls + 1);
	pthcat(m,  0, h);
	pthcat(m, lh, s);
ret:
#if defined(TRACE)
	fprintf(debug, "<-add_home_pth(%s)\n", m);
#endif
	return m;
}

static void
check_args(int argc, char **argv)
{
	char *s;

	if (argc) {
		s = *argv++;
		argc--;
	} else if (!(s = getenv("PWD"))) {
		printf("PWD not set\n");
		s = ".";
	}

	get_arg(s, 0);

	if (bmode) {
		return;
	}

	if (argc) {
		s = *argv;
	}

	get_arg(s, 1);

	if (!fmode) {
		cmp_inodes();
	}
}

static void
cmp_inodes(void)
{
	if (!fmode &&
	    gstat[0].st_ino == gstat[1].st_ino &&
	    gstat[0].st_dev == gstat[1].st_dev) {
		printf("\"%s\" and \"%s\" are the same file\n",
		    syspth[0], syspth[1]);
		exit(0);
	}
}

static void
get_arg(char *s, int i)
{
	char *s2;
	struct filediff f;

	arg[i] = s;

stat:
	if (stat(s, &gstat[i]) == -1) {
		printf(LOCFMT "stat \"%s\": %s\n" LOCVAR, s, strerror(errno));
		exit(1);
	}

	if (!S_ISDIR(gstat[i].st_mode)) {
		if (!S_ISREG(gstat[i].st_mode)) {
			printf("\"%s\": Unsupported file type\n", s);
			exit(1);
		}

		if (!zipfile[i]) { /* break loop */
			f.name = s;
			f.type[i] = gstat[i].st_mode;

			if ((zipfile[i] = unpack(&f, i ? 2 : 1,
			    &zipdir[i], 4|2|1))) {
				s = zipfile[i]->name;
				goto stat;
			}
		}
	}

	if (fmode && *s != '/') {
		if (!(s2 = realpath(s, NULL))) {
			printf(LOCFMT "realpath \"%s\": %s\n" LOCVAR, s,
			    strerror(errno));
			exit(1);
		}
	} else {
		s2 = s;
	}

	if ((pthlen[i] = strlen(s2)) >= PATHSIZ - 1) {
		printf("Path too long: %s\n", s2);
		exit(1);
	}

	while (pthlen[i] > 1 && s2[pthlen[i] - 1] == '/') {
		s2[--pthlen[i]] = 0;
	}

	memcpy(syspth[i], s2, pthlen[i] + 1);

	if (fmode && *s != '/') {
		free(s2);
	}
}

static void
ttcharoff(void)
{
#ifdef VDSUSP
	struct termios tty;
	cc_t vd;

	if (tcgetattr(STDIN_FILENO, &tty) == -1) {
		printf("tcgetattr(): %s\n", strerror(errno));
		return;
	}

#ifdef _POSIX_VDISABLE
	vd = _POSIX_VDISABLE;
#else
	if ((vd = fpathconf(STDIN_FILENO, _PC_VDISABLE)) == -1) {
		printf("fpathconf(): %s\n", strerror(errno));
		vd = '\377';
	}
#endif

	tty.c_cc[VDSUSP] = vd;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &tty) == -1) {
		printf("tcsetattr(): %s\n", strerror(errno));
		return;
	}
#endif
}

static void
runs2x(void)
{
	FILE *fh;
	static const char cmd[] = "ps -o comm";
	unsigned short n;

	if (!(fh = popen(cmd, "r"))) {
		printf("popen(%s): %s\n", cmd, strerror(errno));
		return;
	}

	for (n = 0; fgets(lbuf, BUF_SIZE, fh); ) {
		info_chomp(lbuf);

		if (!strcmp(basename(lbuf), BIN) && n++) {
			printf(
BIN " is already running in this terminal.  Type \"exit\" or ^D (CTRL-d)\n"
"to return to " BIN " or use option -N to start a new " BIN " instance.\n");
			exit(1);
		}
	}

	if (pclose(fh) == -1) {
		printf("pclose(%s): %s\n", cmd, strerror(errno));
	}
}

static void
usage(void)
{
	printf(usage_txt, prog);
	exit(1);
}

void
sig_term(int sig)
{
	int e;

	e = errno;
#if defined(TRACE)
	fprintf(debug, "->sig_term(%d)\n", sig);
#endif

	/* Change out of tmpdirs before deleting them. */
	if (chdir("/") == -1) {
		printerr(strerror(errno), "chdir \"/\" failed");
	}

	/* if !bmode: remove tmp_dirs */
	while (ui_stack) {
		pop_state(0);
	}

	/* if bmode: remove tmp_dirs */
	uz_exit();

#if defined(TRACE)
	fprintf(debug, "<-sig_term\n");
#endif
	if (sig) {
		endwin();
		exit(1);
	}

	errno = e;
}
