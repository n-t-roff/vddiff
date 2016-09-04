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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "compat.h"
#include "avlbst.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"
#include "exec.h"
#include "fs.h"

#define COLOR_LEFTONLY  1
#define COLOR_RIGHTONLY 2
#define COLOR_DIFF      3
#define COLOR_DIR       4
#define COLOR_UNKNOWN   5
#define COLOR_LINK      6

static void ui_ctrl(void);
static void page_down(void);
static void page_up(void);
static void curs_last(void);
static void curs_first(void);
static void curs_down(void);
static void curs_up(void);
static void disp_curs(int);
static void disp_line(unsigned, unsigned);
static void push_state(void);
static void pop_state(void);
static void enter_dir(char *, int);
static void help(void);
static void action(void);
static char *type_name(mode_t);
static void ui_resize(void);
static void set_win_dim(void);

short color = 1;
short color_leftonly  = COLOR_CYAN   ,
      color_rightonly = COLOR_GREEN  ,
      color_diff      = COLOR_RED    ,
      color_dir       = COLOR_YELLOW ,
      color_unknown   = COLOR_BLUE   ,
      color_link      = COLOR_MAGENTA;
unsigned top_idx, curs;

static unsigned listw, listh, statw;
static WINDOW *wlist, *wstat;
static struct ui_state *ui_stack;
/* Line scroll enable. Else only full screen is scrolled */
static short scrollen = 1;

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(void);
# if NCURSES_MOUSE_VERSION >= 2
static void scroll_up(unsigned);
static void scroll_down(unsigned);
# endif

static MEVENT mevent;
#endif

void
build_ui(void)
{
	initscr();

	if (color && !has_colors())
		color = 0;

	if (color) {
		start_color();
		init_pair(COLOR_LEFTONLY , color_leftonly , COLOR_BLACK);
		init_pair(COLOR_RIGHTONLY, color_rightonly, COLOR_BLACK);
		init_pair(COLOR_DIFF     , color_diff     , COLOR_BLACK);
		init_pair(COLOR_DIR      , color_dir      , COLOR_BLACK);
		init_pair(COLOR_UNKNOWN  , color_unknown  , COLOR_BLACK);
		init_pair(COLOR_LINK     , color_link     , COLOR_BLACK);
	}

	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
#ifdef NCURSES_MOUSE_VERSION
	mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED
# if NCURSES_MOUSE_VERSION >= 2
	    | BUTTON4_PRESSED | BUTTON5_PRESSED
# endif
	    , NULL);
#endif
	set_win_dim();

	if (!(wlist = subwin(stdscr, listh, listw, 0, 0))) {
		printf("subwin failed\n");
		return;
	}

	if (!(wstat = subwin(stdscr, 2, statw, LINES-2, 0))) {
		printf("subwin failed\n");
		return;
	}

	if (scrollen) {
		idlok(wlist, TRUE);
		scrollok(wlist, TRUE);
	}

	/* Not in main since build_diff_db() uses printerr() */
	if (recursive) {
		scan = 1;
		build_diff_db(3);
		scan = 0;
	}

	build_diff_db(3);
	disp_list();
	ui_ctrl();
	db_free();
	delwin(wstat);
	delwin(wlist);
	endwin();
}

static void
ui_ctrl(void)
{
	int prev_key, c = 0;

	while (1) {
		prev_key = c;

		switch (c = getch()) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			proc_mevent();
			break;
#endif
		case 'q':
			return;
		case KEY_DOWN: case 'j': case '+':
			curs_down();
			break;
		case KEY_UP: case 'k': case '-':
			curs_up();
			break;
		case KEY_LEFT:
			pop_state();
			break;
		case KEY_RIGHT: case '\n':
			if (!db_num)
				break;

			action();
			break;
		case KEY_NPAGE:
			page_down();
			break;
		case KEY_PPAGE:
			page_up();
			break;
		case 'h': case '?':
			help();
			break;
		case 'p':
			printerr(NULL, "%s", PWD);
			break;
		case 'a':
			werase(wstat);
			wattron(wstat, A_REVERSE);
			mvwprintw(wstat, 0, 0, "<   %s", arg[0]);
			mvwprintw(wstat, 1, 0, ">   %s", arg[1]);
			wrefresh(wstat);
			wattron(wstat, A_NORMAL);
			break;
		case '!': case 'n':
			noequal = noequal ? 0 : 1;
			db_sort();
			top_idx = 0;
			curs    = 0;
			disp_list();
			break;
		case 'c':
			real_diff = real_diff ? 0 : 1;
			db_sort();
			top_idx = 0;
			curs    = 0;
			disp_list();
			break;
		case KEY_RESIZE:
			ui_resize();
			break;
		case 'd':
			if (prev_key != 'd')
				break;
			fs_rm(3, NULL); /* allowed for single sided only */
			break;
		case 'l':
			if (prev_key != 'd')
				break;
			fs_rm(1, NULL);
			break;
		case 'r':
			if (prev_key != 'd')
				break;
			fs_rm(2, NULL);
			break;
		case '<':
			if (prev_key != '<')
				break;
			fs_cp(2, 1);
			break;
		case '>':
			if (prev_key != '>')
				break;
			fs_cp(1, 2);
			break;
		case KEY_HOME:
			curs_first();
			break;
		case 'G':
			if (prev_key == '1') {
				curs_first();
				break;
			}
			/* fall-through */
		case KEY_END:
			curs_last();
			break;
		}
	}
}

static void
help(void) {
	erase();
	move(0, 0);
	addstr(
       "q		Quit\n"
       "h, ?		Display help\n"
       "<UP>, k, -	Move cursor up\n"
       "<DOWN>, j, +	Move cursor down\n"
       "<LEFT>		Leave directory (one directory up)\n"
       "<RIGHT>, <ENTER>\n"
       "		Enter directory or start diff tool\n"
       "<PG-UP>		Scroll one screen up\n"
       "<PG-DOWN>	Scroll one screen down\n"
       "1G		Go to first file\n"
       "G		Go to last file\n"
       "!, n		Toggle display of equal files\n"
       "c		Toggle showing only directories and really different files\n"
       "p		Show current relative work directory\n"
       "a		Show command line directory arguments\n"
       /*
       "<<		Copy from second to first tree\n"
       ">>		Copy from first to second tree\n"
       "dd		Delete file or directory\n"
       "dl		Delete file or directory in first tree\n"
       "dr		Delete file or directory in second tree\n"
       */
	    );
	refresh();
	getch();
	erase();
	refresh();
	disp_list();
}

#ifdef NCURSES_MOUSE_VERSION
static void
proc_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
		if (mevent.y >= (int)listh ||
		    mevent.y >= (int)(db_num - top_idx))
			return;

		disp_curs(0);
		curs = mevent.y;
		disp_curs(1);
		wrefresh(wlist);
		wrefresh(wstat);

		if (mevent.bstate & BUTTON1_DOUBLE_CLICKED)
			action();
# if NCURSES_MOUSE_VERSION >= 2
	} else if (mevent.bstate & BUTTON4_PRESSED) {
		scroll_up(3);
	} else if (mevent.bstate & BUTTON5_PRESSED) {
		scroll_down(3);
# endif
	}
}
#endif

static void
action(void)
{
	struct filediff *f = db_list[top_idx + curs];

	if (f->ltype == f->rtype) {
		if (S_ISDIR(f->ltype))
			enter_dir(f->name, 3);
		else if (S_ISREG(f->ltype)) {
			if (f->diff == '!')
				tool(f->name, 3);
			else
				tool(f->name, 1);
		}
	} else if (!f->ltype) {
		if (S_ISDIR(f->rtype))
			enter_dir(f->name, 2);
		else if (S_ISREG(f->rtype))
			tool(f->name, 2);
	} else if (!f->rtype) {
		if (S_ISDIR(f->ltype))
			enter_dir(f->name, 1);
		else if (S_ISREG(f->ltype))
			tool(f->name, 1);
	}
}

static void
page_down(void)
{
	if (db_num - top_idx <= listh) {
		/* last line is currently displayed */
		if (curs != db_num - top_idx - 1) {
			disp_curs(0);
			curs = db_num - top_idx - 1;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		}
		return;
	}

	top_idx += listh;
	curs = 0;
	disp_list();
}

static void
page_up(void)
{
	if (!top_idx) {
		if (curs) {
			disp_curs(0);
			curs = 0;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		}
		return;
	}

	if (top_idx < listh) {
		top_idx = 0;
		curs -= top_idx;
	} else {
		top_idx -= listh;
		curs = listh - 1;
	}

	disp_list();
}

static void
curs_last(void)
{
	if (db_num - top_idx <= listh) {
		/* last line is currently displayed */
		if (curs != db_num - top_idx - 1) {
			disp_curs(0);
			curs = db_num - top_idx - 1;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		}
		return;
	}

	top_idx = db_num - listh;
	curs = listh - 1;
	disp_list();
}

static void
curs_first(void)
{
	if (!top_idx) {
		if (curs) {
			disp_curs(0);
			curs = 0;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		}
		return;
	}

	top_idx = curs = 0;
	disp_list();
}

static void
curs_down(void)
{
	if (top_idx + curs + 1 >= db_num)
		return;

	if (curs + 1 >= listh) {
		if (scrollen) {
			disp_curs(0);
			wscrl(wlist, 1);
			top_idx++;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		} else {
			page_down();
		}
		return;
	}

	disp_curs(0);
	curs++;
	disp_curs(1);
	wrefresh(wlist);
	wrefresh(wstat);
}

static void
curs_up(void)
{
	if (!curs) {
		if (!top_idx)
			return;

		if (scrollen) {
			disp_curs(0);
			wscrl(wlist, -1);
			top_idx--;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		} else {
			page_up();
		}
		return;
	}

	disp_curs(0);
	curs--;
	disp_curs(1);
	wrefresh(wlist);
	wrefresh(wstat);
}

#if NCURSES_MOUSE_VERSION >= 2
static void
scroll_up(unsigned num)
{
	unsigned move_curs, y, i;

	if (!top_idx) {
		if (!curs)
			return;

		disp_curs(0);

		if (curs >= num)
			curs -= num;
		else
			curs = 0;

		disp_curs(1);
		wrefresh(wlist);
		wrefresh(wstat);
		return;
	}

	if (top_idx < num)
		num = top_idx;

	top_idx -= num;

	if (curs + num >= listh) {
		disp_curs(0);
		curs = listh - 1;
		move_curs = 1;
	} else {
		curs += num;
		move_curs = 0;
	}

	wscrl(wlist, -num);

	for (y = 0, i = top_idx; y < num; y++, i++)
		disp_line(y, i);

	if (move_curs)
		disp_curs(1);
	wrefresh(wlist);
	wrefresh(wstat);
}

static void
scroll_down(unsigned num)
{
	unsigned move_curs, y, i;

	if (top_idx >= db_num - 1)
		return;

	if (top_idx + num >= db_num)
		num = db_num - 1 - top_idx;

	top_idx += num;

	if (curs < num) {
		disp_curs(0);
		curs = 0;
		move_curs = 1;
	} else {
		curs -= num;
		move_curs = 0;
	}

	wscrl(wlist, num);

	for (y = listh - num, i = top_idx + y; y < listh && i < db_num;
	    y++, i++)
		disp_line(y, i);

	if (move_curs)
		disp_curs(1);
	wrefresh(wlist);
	wrefresh(wstat);
}
#endif

static void
disp_curs(int a)
{
	if (a)
		wattron(wlist, A_REVERSE);
	disp_line(curs, top_idx + curs);
}

void
disp_list(void)
{
	unsigned y, i;

	werase(wlist);
	for (y = 0, i = top_idx; y < listh && i < db_num; y++, i++) {
		if (y == curs)
			disp_curs(1);
		else
			disp_line(y, i);
	}
	wrefresh(wlist);
	wrefresh(wstat);
}

static void
disp_line(unsigned y, unsigned i)
{
	int diff, type;
	mode_t mode;
	struct filediff *f = db_list[i];
	char *link = NULL;
	short color_id = 0;

	if (!f->ltype) {
		diff     = '>';
		mode     = f->rtype;
		color_id = COLOR_RIGHTONLY;
	} else if (!f->rtype) {
		diff     = '<';
		mode     = f->ltype;
		color_id = COLOR_LEFTONLY;
	} else if (f->ltype != f->rtype) {
		diff = ' ';
		mode = 0;
		type = '!';
		color_id = COLOR_DIFF;
	} else {
		diff = f->diff;
		mode = f->ltype;
		if (diff == '!')
			color_id = COLOR_DIFF;
	}

	if (S_ISREG(mode)) {
		type = ' ';
	} else if (S_ISDIR(mode)) {
		type = '/';
		if (!color_id) {
			color_id = COLOR_DIR;

			if (is_diff_dir(f->name))
				diff = '!';
		}
	} else if (S_ISLNK(mode)) {
		type = '@';
		if (!color_id)
			color_id = COLOR_LINK;
		if (diff != '!')
			link = f->llink;
	} else if (mode) {
		type = '?';
		if (!color_id)
			color_id = COLOR_UNKNOWN;
	}

	if (color) {
		wattron(wlist, A_BOLD);
		if (color_id)
			wattron(wlist, COLOR_PAIR(color_id));
	}
	mvwprintw(wlist, y, 0, "%c %c %s", diff, type, f->name);
	wattrset(wlist, A_NORMAL);
	if (link) {
		waddstr(wlist, " -> ");
		waddstr(wlist, link);
	}

	werase(wstat);
	if (diff == '!' && type == '@') {
		wattron(wstat, A_REVERSE);
		mvwprintw(wstat, 0, 0, "<   -> %s", f->llink);
		mvwprintw(wstat, 1, 0, ">   -> %s", f->rlink);
	} else if (diff == ' ' && type == '!') {
		wattron(wstat, A_REVERSE);
		mvwprintw(wstat, 0, 0, "<   %s", type_name(f->ltype));
		mvwprintw(wstat, 1, 0, ">   %s", type_name(f->rtype));
	}
}

static char *
type_name(mode_t m)
{
	if      (S_ISREG(m))  return "regular file";
	else if (S_ISDIR(m))  return "directory";
	else if (S_ISLNK(m))  return "symbolic link";
	else if (S_ISSOCK(m)) return "socket";
	else if (S_ISFIFO(m)) return "FIFO";
	else if (S_ISCHR(m))  return "character device";
	else if (S_ISBLK(m))  return "block device";
	else                  return "unkown type";
}

static void
push_state(void)
{
	struct ui_state *st = malloc(sizeof(struct ui_state));
	db_store(st);
	st->llen    = llen;
	st->rlen    = rlen;
	st->top_idx = top_idx;  top_idx = 0;
	st->curs    = curs;     curs    = 0;
	st->next    = ui_stack;
	ui_stack    = st;
}

static void
pop_state(void)
{
	struct ui_state *st = ui_stack;
	if (!st)
		return;
	ui_stack = st->next;
	llen     = st->llen;
	rlen     = st->rlen;
	top_idx  = st->top_idx;
	curs     = st->curs;
	lpath[llen] = 0; /* For 'p' (pwd) */
	db_restore(st);
	free(st);
	disp_list();
}

static void
enter_dir(char *name, int tree)
{
	push_state();
	scan_subdir(name, tree);
	disp_list();
}

void
printerr(char *s2, char *s1, ...)
{
	va_list ap;

	werase(wstat);
	wattrset(wstat, A_NORMAL);
	wmove(wstat, s2 ? 0 : 1, 0);
	va_start(ap, s1);
	vwprintw(wstat, s1, ap);
	va_end(ap);
	if (s2) {
		mvwaddstr(wstat, 1, 0, s2);
		wrefresh(wstat);
		getch();
		werase(wstat);
	}
	wrefresh(wstat);
}

int
dialog(char *quest, char *answ, char *fmt, ...)
{
	va_list ap;
	int c, c2;
	char *s;

	werase(wstat);
	wattrset(wstat, A_REVERSE);
	wmove(wstat, 0, 0);
	va_start(ap, fmt);
	vwprintw(wstat, fmt, ap);
	va_end(ap);
	mvwaddstr(wstat, 1, 0, quest);
	wrefresh(wstat);
	do {
		c = getch();
		for (s = answ; s && (c2 = *s); s++)
			if (c == c2)
				break;
	} while (s && !c2);
	wattrset(wstat, A_NORMAL);
	werase(wstat);
	wrefresh(wstat);
	return c;
}

static void
ui_resize(void)
{
	set_win_dim();

	if (curs >= listh)
		curs = listh -1;

	wresize(wlist, listh, listw);
	delwin(wstat);

	if (!(wstat = subwin(stdscr, 2, statw, LINES-2, 0))) {
		printf("subwin failed\n");
		return;
	}

	disp_list();
}

static void
set_win_dim(void)
{
	listw = statw = COLS;
	listh = LINES - 2;
}
