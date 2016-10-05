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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/wait.h>
#include <signal.h>
#include "compat.h"
#include "main.h"
#include "ui.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "diff.h"

const char *const vimdiff  = "vim -dR";
const char *const diffless = "diff $1 $2 | less";

static size_t add_path(char *, size_t, char *, char *);
static void exec_tool(struct tool *, char *, char *, int);
static void sig_child(int);

struct tool difftool;
struct tool viewtool;

void
tool(char *name, char *rnam, int tree, int ign_ext)
{
	size_t ln, rn, l0, l1, l2, l;
	char **toolp, *cmd;
	struct tool *tmptool = NULL;
	short skipped = 0;
	int c;

	l = ln = strlen(name);
	cmd = lbuf + sizeof lbuf;
	*--cmd = 0;

	if (tree == 3 || ign_ext)
		goto settool;

	while (l) {
		*--cmd = tolower((int)name[--l]);

		if (!skipped && *cmd == '.' &&
		    !str_db_srch(&skipext_db, cmd + 1
#ifdef HAVE_LIBAVLBST
		    , NULL
#endif
		    )) {
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

	if (!tmptool)
settool:
		tmptool = tree == 3 && !ign_ext ? &difftool : &viewtool;

	toolp = tmptool->tool;

	if (!toolp[1]) {
		exec_tool(tmptool, name, rnam, tree);
		return;
	}

	rn = rnam ? strlen(rnam) : ln;
	l0 = strlen(*toolp);
	l1 = toolp[1] ? strlen(toolp[1]) : 0;
	l2 = toolp[2] ? strlen(toolp[2]) : 0;

	l = l0 + (llen + ln + rlen + rn) * 2 + 3 + l1 + l2;
	cmd = malloc(l);
	memcpy(cmd, *toolp, l0);

	lpath[llen] = 0; /* in add_path() used without len */

	if (!bmode)
		rpath[rlen] = 0;

	if (!l1 || tree != 3) {
		if (tree & 1)
			l0 = add_path(cmd, l0, lpath, name);
		if (tree & 2)
			l0 = add_path(cmd, l0, rpath,
			    rnam ? rnam : name);

		if (l1 && tree != 3) {
			memcpy(cmd + l0, toolp[1] + 1, --l1);
			l0 += l1;
		}
	} else {
		switch (*toolp[1]) {
		case '1':
			l0 = add_path(cmd, l0, lpath, name);
			break;
		case '2':
			l0 = add_path(cmd, l0, rpath,
			    rnam ? rnam : name);
			break;
		}

		memcpy(cmd + l0, toolp[1] + 1, --l1);
		l0 += l1;

		if (l2) {
			switch (*toolp[2]) {
			case '1':
				l0 = add_path(cmd, l0, lpath, name);
				break;
			case '2':
				l0 = add_path(cmd, l0, rpath,
				    rnam ? rnam : name);
				break;
			}

			memcpy(cmd + l0, toolp[2] + 1, --l2);
			l0 += l2;
		}
	}

	cmd[l0] = 0;
	sh_cmd(cmd, 0);
	free(cmd);
}

void
sh_cmd(char *cmd, int wait)
{
	erase();
	refresh();
	def_prog_mode();
	endwin();
	system(cmd);

	if (wait) {
		char s[] = "Type <ENTER> to continue ";
		write(STDOUT_FILENO, s, sizeof s);
		fgetc(stdin);
	}

	reset_prog_mode();
	refresh();
	disp_list();
}

static size_t
add_path(char *cmd, size_t l0, char *path, char *name)
{
	size_t l;

	if (*name != '/') {
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
set_tool(struct tool *tool, char *s, int bg)
{
	char **t = tool->tool;
	tool->bg = bg;
	free(*t);
	*t = s;
	t[1] = NULL;
	t[2] = NULL;

	while (*s) {
		if (*s == '$' && (s[1] == '1' || s[1] == '2')) {
			*s++ = 0;

			if (!t[1])
				t[1] = s;
			else if (!t[2])
				t[2] = s;
		}
		s++;
	}
}

static void
exec_tool(struct tool *t, char *name, char *rnam, int tree)
{
	int o, c;
	char *s, **a, **av;
	int status;
	int bg = t->bg;

	if (!rnam)
		rnam = name;

	s = *t->tool;
	o = 0;
	while (1) {
		while ((c = *s++) && !isblank(c));
		if (!c) break;
		while ((c = *s++) &&  isblank(c));
		if (!c) break;
		o++;
	}

	a = av = malloc((1 + o + (tree == 3 ? 2 : 1) + 1) * sizeof(*av));

	if (!o) {
		*a++ = *t->tool;
	} else {
		*a++ = s = strdup(*t->tool);

		while (1) {
			while ((c = *s++) && !isblank(c));
			if (!c) break;
			s[-1] = 0;
			while ((c = *s++) &&  isblank(c));
			if (!c) break;
			*a++ = s - 1;
		}
	}

	if (tree & 1) {
		if (bmode || *name == '/') {
			*a++ = name;
			bg = 0;
		} else {
			pthcat(lpath, llen, name);
			*a++ = lpath;
		}
	}

	if (tree & 2) {
		if (bmode || *rnam == '/') {
			*a++ = rnam;
			bg = 0;
		} else {
			pthcat(rpath, rlen, rnam);
			*a++ = rpath;
		}
	}

	*a = NULL;
	status = exec_cmd(av, bg, NULL, NULL, TRUE);

	if (o)
		free(*av);

	free(av);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 77 &&
	    !strcmp(*t->tool, vimdiff)) {
		set_tool(&difftool, strdup(diffless), 0);
		tool(name, rnam, tree, 0);
	}
}

int
exec_cmd(char **av, int bg, char *path, char *msg, bool tty)
{
	pid_t pid;
	int status = 0;

	erase();
	refresh();
	def_prog_mode();

	if (tty)
		endwin();

	switch ((pid = fork())) {
	case -1:
		break;
	case 0:
		if (path && chdir(path) == -1) {
			printf("chdir \"%s\" failed: %s\n", path,
			    strerror(errno));
			exit(1);
		}

		if (msg)
			puts(msg);

		if (execvp(*av, av) == -1) {
			/* only seen when vddiff exits later */
			printf("exec \"%s\" failed: %s\n", *av,
			    strerror(errno));
			exit(77);
		}

		/* not reached */
		break;
	default:
		if (bg)
			break;

		/* did always return "interrupted sys call" on OI */
		waitpid(pid, &status, 0);
	}

	reset_prog_mode();
	refresh();

	if (pid == -1)
		printerr(strerror(errno), "fork failed");

	disp_list();
	return status;
}

size_t
shell_quote(char *to, char *from, size_t siz)
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
			*to++ = c;
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
	char *av[2];

	if (db_num) {
		f = db_list[top_idx + curs];

		if ((tree == 3 && f->ltype && f->rtype) ||
		    (tree == 1 && !f->ltype) ||
		    (tree == 2 && !f->rtype))
			return;
	}

	if (bmode)
		s = rpath;
	else if (tree == 2 || ((tree & 2) && db_num && f->rtype)) {
		rpath[rlen] = 0;
		s = rpath;
	} else {
		lpath[llen] = 0;
		s = lpath;
	}

	if (!(pw = getpwuid(getuid()))) {
		printerr(strerror(errno), "getpwuid failed");
		return;
	}

	*av = pw->pw_shell;
	av[1] = NULL;
	exec_cmd(av, 0, s, "\nType \"exit\" or '^D' to return to vddiff.\n",
	    TRUE);
}

void
exec_sighdl(void)
{
	struct sigaction act;

	act.sa_handler = sig_child;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(SIGCHLD, &act, NULL) == -1) {
		printf("sigaction SIGCHLD failed: %s\n", strerror(errno));
		exit(1);
	}
}

static void
sig_child(int signo)
{
	(void)signo;
	wait(NULL);
}
