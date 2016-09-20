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
#include "db.h"
#include "diff.h"

static size_t add_path(char *, size_t, char *, char *);
static void exec_tool(struct tool *, char *, char *, int);
static void sig_child(int);

struct tool difftool;
struct tool viewtool;

void
tool(char *name, char *rnam, int tree)
{
	size_t ln, rn, l0, l1, l2, l;
	char **toolp, *cmd;
	struct tool *tmptool = NULL;

	l = ln = strlen(name);

	cmd = lbuf + sizeof lbuf;
	*--cmd = 0;

	while (l) {
		*--cmd = tolower(name[--l]);

		if (name[l] == '.') {
			tmptool = db_srch_ext(++cmd);
			break;
		}

		if (cmd == lbuf)
			break;
	}

	if (!tmptool)
		tmptool = tree == 3 ? &difftool : &viewtool;

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
	disp_list();
}

static size_t
add_path(char *cmd, size_t l0, char *path, char *name)
{
	size_t l;

	l = shell_quote(lbuf, path, sizeof lbuf);
	memcpy(cmd + l0, lbuf, l);
	l0 += l;
	cmd[l0++] = '/';
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

	if (bg)
		return;

	while (*s) {
		if (*s == '$' && (s[1] == '1' || s[1] == '2')) {
			*s++ = 0;
			if (t[1])
				t[2] = s;
			else
				t[1] = s;
		}
		s++;
	}
}

static void
exec_tool(struct tool *t, char *name, char *rnam, int tree)
{
	int o, c;
	char *s, **a, **av;

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
		pthcat(lpath, llen, name);
		*a++ = lpath;
	}

	if (tree & 2) {
		pthcat(rpath, rlen, rnam ? rnam : name);
		*a++ = rpath;
	}

	*a = NULL;
	exec_cmd(av, t->bg, NULL, NULL);

	if (o)
		free(*av);

	free(av);
}

void
exec_cmd(char **av, int bg, char *path, char *msg)
{
	pid_t pid;

	erase();
	refresh();
	def_prog_mode();
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
			exit(1);
		}

		/* not reached */
		break;
	default:
		if (bg)
			break;

		/* did always return "interrupted sys call on OI */
		waitpid(pid, NULL, 0);
	}

	reset_prog_mode();

	if (pid == -1)
		printerr(strerror(errno), "fork failed");

	disp_list();
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
	struct filediff *f = db_list[top_idx + curs];
	char *s;
	struct passwd *pw;
	char *av[2];

	if ((tree == 3 && f->ltype && f->rtype) ||
	    (tree == 1 && !f->ltype) ||
	    (tree == 2 && !f->rtype))
		return;

	if ((tree & 2) && f->rtype) {
		rpath[rlen] = 0;
		s = rpath;
	} else {
		lpath[llen] = 0;
		s = lpath;
	}

	if (!(pw = getpwuid(getuid()))) {
		printerr(strerror(errno),
		    "getpwuid failed");
		return;
	}

	*av = pw->pw_shell;
	av[1] = NULL;
	exec_cmd(av, 0, s, "\nType \"exit\" or '^D' to return to vddiff.\n");
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
