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

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <signal.h>
#include <ctype.h>
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
#include "ed.h"
#include "misc.h"

static void dl_disp(void);
static void dl_line(unsigned, unsigned);
static void dl_down(unsigned short);
static void dl_up(unsigned short);
static void dl_curs(unsigned);
static void dl_scrl_down(unsigned short);
static void dl_scrl_up(unsigned short);
static void dl_pg_down(void);
static void dl_pg_up(void);
static void dl_del(void);
static void dl_act(void);
static void dl_regcomp(void);
static int dl_regexec(int);
static void dl_center(unsigned);
static void dl_first(void);
static void dl_last(void);
static void dl_2mid(void);
static void dl_2top(void);
static void dl_2bot(void);
static void set_comment(void);
#ifdef NCURSES_MOUSE_VERSION
static int dl_mevent(void);
#endif

unsigned bdl_num, ddl_num;
static unsigned dl_num;
static int dl_wnum;
static unsigned bdl_top, ddl_top, dl_top;
static unsigned bdl_pos, ddl_pos, dl_pos;
char ***bdl_list;
char ***ddl_list;

/* Called from ui.c when "Da" is pressed */

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

		if (db_dl_add(s, NULL, NULL) == 1) {
			bdl_num++;
			info_store();
		}

		/* bdl_add() changes string length */
		if (bmode || right_col) {
			pthlen[1] = strlen(syspth[1]);
		} else {
			pthlen[0] = strlen(syspth[0]);
		}
	} else {
		syspth[1][pthlen[1]] = 0;
		syspth[0][pthlen[0]] = 0;

		if (db_dl_add(syspth[0], syspth[1], NULL) == 1) {
			ddl_num++;
			info_store();
		}

		/* db_dl_add() changes string length */
		pthlen[1] = strlen(syspth[1]);
		pthlen[0] = strlen(syspth[0]);
	}
}

static void
dl_del(void)
{
	unsigned i;

	if (bmode || fmode) {
		bdl_del(bdl_list[dl_pos]);
		bdl_num--;
	} else {
		ddl_del(ddl_list[dl_pos]);
		ddl_num--;
	}

	if (!--dl_num) {
		dl_pos = 0;
		return;
	}

	for (i = dl_pos; i < dl_num; i++) {
		if (bmode || fmode) {
			bdl_list[i] = bdl_list[i + 1];
		} else {
			ddl_list[i] = ddl_list[i + 1];
		}
	}

	if (dl_pos == dl_num) {
		dl_pos--;
	}

	/* Instead of wdeleteln() to correct indices */
	dl_disp();
}

void
dl_list(void)
{
	int c, c1;
	unsigned num;
	bool del = FALSE;
	bool act = FALSE;

#if defined(TRACE)
	fprintf(debug, "->dl_list\n");
#endif

	if (stat_info_pth() == 1) {
		info_load();
	}

	if (bmode || fmode) {
		bdl_sort();
		dl_num = bdl_num;
		dl_top = bdl_top;
		dl_pos = bdl_pos;
	} else {
		ddl_sort();
		dl_num = ddl_num;
		dl_top = ddl_top;
		dl_pos = ddl_pos;
	}

	if (!dl_num) {
		printerr(NULL, "List empty");
		goto ret0;
	}

	standendc(wlist);
	dl_wnum = getuwidth(dl_num) + 1;
	dl_disp();
	c = c1 = 0;

	while (1) {
		if (c < '0' || c > '9') {
			num = 0;
		}

		c1 = c;

		while ((c = getch()) == ERR) {
		}

#if defined(TRACE)
		if (isascii(c) && !iscntrl(c)) {
			fprintf(debug, "<>getch: '%c' num=%u\n", c, num);
		} else {
			fprintf(debug, "<>getch: 0x%x num=%u\n", c, num);
		}
#endif

		if (c >= '0' && c <= '9') {
			num = num * 10;
			num += c - '0';
			continue;
		}

		switch (c) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			if (dl_mevent() == 1) {
				act = TRUE;
				goto ret;
			}

			break;
#endif
		case KEY_RESIZE:
			set_win_dim();
			endwin();
			refresh();
			dl_disp();
			break;

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

		case KEY_END:
			dl_last();
			break;

		case 'G':
			if (!num) {
				dl_last();
				break;
			}

			if (num > dl_num) {
				break;
			}

			if (num > 1) {
				dl_pos = num - 1;
				act = TRUE;
				goto ret;
			}

			/* fall through */

		case KEY_HOME:
			dl_first();
			break;

		case 'd':
			if (c1 != 'd') {
				break;
			}

			c = 0;
			/* fall through */

		case KEY_DC:
			del = TRUE;
			dl_del();

			if (!dl_num) {
				goto ret;
			}

			break;

		case KEY_RIGHT:
		case '\n':
			act = TRUE;
			goto ret;

		case 'H':
			dl_2top();
			break;

		case 'M':
			dl_2mid();
			break;

		case 'L':
			dl_2bot();
			break;

		case '/':
			dl_regcomp();
			break;

		case 'N':
			if (regex_mode) {
				dl_regexec(-1);
			}

			break;

		case 'n':
			if (regex_mode) {
				dl_regexec(1);
			}

			break;

		case 'r':
			if (regex_mode) {
				clr_regex();
			}

			break;

		case CTRL('l'):
			endwin();
			refresh();
			dl_disp();
			break;

		case 'A':
			set_comment();
			break;

		default:
			werase(wstat);
			mvwprintw(wstat, 0, 0,
"'q' close, <ENTER> enter directory, \"dd\" delete entry, '/' regex search");
			mvwprintw(wstat, 1, 0,
"\"1G\", 'G', 'H', 'M', 'L'");
			wrefresh(wstat);
		}
	}

ret:
	if (regex_mode) {
		clr_regex();
	}

	if (act) {
		dl_act();
	}

	if (bmode || fmode) {
		bdl_num = dl_num;
		bdl_top = dl_top;
		bdl_pos = dl_pos;

		if (bdl_list) {
			free(bdl_list);
			bdl_list = NULL;
		}
	} else {
		ddl_num = dl_num;
		ddl_top = dl_top;
		ddl_pos = dl_pos;

		if (ddl_list) {
			free(ddl_list);
			ddl_list = NULL;
		}
	}

	if (del) {
		info_store();
	}

	disp_fmode();

ret0:
#if defined(TRACE)
	fprintf(debug, "<-dl_list\n");
#endif
	return;
}

static void
dl_disp(void)
{
	unsigned y, i;

	werase(wlist);
	werase(wstat);

	for (y = 0, i = dl_top; y < listh && i < dl_num; y++, i++) {
		dl_line(y, i);
	}

	dl_curs(1);
	wnoutrefresh(wlist);
	wnoutrefresh(wstat);
	doupdate();
}

static void
dl_line(unsigned y, unsigned i)
{
	ssize_t l;
	unsigned x;

	mvwprintw(wlist, y, 0, "%u", i + 1);

	if (bmode || fmode) {
		wmove(wlist, y, dl_wnum);
		l = putmbsra(wlist, bdl_list[i][0], 0);

		if (bdl_list[i][1] && l > 0 && (x = dl_wnum + l) < listw) {
			wmove(wlist, y, x);
			snprintf(lbuf, sizeof lbuf, " (%s)", bdl_list[i][1]);
			putmbs(wlist, lbuf, -1);
		}
	} else {
		wmove(wlist, y, dl_wnum);
		putmbsra(wlist, ddl_list[i][0], llstw);
		wmove(wlist, y, rlstx);
		l = putmbsra(wlist, ddl_list[i][2], 0);

		if (ddl_list[i][1] && l > 0 && (x = rlstx + l) < listw) {
			wmove(wlist, y, x);
			snprintf(lbuf, sizeof lbuf, " (%s)", ddl_list[i][1]);
			putmbs(wlist, lbuf, -1);
		}
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
	/* Only necessary for old ncurses versions. */
	touchwin(wlist);
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
	/* Only necessary for old ncurses versions. */
	touchwin(wlist);
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
		dl_last();
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
		dl_first();
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
dl_first(void)
{
	if (!dl_pos) {
		return;
	}

	dl_curs(0);
	dl_top = 0;
	dl_pos = 0;
	dl_disp();
}

static void
dl_last(void)
{
	if (dl_pos == dl_num - 1) {
		return;
	}

	dl_curs(0);
	dl_pos = dl_num - 1;

	if (dl_top + listh >= dl_num) {
		/* Last line is already displayed */
		dl_curs(1);
		wrefresh(wlist);
		return;
	}

	dl_disp();
}

#if 0 /* currently not needed */
static void
dl_go(unsigned i)
{
	if (i < dl_top || i >= dl_top + listh) {
		dl_center(i);
		return;
	}

	dl_curs(0);
	dl_pos = i;
	dl_curs(1);
	wrefresh(wlist);
}
#endif

static void
dl_curs(unsigned m)
{
	mvwchgat(wlist, dl_pos - dl_top, 0, -1,
	    m ? get_curs_attr() : get_off_attr(),
	    m ? get_curs_pair() : get_off_pair(),
	    NULL);
}

#ifdef NCURSES_MOUSE_VERSION
static int
dl_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return 0;

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
			return 0;
		}

		dl_curs(0);
		dl_pos = mevent.y;
		dl_curs(1);
		wrefresh(wlist);

		if (mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
			return 1;
		}
	}

	return 0;
}
#endif

static void
dl_act(void)
{
	if (bmode || fmode) {
		enter_dir(bdl_list[dl_pos][0], NULL, FALSE, FALSE, 0 LOCVAR);
	} else {
		while (ui_stack) {
			pop_state(0);
		}

		free_scan_db(TRUE);
		diff_db_free(0);
		pthlen[0] = 0;
		pthlen[1] = 0;
		scan_subdir(ddl_list[dl_pos][0],
		            ddl_list[dl_pos][2], 3);
		disp_list(1);
	}
}

static void
dl_regcomp(void)
{
	int fl = REG_NOSUB;

	if (dl_num < 2) {
		return;
	}

	if (regex_mode) {
		clr_regex();
	}

	if (ed_dialog(enter_regex_txt,
	    "" /* remove existing */, NULL, 0, NULL) ||
	    !*rbuf) {
		return;
	}

	if (magic) {
		fl |= REG_EXTENDED;
	}

	if (!noic) {
		fl |= REG_ICASE;
	}

	if (regcomp(&re_dat, rbuf, fl)) {
		printerr(strerror(errno), "regcomp \"%s\"", rbuf);
		return;
	}

	regex_mode = 1;

	if (!dl_regexec(0)) {
		disp_regex();
	} else {
		printerr(NULL, no_match_txt);
	}
}

/* !0: Not found in *any* file */

static int
dl_regexec(
    /* -1: prev
     *  0: initial search
     *  1: next */
    int dir)
{
	unsigned i, j, k;

	if (dl_num < 2) {
		return 1;
	}

	i = j = dl_pos;

	while (1) {
		if (dir > 0) {
			if (++i >= dl_num) {
				if (nows) {
					goto no_match;
				}

				i = 0;
			}
		} else if (dir < 0) {
			if (i) {
				i--;
			} else if (nows) {
				goto no_match;
			} else {
				i = dl_num - 1;
			}
		}

		if (bmode || fmode) {
			    k = regexec(&re_dat, bdl_list[i][0], 0, NULL, 0);
		} else if ((k = regexec(&re_dat, ddl_list[i][0], 0, NULL, 0))) {
			    k = regexec(&re_dat, ddl_list[i][2], 0, NULL, 0);
		}

		if (!k) {
			dl_center(i);
			return 0;
		}

		if (!dir) {
			dir = 1;
		} else if (i == j) {
			printerr(NULL, no_match_txt);
			return 1;
		}
	}

	return 0;

no_match:
	printerr(NULL, no_match_txt);
	return 0;
}

static void
dl_center(unsigned i)
{
	dl_pos = i;

	if (dl_num <= listh) {
		dl_top = 0;
	} else if (dl_num - i < listh / 2) {
		dl_top = dl_num - listh;
	} else {
		dl_top = dl_pos - listh / 2;
	}

	dl_disp();
}

static void
dl_2mid(void)
{
	dl_curs(0);

	if (dl_top + listh >= dl_num) {
		/* last line displayed */
		dl_pos = (dl_num + dl_top) / 2;
	} else {
		dl_pos = dl_top + listh / 2;
	}

	dl_curs(1);
	wrefresh(wlist);
}

static void
dl_2top(void)
{
	if (dl_pos == dl_top) {
		return;
	}

	dl_curs(0);
	dl_pos = dl_top;
	dl_curs(1);
	wrefresh(wlist);
}

static void
dl_2bot(void)
{
	unsigned i;

	if (dl_top + listh >= dl_num) {
		i = dl_num - 1;
	} else {
		i = dl_top + listh - 1;
	}

	if (dl_pos == i) {
		return;
	}

	dl_curs(0);
	dl_pos = i;
	dl_curs(1);
	wrefresh(wlist);
}

void
dl_info_bdl(FILE *fh)
{
	char *desc = NULL;
	bool set_desc_ = FALSE;

next:
	if (!fgets(lbuf, BUF_SIZE, fh)) {
		printerr("Too few arguments", "\"%s\" in \"%s\"",
		    info_dir_txt, info_pth);
	}

	info_chomp(lbuf);

	if (set_desc_) {
		set_desc_ = FALSE;
		free(desc);
		desc = strdup(lbuf);
		goto next;
	} else if (!strcmp(lbuf, info_desc_txt)) {
		set_desc_ = TRUE;
		goto next;
	}

	if (db_dl_add(lbuf, NULL, desc) == 1) {
		bdl_num++;
	}
}

void
dl_info_ddl(FILE *fh)
{
	char *desc = NULL;

next:
	if (!fgets(lbuf, BUF_SIZE, fh) ||
	    !fgets(rbuf, BUF_SIZE, fh)) {
		printerr("Too few arguments", "\"%s\" in \"%s\"",
		    info_ddir_txt, info_pth);
	}

	info_chomp(lbuf);
	info_chomp(rbuf);

	if (!strcmp(lbuf, info_desc_txt)) {
		free(desc);
		desc = strdup(rbuf);
		goto next;
	}

	if (db_dl_add(lbuf, rbuf, desc) == 1) {
		ddl_num++;
	}
}

static void
set_comment(void)
{
	char **entry_;

	if (!dl_num) {
		return;
	}

	entry_ = bmode || fmode ? bdl_list[dl_pos] :
	                          ddl_list[dl_pos] ;

	if (!ed_dialog("Enter comment (<ESC> to cancel):",
	    entry_[1], NULL, 0, NULL)) {
		free(entry_[1]);

		if (!*rbuf) {
			entry_[1] = NULL;
		} else {
			entry_[1] = strdup(rbuf);
		}

		/* Clear line else trailing chars are not removed when
		   comment is shortened */
		wmove(wlist, dl_pos - dl_top, 0);
		wclrtoeol(wlist);
		dl_line(dl_pos - dl_top, dl_pos);
		dl_curs(1);
		touchwin(wlist);
		wrefresh(wlist);
		info_store();
	}
}
