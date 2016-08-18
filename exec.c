#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "main.h"
#include "ui.h"

void
tool(char *name)
{
	size_t ln = strlen(name);
	size_t lt = strlen(difftool);
	size_t l;
	char *cmd;
	/*  " "        "/"      " "        " "      "\0" */
	l = lt + 1 + llen + 1 + ln + 1 + rlen + 1 + ln + 1;
	cmd = malloc(l);
	memcpy(cmd, difftool, lt);
	cmd[lt++] = ' ';
	memcpy(cmd + lt, lpath, llen);
	lt += llen;
	cmd[lt++] = '/';
	memcpy(cmd + lt, name, ln);
	lt += ln;
	cmd[lt++] = ' ';
	memcpy(cmd + lt, rpath, rlen);
	lt += rlen;
	cmd[lt++] = '/';
	memcpy(cmd + lt, name, ln + 1);
	def_prog_mode();
	endwin();
	system(cmd);
	reset_prog_mode();
	free(cmd);
	disp_list();
}
