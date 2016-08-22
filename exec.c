#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "main.h"
#include "ui.h"

static size_t add_path(char *, size_t, char *, size_t, char *, size_t);

static char *difftool[3];

void
tool(char *name)
{
	size_t ln = strlen(name);
	size_t l0 = strlen(*difftool);
	size_t l1 = difftool[1] ? strlen(difftool[1]) : 0;
	size_t l2 = difftool[2] ? strlen(difftool[2]) : 0;
	size_t l;
	char *cmd;

	/*  " "        "/"      " "        " "      "\0" */
	l = l0 + 1 + llen + 1 + ln + 1 + rlen + 1 + ln + 1 + l1 + l2;
	cmd = malloc(l);
	memcpy(cmd, *difftool, l0);

	if (!l1) {
		l0 = add_path(cmd, l0, lpath, llen, name, ln);
		l0 = add_path(cmd, l0, rpath, rlen, name, ln);
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
set_difftool(char *s)
{
	static char *m;

	free(m);
	m = s = strdup(s);
	*difftool = s;
	difftool[1] = NULL;
	difftool[2] = NULL;

	while (*s) {
		if (*s == '$' && (s[1] == '1' || s[1] == '2')) {
			*s++ = 0;
			if (difftool[1])
				difftool[2] = s;
			else
				difftool[1] = s;
		}
		s++;
	}
}
