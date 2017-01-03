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
static void dl_line(unsigned, unsigned);
static void dl_down(unsigned short);
static void dl_up(unsigned short);
static void dl_curs(unsigned);
static void dl_scrl_down(unsigned short);
static void dl_scrl_up(unsigned short);
static void dl_pg_down(void);
static void dl_pg_up(void);
static int bdl_add(char *);
#ifdef NCURSES_MOUSE_VERSION
static void dl_mevent(void);
#endif

unsigned bdl_num;
unsigned ddl_num;
unsigned dl_num;
static unsigned dl_top;
static unsigned dl_pos;
char **bdl_list;
char ***ddl_list;

void
dl_add(void)
{
	if (bmode || fmode) {
		char *s;

		if (bmode || right_col) {
			syspth[1][pthlen[1]] = 0;
			s = syspth[1];
		} else {
			syspth[0][pthlen[0]] = 0;
			s = syspth[0];
		}

		if (!bdl_add(s)) {
			info_store();
		}
	} else {
		syspth[1][pthlen[1]] = 0;
		syspth[0][pthlen[0]] = 0;

		if (ddl_add(syspth[0], syspth[1]) == 1) {
			ddl_num++;
			info_store();
		}
	}
}

static int
bdl_add(char *s)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
	int i;

	if ((i = str_db_srch(&bdl_db, s, &n))) {
		str_db_add(&bdl_db, strdup(s), i, n);
		bdl_num++;
		return 0;
	}

	return 1;
#else
	char *s2;

	s = strdup(s);
	s2 = str_db_add(&bdl_db, s);

	if (s2 == s) {
		bdl_num++;
		return 0;
	} else {
		free(s);
		return 1;
	}
#endif
}

void
dl_list(void)
{
	if (bmode || fmode) {
		bdl_list = str_db_sort(bdl_db, bdl_num);
		dl_num = bdl_num;
	} else {
		ddl_sort();
		dl_num = ddl_num;
	}

	dl_top = 0;
	standendc(wlist);
	dl_disp();

	while (1) {
		int c;

		switch (c = getch()) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			dl_mevent();
			break;
#endif
		case KEY_LEFT:
		case 'q':
			goto ret;

		case KEY_DOWN:
		case 'j':
		case '+':
			dl_down(1);
			break;

		case KEY_UP:
		case 'k':
		case '-':
			dl_up(1);
			break;

		case KEY_NPAGE:
		case ' ':
			dl_pg_down();
			break;

		case KEY_PPAGE:
		case KEY_BACKSPACE:
		case CERASE:
			dl_pg_up();
			break;

		case CTRL('l'):
			endwin();
			refresh();
			break;
		}
	}

ret:
	if (bdl_list) {
		free(bdl_list);
		bdl_list = NULL;
	} else if (ddl_list) {
		free(ddl_list);
		ddl_list = NULL;
	}

	disp_fmode();
}

static void
dl_disp(void)
{
	unsigned y, i;

	werase(wlist);
	werase(wstat);

	for (y = 0, i = dl_top; y < listh && i < dl_num; y++, i++) {
		dl_line(y, i);

		if (i == dl_pos) {
			mvwchgat(wlist, y, 0, -1, A_REVERSE, 0, NULL);
		}
	}

	wnoutrefresh(wlist);
	wnoutrefresh(wstat);
	doupdate();
}

static void
dl_line(unsigned y, unsigned i)
{
	if (bmode || fmode) {
		wmove(wlist, y, 0);
		putmbsra(wlist, bdl_list[i], 0);
	} else {
		wmove(wlist, y, 0);
		putmbsra(wlist, ddl_list[i][0], llstw);
		wmove(wlist, y, rlstx);
		putmbsra(wlist, ddl_list[i][1], 0);
	}
}

static void
dl_down(unsigned short d)
{
	if (dl_pos == dl_num - 1) {
		return;
	}

	dl_curs(0);

	if (dl_pos + d >= dl_num) {
		dl_pos = dl_num - 1;
	} else {
		dl_pos += d;
	}

	if (dl_pos >= dl_top + listh) {
		dl_scrl_down(dl_pos - dl_top - listh + 1);
	}

	dl_curs(1);
	wrefresh(wlist);
}

static void
dl_up(unsigned short d)
{
	if (!dl_pos) {
		return;
	}

	dl_curs(0);

	if (d > dl_pos) {
		dl_pos = 0;
	} else {
		dl_pos -= d;
	}

	if (dl_pos < dl_top) {
		dl_scrl_up(dl_top - dl_pos);
	}

	dl_curs(1);
	wrefresh(wlist);
}

static void
dl_scrl_down(unsigned short n)
{
	if (dl_top + listh >= dl_num) {
		return;
	}

	if (dl_top + listh + n >= dl_num) {
		n = dl_num - dl_top - listh;
	}

	dl_top += n;
	wscrl(wlist, n);

	if (dl_pos < dl_top) {
		dl_pos = dl_top;
		dl_curs(1);
	}

	for (; n; n--) {
		dl_line(listh - n, dl_top + listh - n);
	}
}

static void
dl_scrl_up(unsigned short n)
{
	if (!dl_top) {
		return;
	}

	if (n > dl_top) {
		n = dl_top;
	}

	dl_top -= n;
	wscrl(wlist, -n);

	if (dl_pos >= dl_top + listh) {
		dl_pos = dl_top + listh - 1;
		dl_curs(1);
	}

	while (n--) {
		dl_line(n, dl_top + n);
	}
}

static void
dl_pg_down(void)
{
	if (dl_top + listh >= dl_num) {
		return;
	}

	if (dl_top + 2 * listh >= dl_num) {
		dl_scrl_down(listh);
		wrefresh(wlist);
		return;
	}

	dl_top += listh;
	dl_pos = dl_top;
	dl_disp();
}

static void
dl_pg_up(void)
{
	if (!dl_top) {
		return;
	}

	if (dl_top < listh) {
		dl_scrl_up(listh);
		wrefresh(wlist);
		return;
	}

	dl_top -= listh;
	dl_pos = dl_top + listh - 1;

	if (dl_pos >= dl_num) {
		dl_pos = dl_num - 1;
	}

	dl_disp();
}

static void
dl_curs(unsigned m)
{
	mvwchgat(wlist, dl_pos - dl_top, 0, -1,
	    m == 0 ? A_NORMAL : A_REVERSE, 0, NULL);
}

#ifdef NCURSES_MOUSE_VERSION
static void
dl_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

#if NCURSES_MOUSE_VERSION >= 2
	if (mevent.bstate & BUTTON4_PRESSED) {
		dl_scrl_up(3);
		wrefresh(wlist);
	} else if (mevent.bstate & BUTTON5_PRESSED) {
		dl_scrl_down(3);
		wrefresh(wlist);
	} else
#endif
	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED ||
	    mevent.bstate & BUTTON1_PRESSED) {

		if (mevent.y >= (int)listh) {
			return;
		}

		dl_curs(0);
		dl_pos = mevent.y;
		dl_curs(1);
		wrefresh(wlist);
	}
}
#endif

void
dl_info_bdl(FILE *fh)
{
	if (!fgets(lbuf, BUF_SIZE, fh)) {
		printerr("Too few arguments", "\"%s\" in \"%s\"",
		    info_dir_txt, info_pth);
	}

	info_chomp(lbuf);
	bdl_add(lbuf);
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
