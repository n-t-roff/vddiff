/*
Copyright (c) 2016, Carsten Kunze <carsten.kunze@arcor.de>

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

int yyparse(void);

static char *prog;
char *pwd, *rpwd, *arg[2];
size_t pthlen[2];
char syspth[2][PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
struct stat stat1, stat2;
regex_t fn_re;
short recursive, scan;
short nosingle;
#ifdef TRACE
FILE *debug;
char tlpth[PATHSIZ], trpth[PATHSIZ];
#endif

static void check_args(int, char **);
static int read_rc(char *);
static void ttcharoff(void);
static void usage(void);

static char *usage_txt =
"Usage: %s [-u [<RC file>]] [-BbCcdEefgIiklMmnoqRrVWXy] [-F <pattern>]\n"
"	[-G <pattern>] [-t <diff_tool>] [-v <view_tool>] [<directory_1>\n"
"	[<directory_2>]]\n";
static char *getopt_arg = "BbCcdEeF:fG:gIiklMmnoqRrt:Vv:WXy";

bool bmode;
bool qdiff;
bool find_name;
static bool dontdiff;
bool dontcmp;
bool force_exec, force_fs, force_multi;
bool readonly;
bool nofkeys;

int
main(int argc, char **argv)
{
	int opt;

	prog = *argv;
	setlocale(LC_ALL, "");

#ifdef TRACE
	{
		size_t l;
		static const char * const s = TRACE;

		l = strlen(s);
		memcpy(lbuf, s, l);
		snprintf(lbuf + l, BUF_SIZE - l, "%lu",
		    (unsigned long)getuid());

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
	set_tool(&viewtool, strdup("less --"), 0);

	if (argc < 2 || argv[1][0] != '-' || argv[1][1] != 'u') {
		if (read_rc(NULL))
			return 1;
	} else {
		argc--; argv++;

		if (argc > 1 && argv[1][0] != '-' &&
		    stat(argv[1], &stat1) == 0 && S_ISREG(stat1.st_mode)) {
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

		case 'n':
			noequal = 1;
			break;
		case 'o':
			nosingle = 1;
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
			printf("%s %s\n\tCompile option(s): "
#if defined HAVE_NCURSESW_CURSES_H
			    "ncursesw"
#elif defined HAVE_NCURSES_CURSES_H
			    "ncurses"
#else
			    "curses"
#endif

#if !defined(NCURSES_MOUSE_VERSION)
			    " (currently no mouse support in vddiff)"
#elif NCURSES_MOUSE_VERSION < 2
			    " (currently no mouse scroll wheel support"
			    " in vddiff)"
#endif
			    ", "
#ifdef HAVE_LIBAVLBST
			    "libavlbst"
#else
			    "tsearch"
#endif
			    "\n", prog, version);
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

	if (argc || fmode) {
		check_args(argc, argv);
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
	inst_sighdl(SIGCHLD, sig_child);
	inst_sighdl(SIGINT , sig_term);
	inst_sighdl(SIGTERM, sig_term);
	ttcharoff();
	build_ui();
	return 0;
}

static int
read_rc(char *upath)
{
	static char rc_name[] = ".vddiffrc";
	char *rc_path;
	int rv = 0;
	extern FILE *yyin;

	if (upath) {
		rc_path = upath;
	} else if (!(rc_path = add_home_pth(rc_name))) {
		return 1;
	}

	if (stat(rc_path, &stat1) == -1) {
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
add_home_pth(char *s)
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
	char *s, *s2;

	if (argc) {
		s = *argv++;
		argc--;
	} else if (!(s = getenv("PWD"))) {
		printf("PWD not set\n");
		s = ".";
	}

	arg[0] = s;

	if (stat(s, &stat1) == -1) {
		printf(LOCFMT "stat \"%s\": %s\n" LOCVAR, s, strerror(errno));
		exit(1);
	}

	if (!S_ISDIR(stat1.st_mode)) {
		printf("\"%s\" is not a directory\n", s);
		exit(1);
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

	if ((pthlen[0] = strlen(s2)) >= PATHSIZ - 1) {
		printf("Path too long: %s\n", s2);
		exit(1);
	}

	while (pthlen[0] > 1 && s2[pthlen[0] - 1] == '/') {
		s2[--pthlen[0]] = 0;
	}

	memcpy(syspth[0], s2, pthlen[0] + 1);

	if (fmode && *s != '/')
		free(s2);

	if (bmode)
		return;

	if (argc)
		s = *argv;

	arg[1] = s;

	if (stat(s, &stat2) == -1) {
		printf(LOCFMT "stat \"%s\": %s\n" LOCVAR, s, strerror(errno));
		exit(1);
	}

	if (!fmode &&
	    stat1.st_ino == stat2.st_ino &&
	    stat1.st_dev == stat2.st_dev) {
		printf("\"%s\" and \"%s\" are the same directory\n",
		    syspth[0], s);
		exit(0);
	}

	if (!S_ISDIR(stat2.st_mode)) {
		printf("\"%s\" is not a directory\n", s);
		exit(1);
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

	if ((pthlen[1] = strlen(s2)) >= PATHSIZ - 1) {
		printf("Path too long: %s\n", s2);
		exit(1);
	}

	while (pthlen[1] > 1 && s2[pthlen[1] - 1] == '/') {
		s2[--pthlen[1]] = 0;
	}

	memcpy(syspth[1], s2, pthlen[1] + 1);

	if (fmode && *s != '/')
		free(s2);
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
usage(void)
{
	printf(usage_txt, prog);
	exit(1);
}

void
sig_term(int sig)
{
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
}
