/*
Copyright (c) 2017, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <sys/types.h>
#include <regex.h>
#include <signal.h>
#include "compat.h"
#include "dl.h"
#include "main.h"
#include "tc.h"
#include "diff.h"
#include "exec.h"
#include "uzp.h"
#include "ui.h"
#include "ui2.h"
#include "db.h"
#include "info.h"

static void dl_disp(void);

unsigned bdl_num;
unsigned ddl_num;
static unsigned dl_top;
char ***ddl_list;

void
dl_add(void)
{
	if (bmode) {
	} else if (fmode) {
	} else {
		syspth[1][pthlen[1]] = 0;
		syspth[0][pthlen[0]] = 0;

		if (ddl_add(syspth[0], syspth[1]) == 1) {
			ddl_num++;
			info_store();
		}
	}
}

void
dl_list(void)
{
	if (bmode || fmode) {
	} else {
		ddl_sort();
	}

	standendc(wlist);
	dl_disp();

	if (ddl_list) {
		free(ddl_list);
		ddl_list = NULL;
	}
}

static void
dl_disp(void)
{
	unsigned y, i, n;

	werase(wlist);
	werase(wstat);
	n = bmode || fmode ? bdl_num : ddl_num;

	for (y = 0, i = dl_top; y < listh && i < n; y++, i++) {
		wmove(wlist, y, 0);

		if (y <= 9) {
			waddch(wlist, '0' + y);
		} else if (y <= 10 + 'z' - 'a') {
			waddch(wlist, 'a' + y - 10);
		} else if (y <= 10 + 'z' - 'a' + 'Z' - 'A') {
			waddch(wlist, 'A' + y - 10 - ('z' - 'a'));
		}

		if (bmode || fmode) {
		} else {
			wmove(wlist, y, 2);
			putmbsra(wlist, ddl_list[i][0], llstw);
			wmove(wlist, y, rlstx);
			putmbsra(wlist, ddl_list[i][1], 0);
		}
	}

	wrefresh(wlist);
}
