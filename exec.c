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
#include "compat.h"
#include "main.h"
#include "ui.h"

static size_t add_path(char *, size_t, char *, size_t, char *, size_t);

char *difftool[3];
char *viewtool[3];

void
tool(char *name, int tree)
{
	size_t ln = strlen(name);
	size_t l0 = tree == 3 ? strlen(*difftool) :
	                        strlen(*viewtool) ;
	size_t l1 = tree == 3 ?
	            difftool[1] ? strlen(difftool[1]) : 0 :
	            viewtool[1] ? strlen(viewtool[1]) : 0 ;
	size_t l2 = tree != 3 ? 0 :
	            difftool[2] ? strlen(difftool[2]) : 0 ;
	size_t l;
	char *cmd;

	/*  " "        "/"      " "        " "      "\0" */
	l = l0 + 1 + llen + 1 + ln + 1 + rlen + 1 + ln + 1 + l1 + l2;
	cmd = malloc(l);
	memcpy(cmd, tree == 3 ? *difftool : *viewtool, l0);

	if (!l1 || tree != 3) {
		if (tree & 1)
			l0 = add_path(cmd, l0, lpath, llen, name, ln);
		if (tree & 2)
			l0 = add_path(cmd, l0, rpath, rlen, name, ln);

		if (l1 && tree != 3) {
			memcpy(cmd + l0, viewtool[1] + 1, --l1);
			l0 += l1;
		}
	} else {
		switch (*difftool[1]) {
		case '1':
			l0 = add_path(cmd, l0, lpath, llen, name, ln);
			break;
		case '2':
			l0 = add_path(cmd, l0, rpath, rlen, name, ln);
			break;
		}

		memcpy(cmd + l0, difftool[1] + 1, --l1);
		l0 += l1;

		if (l2) {
			switch (*difftool[2]) {
			case '1':
				l0 = add_path(cmd, l0, lpath, llen, name, ln);
				break;
			case '2':
				l0 = add_path(cmd, l0, rpath, rlen, name, ln);
				break;
			}

			memcpy(cmd + l0, difftool[2] + 1, --l2);
			l0 += l2;
		}
	}

	cmd[l0] = 0;

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
	cmd[l0++] = ' ';
	memcpy(cmd + l0, path, len);
	l0 += len;
	cmd[l0++] = '/';
	memcpy(cmd + l0, name, ln);
	l0 += ln;
	return l0;
}

void
set_tool(char **t, char *s)
{
	free(*t);
	*t = s = strdup(s);
	t[1] = NULL;
	t[2] = NULL;

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
