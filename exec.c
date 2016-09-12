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
#include <avlbst.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "compat.h"
#include "main.h"
#include "ui.h"
#include "exec.h"
#include "db.h"
#include "diff.h"

static size_t add_path(char *, size_t, char *, size_t, char *, size_t);
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

	/*             "/"             " "      "\0"       */
	l = l0 + llen + 1 + ln + rlen + 1 + rn + 1 + l1 + l2;
	cmd = malloc(l);
	memcpy(cmd, *toolp, l0);

	if (!l1 || tree != 3) {
		if (tree & 1)
			l0 = add_path(cmd, l0, lpath, llen, name, ln);
		if (tree & 2)
			l0 = add_path(cmd, l0, rpath, rlen,
			    rnam ? rnam : name, rn);

		if (l1 && tree != 3) {
			memcpy(cmd + l0, toolp[1] + 1, --l1);
			l0 += l1;
		}
	} else {
		switch (*toolp[1]) {
		case '1':
			l0 = add_path(cmd, l0, lpath, llen, name, ln);
			break;
		case '2':
			l0 = add_path(cmd, l0, rpath, rlen,
			    rnam ? rnam : name, rn);
			break;
		}

		memcpy(cmd + l0, toolp[1] + 1, --l1);
		l0 += l1;

		if (l2) {
			switch (*toolp[2]) {
			case '1':
				l0 = add_path(cmd, l0, lpath, llen, name, ln);
				break;
			case '2':
				l0 = add_path(cmd, l0, rpath, rlen,
				    rnam ? rnam : name, rn);
				break;
			}

			memcpy(cmd + l0, toolp[2] + 1, --l2);
			l0 += l2;
		}
	}

	cmd[l0] = 0;
	erase();
	refresh();
	def_prog_mode();
	endwin();
	system(cmd);
	reset_prog_mode();
	free(cmd);
	disp_list();
}

static size_t
add_path(char *cmd, size_t l0, char *path, size_t len, char *name, size_t ln)
{
	memcpy(cmd + l0, path, len);
	l0 += len;
	cmd[l0++] = '/';
	memcpy(cmd + l0, name, ln);
	l0 += ln;
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
	pid_t pid;

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
	erase();
	refresh();
	def_prog_mode();
	endwin();

	switch ((pid = fork())) {
	case -1:
		break;
	case 0:
		if (execvp(*av, av) == -1) {
			/* only seen when vddiff exits later */
			printf("exec %s failed: %s\n", *av, strerror(errno));
			exit(1);
		}
		/* not reached */
		break;
	default:
		if (t->bg)
			break;

		/* did always return "interrupted sys call on OI */
		waitpid(pid, NULL, 0);
	}

	if (o)
		free(*av);

	free(av);
	reset_prog_mode();

	if (pid == -1)
		printerr(strerror(errno), "fork failed");

	disp_list();
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
