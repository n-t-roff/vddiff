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

#include <string.h>
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
char **bdl_list;
char ***ddl_list;

void
dl_add(void)
{
	if (bmode || fmode) {
		char *s;
#ifdef HAVE_LIBAVLBST
		struct bst_node *n;
		int i;
#else
		char *s2;
#endif

		if (bmode || right_col) {
			syspth[1][pthlen[1]] = 0;
			s = syspth[1];
		} else {
			syspth[0][pthlen[0]] = 0;
			s = syspth[0];
		}

#ifdef HAVE_LIBAVLBST
		if ((i = str_db_srch(&bdl_db, s, &n))) {
			str_db_add(&bdl_db, strdup(s), i, n);
			bdl_num++;
			info_store();
		}
#else
		s = strdup(s);
		s2 = str_db_add(&bdl_db, s);

		if (s2 == s) {
			bdl_num++;
			info_store();
		} else {
			free(s);
		}
#endif
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
		bdl_list = str_db_sort(bdl_db, bdl_num);
	} else {
		ddl_sort();
	}

	standendc(wlist);
	dl_disp();

	if (bdl_list) {
		free(bdl_list);
		bdl_list = NULL;
	} else if (ddl_list) {
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
			wmove(wlist, y, 2);
			putmbsra(wlist, bdl_list[i], 0);
		} else {
			wmove(wlist, y, 2);
			putmbsra(wlist, ddl_list[i][0], llstw);
			wmove(wlist, y, rlstx);
			putmbsra(wlist, ddl_list[i][1], 0);
		}
	}

	wrefresh(wlist);
}

void
dl_info_ddl(FILE *fh)
{
	if (!fgets(lbuf, BUF_SIZE, fh) ||
	    !fgets(rbuf, BUF_SIZE, fh)) {
		printerr("Too few arguments", "\"%s\" in \"%s\"",
		    info_ddir_txt, info_pth);
	}

	info_chomp(lbuf);
	info_chomp(rbuf);

	if (ddl_add(lbuf, rbuf) == 1) {
		ddl_num++;
	}
}
