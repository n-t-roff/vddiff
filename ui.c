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

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <avlbst.h>
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"
#include "exec.h"
#include "fs.h"
#include "ed.h"

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
static int last_line_is_disp(void);
static int first_line_is_top(void);
static void curs_down(void);
static void curs_up(void);
static void disp_curs(int);
static void disp_line(unsigned, unsigned, int);
static void push_state(void);
static void pop_state(void);
static void enter_dir(char *, char *, int);
static void help(void);
static void action(void);
static char *type_name(mode_t);
static void ui_resize(void);
static void set_win_dim(void);
static void statcol(void);
static void file_stat(struct filediff *);
static void disp_help(void);
static void help_pg_down(void);
static void help_pg_up(void);
static void help_down(void);
static void help_up(void);
static void set_mark(void);
static void clr_mark(void);
static void disp_mark(void);
static void yank_name(int);
static void srch_file(char *);
static void no_file(void);
static void center(unsigned);

short color = 1;
short color_leftonly  = COLOR_CYAN   ,
      color_rightonly = COLOR_GREEN  ,
      color_diff      = COLOR_RED    ,
      color_dir       = COLOR_YELLOW ,
      color_unknown   = COLOR_BLUE   ,
      color_link      = COLOR_MAGENTA;
unsigned top_idx, curs, statw;

#ifdef HAVE_CURSES_WCH
wchar_t *sh_str[10];
#else
char *sh_str[10];
#endif

static unsigned listw, listh, help_top;
static WINDOW *wlist;
WINDOW *wstat;
static struct ui_state *ui_stack;
/* Line scroll enable. Else only full screen is scrolled */
static short scrollen = 1;
static struct filediff *mark;
static short wstat_dirty;
static unsigned srch_idx;

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(void);
# if NCURSES_MOUSE_VERSION >= 2
static void scroll_up(unsigned);
static void scroll_down(unsigned);
static void help_mevent(void);
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

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
#ifdef NCURSES_MOUSE_VERSION
	mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_PRESSED
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

	printerr(NULL, "Reading directories...");

	/* Not in main since build_diff_db() uses printerr() */
	if (recursive && !bmode) {
		scan = 1;
		build_diff_db(3);
		scan = 0;
	}

	build_diff_db(bmode ? 1 : 3);
	disp_list();
	ui_ctrl();
	diff_db_free();
	delwin(wstat);
	delwin(wlist);
	erase();
	refresh();
	endwin();
}

static void
ui_ctrl(void)
{
	int key[2] = { 0, 0 }, c = 0, i;
	enum sorting prev_sorting;

	while (1) {
next_key:
		key[1] = *key;
		*key = c;
		c = getch();

		for (i = 0; i < (ssize_t)(sizeof(sh_str)/sizeof(*sh_str));
		    i++) {
			if (c != KEY_F(i))
				continue;

			if (ed_dialog(
			    "Type text to be saved for function key:",
			    "", NULL, 1))
				break;

			free(sh_str[i]);
#ifdef HAVE_CURSES_WCH
			sh_str[i] = linebuf;
			linebuf = NULL;
			clr_edit();
			printerr(NULL, "%ls"
#else
			sh_str[i] = strdup(rbuf);
			clr_edit();
			printerr(NULL, "%s"
#endif
			    " saved for F%d", sh_str[i], i);
			goto next_key;
		}

		switch (c) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			proc_mevent();
			break;
#endif
		case 'q':
			return;
		case KEY_DOWN:
		case 'j':
		case '+':
			c = 0;
			curs_down();
			break;
		case KEY_UP:
		case 'k':
		case '-':
			c = 0;
			curs_up();
			break;
		case KEY_LEFT:
			if (bmode)
				enter_dir("..", NULL, 1);
			else
				pop_state();

			break;
		case KEY_RIGHT:
		case '\n':
			if (!db_num) {
				no_file();
				break;
			}

			action();
			break;
		case KEY_NPAGE:
		case ' ':
			c = 0;
			page_down();
			break;
		case KEY_PPAGE:
		case KEY_BACKSPACE:
			page_up();
			break;
		case 'h':
		case '?':
			c = 0;
			help();
			break;
		case 'p':
			if (*key == 'e') {
				c = 0;
				fs_chmod(3);
				break;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					c = 0;
					fs_chmod(1);
					break;
				} else if (*key == 'r') {
					c = 0;
					fs_chmod(2);
					break;
				}
			}

			c = 0;

			if (bmode)
				break;

			lpath[llen] = 0;
			rpath[rlen] = 0;

			if (!*pwd && !*rpwd)
				printerr(NULL, "At top directory");
			else if (!*pwd || !rpwd || !strcmp(PWD, RPWD))
				printerr(NULL, "%s", *pwd ? PWD : RPWD);
			else {
				werase(wstat);
				statcol();
				mvwaddstr(wstat, 0, 2, PWD);
				mvwaddstr(wstat, 1, 2, RPWD);
				wrefresh(wstat);
			}

			break;
		case 'f':
			c = 0;

			if (bmode)
				break;

			lpath[llen] = 0;
			rpath[rlen] = 0;
			werase(wstat);
			statcol();
			mvwaddstr(wstat, 0, 2, lpath);
			mvwaddstr(wstat, 1, 2, rpath);
			wrefresh(wstat);
			break;
		case 'a':
			c = 0;

			if (bmode)
				break;

			werase(wstat);
			statcol();
			mvwaddstr(wstat, 0, 2, arg[0]);
			mvwaddstr(wstat, 1, 2, arg[1]);
			wrefresh(wstat);
			break;
		case '!':
		case 'n':
			if (c == 'n') {
				if (*key == 'e') {
					c = 0;
					fs_rename(3);
					break;
				} else if (key[1] == 'e') {
					if (*key == 'l') {
						c = 0;
						fs_rename(1);
						break;
					} else if (*key == 'r') {
						c = 0;
						fs_rename(2);
						break;
					}
				}
			}

			c = 0;
			noequal = noequal ? 0 : 1;
			diff_db_sort();
			top_idx = 0;
			curs    = 0;
			disp_list();
			break;
		case 'c':
			c = 0;
			real_diff = real_diff ? 0 : 1;
			diff_db_sort();
			top_idx = 0;
			curs    = 0;
			disp_list();
			break;
		case KEY_RESIZE:
			ui_resize();
			break;
		case 'd':
			if (*key != 'd')
				break;
			c = 0;
			fs_rm(3, NULL); /* allowed for single sided only */
			break;
		case 'l':
			if (*key == 'd') {
				c = 0;
				fs_rm(1, NULL);
			} else if (*key == 's') {
				c = 0;
				open_sh(1);
			}

			break;
		case 'r':
			if (*key == 'd') {
				c = 0;
				fs_rm(2, NULL);
				break;
			} else if (*key == 's') {
				c = 0;
				open_sh(2);
				break;
			}

			if (mark) {
				c = 0;
				clr_mark();
			} else if (edit) {
				c = 0;
				clr_edit();
			}

			break;
		case '<':
			if (*key != '<')
				break;
			c = 0;
			fs_cp(1);
			break;
		case '>':
			if (*key != '>')
				break;
			c = 0;
			fs_cp(2);
			break;
		case KEY_HOME:
			curs_first();
			break;
		case 'G':
			if (*key == '1') {
				c = 0;
				curs_first();
				break;
			}
			/* fall-through */
		case KEY_END:
			c = 0;
			curs_last();
			break;
		case 'm':
			c = 0;
			set_mark();
			break;
		case 'y':
			c = 0;
			yank_name(0);
			break;
		case 'Y':
			c = 0;
			yank_name(1);
			break;
		case '$':
			c = 0;
			if (!ed_dialog("Type command (<ESC> to cancel):",
			    NULL /* must be NULL !!! */, NULL, 0))
				sh_cmd(rbuf, 1);
			break;
		case 'e':
			break;
		case '/':
			if (!db_num) {
				no_file();
				break;
			}

			if (sorting != SORTMIXED) {
				prev_sorting = sorting;
				sorting = SORTMIXED;
				rebuild_db();
			}

			srch_idx = 0;
			ed_dialog("Type first characters of filename:",
			    "" /* remove existing */, srch_file, 0);
			sorting = prev_sorting;
			break;
		case 'u':
			c = 0;
			rebuild_db();
			break;
		case 's':
			c = 0;
			open_sh(3);
			break;
		default:
			printerr(NULL, "Invalid input '%c' (type h for help).",
			    isgraph(c) ? c : '?');
		}
	}
}

static char *helptxt[] = {
       "q		Quit",
       "h, ?		Display help",
       "<UP>, k, -	Move cursor up",
       "<DOWN>, j, +	Move cursor down",
       "<LEFT>		Leave directory (one directory up)",
       "<RIGHT>, <ENTER>",
       "		Enter directory or start diff tool",
       "<PG-UP>, <BACKSPACE>",
       "		Scroll one screen up",
       "<PG-DOWN>, <SPACE>",
       "		Scroll one screen down",
       "<HOME>, 1G	Go to first file",
       "<END>, G	Go to last file",
       "/		Search file",
       "!, n		Toggle display of equal files",
       "c		Toggle showing only directories and really different files",
       "p		Show current relative work directory",
       "a		Show command line directory arguments",
       "f		Show full path",
       "<<		Copy from second to first tree",
       ">>		Copy from first to second tree",
       "dd		Delete file or directory",
       "dl		Delete file or directory in first tree",
       "dr		Delete file or directory in second tree",
       "en		Rename file",
       "eln		Rename left file",
       "ern		Rename right file",
       "ep		Change file mode",
       "elp		Change mode of left file",
       "erp		Change mode of right file",
       "m		Mark file or directory",
       "r		Remove edit line or mark",
       "y		Copy file path to edit line",
       "Y		Copy file path in reverse order to edit line",
       "$		Enter shell command",
       "<F1> - <F10>	Define string to be used in shell command",
       "u		Update file list",
       "s		Open shell",
       "sl		Open shell in left directory",
       "sr		Open shell in right directory" };

#define HELP_NUM (sizeof(helptxt) / sizeof(*helptxt))

static void
help(void) {

	int c;

	help_top = 0;
	disp_help();

	while (1) {
		switch (c = getch()) {
#if NCURSES_MOUSE_VERSION >= 2
		case KEY_MOUSE:
			help_mevent();
			break;
#endif
		case 'q':
			goto exit;
		case KEY_DOWN:
		case 'j':
		case '+':
			help_down();
			break;
		case KEY_UP:
		case 'k':
		case '-':
			help_up();
			break;
		case KEY_NPAGE:
		case ' ':
			help_pg_down();
			break;
		case KEY_PPAGE:
		case KEY_BACKSPACE:
			help_pg_up();
			break;
		}
	}

exit:
	disp_list();
}

#if NCURSES_MOUSE_VERSION >= 2
static void
help_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

	if (mevent.bstate & BUTTON4_PRESSED)
		help_up();
	else if (mevent.bstate & BUTTON5_PRESSED)
		help_down();
}
#endif

static void
disp_help(void)
{
	unsigned y, i;

	werase(wlist);
	werase(wstat);

	for (y = 0, i = help_top; y < listh && i < HELP_NUM; y++, i++)
		mvwaddstr(wlist, y, 0, helptxt[i]);

	if (!help_top && HELP_NUM > listh)
		printerr(NULL, "Scroll down for more");

	wrefresh(wlist);
	wrefresh(wstat);
}

static void
help_pg_down(void)
{
	if (help_top + listh >= HELP_NUM) {
		printerr(NULL, "At bottom");
		return; /* last line on display */
	}

	help_top += listh;
	disp_help();
}

static void
help_pg_up(void)
{
	if (!help_top) {
		printerr(NULL, "At top");
		return;
	}

	help_top = help_top > listh ? help_top - listh : 0;
	disp_help();
}

static void
help_down(void)
{
	if (!scrollen) {
		help_pg_down();
		return;
	}

	if (help_top + listh >= HELP_NUM) {
		printerr(NULL, "At bottom");
		return; /* last line on display */
	}

	wscrl(wlist, 1);
	help_top++;
	mvwaddstr(wlist, listh - 1, 0, helptxt[help_top + listh - 1]);
	werase(wstat);
	wrefresh(wstat);
	wrefresh(wlist);
}

static void
help_up(void)
{
	if (!scrollen) {
		help_pg_up();
		return;
	}

	if (!help_top) {
		printerr(NULL, "At top");
		return;
	}

	wscrl(wlist, -1);
	help_top--;
	mvwaddstr(wlist, 0, 0, helptxt[help_top]);
	werase(wstat);
	wrefresh(wstat);
	wrefresh(wlist);
}

#ifdef NCURSES_MOUSE_VERSION
static void
proc_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED ||
	    mevent.bstate & BUTTON1_PRESSED) {
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

	if (mark) {
		mode_t ltyp = 0, rtyp = 0;
		char *lnam, *rnam;

		if (mark->ltype) {
			if (f->ltype) {
				printerr(NULL, "Both files in same directory");
				return;
			}

			lnam = mark->name;
			ltyp = mark->ltype;
			rnam = f->name;
			rtyp = f->rtype;

		} else if (mark->rtype) {
			if (f->rtype) {
				printerr(NULL, "Both files in same directory");
				return;
			}

			rnam = mark->name;
			rtyp = mark->rtype;
			lnam = f->name;
			ltyp = f->ltype;
		}

		if ((ltyp & S_IFMT) != (rtyp & S_IFMT)) {
			printerr(NULL, "Different file type");
			return;
		}

		if (S_ISREG(ltyp))
			tool(lnam, rnam, 3);
		else if (S_ISDIR(ltyp)) {
			enter_dir(lnam, rnam, 3);
		}

		return;
	}

	if ((f->ltype & S_IFMT) == (f->rtype & S_IFMT)) {
		if (S_ISDIR(f->ltype))
			enter_dir(f->name, NULL, 3);
		else if (S_ISREG(f->ltype)) {
			if (f->diff == '!')
				tool(f->name, NULL, 3);
			else
				tool(f->name, NULL, 1);
		} else
			goto typerr;
	} else if (!f->ltype) {
		if (S_ISDIR(f->rtype))
			enter_dir(f->name, NULL, 2);
		else if (S_ISREG(f->rtype))
			tool(f->name, NULL, 2);
		else
			goto typerr;
	} else if (!f->rtype) {
		if (S_ISDIR(f->ltype))
			enter_dir(f->name, NULL, 1);
		else if (S_ISREG(f->ltype))
			tool(f->name, NULL, 1);
		else
			goto typerr;
	} else
		printerr(NULL, "Different file type");

	return;

typerr:
	printerr(NULL, "Not a directory or regular file");
}

static void
page_down(void)
{
	if (last_line_is_disp()) {
		printerr(NULL, "At bottom");
		return;
	}

	top_idx += listh;
	curs = 0;
	disp_list();
}

static void
curs_last(void)
{
	if (last_line_is_disp())
		return;

	top_idx = db_num - listh;
	curs = listh - 1;
	disp_list();
}

static void
page_up(void)
{
	if (first_line_is_top()) {
		printerr(NULL, "At top");
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
curs_first(void)
{
	if (first_line_is_top())
		return;

	top_idx = curs = 0;
	disp_list();
}

static int
last_line_is_disp(void)
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
		return 1;
	}

	return 0;
}

static int
first_line_is_top(void)
{
	if (!top_idx) {
		if (curs) {
			disp_curs(0);
			curs = 0;
			disp_curs(1);
			wrefresh(wlist);
			wrefresh(wstat);
		}
		return 1;
	}

	return 0;
}

static void
curs_down(void)
{
	if (top_idx + curs + 1 >= db_num) {
		printerr(NULL, "At bottom");
		return;
	}

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
		if (!top_idx) {
			printerr(NULL, "At top");
			return;
		}

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
		if (!curs) {
			printerr(NULL, "At top");
			return;
		}

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
		disp_line(y, i, 0);

	if (move_curs)
		disp_curs(1);
	wrefresh(wlist);
	wrefresh(wstat);
}

static void
scroll_down(unsigned num)
{
	unsigned move_curs, y, i;

	if (top_idx >= db_num - 1) {
		printerr(NULL, "At bottom");
		return;
	}

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
		disp_line(y, i, 0);

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
	disp_line(curs, top_idx + curs, a);
	if (a)
		wattroff(wlist, A_REVERSE);
}

void
disp_list(void)
{
	unsigned y, i;

	/* For the case that entries had been removed */
	if (top_idx >= db_num)
		top_idx = db_num ? db_num - 1 : 0;

	if (top_idx + curs >= db_num)
		curs = db_num ? db_num - top_idx - 1 : 0;

	werase(wlist);

	if (!db_num) {
		no_file();
		goto exit;
	}

	for (y = 0, i = top_idx; y < listh && i < db_num; y++, i++) {
		if (y == curs)
			disp_curs(1);
		else
			disp_line(y, i, 0);
	}

exit:
	wrefresh(wlist);
	wrefresh(wstat);
}

static void
disp_line(unsigned y, unsigned i, int info)
{
	int diff, type;
	mode_t mode;
	struct filediff *f = db_list[i];
	char *link = NULL;
	short color_id = 0;

	if (bmode) {
		goto no_diff;
	} else if (!f->ltype) {
		diff     = '>';
		mode     = f->rtype;
		color_id = COLOR_RIGHTONLY;
	} else if (!f->rtype) {
		diff     = '<';
		mode     = f->ltype;
		color_id = COLOR_LEFTONLY;
	} else if ((f->ltype & S_IFMT) != (f->rtype & S_IFMT)) {
		diff = ' ';
		mode = 0;
		type = '!';
		color_id = COLOR_DIFF;
	} else {
no_diff:
		diff = f->diff;
		mode = f->ltype;
		if (diff == '!')
			color_id = COLOR_DIFF;
	}

	if (S_ISREG(mode)) {
		if (mode & 0100)
			type = '*';
		else
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
			link = f->llink ? f->llink : f->rlink;
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
	if (bmode)
		mvwprintw(wlist, y, 0, "%c %s", type, f->name);
	else
		mvwprintw(wlist, y, 0, "%c %c %s", diff, type, f->name);
	wattrset(wlist, A_NORMAL);
	if (link) {
		waddstr(wlist, " -> ");
		waddstr(wlist, link);
	}

	if (!info)
		return;

	if (mark) {
		if (wstat_dirty)
			disp_mark();
		return;
	} else if (edit) {
		if (wstat_dirty)
			disp_edit();
		return;
	}

	werase(wstat);
	if (diff == '!' && type == '@') {
		statcol();
		mvwaddstr(wstat, 0, 2, f->llink);
		mvwaddstr(wstat, 1, 2, f->rlink);
	} else if (diff == ' ' && type == '!') {
		statcol();
		mvwaddstr(wstat, 0, 2, type_name(f->ltype));
		mvwaddstr(wstat, 1, 2, type_name(f->rtype));
	} else if (type == '/' || type == ' ' || type == '*') {
		statcol();
		file_stat(f);
	}
}

static void
statcol(void)
{
	if (bmode)
		return;

	wattron(wstat, A_REVERSE);
	mvwaddch(wstat, 0, 0, '<');
	mvwaddch(wstat, 1, 0, '>');
	wattroff(wstat, A_REVERSE);
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
file_stat(struct filediff *f)
{
	int x = 2, w, w1, w2, yl;
	struct passwd *pw;
	struct group *gr;

	x  = bmode ? 0 : 2;
	yl = 0;

	if (bmode) {
		/* TODO: Right align */
		mvwaddstr(wstat, 1, 0, rpath);
	}

	if (f->ltype)
		mvwprintw(wstat, yl, x, "%04o", f->ltype & 07777);
	if (f->rtype)
		mvwprintw(wstat, 1, x, "%04o", f->rtype & 07777);
	x += 5;

	if (f->ltype) {
		if ((pw = getpwuid(f->luid)))
			memcpy(lbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->luid);
	}

	if (f->rtype) {
		if ((pw = getpwuid(f->ruid)))
			memcpy(rbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(rbuf, sizeof rbuf, "%u", f->ruid);
	}

	if (f->ltype)
		mvwaddstr(wstat, yl, x, lbuf);
	if (f->rtype)
		mvwaddstr(wstat, 1, x, rbuf);

	w1 = f->ltype ? strlen(lbuf) : 0;
	w2 = f->rtype ? strlen(rbuf) : 0;
	x += w1 > w2 ? w1 : w2;
	x++;

	if (f->ltype) {
		if ((gr = getgrgid(f->lgid)))
			memcpy(lbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->lgid);
	}

	if (f->rtype) {
		if ((gr = getgrgid(f->rgid)))
			memcpy(rbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(rbuf, sizeof rbuf, "%u", f->rgid);
	}

	if (f->ltype)
		mvwaddstr(wstat, yl, x, lbuf);
	if (f->rtype)
		mvwaddstr(wstat, 1, x, rbuf);

	w1 = f->ltype ? strlen(lbuf) : 0;
	w2 = f->rtype ? strlen(rbuf) : 0;
	x += w1 > w2 ? w1 : w2;
	x++;

	if (f->ltype && !S_ISDIR(f->ltype))
		w1 = snprintf(lbuf, sizeof lbuf, "%lld", (long long)f->lsiz);
	if (f->rtype && !S_ISDIR(f->rtype))
		w2 = snprintf(rbuf, sizeof lbuf, "%lld", (long long)f->rsiz);

	w = w1 > w2 ? w1 : w2;

	if (f->ltype && !S_ISDIR(f->ltype))
		mvwaddstr(wstat, yl, x + w - w1, lbuf);
	if (f->rtype && !S_ISDIR(f->rtype))
		mvwaddstr(wstat, 1, x + w - w2, rbuf);

	x += w + 1;

	if (f->ltype && !S_ISDIR(f->ltype))
		mvwaddstr(wstat, yl, x, ctime(&f->lmtim));
	if (f->rtype && !S_ISDIR(f->rtype))
		mvwaddstr(wstat, 1, x, ctime(&f->rmtim));
}

static void
set_mark(void)
{
	struct filediff *f = db_list[top_idx + curs];
	mode_t mode = f->ltype ? f->ltype : f->rtype;

	if (f->ltype && f->rtype) {
		printerr(NULL, "Both files present");
		return;
	}

	if (!S_ISDIR(mode) && !(S_ISREG(mode))) {
		printerr(NULL, "Not a directory or regular file");
		return;
	}

	mark = f;
	disp_mark();
}

static void
disp_mark(void)
{
	werase(wstat);
	wattrset(wstat, A_REVERSE);
	mvwaddstr(wstat, 1, 0, mark->name);
	wattrset(wstat, A_NORMAL);
	wrefresh(wstat);
}

static void
clr_mark(void)
{
	mark = NULL;
	werase(wstat);
	wrefresh(wstat);
}

#define YANK(x) \
	do { \
		if (f->x##type) { \
			ed_append(" "); \
			/* here because 1st ed_append deletes lbuf */ \
			shell_quote(lbuf, x##path, sizeof lbuf); \
			ed_append(lbuf); \
			*lbuf = 0; \
		} \
	} while (0)

static void
yank_name(int reverse)
{
	struct filediff *f;

	f = db_list[top_idx + curs];

	if (f->ltype)
		pthcat(lpath, llen, f->name);

	if (f->rtype)
		pthcat(rpath, rlen, f->name);

	if (reverse)
		YANK(r);

	YANK(l);

	if (!reverse)
		YANK(r);

	disp_edit();
}

static void
no_file(void)
{
	if (real_diff && !bmode)
		printerr(NULL, "No file (type c to view all files).");
	else
		printerr(NULL, "No file");
}

static void
srch_file(char *pattern)
{
	unsigned idx;
	int o, oo;


	if (!*pattern || !db_num)
		return;

	if (srch_idx >= db_num)
		srch_idx = db_num - 1;

	idx = srch_idx;
	o = 0;

	while (1) {
		oo = o;

		if (!(o = strncmp(db_list[idx]->name, pattern, strlen(pattern)))) {
			center(srch_idx = idx);
			break;
		} else if (o < 0) {
			if (oo > 0 || ++idx >= db_num)
				break;
		} else if (o > 0) {
			if (oo < 0 || !idx)
				break;

			idx--;
		}
	}
}

static void
center(unsigned idx)
{
	if (db_num <= listh || idx <= listh / 2)
		top_idx = 0;
	else if (db_num - idx < listh / 2)
		top_idx = db_num - listh;
	else
		top_idx = idx - listh / 2;

	curs = idx - top_idx;
	disp_list();
}

static void
push_state(void)
{
	struct ui_state *st = malloc(sizeof(struct ui_state));
	diff_db_store(st);
	st->llen    = llen;
	st->rlen    = rlen;
	st->top_idx = top_idx;  top_idx = 0;
	st->curs    = curs;     curs    = 0;
	st->mark    = mark;     mark    = NULL;
	st->next    = ui_stack;
	ui_stack    = st;
}

static void
pop_state(void)
{
	struct ui_state *st = ui_stack;

	if (!st) {
		printerr(NULL, "At top directory");
		return;
	}

	ui_stack = st->next;
	llen     = st->llen;
	rlen     = st->rlen;
	top_idx  = st->top_idx;
	curs     = st->curs;
	mark     = st->mark;
	lpath[llen] = 0; /* For 'p' (pwd) */
	diff_db_restore(st);
	free(st);
	disp_list();
}

static void
enter_dir(char *name, char *rnam, int tree)
{
	if (!bmode) {
		push_state();
		scan_subdir(name, rnam, tree);
	} else {
		struct bst_node *n;
		unsigned *uv;

		db_set_curs(rpath, top_idx, curs);

		if (chdir(name) == -1) {
			printerr(strerror(errno),
			    "chdir %s failed", name);
			return;
		}

		top_idx = 0;
		curs = 0;
		mark = NULL;
		diff_db_free();
		scan_subdir(NULL, NULL, 1);

		if ((n = db_srch_str(curs_db, rpath))) {
			uv = n->data.p;
			top_idx = *uv++;
			curs    = *uv;
		}
	}

	disp_list();
}

void
printerr(char *s2, char *s1, ...)
{
	va_list ap;

	wstat_dirty = 1;
	werase(wstat);
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

	wstat_dirty = 1;
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
