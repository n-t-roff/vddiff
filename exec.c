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

Uses system() signal code by W. Richard Stevens.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/wait.h>
#include <stdarg.h>
#include "compat.h"
#include "main.h"
#include "ui.h"
#include "ui2.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "diff.h"
#include "fs.h"
#include "tc.h"
#include "info.h"
#include "misc.h"

const char *const vimdiff  = "vim -dR --";
const char *const diffless = "diff -- $1 $2 | less -Q";

struct argvec {
    const char **begin;
    const char **end;
};

static size_t add_path(char *, size_t, const char *const, const char *const);
static struct strlst *addarg(char *);
static void exec_tool(struct tool *, const char *const, const char *const,
	int);
static int shell_char(int);
static int tmpbasecmp(const char *);
static void str2argvec(const char *, struct argvec *);

struct tool difftool;
struct tool viewtool;
char *ishell;
char *nishell;
bool wait_after_exec;
bool exec_nocurs;

/* If tree==3 and only name is given, name is applied to both trees.
 * If tree!=3 but name and rnam are given, both names are applied to the
 * tree side. */

void
tool(const char *const name, const char *const rnam, int tree,
    /* 1: ignore extension */
    /* 2: execute */
    unsigned short mode)
{
    const char *cmd;
	struct tool *tmptool = NULL;

#ifdef TRACE
	TRCPTH;
	fprintf(debug, "->tool(%s,%s,%d,%d) lp(%s) rp(%s)\n",
	    name, rnam, tree, mode, trcpth[0], trcpth[1]);
#endif

	if (str_eq_dotdot(name) || str_eq_dotdot(rnam)) {
		goto ret;
	}

	if (mode & 2) {
        static const char *a[] = { NULL, NULL };

		if (dialog(y_n_txt, NULL, "Really execute %s?",
		    name) != 'y') {
			mode &= ~2;
		} else {
			pthcat(syspth[right_col], pthlen[right_col], name);
			*a = syspth[right_col];
			exec_cmd(a, TOOL_BG|TOOL_NOLIST, NULL, NULL);
			goto ret;
		}
	}

	/* Make diff instead calling a type specific viewer */

	if (tree == 3 ||
	   /* Case: fmode and both files are on same side */
	   (name && rnam) ||
	   (mode & 1))
		goto settool;

	tmptool = check_ext_tool(name);

	if (!tmptool) {
settool:
		tmptool = (tree == 3 || (name && rnam)) && !(mode & 1) ?
		    &difftool : &viewtool;
	}

	if (tmptool->flags & TOOL_SHELL) {
		cmd = exec_mk_cmd(tmptool, name, rnam, tree);
		exec_cmd(&cmd, tmptool->flags | TOOL_TTY, NULL, NULL);
        free(const_cast_ptr(cmd));
	} else {
		exec_tool(tmptool, name, rnam, tree);
	}

ret:
#ifdef TRACE
	fprintf(debug, "<-tool\n");
#endif
	return;
}

#define GRWCMD \
	do { \
		if (csiz - clen < PATHSIZ) { \
			csiz *= 2; \
			cmd = realloc(cmd, csiz); \
		} \
	} while (0)

/* Uses and modifies `lbuf` */

struct tool *
check_ext_tool(const char *name)
{
	size_t l;
	char *cmd;
	int c;
	struct tool *tmptool = NULL;
	short skipped = 0;

	l = strlen(name);
	cmd = lbuf + sizeof lbuf;
	*--cmd = 0;

	while (l) {
        *--cmd = (char)tolower((int)name[--l]);

		if (!skipped && *cmd == '.' &&
		    !str_db_srch(&skipext_db, cmd + 1, NULL)) {
			*cmd = 0;
			skipped = 1;
		}

		if (cmd == lbuf)
			break;
	}

	while ((c = *cmd++)) {
		if (c == '.' && *cmd && (tmptool = db_srch_ext(cmd)))
			break;
	}

	return tmptool;
}

/* Only used for commands which are executed by /bin/sh (not exec() directly */

char *
exec_mk_cmd(struct tool *tmptool, const char *const name,
	const char *const rnam, int tree)
{
	size_t csiz, clen, l;
	char *cmd, *s;
	const char *nam2;
	struct strlst *args;
	int c;

#if defined(TRACE)
	fprintf(debug,
	    "->exec_mk_cmd tool(%s) flags(0x%x) name(%s) rnam(%s) tree(%d)",
	    tmptool->tool, tmptool->flags, name, rnam, tree);
	{
		struct strlst *debug_lst;
		for (debug_lst = tmptool->args; debug_lst;
		    debug_lst = debug_lst->next) {
			fprintf(debug, " arg(%s)", debug_lst->str);
		}
	}
	fputs("\n", debug);
#endif
	nam2 = rnam ? rnam : name;

	csiz = 2 * PATHSIZ;
	cmd = malloc(csiz);
	clen = strlen(s = tmptool->tool);
	memcpy(cmd, s, clen);
	syspth[0][pthlen[0]] = 0; /* in add_path() used without len */

	if (!bmode)
		syspth[1][pthlen[1]] = 0;

	if (tmptool->flags & TOOL_NOARG) {
	} else if ((args = tmptool->args)) {
		do {
			s = args->str;
			GRWCMD;
			c = *s;

			if ((!right_col && c == '1') ||
			    ( right_col && c == '2')) {

				clen = add_path(cmd, clen, syspth[0], name);

			} else if ((!right_col && c == '2') ||
			           ( right_col && c == '1')) {

				clen = add_path(cmd, clen, syspth[1], nam2);
			}

			if ((l = strlen(s) - 1)) {
				GRWCMD;
				memcpy(cmd + clen, s + 1, l);
				clen += l;
			}
		} while ((args = args->next));
	} else {
		if (tree != 3 && name && rnam) {
			cmd[clen++] = ' ';
			GRWCMD;
			clen = add_path(cmd, clen,
			    tree == 1 ? syspth[0] : syspth[1], name);
			cmd[clen++] = ' ';
			GRWCMD;
			clen = add_path(cmd, clen,
			    tree == 1 ? syspth[0] : syspth[1], rnam);
		} else {
			if (tree & 1) {
				cmd[clen++] = ' ';
				GRWCMD;
				clen = add_path(cmd, clen, syspth[0], name);
			}

			if (tree & 2) {
				cmd[clen++] = ' ';
				GRWCMD;
				clen = add_path(cmd, clen, syspth[1], nam2);
			}
		}
	}

	cmd[clen] = 0;
#if defined(TRACE)
	fprintf(debug, "<-exec_mk_cmd (%s)\n", cmd);
#endif
	return cmd;
}

static size_t
add_path(char *cmd, size_t l0, const char *const path, const char *const name)
{
	size_t l;

	if (!bmode && *name != '/') {
		l = shell_quote(lbuf, path, sizeof lbuf);
		memcpy(cmd + l0, lbuf, l);
		l0 += l;
		cmd[l0++] = '/';
	}

	l = shell_quote(lbuf, name, sizeof lbuf);
	memcpy(cmd + l0, lbuf, l);
	l0 += l;
	return l0;
}

void
free_tool(struct tool *t)
{
	struct strlst *p1, *p2;

	free(t->tool);
	p1 = t->args;

	while (p1) {
		p2 = p1->next;
		free(p1);
		p1 = p2;
	}
}

void
set_tool(struct tool *_tool, char *s, tool_flags_t flags)
{
	char *b;
	int c;
	struct strlst **next, *args;
	bool sh = FALSE;

#if defined(TRACE)
	fprintf(debug, "<>set_tool(%s)\n", s);
#endif
	free_tool(_tool);
	_tool->tool = b = s;
	_tool->flags = flags;
	next = &_tool->args;

	while ((c = *s)) {
		if (c == '$') {
			sh = TRUE;

			if (s[1] == '1' || s[1] == '2') {
				*s++ = 0;
				args = addarg(s);
				*next = args;
				next = &args->next;
			}
		} else if (!sh && shell_char(c))
			sh = TRUE;

		s++;
	}

	while (--s >= b && isblank((int)*s))
		*s = 0;

	if (*s == '#' && (s == b || isblank((int)s[-1]))) {
		_tool->flags |= TOOL_NOARG;

		while (--s >= b && isblank((int)*s))
			*s = 0;
	}

	if (sh)
		_tool->flags |= TOOL_SHELL;
}

static struct strlst *
addarg(char *s)
{
	struct strlst *p;

	p = malloc(sizeof(struct strlst));
	p->str = s;
	p->next = NULL;
	return p;
}

static int
shell_char(int c)
{
	static const char s[] = "|&;<>()`\\\"'[#~";
	int sc;
	const char *p;

	for (p = s; (sc = *p++); )
		if (c == sc)
			return 1;

	return 0;
}

static void
exec_tool(struct tool *t, const char *const name, const char *const rnam,
	int tree)
{
	const char *nam2;
	char *s1, *s2;
	int status;
	tool_flags_t flags;
	struct argvec av;

	nam2 = rnam ? rnam : name;

	flags = t->flags | TOOL_TTY;
	str2argvec(t->tool, &av);

	if (!tmpbasecmp(name) || !tmpbasecmp(rnam))
		flags &= ~TOOL_BG;

	s1 = s2 = NULL;

	if ((tree & 1) || (tree != 3 && name && rnam)) {
		if (*name == '/' || bmode) {
			*av.end++ = name;

		} else if (tree & 1) {
			pthcat(syspth[0], pthlen[0], name);
			*av.end++ = s1 = strdup(syspth[0]);

		} else if (tree == 2) {
			pthcat(syspth[1], pthlen[1], name);
			*av.end++ = s1 = strdup(syspth[1]);
		}
	}

	if ((tree & 2) || (tree != 3 && name && rnam)) {
		if (*nam2 == '/' || bmode) {
			*av.end++ = nam2;

		} else if (tree & 2) {
			pthcat(syspth[1], pthlen[1], nam2);
			*av.end++ = s2 = strdup(syspth[1]);

		} else if (tree == 1) {
			pthcat(syspth[0], pthlen[0], nam2);
			*av.end++ = s2 = strdup(syspth[0]);
		}
	}

	*av.end = NULL;
	status = exec_cmd(av.begin, flags, NULL, NULL);
	syspth[0][pthlen[0]] = 0;

	if (tree & 2) {
		syspth[1][pthlen[1]] = 0;
	}

	free(s1);
	free(s2);
    free(const_cast_ptr(*av.begin));
	free(av.begin);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 77 &&
	    !strcmp(t->tool, vimdiff)) {
		set_tool(&difftool, strdup(diffless), 0);
		tool(name, rnam, tree, 0);
	}
}

static void
str2argvec(const char *cmd, struct argvec *av)
{
    int c;
	const char *s;
	char *s2;

	s = cmd;
    size_t o = 0;

	while (1) {
		while ((c = *s++) && !isblank(c));
		if (!c) break;
		while ((c = *s++) &&  isblank(c));
		if (!c) break;
		o++;
	}

	/* tool + any opt (= o) + 2 args + NULL = o + 4 */

    av->begin = av->end = malloc((o + 4) * sizeof(*av));
	*av->end++ = s2 = strdup(cmd);

	while (o) {
        while ((c = *s2++) && !isblank(c))
            ;
        if (!c)
            break;
		s2[-1] = 0;
        while ((c = *s2++) &&  isblank(c))
            ;
        if (!c)
            break;
		*av->end++ = s2 - 1;
	}
}

static int
tmpbasecmp(const char *p)
{
	/* cache */
	static size_t l;
	static char *t;
	static const char * const pf = TMPPREFIX;

	if (!p || *p != '/')
		return 1;

	if (!t) {
		size_t l2, l3;

		l2 = strlen(gettmpdirbase());
		l3 = strlen(pf);
		l = l2 + l3;
		t = malloc(l + 1);
		memcpy(t, gettmpdirbase(), l2);
		memcpy(t + l2, pf, l3 + 1);
	}

	return strncmp(p, t, l);
}

int
exec_cmd(const char *const *const av, tool_flags_t flags, char *path, const char *const msg)
{
	pid_t pid;
	int status = 0;
	/* Signal code (c) W. Richard Stevens */
	struct sigaction intr, quit;
	sigset_t smsk;
	char prompt[] = "Type <ENTER> to continue ";

#if defined(TRACE)
	fprintf(debug, "->exec_cmd(\"%s\"%s%s)\n", *av,
	    flags & TOOL_NOLIST ? " TOOL_NOLIST" : "",
	    flags & TOOL_UDSCR ? " TOOL_UDSCR" : "");
#endif
	if (wstat && !(flags & TOOL_NOCURS)) {
		exec_nocurs = TRUE;
		erase();
		refresh();
		def_prog_mode();

		if (flags & TOOL_TTY) {
			endwin();
		}
	}

	exec_set_sig(&intr, &quit, &smsk);

	switch ((pid = fork())) {
	case -1:
		break;
	case 0:
		exec_res_sig(&intr, &quit, &smsk);

		if (path && chdir(path) == -1) {
			printf("chdir \"%s\": %s\n", path,
			    strerror(errno));
			exit(1);
		}

		if (msg) {
			puts(msg);
		}

        if (flags & TOOL_SHELL) {
            const char *const shell = nishell ? nishell : "sh";
#if defined(TRACE)
            fprintf(debug, "  execlp(%s -c %s)\n", shell, *av);
#endif
            execlp(shell, shell, "-c", *av, NULL);
            /* only seen when vddiff exits later */
            printf("exec %s -c \"%s\": %s\n", shell, *av, strerror(errno));
        } else {
#if defined(TRACE)
            fprintf(debug, "  execvp(%s)\n", *av);
#endif
            execvp(*av, av);
            /* only seen when vddiff exits later */
            printf("exec \"%s\": %s\n", *av, strerror(errno));
        }
        /* Only reached in case exec() fails. */
		write(STDOUT_FILENO, prompt, sizeof prompt);
		fgetc(stdin);
		_exit(77);
	default:
		if (!wait_after_exec /* key 'W' */
		    && (flags & TOOL_BG)
		    /* "ext wait ..." sets BG + WAIT */
		    && !(flags & TOOL_WAIT)) {
			break;
		}

		if (!(flags & TOOL_BG)) {
			pid_t wpid;
			/* did always return "interrupted sys call" on OI */
			while (-1 == (wpid = waitpid(pid, &status, 0)) &&
			    EINTR == errno) {
#if defined(TRACE)
				fprintf(debug,
				    "  waitpid(%d,%d)->%d errno=%s\n",
				    pid, status, wpid, strerror(errno));
#endif
			}
#if defined(TRACE)
			fprintf(debug, "  waitpid(%d,%d)->%d\n",
			    pid, status, wpid);
#endif
		}

		if (wait_after_exec /* key 'W' */
		    || (flags & TOOL_WAIT)) {
			write(STDOUT_FILENO, prompt, sizeof prompt);
			fgetc(stdin);
		}
	}

	if (wstat && !(flags & TOOL_NOCURS)) {
		doupdate();
	}

	if (pid == -1) {
		printerr(strerror(errno), "fork");
	}

	exec_res_sig(&intr, &quit, &smsk);

	if (wstat && !(flags & TOOL_NOCURS)) {
		if (!(flags & TOOL_UDSCR)) {
			rebuild_scr();
		} else if (!(flags & TOOL_NOLIST)) {
			disp_fmode();
		}

		exec_nocurs = TRUE;
	}

#if defined(TRACE)
	fprintf(debug, "<-exec_cmd pid=%d\n", (int)pid);
#endif
	return status;
}

void
exec_set_sig(struct sigaction *intr, struct sigaction *quit, sigset_t *smsk)
{
	struct sigaction ign;
	sigset_t cmsk;

	ign.sa_handler = SIG_IGN;

	if (sigemptyset(&ign.sa_mask) == -1) {
		printerr(strerror(errno), "sigemptyset");
	}

	ign.sa_flags = 0;

	if (sigaction(SIGINT, &ign, intr) == -1) {
		printerr(strerror(errno), "sigaction SIGINT");
	}

	if (sigaction(SIGQUIT, &ign, quit) == -1) {
		printerr(strerror(errno), "sigaction SIGQUIT");
	}

	if (sigemptyset(&cmsk) == -1) {
		printerr(strerror(errno), "sigemptyset");
	}

	if (sigaddset(&cmsk, SIGCHLD) == -1) {
		printerr(strerror(errno), "sigaddset SIGCHLD");
	}

	if (sigprocmask(SIG_BLOCK, &cmsk, smsk) == -1) {
		printerr(strerror(errno), "sigprocmask SIG_BLOCK");
	}
}

void
exec_res_sig(struct sigaction *intr, struct sigaction *quit, sigset_t *smsk)
{
	if (sigaction(SIGINT, intr, NULL) == -1) {
		printerr(strerror(errno), "sigaction SIGINT");
	}

	if (sigaction(SIGQUIT, quit, NULL) == -1) {
		printerr(strerror(errno), "sigaction SIGQUIT");
	}

	if (sigprocmask(SIG_SETMASK, smsk, NULL) == -1) {
		printerr(strerror(errno), "sigprocmask SIG_SETMASK");
	}
}

size_t
shell_quote(char *to, const char *from, size_t siz)
{
	int c;
	size_t len = 0;

	siz--; /* for last \0 byte */

	while (len < siz && (c = *from++)) {
		switch (c) {
		case '|':
		case '&':
		case ';':
		case '<': case '>':
		case '(': case ')':
		case '$':
		case '`':
		case '\\':
		case '"':
		case '\'':
		case ' ':
		case '\t':
		case '\n':
			*to++ = '\\';
			len++;

			if (len >= siz)
				break;

			/* fall through */
		default:
            *to++ = (char)c;
			len++;
		}
	}

	*to = 0;
	return len;
}

void
open_sh(int tree)
{
	struct filediff *f = NULL;
	char *s;
	struct passwd *pw;
	struct argvec av;
	char *cmd;

	/* Provide 's'/"sl"/"sr" functionallity in diff mode */
	if (!bmode && !fmode && db_num[0]) {
		f = db_list[0][top_idx[0] + curs[0]];

		if (tree == 3 && f->type[0] && f->type[1]) {
			/* Return to allow input of 'l' or 'r' */
			return;
		}
	}

	if (bmode) {
		s = syspth[1];
	} else if (tree == 2 ||
	    ((tree & 2) &&
	     ((fmode && right_col) ||
	      (!fmode && db_num[0] && f->type[1])))) {
		syspth[1][pthlen[1]] = 0;
		s = syspth[1];
	} else {
		syspth[0][pthlen[0]] = 0;
		s = syspth[0];
	}

	if (ishell) {
		cmd = ishell;
	} else {
		if (!(pw = getpwuid(getuid()))) {
			printerr(strerror(errno), "getpwuid failed");
			return;
		}

		cmd = pw->pw_shell;
	}

	str2argvec(cmd, &av);
	*av.end = NULL;
	exec_cmd(av.begin, TOOL_NOLIST | TOOL_TTY, s,
	    "\nType \"exit\" or '^D' to return to " BIN ".\n");
    free(const_cast_ptr(*av.begin));
	free(av.begin);

	/* exec_cmd() did likely create or delete files */
	rebuild_db(0);
}

void
inst_sighdl(int sig, void (*hdl)(int))
{
	struct sigaction act;

	act.sa_handler = hdl;
	sigemptyset(&act.sa_mask);
	act.sa_flags =
#ifdef SA_RESTART
	    SA_RESTART;
#else
	    0;
#endif

	if (sigaction(sig, &act, NULL) == -1) {
		printf("sigaction %d: %s\n", sig, strerror(errno));
		exit(1);
	}
}

/* Signal handler */

void
sig_child(int signo)
{
	int e;
	int st;
	pid_t p;

	(void)signo;
	e = errno;

	while (1) {
		switch ((p = waitpid(-1, &st, WNOHANG))) {
		case -1:
			/* Normally this test is not necessary. waitpid should
			 * return 0 if there are no further child processes.
			 * This is maybe a Linux bug */
#if defined(TRACE)
			if (errno != ECHILD) {
				fprintf(debug, "  sig_child(): waitpid(): %s\n",
					strerror(errno));
			}
#endif
			/* fall through */
		case 0:
			goto last;

		default:
			if (p == info_pid) {
				info_pid = 0;
			}
#if defined(TRACE)
			fprintf(debug, "  sig_child pid=%d\n", (int)p);
#endif
		}
	}
last:

	errno = e;
}
