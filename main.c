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
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdint.h>
#include <time.h>
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
#include "misc.h"
#include "fs.h"
#ifdef TEST
# include "test.h"
#endif

int yyparse(void);

const char *prog;
const char *pwd, *rpwd, *arg[2];
size_t pthlen[2];
char syspth[2][PATHSIZ], lbuf[BUF_SIZE], rbuf[BUF_SIZE];
struct stat gstat[2];
short recursive, scan;
short nosingle;
#ifdef TRACE
FILE *debug;
char trcpth[2][PATHSIZ];
#endif
static struct filediff *zipfile[2];
static char *zipdir[2];

/* Handles case of compare of a file and a directory
 * according POSIX diff(1).
 *
 * i:
 *   0: Dir is 1st argument, file is 2nd
 *   1: File is 1st argument, dir is 2nd */

static void arg_diff(int i);
static void check_args(int, char **);
static void cmp_inodes(void);
static int read_rc(const char *const);
static void ttcharoff(void);
static void runs2x(void);
static void check_opts(void);
static void set_opts(void);
#ifdef DEBUG
static void check_tmp_dir_left(void);
#endif

static const char rc_name[] = "." BIN "rc";
static const char etc_rc_dir[] = "/etc/" BIN "/" BIN "rc";
static const char etc_rc_name[] = "/etc/" BIN "rc";
char *printwd;
bool bmode;
bool qdiff;
static bool dontdiff;
bool dontcmp;
bool force_exec, force_fs, force_multi;
bool readonly;
bool nofkeys;
static bool run2x;
static sigjmp_buf term_jmp_buf;
static volatile sig_atomic_t term_jmp_buf_valid;
static bool lstat_args;
bool summary;
static bool cli_cp;
static bool cli_mv;
bool cli_rm;
bool cli_mode;
bool verbose;
bool dont_overwrite;
bool overwrite_if_old;
bool nodialog;
bool find_dir_name_only;
bool exit_on_error;
#ifdef DEBUG
static bool tmp_dir_check;
static bool skip_tmp_dir_check;
#endif

#if defined(TRACE) && defined(DEBUG)
# define SET_EXIT_DIFF \
    do { \
        fprintf(debug, LOCFMT "<>SET_EXIT_DIFF\n" LOCVAR); \
        if (exit_status == EXIT_SUCCESS) { \
            fprintf(debug, LOCFMT "  change success -> diff\n" LOCVAR); \
            exit_status = EXIT_STATUS_DIFF; \
        } \
    } while (0)
#else
# define SET_EXIT_DIFF \
    do { \
        if (exit_status == EXIT_SUCCESS) \
            exit_status = EXIT_STATUS_DIFF; \
    } while (0)
#endif

int
main(int argc, char **argv)
{
	int opt;
	int i;
    int exit_status = EXIT_SUCCESS;

	prog = *argv;
	setlocale(LC_ALL, "");
#ifdef DEBUG
    if (argc > 1 && !strcmp(argv[1], "-Z")) {
        skip_tmp_dir_check = TRUE;
        argc--; argv++;
    }
    tmp_dir_check = !(argc < 3 ||
                      strcmp(argv[1], "-u") ||
                      strcmp(argv[2], "-S") ||
                      strcmp(argv[3], "-N"));
#endif
    tzset();
#ifdef TRACE
	{
        const char *const s =
#ifdef DEBUG
                tmp_dir_check ? "/dev/null" :
#endif
                                TRACE;
        const size_t l = strlen(s);
		memcpy(lbuf, s, l);
#ifdef DEBUG
        if (!tmp_dir_check)
#endif
            snprintf(lbuf + l, BUF_SIZE - l, "%lu",
                     (unsigned long)getuid());
		if (!(debug = fopen(lbuf, "w"))) {
            fprintf(stderr, "fopen(%s): %s\n", lbuf, strerror(errno));
            return EXIT_STATUS_ERROR;
		}
		setbuf(debug, NULL);
	}
#endif

#ifdef TEST
	test();
    return EXIT_SUCCESS;
#endif

#ifdef HAVE_LIBAVLBST
	db_init();
#endif
	if (uz_init()) {
		return 1;
	}
#if defined(DEBUG)
    if (!tmp_dir_check && !skip_tmp_dir_check)
        check_tmp_dir_left();
#endif

	set_tool(&difftool, strdup(vimdiff), 0);
    set_tool(&viewtool, strdup("less -Q --"), 0);

    if (argc < 2 || argv[1][0] != '-' || argv[1][1] != 'u') {
        if (read_rc(NULL))
            return EXIT_STATUS_ERROR;
    } else {
#if defined(TRACE)
        fprintf(debug, "  main: Option -u\n");
#endif
        if (!argv[1][2]) { /* if 'u' is not directly followed by a character */
            argc--; argv++;

            /* argv[1] is only treated as an argument to -u
             * if it does not start with `-` and it exists
             * and is a regular file */
            if (argc > 1 && argv[1][0] != '-' &&
                    stat(argv[1], &gstat[0]) == 0 && S_ISREG(gstat[0].st_mode))
            {
                if (read_rc(argv[1]))
                    return EXIT_STATUS_ERROR;

                argc--; argv++;
            }
        }
    }

    while ((opt =
            getopt(argc, argv,
                   /* HhJjKwZz */
                   "AaBbCcDdEeF:fG:gIikLlMmNnOoP:pQqRrSsTt:UuVv:WXx:Yy"
#if defined (DEBUG)
                   "Z"
#endif
                   )
            ) != -1)
    {
		switch (opt) {
        case 'A':
            cli_cp = TRUE;
            cli_mode = TRUE;
            break;
        case 'a':
            preserve_all = TRUE;
            break;
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

        case 'D':
            cli_rm = TRUE;
            cli_mode = TRUE;
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
                return EXIT_STATUS_ERROR;
			}

			break;

		case 'f':
			sorting = FILESFIRST;
			break;

		case 'G':
			if (gq_init(optarg)) {
                return EXIT_STATUS_ERROR;
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

        case 'L':
            lstat_args = TRUE;
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
        case 'O':
            dont_overwrite = TRUE;
            break;
		case 'o':
			nosingle = 3;
			break;

		case 'P':
			printwd = optarg;
			break;

        case 'p':
            verbose = TRUE;
            break;
        case 'Q':
            exit_on_error = TRUE;
            break;
		case 'q':
            qdiff = TRUE;
            cli_mode = TRUE;
            nodialog = TRUE;
			break;

		case 'R':
			readonly = TRUE;
			nofkeys = TRUE;
			break;

		case 'r':
			recursive = 1;
			break;

        case 'S':
            bmode = TRUE;
            cli_mode = TRUE;
            nodialog = TRUE;
            break;

        case 's':
            summary = TRUE;
            break;

        case 'T':
            cli_cp = TRUE;
            cli_mv = TRUE;
            cli_mode = TRUE;
            break;

        case 't':
			set_tool(&difftool, strdup(optarg), 0);
			break;
        case 'U':
            overwrite_if_old = TRUE;
            break;
        case 'u':
            /* already evaluated, ignore */
            break;
		case 'V':
            printf(BIN " "VERSION"\n\tCompile option(s): "
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
                "\n");
            return EXIT_SUCCESS;
		case 'v':
			set_tool(&viewtool, strdup(optarg), 0);
			break;

		case 'W':
			force_fs = TRUE;
			break;

		case 'X':
			force_exec = TRUE;
			break;
        case 'x':
            if (find_dir_name_init(optarg)) {
                return EXIT_STATUS_ERROR;
            }
            break;
        case 'Y':
            nodialog = TRUE;
            break;
		case 'y':
			twocols = TRUE;
			break;

		default:
            if (opt != '?')
                fprintf(stderr, "%s: Internal error for option %c\n", prog, opt);
            exit(EXIT_STATUS_ERROR);
		}
	}

    argc -= optind;
    argv += optind;

    check_opts();
    set_opts();

    if (!run2x) {
		runs2x();
	}

    if (cli_rm) {
        if (argc < 1) { /* -D */
            fprintf(stderr, "%s: At least one argument expected\n", prog);
            exit(EXIT_STATUS_ERROR);
            /* not reached */
        }
    } else if (cli_cp) {
        if (argc < 2) { /* -A || -T */
            fprintf(stderr, "%s: At least two arguments expected\n", prog);
            exit(EXIT_STATUS_ERROR);
            /* not reached */
        }
    } else if (cli_mode && bmode) { /* -S */
        if (argc > 1) {
            fprintf(stderr, "%s: None or at most one argument expected\n", prog);
            exit(EXIT_STATUS_ERROR);
            /* not reached */
        }
    } else if (argc > 2 || (qdiff && argc != 2)) {
        fprintf(stderr, "%s: Two arguments expected\n", prog);
        exit(EXIT_STATUS_ERROR);
        /* not reached */
    }
    if (!nodialog && (isatty(STDIN_FILENO)  != 1 ||
                      isatty(STDOUT_FILENO) != 1))
    {
        nodialog = TRUE;
    }

    if (cli_mode) {
    } else if (argc < 2) {
		if (twocols)
			fmode = TRUE;
		else
			bmode = TRUE;
    } else if (dontdiff) { /* Exactly 2 args + option -B */
		twocols = TRUE;
		fmode = TRUE;
	}

	inst_sighdl(SIGCHLD, sig_child);
	inst_sighdl(SIGINT , sig_term);
	inst_sighdl(SIGTERM, sig_term);
	ttcharoff();

    if ((argc || fmode) &&
            /* Process manually, can have more than two arguments.
             * Don't unpack archives (not expected). */
            !(cli_cp || cli_rm))
    {
		check_args(argc, argv);

        if (zipfile[0]) {
            setpthofs(bmode ? 5 : 4, arg[0], zipfile[0]->name);
        }

        if (zipfile[1]) {
            /* 2: don't set vpath[0] */
            setpthofs(6, arg[1], zipfile[1]->name);
        }

        if (S_ISLNK(gstat[0].st_mode) ||
            S_ISLNK(gstat[1].st_mode))
        { /* Possible with -L only */
            if (!S_ISLNK(gstat[0].st_mode) ||
                !S_ISLNK(gstat[1].st_mode))
            {
                printf("Different file type: %s and %s\n",
                       syspth[0], syspth[1]);
                SET_EXIT_DIFF;
            } else {
                char *a = NULL;
                char *b = NULL;

                switch (cmp_symlink(&a, &b)) {
                case 0:
                    if (!qdiff) { /* Don't report equal files with -q */
                        printf("Equal symbolic links %s and %s -> %s\n",
                               syspth[0], syspth[1], a);
                    }
                    break;
                case 1:
                    printf("Symbolic links differ: %s -> %s, %s -> %s\n",
                           syspth[0], a, syspth[1], b);
                    SET_EXIT_DIFF;
                    break;
                default: /* 2 or 3 */
                    exit_status = EXIT_STATUS_ERROR;
                }

                free(b);
                free(a);
            }

            goto rmtmp;

        } else if (!S_ISDIR(gstat[0].st_mode)) {
			if (argc < 2) {
                if (cli_mode && gq_pattern) {
                    const char *base = buf_basename(syspth[0], &pthlen[0]);

                    if (!base) {
                        fputs(oom_msg, stderr);
                    } else {
                        if (file_grep(base))
                            SET_EXIT_DIFF;
                        free(const_cast_ptr(base));
                    }
                } else {
                    tool(syspth[0], NULL, 1, 0);
                }
			/* check_args() uses stat(), hence type can't
			 * be symlink */
			} else if (S_ISREG(gstat[0].st_mode)
			        && S_ISREG(gstat[1].st_mode))
			{
                if (qdiff) {
                    int v = cmp_file(syspth[0], gstat[0].st_size,
                                     syspth[1], gstat[1].st_size, 1);

                    switch (v) {
                    case 0:
                        break;
                    case 1:
                        printf("Files %s and %s differ\n",
                               syspth[0], syspth[1]);
                        SET_EXIT_DIFF;
                        break;
                    default: /* 2 or 3 */
                        exit_status = EXIT_STATUS_ERROR;
                    }
                } else {
                    tool("", "", 3, 0);
                }
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
            fprintf(stderr, "%s: " LOCFMT "getcwd failed: %s\n",
                prog LOCVAR, strerror(errno));
            return EXIT_STATUS_ERROR;
		}

		pthlen[0] = strlen(syspth[0]);
	}

	pwd  = syspth[0] + pthlen[0];
	rpwd = syspth[1] + pthlen[1];
	info_load();

	if (!sigsetjmp(term_jmp_buf,
		/* keep signal masked */
		0))
	{
		term_jmp_buf_valid = 1;

        if (cli_rm) {
            if (do_cli_rm(argc, argv))
                exit_status = EXIT_STATUS_ERROR;
        } else if (cli_cp) {
            if (do_cli_cp(argc, argv, cli_mv ? 1 : 0))
                exit_status = EXIT_STATUS_ERROR;
        } else {
            /* v is return value of qdiff, -SF, -SG, or -Sx */
            const int v = build_ui();
            if (v == 1) {
                if (qdiff)
                    SET_EXIT_DIFF;
                /* else: For pattern match v == 1 means "match found"
                 * -> exit status 0. */
            } else if (v) {
                exit_status = EXIT_STATUS_ERROR;
            }
            /* v == 0 */
            else if (!qdiff && cli_mode && file_pattern) {
                SET_EXIT_DIFF; /* no pattern match */
            }
        }

		/* Ignore signals from now on.
		 * Race does not matter, if signal wins program is exited
		 * before code is executed twice. */
		term_jmp_buf_valid = 0;
	}

    if (cli_mode) {
        if (summary) {
            if (!gq_pattern && find_name)
                printf("%'ld files processed\n", tot_cmp_file_count);
            else if (!gq_pattern && find_dir_name)
                printf("%'ld directories processed\n", tot_cmp_file_count);
            else
                printf("%'ld files (%'jd bytes) %s\n",
                       tot_cmp_file_count,
                       (intmax_t)tot_cmp_byte_count,
                       qdiff ? "compared" :
                       cli_mv ? "moved" :
                       cli_cp ? "copied" :
                       cli_rm ? "removed" :
                       gq_pattern ? "processed" : "");
        }
    } else {
		remove_tmp_dirs();
		bkgd(A_NORMAL);
		erase();
		refresh();
		endwin();
	}

	if (printwd) {
		wr_last_path();
	}

rmtmp:
	for (i = 0; i < 2; i++) {
		if (zipdir[i]) {
            rmtmpdirs(zipdir[i]);
		}
#if defined(TRACE)
		else {
			fprintf(debug, "  zipdir[%d]==0\n", i);
		}
#endif
	}
#if defined(DEBUG)
    if (!(bmode && cli_mode) /* ??? */
            && !skip_tmp_dir_check)
        check_tmp_dir_left();
#endif
    return exit_status;
}

static void
arg_diff(int i)
{
	char *s, *s2;

	s = strdup(syspth[i ? 0 : 1]);
	s2 = basename(s);

	pthlen[i] = pthcat(syspth[i], pthlen[i], s2);

	if (stat(syspth[i], &gstat[i]) == -1) {
        fprintf(stderr, "%s: " LOCFMT "stat \"%s\": %s\n",
                prog LOCVAR, syspth[i], strerror(errno));
        exit(EXIT_STATUS_ERROR);
	}

    cmp_inodes();
	tool("", "", 3, 0);
	free(s);
}

static int
read_rc(const char *const upath)
{
    const char *rc_path;
	int rv = 0;
	extern FILE *yyin;
    bool try_etc_dir = TRUE;
    bool try_etc_name = TRUE;

	if (upath) {
        if (!(rc_path = strdup(upath)))
            return 1;
	} else if (!(rc_path = add_home_pth(rc_name))) {
        return 1;
	}
test_again:
    if (stat(rc_path, &gstat[0]) != -1) {
        cur_rc_dev = gstat[0].st_dev;
        cur_rc_ino = gstat[0].st_ino;
    } else {
        if (errno == ENOENT) {
#if defined(TRACE)
            fprintf(debug, "  read_rc: %s not found\n", rc_path);
#endif
            if (try_etc_dir) {
                try_etc_dir = FALSE;
                free(const_cast_ptr(rc_path));
                if (!(rc_path = strdup(etc_rc_dir)))
                    return 1;
                goto test_again;
            } else if (try_etc_name) {
                try_etc_name = FALSE;
                free(const_cast_ptr(rc_path));
                if (!(rc_path = strdup(etc_rc_name)))
                    return 1;
                goto test_again;
            }
			goto free;
        }
        fprintf(stderr, "%s: " LOCFMT "stat(%s): %s\n",
                prog LOCVAR, rc_path, strerror(errno));
		rv = 1;
		goto free;
	}

	if (!(yyin = fopen(rc_path, "r"))) {
        fprintf(stderr, "%s: " LOCFMT "fopen(%s): %s\n",
                prog LOCVAR, rc_path, strerror(errno));
		rv = 1;
		goto free;
	}

	cur_rc_filenam = rc_path;
	rv = yyparse();

	if (fclose(yyin) == EOF) {
        fprintf(stderr, "%s: " LOCFMT "fclose(%s): %s\n",
                prog LOCVAR, rc_path, strerror(errno));
	}
free:
    free(const_cast_ptr(rc_path));
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
        fprintf(stderr, "%s: HOME not set\n", prog);
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
    const char *s;

	if (argc) {
		s = *argv++;
		argc--;
	} else if (!(s = getenv("PWD"))) {
        fprintf(stderr, "%s: PWD not set\n", prog);
        s = ".";
	}

	get_arg(s, 0);

	if (bmode) {
        /* gstat[1] is tested even in bmode */
        memset(&gstat[1], 0, sizeof(gstat[1]));
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
        exit(EXIT_SUCCESS);
	}
}

void
get_arg(const char *s, int i)
{
    char *s2;
	struct filediff f;
    bool free_path = FALSE;

	arg[i] = s;

stat:
    if (( lstat_args && lstat(s, &gstat[i]) == -1) ||
        (!lstat_args &&  stat(s, &gstat[i]) == -1))
    {
        if (cli_cp && i && errno == ENOENT) {
            /* second argument is to be created */
            gstat[i].st_mode = 0; /* remember that file does not exist */
            goto set_path;
        }

        fprintf(stderr, "%s: " LOCFMT "stat(%s): %s\n",
                prog LOCVAR, s, strerror(errno));
        exit(EXIT_STATUS_ERROR);
	}

    if (!S_ISDIR(gstat[i].st_mode) &&
        !S_ISLNK(gstat[i].st_mode))
    {
		if (!S_ISREG(gstat[i].st_mode)) {
            fprintf(stderr, "%s: " LOCFMT "\"%s\": Unsupported file type\n",
                    prog LOCVAR, s);
            exit(EXIT_STATUS_ERROR);
		}

        if (!cli_cp && !cli_rm && !zipfile[i]) { /* break "goto stat" loop */
			f.name = s;
			f.type[i] = gstat[i].st_mode;

            if ((zipfile[i] = unpack(&f, i ? 2 : 1,
                                     &zipdir[i], 4|2|1)))
            {
                s = zipfile[i]->name;
                goto stat;
            }
		}
	}

set_path:
    if (!cli_cp && !cli_rm && fmode && *s != '/') {
        if ((s2 = realpath(s, NULL))) {
            free_path = TRUE;
        } else {
            fprintf(stderr, "%s: " LOCFMT "realpath(%s): %s\n",
                    prog LOCVAR, s, strerror(errno));
            exit(EXIT_STATUS_ERROR);
		}
	} else {
        s2 = const_cast_ptr(s);
	}

    /* Set path length in pthlen[i]
     * and copy command line argument to syspth[i] */

	if ((pthlen[i] = strlen(s2)) >= PATHSIZ - 1) {
        fprintf(stderr, "%s: " LOCFMT "Path too long: %s\n",
                prog LOCVAR, s2);
        exit(EXIT_STATUS_ERROR);
	}

	while (pthlen[i] > 1 && s2[pthlen[i] - 1] == '/') {
		s2[--pthlen[i]] = 0;
	}

	memcpy(syspth[i], s2, pthlen[i] + 1);

    if (free_path) {
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
            exit(EXIT_STATUS_ERROR);
		}
	}

	if (pclose(fh) == -1) {
		printf("pclose(%s): %s\n", cmd, strerror(errno));
	}
}

static void check_opts(void) {
    bool error = FALSE;
    if (qdiff && dontdiff) {
        fprintf(stderr, "%s: Option -B is incompatible with -q\n", prog);
        error = TRUE;
    }
    if (bmode && cli_mode /* -S */
            && !file_pattern) /* -F || -G */
    {
        fprintf(stderr, "%s: Option -S can be used with -F, -G, or -x only\n", prog);
        error = TRUE;
    }
    if (exit_on_error && !cli_mode) {
        fprintf(stderr, "%s: Option -Q can be used with -q or -S only\n", prog);
        error = TRUE;
    }
    if (error)
        exit(EXIT_STATUS_ERROR);
}

static void set_opts(void)
{
    if (find_dir_name && !find_name && !gq_pattern)
        find_dir_name_only = TRUE;
    if (exit_on_error)
        nodialog = TRUE;
}

void
sig_term(int sig)
{
	(void)sig;

	if (!term_jmp_buf_valid) {
		return;
	}

#if defined(TRACE)
	fprintf(debug, "->sig_term(%d)\n", sig);
#endif
	siglongjmp(term_jmp_buf, 1);
}

void
remove_tmp_dirs(void)
{
#if defined(TRACE)
	fprintf(debug, "->remove_tmp_dirs()\n");
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
	fprintf(debug, "<-remove_tmp_dirs()\n");
#endif
}

#if defined(DEBUG)
static void check_tmp_dir_left(void)
{
    if (snprintf(lbuf, sizeof(lbuf), BIN " -u -S -N -x ." BIN ". %s",
                 tmpdirbase) != -1)
    {
        const int i_ = system(lbuf);
        if (i_ == -1) {
            fprintf(stderr, "system(%s): %s\n", lbuf, strerror(errno));
            exit(EXIT_STATUS_ERROR);
        } else if (!i_) {
            fprintf(stderr, "\"%s\": Unexpected temporary directory found\n",
                    lbuf);
            exit(EXIT_STATUS_ERROR);
        }
    }
}
#endif
