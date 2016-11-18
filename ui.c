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
#include <regex.h>
#include <time.h>
#ifdef USE_SYS_MKDEV_H
# include <sys/mkdev.h>
#endif
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "uzp.h"
#include "exec.h"
#include "db.h"
#include "fs.h"
#include "ed.h"
#include "ui2.h"
#include "tc.h"

static void ui_ctrl(void);
static void page_down(void);
static void page_up(void);
static void curs_last(void);
static void curs_first(void);
static int last_line_is_disp(void);
static int first_line_is_top(void);
static void curs_down(void);
static void curs_up(void);
static void disp_line(unsigned, unsigned, int);
static void push_state(char *, char *, bool, bool);
static void pop_state(short);
static void enter_dir(char *, char *, bool, bool);
static void help(void);
static char *type_name(mode_t);
static void ui_resize(void);
static void set_win_dim(void);
static void statcol(void);
static void file_stat(struct filediff *, struct filediff *);
static void set_file_info(struct filediff *, mode_t, int *, short *, int *);
static int disp_name(WINDOW *, int, int, int, int, struct filediff *, int,
    short, char *, int);
static size_t getfilesize(char *, size_t, off_t);
static size_t gettimestr(char *, size_t, time_t *);
static void disp_help(void);
static void help_pg_down(void);
static void help_pg_up(void);
static void help_down(short);
static void help_up(unsigned short);
static void set_mark(void);
static void disp_mark(void);
static void yank_name(int);
static void scroll_up(unsigned, bool);
static void scroll_down(unsigned, bool);

short color = 1;
short color_leftonly  = COLOR_CYAN   ,
      color_rightonly = COLOR_GREEN  ,
      color_diff      = COLOR_RED    ,
      color_dir       = COLOR_YELLOW ,
      color_unknown   = COLOR_BLUE   ,
      color_link      = COLOR_MAGENTA,
      color_normal    = COLOR_WHITE  ,
      color_cursor_fg = COLOR_BLACK  ,
      color_cursor_bg = COLOR_WHITE  ,
      color_error_fg  = COLOR_WHITE  ,
      color_error_bg  = COLOR_RED    ,
      color_mark_fg   = COLOR_WHITE  ,
      color_mark_bg   = COLOR_BLUE   ,
      color_bg        = COLOR_BLACK  ;
unsigned top_idx, curs, statw;

#ifdef HAVE_CURSES_WCH
wchar_t *sh_str[FKEY_NUM];
#else
char *sh_str[FKEY_NUM];
#endif
char *fkey_cmd[FKEY_NUM];
unsigned fkey_flags[FKEY_NUM];

static unsigned listw, help_top;
unsigned listh;
WINDOW *wlist;
WINDOW *wstat;
static struct ui_state *ui_stack;
/* Line scroll enable. Else only full screen is scrolled */
struct filediff *mark;
char *gl_mark, *mark_lnam, *mark_rnam;
static struct history sh_cmd_hist;

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(void);
# if NCURSES_MOUSE_VERSION >= 2
static void help_mevent(void);
# endif

static MEVENT mevent;
#endif

static bool scrollen = TRUE;
static bool wstat_dirty;
static bool dir_change;

void
build_ui(void)
{
	if (qdiff)
		goto do_diff;

	initscr();

	if (color && (!has_colors() || start_color() == ERR))
		color = 0;

	if (color) {
		init_pair(PAIR_LEFTONLY , color_leftonly , color_bg       );
		init_pair(PAIR_RIGHTONLY, color_rightonly, color_bg       );
		init_pair(PAIR_DIFF     , color_diff     , color_bg       );
		init_pair(PAIR_DIR      , color_dir      , color_bg       );
		init_pair(PAIR_UNKNOWN  , color_unknown  , color_bg       );
		init_pair(PAIR_LINK     , color_link     , color_bg       );
		init_pair(PAIR_NORMAL   , color_normal   , color_bg       );
		init_pair(PAIR_CURSOR   , color_cursor_fg, color_cursor_bg);
		init_pair(PAIR_ERROR    , color_error_fg , color_error_bg );
		init_pair(PAIR_MARK     , color_mark_fg  , color_mark_bg  );
		init_pair(PAIR_HEADLINE , COLOR_BLACK    , COLOR_YELLOW   );
		bkgd(COLOR_PAIR(PAIR_NORMAL));
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

	if (fmode)
		open2cwins();

	if (scrollen) {
		idlok(wlist, TRUE);
		scrollok(wlist, TRUE);

		if (fmode) {
			idlok(wllst, TRUE);
			scrollok(wllst, TRUE);
			idlok(wrlst, TRUE);
			scrollok(wrlst, TRUE);
			idlok(wmid, TRUE);
			scrollok(wmid, TRUE);
		}
	}

do_diff:
	/* Not in main since build_diff_db() uses printerr() */
	if (recursive) {
		scan = 1;
		build_diff_db(bmode ? 1 : 3);
		scan = 0;

		if (qdiff)
			return;
	}

	if (bmode) {
		if (chdir(lpath) == -1) {
			printerr(strerror(errno), "chdir \"%s\" failed", lpath);
			goto exit;
		}

		*lpath = '.';
		lpath[1] = 0;
		llen = 1;
	}

	if (bmode)
		build_diff_db(1);
	else if (fmode) {
		build_diff_db(1);
		build_diff_db(2);
	} else
		build_diff_db(3);

	if (qdiff)
		return;

	disp_fmode();

	if (twocols)
		prt2chead();

	ui_ctrl();

	/* Change out of tmpdirs before deleting them. */
	if (chdir("/") == -1)
		printerr(strerror(errno), "chdir \"/\" failed");

	/* if !bmode: remove tmp_dirs */
	while (ui_stack)
		pop_state(0);

	/* if bmode: remove tmp_dirs */
	uz_exit();

exit:
	erase();
	refresh();
	endwin();
}

static void
ui_ctrl(void)
{
	static struct history opt_hist;
	int key[2] = { 0, 0 }, c = 0, i;
	unsigned short num;
	long u;
	struct filediff *f;

	while (1) {
next_key:
		key[1] = *key;
		*key = c;

		if (!c) {
			num = 1;
			u = top_idx + curs;
		}

		if ((c = getch()) == ERR)
			goto next_key;

		if (test_fkey(c, u, num)) {
			c = 0;
			goto next_key;
		}

		/* case '1' see below */
		for (i = '2'; i <= '9'; i++)
			if (c == i) {
				num = i - '0';
				goto next_key;
			}

		switch (c) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			c = 0;
			proc_mevent();
			break;
#endif
		case '1':
			/* !!! DON'T USE c = 0 !!!
			 * else "1G" won't work anymore! */
			break;
		case 'q':
			if (dialog(y_n_txt, NULL,
			    "Quit vddiff?") != 'y')
				break;

			/* fall through */
		case 'Q':
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

			if (*key == 'z') {
				if (!top_idx)
					break;
				else if (top_idx >= listh - 1 - curs) {
					top_idx -= listh - 1 - curs;
					curs = listh - 1;
				} else {
					curs += top_idx;
					top_idx = 0;
				}

				disp_list();
				break;
			}

			curs_up();
			break;
		case KEY_LEFT:
			if (*key == '|') {
				c = *key;

				if (COLS / 2 + midoffs < 0)
					break;

				midoffs -= 10;
				set_win_dim();

				if (fmode) {
					resize_fmode();
				} else
					disp_list();

				break;
			}

			c = 0;

			if (bmode || fmode)
				enter_dir("..", NULL, FALSE, FALSE);
			else
				pop_state(1);

			break;
		case '\n':
			if (*key == 'z') {
				if (!curs) {
					c = 0;
					break;
				}

				top_idx += curs;
				curs = 0;
				disp_list();
				c = 0;
				break;
			}

			/* fall through */
		case KEY_RIGHT:
			if (*key == '|') {
				c = *key;

				if (midoffs > COLS / 2)
					break;

				midoffs += 10;
				set_win_dim();

				if (fmode) {
					resize_fmode();
				} else
					disp_list();

				break;
			}

			if (!db_num) {
				no_file();
				c = 0;
				break;
			}

			action(0, 3, c == '\n' ? 1 : 0, FALSE);
			c = 0;
			break;
		case '=':
			if (*key == '|') {
				c = *key;

				if (!midoffs)
					break;

				midoffs = 0;
				set_win_dim();
				disp_list();
				break;
			}

			break;
		case 'b':
			c = 0;
			bindiff();
			break;
		case KEY_NPAGE:
		case ' ':
			c = 0;
			page_down();
			break;
		case KEY_PPAGE:
		case KEY_BACKSPACE:
		case CERASE:
			c = 0;
			page_up();
			break;
		case CTRL('e'):
			scroll_down(1, FALSE);
			break;
		case CTRL('y'):
			scroll_up(1, FALSE);
			break;
		case CTRL('d'):
			scroll_down(listh/2, TRUE);
			break;
		case CTRL('u'):
			scroll_up(listh/2, TRUE);
			break;
		case 'h':
		case '?':
			c = 0;
			help();
			break;
		case 'p':
			if (*key == 'e') {
				c = 0;
				fs_chmod(3, u, num);
				break;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					c = 0;
					fs_chmod(1, u, num);
					break;
				} else if (*key == 'r') {
					c = 0;
					fs_chmod(2, u, num);
					break;
				}
			}

			if (!bmode) {
				c = 0;
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
			}

			/* fall through */
		case 'f':
			if (!bmode) {
				c = 0;
				lpath[llen] = 0;
				rpath[rlen] = 0;
				werase(wstat);
				statcol();
				mvwaddstr(wstat, 0, 2, lpath);
				mvwaddstr(wstat, 1, 2, rpath);
				wrefresh(wstat);
				break;
			}

			/* fall through */
		case 'a':
			c = 0;

			if (bmode) {
				if (!getcwd(rpath, sizeof rpath)) {
					printerr(strerror(errno),
					    "getcwd failed");
					break;
				}

				werase(wstat);
				mvwaddstr(wstat, 1, 0, rpath);
				wrefresh(wstat);
				break;
			}

			werase(wstat);
			statcol();
			mvwaddstr(wstat, 0, 2, arg[0]);
			mvwaddstr(wstat, 1, 2, arg[1]);
			wrefresh(wstat);
			break;
		case 'n':
			c = 0;

			if (regex) {
				regex_srch(1);
				break;
			} else if (*key == 'e') {
				fs_rename(3);
				break;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					fs_rename(1);
					break;
				} else if (*key == 'r') {
					fs_rename(2);
					break;
				}
			}

			/* fall through */
		case '!':
			c = 0;
			noequal = noequal ? 0 : 1;
			re_sort_list();
			break;
		case 'c':
			c = 0;
			real_diff = real_diff ? 0 : 1;
			re_sort_list();
			break;
		case '&':
			c = 0;
			nosingle = nosingle ? 0 : 1;
			re_sort_list();
			break;
		case 'E':
			c = 0;
			file_pattern = file_pattern ? FALSE : TRUE;
			re_sort_list();
			break;
		case KEY_RESIZE:
			ui_resize();
			break;
		case 'P':
			if (bmode) {
				c = 0;
				fs_mkdir(1);
				break;
			}

			break;
		case 'd':
			if (*key == 'S') {
				c = 0;

				if (sorting == DIRSFIRST)
					break;

				sorting = DIRSFIRST;
				rebuild_db(1);
				break;

			} else if (*key != 'd')
				break;

			c = 0;
			/* allowed for single sided only */
			fs_rm(3, NULL, u, num);
			break;
		case 't':
			if (*key == 'S') {
				c = 0;

				if (sorting == SORTMTIME)
					break;

				sorting = SORTMTIME;
				rebuild_db(1);
				break;
			}

			break;
		case 'v':
			if (!db_num)
				break;

			f = db_list[top_idx + curs];

			if (S_ISREG(f->ltype) && !S_ISREG(f->rtype)) {
				c = 0;
				action(0, 1, 0, TRUE);
				break;
			}

			if (S_ISREG(f->rtype) && !S_ISREG(f->ltype)) {
				c = 0;
				action(0, 2, 0, TRUE);
				break;
			}

			break;
		case 'l':
			switch (*key) {
			case 'd':
				c = 0;
				fs_rm(1, NULL, u, num);
				goto next_key;
			case 's':
				c = 0;
				open_sh(1);
				goto next_key;
			case 'v':
				c = 0;
				action(0, 1, 0, TRUE);
				goto next_key;
			case 'o':
				c = 0;
				action(0, 1, 0, FALSE);
				goto next_key;
			case 'e':
				goto next_key;
			case 'P':
				c = 0;
				fs_mkdir(1);
				goto next_key;
			default:
				;
			}

			c = 0;
			standendc(wlist);
			werase(wlist);

			for (i = 0;
			    i < (ssize_t)(sizeof(sh_str)/sizeof(*sh_str));
			    i++) {
				mvwprintw(wlist, i, 0, "F%d", i + 1);

				if (fkey_cmd[i]) {
					mvwprintw(wlist, i, 5,
					    "\"%c %s\"", FKEY_CMD_CHR(i),
					    fkey_cmd[i]);
					continue;
				}

				if (!sh_str[i])
					continue;

				mvwprintw(wlist, i, 5,
#ifdef HAVE_CURSES_WCH
				    "\"%ls\""
#else
				    "\"%s\""
#endif
				    , sh_str[i]);
			}

			anykey();
			break;
		case 'r':
			switch (*key) {
			case 'd':
				c = 0;
				fs_rm(2, NULL, u, num);
				goto next_key;
			case 's':
				c = 0;
				open_sh(2);
				goto next_key;
			case 'v':
				c = 0;
				action(0, 2, 0, TRUE);
				goto next_key;
			case 'o':
				c = 0;
				action(0, 2, 0, FALSE);
				goto next_key;
			case 'e':
				goto next_key;
			case 'P':
				c = 0;
				fs_mkdir(2);
				goto next_key;
			default:
				;
			}

			if (mark) {
				c = 0;
				clr_mark();
			} else if (edit) {
				c = 0;
				clr_edit();
			} else if (regex) {
				c = 0;
				clr_regex();
			}

			break;
		case '<':
			if (*key != '<')
				break;

			c = 0;
			fs_cp(1, u, num);
			break;
		case '>':
			if (*key != '>')
				break;

			c = 0;
			fs_cp(2, u, num);
			break;
		case KEY_HOME:
			c = 0;
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
			if (*key == 'S') {
				c = 0;

				if (sorting == SORTMIXED)
					break;

				sorting = SORTMIXED;
				rebuild_db(1);
				break;
			}

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
			    NULL /* must be NULL !!! */, NULL, 0,
			    &sh_cmd_hist) && *rbuf) {
				char *s = rbuf;

				exec_cmd(&s, TOOL_WAIT | TOOL_NOLIST |
				    TOOL_TTY | TOOL_SHELL, NULL, NULL);
			}

			/* exec_cmd() did likely create or delete files */
			rebuild_db(0);
			break;
		case 'e':
			break;
		case '/':
			c = 0;
			ui_srch();
			break;
		case 'S':
			if (*key == 'S') {
				c = 0;

				if (sorting == SORTSIZE)
					break;

				sorting = SORTSIZE;
				rebuild_db(1);
				break;
			}

			break;
		case 'u':
			if (*key == 'e') {
				c = 0;
				fs_chown(3, 0, u, num);
				break;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					c = 0;
					fs_chown(1, 0, u, num);
					break;
				} else if (*key == 'r') {
					c = 0;
					fs_chown(2, 0, u, num);
					break;
				}
			}

			c = 0;
			rebuild_db(0);
			break;
		case 'g':
			if (*key == 'e') {
				c = 0;
				fs_chown(3, 1, u, num);
				break;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					c = 0;
					fs_chown(1, 1, u, num);
					break;
				} else if (*key == 'r') {
					c = 0;
					fs_chown(2, 1, u, num);
					break;
				}
			}

			break;
		case 's':
			/* Don't clear c here! Else sl and sr won't work! */
			open_sh(3);
			break;
		case 'z':
			break;
		case '.':
			if (*key == 'z') {
				c = 0;
				center(top_idx + curs);
			}

			break;
		case 'F':
			c = 0;
			followlinks = followlinks ? 0 : 1;
			rebuild_db(1);
			break;
		case 'H':
			c = 0;

			if (!curs)
				break;

			curs = 0;
			disp_list();
			break;
		case 'M':
			c = 0;

			if (db_num < top_idx + listh)
				curs = (db_num - top_idx) / 2;
			else
				curs = listh / 2;

			disp_list();
			break;
		case 'L':
			c = 0;

			if (db_num < top_idx + listh)
				curs = db_num - top_idx - 1;
			else
				curs = listh - 1;

			disp_list();
			break;
		case 'o':
			if (!db_num)
				break;

			f = db_list[top_idx + curs];

			if (S_ISREG(f->ltype) && !S_ISREG(f->rtype)) {
				c = 0;
				action(0, 1, 0, FALSE);
				break;
			}

			if (S_ISREG(f->rtype) && !S_ISREG(f->ltype)) {
				c = 0;
				action(0, 2, 0, FALSE);
				break;
			}

			break;
		case ':':
			c = 0;

			if (!ed_dialog("Enter option:",
			    NULL /* must be NULL !!! */, NULL, 0, &opt_hist)) {
				parsopt(rbuf);
			}

			break;
		case CTRL('L'):
			endwin();
			refresh();
			break;
		case 'W':
			wait_after_exec = wait_after_exec ? FALSE : TRUE;
			filt_stat();
			wrefresh(wstat);
			break;
		case '`':
			if (mark_idx < 0) {
				standoutc(wstat);
				/* calls standend */
				printerr(NULL, "No local mark");
				c = 0;
				break;
			}

			u = top_idx + curs;

			if (u <= mark_idx)
				num = mark_idx - u + 1;
			else {
				num = u - mark_idx + 1;
				u = mark_idx;
			}

			break;
		case '\t':
			if (!fmode)
				break;

			c = 0;
			disp_curs(0);
			wnoutrefresh(getlstwin());
			right_col = right_col ? FALSE : TRUE;
			prt2chead();
			disp_curs(1);
			wnoutrefresh(getlstwin());
			wnoutrefresh(wstat);
			doupdate();
			break;
		case '|':
			if (!twocols)
				c = 0;

			break;
		case CTRL('w'):
			c = 0;
			tgl2c();
			break;
		case 'N':
			if (regex) {
				c = 0;
				regex_srch(-1);
				break;
			}

			/* fall through */
		default:
			if (isgraph(c))
				printerr(NULL,
				    "Invalid input '%c' (type 'h' for help).",
				    c);
			else
				printerr(NULL,
				    "Invalid character code 0x%x"
				    " (type 'h' for help).", c);

			c = 0;
		}
	}
}

static char *helptxt[] = {
       "Type 'q' to quit help, scroll with <DOWN>, <UP>, <PAGE-DOWN>, and <PAGE-UP>.",
       "",
       "Q		Quit vddiff",
       "h, ?		Display help",
       "^L		Refresh display",
       "<TAB>		In two-column mode: Toggle column",
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
       "|<LEFT>		In two-column mode: Enlarge right column",
       "|<RIGHT>	In two-column mode: Enlarge left column",
       "|=		In two-column mode: Make column widths equal",
       "^W		In diff mode: Toggle two-column mode",
       "/		Search file by typing first letters of filename",
       "//		Search file with regular expression",
       "Sd		Sort files with directories on top",
       "Sm		Sort files by name only",
       "SS		Sort files by size only",
       "St		Sort files by modification time only",
       "H		Put cursor to top line",
       "M		Put cursor on middle line",
       "L		Put cursor on bottom line",
       "z<ENTER>	Put selected line to top of screen",
       "z.		Center selected file",
       "z-		Put selected line to bottom of screen",
       "^E		Scroll one line down",
       "^Y		Scroll one line up",
       "^D		Scroll half screen down",
       "^U		Scroll half screen up",
       "!, n		Toggle display of equal files",
       "c		Toggle showing only directories and really different files",
       "&		Toggle display of files which are on one side only",
       "F		Toggle following symbolic links",
       "E		Toggle file name or file content filter",
       "p		Show current relative work directory",
       "a		Show command line directory arguments",
       "f		Show full path",
       "[<n>]<<		Copy from second to first tree",
       "[<n>]>>		Copy from first to second tree",
       "`<<		Copy from second to first tree (range cursor...mark)",
       "`>>		Copy from first to second tree (range cursor...mark)",
       "[<n>]dd		Delete file or directory",
       "[<n>]dl		Delete file or directory in first tree",
       "[<n>]dr		Delete file or directory in second tree",
       "`dd		Delete file or directory (range cursor...mark)",
       "`dl		Delete file or directory in first tree (range cursor...mark)",
       "`dr		Delete file or directory in second tree (range cursor...mark)",
       "en		Rename file",
       "eln		Rename left file",
       "ern		Rename right file",
       "[<n>]ep		Change file mode",
       "[<n>]elp	Change mode of left file",
       "[<n>]erp	Change mode of right file",
       "`ep		Change file mode (range cursor...mark)",
       "`elp		Change mode of left file (range cursor...mark)",
       "`erp		Change mode of right file (range cursor...mark)",
       "[<n>]eu		Change file owner",
       "[<n>]elu	Change owner of left file",
       "[<n>]eru	Change owner or right file",
       "`eu		Change file owner (range cursor...mark)",
       "`elu		Change owner of left file (range cursor...mark)",
       "`eru		Change owner or right file (range cursor...mark)",
       "[<n>]eg		Change file group",
       "[<n>]elg	Change group of left file",
       "[<n>]erg	Change group or right file",
       "`eg		Change file group (range cursor...mark)",
       "`elg		Change group of left file (range cursor...mark)",
       "`erg		Change group or right file (range cursor...mark)",
       "P		Create directory (bmode only)",
       "Pl		Create directory in left tree",
       "Pr		Create directory in right tree",
       "m		Mark file or directory",
       "r		Remove mark, edit line or regex search",
       "b		Binary diff to marked file",
       "y		Copy file path to edit line",
       "Y		Copy file path in reverse order to edit line",
       "$		Enter shell command",
       "[<n>]<F1> - <F12>",
       "		Define string to be used in (or as) shell command",
       "		or execute shell command",
       "l		List function key strings",
       "u		Update file list",
       "s		Open shell",
       "sl		Open shell in left directory",
       "sr		Open shell in right directory",
       "o		Open file (instead of diff tool)",
       "ol		Open left file or directory",
       "or		Open right file or directory",
       "v		View raw file contents",
       "vl		View raw left file contents",
       "vr		View raw right file contents",
       ":		Enter configuration option",
       "W		Toggle wait for <ENTER> after running external tool" };

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
		case KEY_LEFT:
		case 'q':
			goto exit;
		case KEY_DOWN:
		case 'j':
		case '+':
			help_down(3);
			break;
		case KEY_UP:
		case 'k':
		case '-':
			help_up(3);
			break;
		case KEY_NPAGE:
		case ' ':
			help_pg_down();
			break;
		case KEY_PPAGE:
		case KEY_BACKSPACE:
		case CERASE:
			help_pg_up();
			break;
		case CTRL('L'):
			endwin();
			refresh();
			break;
		default:
			if (isgraph(c))
				printerr(NULL,
				    "Invalid input '%c' ('q' quits help).", c);
			else
				printerr(NULL,
				    "Invalid character code 0x%x"
				    " ('q' quits help).", c);
		}
	}

exit:
	disp_fmode();
}

#if NCURSES_MOUSE_VERSION >= 2
static void
help_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

	if (mevent.bstate & BUTTON4_PRESSED)
		help_up(3);
	else if (mevent.bstate & BUTTON5_PRESSED)
		help_down(3);
}
#endif

static void
disp_help(void)
{
	unsigned y, i;

	standendc(wlist);
	werase(wlist);
	werase(wstat);

	for (y = 0, i = help_top; y < listh && i < HELP_NUM; y++, i++)
		mvwaddstr(wlist, y, 0, helptxt[i]);

	wrefresh(wlist);

	if (!help_top && HELP_NUM > listh)
		printerr(NULL, "Scroll down for more");
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
help_down(short n)
{
	int i;

	if (!scrollen) {
		help_pg_down();
		return;
	}

	if ((i = HELP_NUM - listh - help_top) < n)
		n = i;

	if (i <= 0) {
		printerr(NULL, "At bottom");
		return; /* last line on display */
	}

	wscrl(wlist, n);
	help_top += n;

	for (i = n; i > 0; i--)
		mvwaddstr(wlist, listh - i, 0, helptxt[help_top + listh - i]);

	werase(wstat);
	wnoutrefresh(wlist);
	wrefresh(wstat);
}

static void
help_up(unsigned short n)
{
	int i;

	if (!scrollen) {
		help_pg_up();
		return;
	}

	if (!help_top) {
		printerr(NULL, "At top");
		return;
	}

	if (n > help_top)
		n = help_top;

	wscrl(wlist, -n);
	help_top -= n;

	for (i = 0; i < n; i++)
		mvwaddstr(wlist, i, 0, helptxt[help_top + i]);

	werase(wstat);
	wnoutrefresh(wlist);
	wrefresh(wstat);
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
		refr_scr();

		if (mevent.bstate & BUTTON1_DOUBLE_CLICKED)
			action(0, 3, 0, FALSE);
# if NCURSES_MOUSE_VERSION >= 2
	} else if (mevent.bstate & BUTTON4_PRESSED) {
		scroll_up(3, FALSE);
	} else if (mevent.bstate & BUTTON5_PRESSED) {
		scroll_down(3, FALSE);
# endif
	}
}
#endif

void
action(
    /* Used by function key starting with "$ ".
     * Ignore file type and file name extension, just use plain file name.
     * (Don't enter directories!) */
    short ign_ext,
    short tree,
    /* 0: <RIGHT> or double click
     * 1: <ENTER> */
    unsigned short act,
    /* Used by 'v', "vl" and "vr":
     * Unzip file but then view raw file using standard view tool. */
    bool raw_cont)
{
	struct filediff *f1, *f2, *z1 = NULL, *z2 = NULL;
	char *t1 = NULL, *t2 = NULL;
	char *err = NULL;
	static char *typerr = "Not a directory or regular file";
	static char *typdif = "Different file type";

	if (!db_num)
		return;

	if (fmode) {
		if (right_col)
			f1 = f2 = db2_list[top_idx2 + curs2];
		else
			f1 = f2 = db_list[top_idx + curs];
	} else
		f1 = f2 = db_list[top_idx + curs];

	if (mark && act) {
		struct filediff *m = mark;
		mode_t ltyp = 0, rtyp = 0;
		char *lnam, *rnam;

		if (!ign_ext) {
			/* check if mark needs to be unzipped */
			if ((z1 = unpack(m, m->ltype ? 1 : 2, &t1, 1)))
				m = z1;

			/* check if other file needs to be unchecked */
			if ((z2 = unpack(f1, m->ltype ? 2 : 1, &t2, 1)))
				f1 = z2;
		}

		if (bmode) {
			/* Take all files from left side. */
			lnam = m->name ? m->name : gl_mark;

			if (chk_mark(lnam, 1))
				goto ret;

			ltyp = m->ltype;
			rnam = f1->name;
			rtyp = f1->ltype;

		} else if (m->ltype && f1->rtype && (tree & 2)) {
			lnam = m->name ? m->name : mark_lnam;

			if (chk_mark(lnam, 1))
				goto ret;

			ltyp = m->ltype;
			rnam = f1->name;
			rtyp = f1->rtype;

		} else if (f1->ltype && m->rtype && (tree & 1)) {
			lnam = f1->name;
			ltyp = f1->ltype;
			rnam = m->name ? m->name : mark_rnam;

			if (chk_mark(rnam, 2))
				goto ret;

			rtyp = m->rtype;
		} else {
			err = "Both files are in same directory";
			goto ret;
		}

		if ((ltyp & S_IFMT) != (rtyp & S_IFMT) &&
		    !S_ISDIR(ltyp) && !S_ISDIR(rtyp)) {
			err = typdif;
			goto ret;
		}

		if (ign_ext || (S_ISREG(ltyp) && S_ISREG(rtyp)))
			tool(lnam, rnam, 3, ign_ext || raw_cont);

		else if (S_ISDIR(ltyp) || S_ISDIR(rtyp)) {
			if (bmode) {
				t2 = NULL;
				enter_dir(rnam, NULL, FALSE, FALSE);

				/* In bmode the unpacked dir is in z2.
				 * Hence z1 is useless and can be removed. */
				if (S_ISDIR(ltyp) && z1) {
					t1 = strdup(lnam);
					/* remove "/[lr]" */
					t1[strlen(t1) - 2] = 0;
				}
			} else if (!S_ISDIR(ltyp)) {
				t2 = NULL;
				enter_dir(NULL, rnam, FALSE, z2 ? TRUE : FALSE);

			} else if (!S_ISDIR(rtyp)) {
				t1 = NULL;
				enter_dir(lnam, NULL, z1 ? TRUE : FALSE, FALSE);
			} else {
				/* If t<n> == NULL tmpdir is not removed.
				 * It must not be removed before it is left
				 * with "cd .." later. */
				t1 = t2 = NULL;
				enter_dir(lnam, rnam, z1 ? TRUE : FALSE,
				    z2 ? TRUE : FALSE);
			}
		}

		goto ret;
	}

	if (!ign_ext) {
		if (f1->ltype && (z1 = unpack(f1, 1, &t1, 1)))
			f1 = z1;

		if (f2->rtype && (z2 = unpack(f2, 2, &t2, 1)))
			f2 = z2;
	}

	if (!f1->ltype ||
	    /* Tested here to support "or" */
	    tree == 2) {
		if (S_ISREG(f2->rtype) || ign_ext)
			tool(f2->name, NULL, 2, ign_ext || raw_cont);
		else if (S_ISDIR(f2->rtype)) {
			t1 = t2 = NULL;
			enter_dir(NULL, f2->name, FALSE, z2 ? TRUE : FALSE);
		} else {
			err = typerr;
			goto ret;
		}
	} else if (!f2->rtype || tree == 1) {
		if (S_ISREG(f1->ltype) || ign_ext)
			tool(f1->name, NULL, 1, ign_ext || raw_cont);
		else if (S_ISDIR(f1->ltype)) {
			t1 = t2 = NULL;
			enter_dir(f1->name, NULL, z1 ? TRUE : FALSE, FALSE);
		} else {
			err = typerr;
			goto ret;
		}
	} else if ((f1->ltype & S_IFMT) == (f2->rtype & S_IFMT)) {
		if (ign_ext || raw_cont)
			tool(f1->name, f2->name, 3, 1);
		else if (S_ISREG(f1->ltype)) {
			if (f1->diff == '!')
				tool(f1->name, f2->name, 3, 0);
			else
				tool(f1->name, NULL, 1, 0);
		} else if (S_ISDIR(f1->ltype)) {
			t1 = t2 = NULL;
			enter_dir(f1->name, f2->name, z1 ? TRUE : FALSE,
				    z2 ? TRUE : FALSE);
		} else {
			err = typerr;
			goto ret;
		}
	} else
		err = typdif;

ret:
	if (z1)
		free_zdir(z1, t1);

	if (z2)
		free_zdir(z2, t2);

	if (err)
		printerr(NULL, err);
}

static void
page_down(void)
{
	if (last_line_is_disp()) {
		printerr(NULL, "At bottom");
		return;
	}

	if (right_col) {
		top_idx2 += listh;
		curs2 = 0;
	} else {
		top_idx += listh;
		curs = 0;
	}

	disp_list();
}

static void
curs_last(void)
{
	if (last_line_is_disp())
		return;

	if (right_col) {
		top_idx2 = db2_num - listh;
		curs2 = listh - 1;
	} else {
		top_idx = db_num - listh;
		curs = listh - 1;
	}

	disp_list();
}

static void
page_up(void)
{
	unsigned *ti, *cu;

	if (first_line_is_top()) {
		printerr(NULL, "At top");
		return;
	}

	if (right_col) {
		ti = &top_idx2;
		cu = &curs2;
	} else {
		ti = &top_idx;
		cu = &curs;
	}

	if (*ti < listh) {
		*ti = 0;
		*cu -= *ti;
	} else {
		*ti -= listh;
		*cu = listh - 1;
	}

	disp_list();
}

static void
curs_first(void)
{
	if (first_line_is_top())
		return;

	if (right_col)
		top_idx2 = curs2 = 0;
	else
		top_idx = curs = 0;

	disp_list();
}

static int
last_line_is_disp(void)
{
	unsigned dn, ti, *cu;

	if (right_col) {
		dn = db2_num;
		ti = top_idx2;
		cu = &curs2;
	} else {
		dn = db_num;
		ti = top_idx;
		cu = &curs;
	}

	if (dn - ti <= listh) {
		/* last line is currently displayed */
		if (*cu != dn - ti - 1) {
			disp_curs(0);
			*cu = dn - ti - 1;
			disp_curs(1);
			refr_scr();
		}
		return 1;
	}

	return 0;
}

static int
first_line_is_top(void)
{
	unsigned ti, *cu;

	if (right_col) {
		ti = top_idx2;
		cu = &curs2;
	} else {
		ti = top_idx;
		cu = &curs;
	}

	if (!ti) {
		if (*cu) {
			disp_curs(0);
			*cu = 0;
			disp_curs(1);
			refr_scr();
		}
		return 1;
	}

	return 0;
}

static void
curs_down(void)
{
	if ((right_col ? top_idx2 + curs2 : top_idx + curs) + 1 >=
	    (right_col ? db2_num : db_num)) {
		printerr(NULL, "At bottom");
		return;
	}

	if ((right_col ? curs2 : curs) + 1 >= listh) {
		if (scrollen) {
			disp_curs(0);
			wscrl(getlstwin(), 1);

			if (right_col)
				top_idx2++;
			else
				top_idx++;

			disp_curs(1);
			refr_scr();
		} else {
			page_down();
		}
		return;
	}

	disp_curs(0);

	if (right_col)
		curs2++;
	else
		curs++;

	disp_curs(1);
	refr_scr();
}

static void
curs_up(void)
{
	if (!(right_col ? curs2 : curs)) {
		if (!(right_col ? top_idx2 : top_idx)) {
			printerr(NULL, "At top");
			return;
		}

		if (scrollen) {
			disp_curs(0);
			wscrl(getlstwin(), -1);

			if (right_col)
				top_idx2--;
			else
				top_idx--;

			disp_curs(1);
			refr_scr();
		} else {
			page_up();
		}
		return;
	}

	disp_curs(0);

	if (right_col)
		curs2--;
	else
		curs--;

	disp_curs(1);
	refr_scr();
}

static void
scroll_up(unsigned num, bool keepscrpos)
{
	unsigned move_curs, y, i;

	if (!top_idx) {
		if (!curs || keepscrpos) {
			printerr(NULL, "At top");
			return;
		}

		disp_curs(0);

		if (curs >= num)
			curs -= num;
		else
			curs = 0;

		disp_curs(1);
		refr_scr();
		return;
	}

	if (top_idx < num)
		num = top_idx;

	if (keepscrpos) {
		disp_curs(0);
		move_curs = 1;

	} else if (curs + num >= listh) {
		disp_curs(0);
		curs = listh - 1;
		move_curs = 1;
	} else {
		curs += num;
		move_curs = 0;
	}

	wscrl(getlstwin(), -((int)num));
	top_idx -= num;

	for (y = 0, i = top_idx; y < num; y++, i++)
		disp_line(y, i, 0);

	if (move_curs)
		disp_curs(1);

	refr_scr();
}

static void
scroll_down(unsigned num, bool keepscrpos)
{
	unsigned move_curs, y, i, ti;
	WINDOW *w;

	w = getlstwin();

	if (top_idx >= db_num - 1) {
		printerr(NULL, "At bottom");
		return;
	}

	if (top_idx + num >= db_num)
		num = db_num - 1 - top_idx;

	ti = top_idx + num;

	if (keepscrpos) {
		disp_curs(0);
		move_curs = 1;

		if (ti + curs >= db_num)
			curs = db_num - 1 - ti;

	} else if (curs < num) {
		disp_curs(0);
		curs = 0;
		move_curs = 1;
	} else {
		curs -= num;
		move_curs = 0;
	}

	wscrl(w, num);
	top_idx = ti;

	for (y = listh - num, i = top_idx + y;
	    y < listh && ((twocols && !fmode) || i < db_num);
	    y++, i++)
		if (i >= db_num) {
			standoutc(w);
			mvwaddch(w, y, llstw, ' ');
		} else
			disp_line(y, i, 0);

	if (move_curs)
		disp_curs(1);

	refr_scr();
}

void
disp_curs(
    /* 0: Remove cursor
     * 1: Normal cursor
     * 2: Mark */
    int a)
{
	WINDOW *w;

	w = getlstwin();

	if (fmode) { /* if "fmode"->"twocols" bar is used */
		if (!a)
			mvwchgat(w, right_col ? curs2 : curs,
			    0, -1, 0, 0, NULL);
	} else if (a) {
		standoutc(w);
	} else if (top_idx + curs == mark_idx) {
		a = 1;
		markc(w);
	}

	disp_line(right_col ? curs2 : curs,
	    right_col ? top_idx2 + curs2 : top_idx + curs, a);

	if (fmode) { /* if "fmode"->"twocols" bar is used */
		switch (a) {
		case 1:
			mvwchgat(w, right_col ? curs2 : curs, 0, -1,
			    color ? 0 : A_STANDOUT, color ? PAIR_CURSOR : 0,
			    NULL);
			break;
		case 2:
			mvwchgat(w, right_col ? curs2 : curs, 0, -1,
			    color ? 0 : A_BOLD | A_UNDERLINE,
			    color ? PAIR_MARK : 0, NULL);
			break;
		}
	}
}

void
disp_list(void) /* 0: both, 1: left, 2: right */
{
	unsigned y, i;
	unsigned ti, cu, dn;
	WINDOW *w;

	w = getlstwin();

	if (right_col) {
		ti = top_idx2;
		dn = db2_num;
		cu = curs2;
	} else {
		ti = top_idx;
		dn = db_num;
		cu = curs;
	}

	/* For the case that entries had been removed */
	if (ti >= dn)
		ti = dn ? dn - 1 : 0;

	if (ti + cu >= dn)
		cu = dn ? dn - ti - 1 : 0;

	if (right_col) {
		top_idx2 = ti;
		curs2 = cu;
	} else {
		top_idx = ti;
		curs = cu;
	}

	werase(w);

	if (!dn) {
		no_file();
		goto exit;
	}

	for (y = 0, i = ti; y < listh && ((twocols && !fmode) || i < dn);
	    y++, i++) {
		if (i >= dn) {
			standoutc(w);
			mvwaddch(w, y, llstw, ' ');
		} else if (y == cu)
			disp_curs(1);
		else {
			int j = 0;

			if (ti + y == mark_idx) {
				j = 1;
				markc(w);
			}

			disp_line(y, i, j);
		}
	}

exit:
	refr_scr();
}

static void
disp_line(
    /* display line */
    unsigned y,
    /* DB index */
    unsigned i,
    /* 1: Is cursor line */
    int info)
{
	int diff, type[2];
	mode_t mode[2];
	struct filediff *f;
	short color_id = 0;
	WINDOW *w;
	attr_t a;
	int mx;
	short cp;

	w = getlstwin();
	f = right_col ? db2_list[i] : db_list[i];
	mx = twocols ? llstw : 0;

	if (twocols && !fmode)
		wattr_get(w, &a, &cp, NULL);

	if (fmode || bmode) {
		goto no_diff;
	} else if (!f->ltype) {
		diff     = '>';
		*mode     = f->rtype;
		color_id = PAIR_RIGHTONLY;
	} else if (!f->rtype) {
		diff     = '<';
		*mode     = f->ltype;
		color_id = PAIR_LEFTONLY;
	} else if ((f->ltype & S_IFMT) != (f->rtype & S_IFMT)) {
		if (twocols && !fmode)
			diff = 'X';
		else {
			diff = ' ';
			*mode = 0;
			type[0] = '!';
		}

		color_id = PAIR_DIFF;
	} else {
no_diff:
		diff = f->diff;
		*mode = right_col ? f->rtype : f->ltype;

		if (diff == '!')
			color_id = PAIR_DIFF;
		else if (diff == '-')
			color_id = PAIR_ERROR;
	}

	if (twocols && !fmode && diff == '>')
			goto prtc2;

	set_file_info(f, twocols && !fmode ? f->ltype : *mode, type, &color_id,
	    &diff);
	disp_name(w, y, 0, mx, info, f, *type, color_id,
	    twocols || f->llink ? f->llink : f->rlink, diff);

	if (twocols && !fmode) {
prtc2:
		if (diff != '<') {
			wattr_set(w, a, cp, NULL);
			set_file_info(f, f->rtype, type+1, &color_id, &diff);
			disp_name(w, y, rlstx, 0, info, f, type[1], color_id,
			    f->rlink, diff);
		}

		standoutc(w);
		wattron(w, A_BOLD);
		mvwaddch(w, y, llstw, diff);
	}

	if (!info)
		return;

	if (dir_change) {
	} else if (mark) {
		if (wstat_dirty)
			disp_mark();
		return;
	} else if (edit) {
		if (wstat_dirty)
			disp_edit();
		return;
	} else if (regex) {
		if (wstat_dirty)
			disp_regex();
		return;
	}

	if (twocols) {
		wmove(wstat, 0, 0);
		wclrtoeol(wstat);
	} else
		werase(wstat);

	if (type[0] == '!') {
		statcol();
		mvwaddstr(wstat, 0, 2, type_name(f->ltype));

		if (f->llink) {
			addmbs(wstat, " -> ", mx);
			addmbs(wstat, f->llink, mx);
		}

		mvwaddstr(wstat, 1, 2, type_name(f->rtype));

		if (f->rlink) {
			addmbs(wstat, " -> ", 0);
			addmbs(wstat, f->rlink, 0);
		}
	} else {
		statcol();

		if (fmode) {
			file_stat(db_list[curs], db2_list[curs2]);
		} else
			file_stat(f, NULL);
	}

	filt_stat();
	dir_change = FALSE;
}

static void
set_file_info(struct filediff *f, mode_t m, int *t, short *ct, int *d)
{
	if (S_ISREG(m)) {
		if (m & 0100)
			*t = '*';
		else
			*t = ' ';
	} else if (S_ISDIR(m)) {
		*t = '/';

		if (!*ct) {
			*ct = PAIR_DIR;

			if (is_diff_dir(f->name))
				*d = '!';
		}
	} else if (S_ISLNK(m)) {
		*t = '@';

		if (!*ct)
			*ct = PAIR_LINK;
	} else if (m) {
		if      (S_ISCHR(m))  *t = 'c';
		else if (S_ISBLK(m))  *t = 'b';
		else if (S_ISSOCK(m)) *t = '=';
		else if (S_ISFIFO(m)) *t = '|';
		else                  *t = '?';

		if (!*ct)
			*ct = PAIR_UNKNOWN;
	}
}

static int
disp_name(WINDOW *w, int y, int x, int mx, int o, struct filediff *f, int t,
    short ct, char *l, int d)
{
	int i;

	if (color && !o) {
		wattron(w, A_BOLD);

		if (ct)
			wattron(w, COLOR_PAIR(ct));
		else
			/* not attrset, else bold is off */
			wattron(w, COLOR_PAIR(PAIR_NORMAL));
	}

	if (twocols || bmode)
		mvwprintw(w, y, x, "%c ", t);
	else
		mvwprintw(w, y, x, "%c %c ", d, t);

	i = addmbs(w, f->name, mx);
	standendc(w);

	if (i)
		return 1;

	if (l) {
		addmbs(w, " -> ", mx);
		putmbsra(w, l, mx);
	}

	return 0;
}

static void
statcol(void)
{
	if (twocols || dir_change || bmode)
		return;

	standoutc(wstat);
	mvwaddch(wstat, 0, 0, '<');
	mvwaddch(wstat, 1, 0, '>');
	standendc(wstat);
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
file_stat(struct filediff *f, struct filediff *f2)
{
	int x, x2, w, w1, w2, yl, yr, lx1, lx2, mx1;
	struct passwd *pw;
	struct group *gr;
	mode_t ltyp, rtyp;

	if (!f2)
		f2 = f;

	standendc(wstat);
	x  = twocols || bmode ? 0 : 2;
	x2 = twocols ? rlstx :
	     bmode   ? 0     : 2;
	yl = 0;
	yr = twocols ? 0 : 1;
	ltyp = f->ltype;
	rtyp = f2->rtype;
	mx1 = twocols ? llstw : 0;

	if (twocols) {
		prt2chead();
		standoutc(wstat);
		mvwaddch(wstat, 0, llstw, ' ');
		standendc(wstat);
	} else if (bmode) {
		wmove(wstat, 1, 0);
		putmbsra(wstat, rpath, 0);
	} else if (dir_change) {
		lpath[llen] = 0;
		rpath[rlen] = 0;
		mvwaddstr(wstat, 1, 0, *pwd ? PWD : RPWD);
		return;
	}

	if (S_ISLNK(ltyp)) {
		wmove(wstat, yl, x);
		addmbs(wstat, "-> ", mx1);
		putmbsra(wstat, f->llink, mx1);
		ltyp = 0;
	}

	if (S_ISLNK(rtyp)) {
		wmove(wstat, yr, x2);
		addmbs(wstat, "-> ", 0);
		putmbsra(wstat, f2->rlink, 0);
		rtyp = 0;
	}

	if (ltyp)
		mvwprintw(wstat, yl, x, "%04o", ltyp & 07777);

	if (rtyp)
		mvwprintw(wstat, yr, x2, "%04o", rtyp & 07777);

	x += 5;
	x2 += 5;

	if (twocols && x >= llstw)
		ltyp = 0;

	if (twocols && x2 >= COLS)
		rtyp = 0;

	if (ltyp) {
		if ((pw = getpwuid(f->luid)))
			memcpy(lbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->luid);
	}

	if (rtyp) {
		if ((pw = getpwuid(f2->ruid)))
			memcpy(rbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(rbuf, sizeof rbuf, "%u", f2->ruid);
	}

	if (ltyp) {
		wmove(wstat, yl, x);
		addmbs(wstat, lbuf, mx1);
	}

	if (rtyp) {
		wmove(wstat, yr, x2);
		addmbs(wstat, rbuf, 0);
	}

	w1 = ltyp ? strlen(lbuf) + 1 : 0;
	w2 = rtyp ? strlen(rbuf) + 1 : 0;

	if (twocols) {
		x += w1;
		x2 += w2;
	} else {
		x += w1 > w2 ? w1 : w2;
		x2 = x;
	}

	if (twocols && x >= llstw)
		ltyp = 0;

	if (twocols && x2 >= COLS)
		rtyp = 0;

	if (ltyp) {
		if ((gr = getgrgid(f->lgid)))
			memcpy(lbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->lgid);
	}

	if (rtyp) {
		if ((gr = getgrgid(f2->rgid)))
			memcpy(rbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(rbuf, sizeof rbuf, "%u", f2->rgid);
	}

	if (ltyp) {
		wmove(wstat, yl, x);
		addmbs(wstat, lbuf, mx1);
	}

	if (rtyp) {
		wmove(wstat, yr, x2);
		addmbs(wstat, rbuf, 0);
	}

	w1 = ltyp ? strlen(lbuf) + 1 : 0;
	w2 = rtyp ? strlen(rbuf) + 1 : 0;
	lx1 = x + w1;
	lx2 = x2 + w2;

	if (twocols) {
		x += w1;
		x2 += w2;
	} else {
		x += w1 > w2 ? w1 : w2;
		x2 = x;
	}

	if (twocols && x >= llstw)
		ltyp = 0;

	if (twocols && x2 >= COLS)
		rtyp = 0;

	if (ltyp && !S_ISDIR(ltyp)) {
		if (S_ISCHR(ltyp) || S_ISBLK(ltyp))
			w1 = snprintf(lbuf, sizeof lbuf, "%lu, %lu",
			    (unsigned long)major(f->lrdev),
			    (unsigned long)minor(f->lrdev));
		else
			w1 = getfilesize(lbuf, sizeof lbuf, f->lsiz);

		w1++;
	} else
		w1 = 0;

	if (rtyp && !S_ISDIR(rtyp)) {
		if (S_ISCHR(rtyp) || S_ISBLK(rtyp))
			w2 = snprintf(rbuf, sizeof rbuf, "%lu, %lu",
			    (unsigned long)major(f2->rrdev),
			    (unsigned long)minor(f2->rrdev));
		else
			w2 = getfilesize(rbuf, sizeof rbuf, f2->rsiz);

		w2++;
	} else
		w2 = 0;

	w = w1 > w2 ? w1 : w2;

	if (ltyp && !S_ISDIR(ltyp)) {
		wmove(wstat, yl, twocols ? x : x + w - w1);
		addmbs(wstat, lbuf, mx1);

		if (twocols)
			x += w1;
	}

	if (rtyp && !S_ISDIR(rtyp)) {
		wmove(wstat, yr, twocols ? x2 : x2 + w - w2);
		addmbs(wstat, rbuf, 0);

		if (twocols)
			x2 += w2;
	}

	if (!twocols && (
	    (ltyp && !S_ISDIR(ltyp)) ||
	    (rtyp && !S_ISDIR(rtyp)))) {
		x += w;
		x2 = x;
	}

	if (twocols && x >= llstw)
		ltyp = 0;

	if (twocols && x2 >= COLS)
		rtyp = 0;

	if (ltyp) {
		lx1 = x + gettimestr(lbuf, sizeof lbuf, &f->lmtim);
		wmove(wstat, yl, x);
		addmbs(wstat, lbuf, mx1);
	}

	if (rtyp) {
		lx2 = x2 + gettimestr(rbuf, sizeof rbuf, &f2->rmtim);
		wmove(wstat, yr, x2);
		addmbs(wstat, rbuf, 0);
	}

	if (ltyp && f->llink) {
		wmove(wstat, yl, lx1);
		addmbs(wstat, " -> ", mx1);
		putmbsra(wstat, f->llink, mx1);
	}

	if (rtyp && f2->rlink) {
		wmove(wstat, yr, lx2);
		addmbs(wstat, " -> ", 0);
		putmbsra(wstat, f2->rlink, 0);
	}
}

static size_t
getfilesize(char *buf, size_t bufsiz, off_t size)
{
	char *unit;
	float f;

	if (!(scale || twocols) || size < 1024)
		return snprintf(buf, bufsiz, "%'lld", (long long)size);
	else {
		f = size / 1024.0;
		unit = "K";

		if (f >= 1024) {
			f /= 1024.0;
			unit = "M";
		}

		if (f >= 1024) {
			f /= 1024.0;
			unit = "G";
		}

		if (f >= 1024) {
			f /= 1024.0;
			unit = "T";
		}
	}

	return snprintf(buf, bufsiz, "%.1f%s", f, unit);
}

static size_t
gettimestr(char *buf, size_t bufsiz, time_t *t)
{
	struct tm *tm;
	bool o;

	if (!(tm = localtime(t))) {
		printerr(strerror(errno), "localtime failed");
		*buf = 0;
		return 0;
	}

	o = time(NULL) - *t > 3600 * 24 * (366 / 2);

	return strftime(buf, bufsiz, o ? "%b %e  %Y" : "%b %e %k:%M", tm);
}

static void
set_mark(void)
{
	struct filediff *f;

	if (!db_num)
		return;

	if (mark)
		clr_mark();

	mark_idx = top_idx + curs;
	f = db_list[mark_idx];
	mode_t mode = f->ltype ? f->ltype : f->rtype;

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
	filt_stat();
	standoutc(wstat);
	mvwaddstr(wstat, 1, 0, gl_mark ? gl_mark : mark->name);
	standendc(wstat);
	wrefresh(wstat);
}

void
clr_mark(void)
{
	if (gl_mark) {
		free(gl_mark);
		free(mark_lnam);
		free(mark_rnam);
		free(mark);
		gl_mark = NULL;
	} else if (mark_idx >= top_idx && mark_idx < top_idx + listh &&
	    mark_idx != top_idx + curs) {
		disp_line(mark_idx - top_idx, mark_idx, 0);
		wnoutrefresh(wlist);
	}


	mark_idx = -1;
	mark = NULL;
	werase(wstat);
	filt_stat();
	wnoutrefresh(wstat);
	doupdate();
}

void
mark_global(void)
{
	struct filediff *m;

	mark_idx = -1;
	m = malloc(sizeof(struct filediff));
	*m = *mark;
	mark = m;
	mark_lnam = NULL;
	mark_rnam = NULL;

	if (bmode) {
		pthcat(lpath, llen, m->name);

		if (!(gl_mark = realpath(lpath, NULL))) {
			/* Ignore error if file had just been deleted */
			if (errno != ENOENT)
				printerr(strerror(errno),
				    "realpath \"%s\" failed", lpath);
			lpath[llen] = 0;
			goto error;
		}

		lpath[llen] = 0;
	} else {
		if (!*lpath)
			m->ltype = 0; /* tmp dir left */

		if (!*rpath)
			m->rtype = 0;

		if (!m->ltype && !m->rtype)
			goto error;

		if (m->ltype) {
			pthcat(lpath, llen, m->name);
			gl_mark = strdup(lpath);
			mark_lnam = strdup(lpath);
			lpath[llen] = 0;
		}

		if (m->rtype) {
			pthcat(rpath, rlen, m->name);

			if (!m->ltype)
				gl_mark = strdup(rpath);

			mark_rnam = strdup(rpath);
			rpath[rlen] = 0;
		}
	}

	m->name = NULL;
	return;

error:
	free(m);
	clr_mark();
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

	if (!db_num)
		return;

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

void
no_file(void)
{
	if (((real_diff || noequal || nosingle) && !bmode) || file_pattern)
		printerr(NULL,
		    "No file in list (type %s%s%s to disable filters).",
		    !file_pattern ? "" : "'E'",
		    !real_diff ? "" : "'c'",
		    !noequal ? "" : !real_diff ? "'!'" : " or '!'",
		    !nosingle ? "" :
		    !(real_diff || noequal) ? "'&'" : " or '&'");
	else
		printerr(NULL, "Directory is empty.");
}

void
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
push_state(char *name, char *rnam, bool lzip, bool rzip)
{
	struct ui_state *st = malloc(sizeof(struct ui_state));
	diff_db_store(st);
	st->llen = llen;
	st->rlen = rlen;

	/* If an absolute path is given, save the current path for the
	 * time when this absolute path is left later */
	if (!(name && *name == '/'))
		st->lpth = NULL;
	else {
		st->lpth = strdup(lpath);
		*lpath = 0;
		llen = 0;
		pwd = lpath;
	}

	if (!(rnam && *rnam == '/'))
		st->rpth = NULL;
	else {
		st->rpth = strdup(rpath);
		*rpath = 0;
		rlen = 0;
		rpwd = rpath;
	}

	st->lzip = lzip ? strdup(name) : NULL;
	st->rzip = rzip ? strdup(rnam) : NULL;
	st->top_idx = top_idx;  top_idx = 0;
	st->curs    = curs;     curs    = 0;
	st->tree    = subtree;
	st->next    = ui_stack;
	ui_stack    = st;
}

static void
pop_state(
    /* 1: Normal mode
     * 0: Cleanup mode */
    short mode)
{
	struct ui_state *st = ui_stack;

	if (!st) {
		printerr(NULL, "At top directory");
		return;
	}

	if (st->lzip) {
		if (mode && (
		      /* A global mark inside the archive */
		      (gl_mark && mark_lnam &&
		       !strncmp(lpath, mark_lnam, llen)) ||
		      /* A local mark -> can only be in archive
		       * since we are just leaving a archive */
		      (mark && !gl_mark)
		    ))
			clr_mark();

		st->lzip[strlen(st->lzip) - 2] = 0;
		rmtmpdirs(st->lzip);
	}

	if (st->rzip) {
		if (mode && (
		      (gl_mark && mark_rnam &&
		       !strncmp(rpath, mark_rnam, rlen)) ||
		      (mark && !gl_mark)
		    ))
			clr_mark();

		st->rzip[strlen(st->rzip) - 2] = 0;
		rmtmpdirs(st->rzip);
	}

	if (mark) {
		if (mode) {
			if (!gl_mark)
				mark_global();
		} else
			clr_mark();
	}

	llen = st->llen;
	rlen = st->rlen;

	if (st->lpth) {
		if (mode)
			memcpy(lpath, st->lpth, llen);

		lpath[llen] = 0;
		free(st->lpth);
	}

	if (st->rpth) {
		if (mode)
			memcpy(rpath, st->rpth, rlen);

		rpath[rlen] = 0;
		free(st->rpth);
	}

	top_idx  = st->top_idx;
	curs     = st->curs;
	subtree  = st->tree;
	ui_stack = st->next;
	lpath[llen] = 0; /* For 'p' (pwd) */
	diff_db_restore(st);
	free(st);

	if (mode) {
		dir_change = TRUE;
		disp_list();
	}
}

static void
enter_dir(char *name, char *rnam, bool lzip, bool rzip)
{
	dir_change = TRUE;

	if (mark && !gl_mark)
		mark_global();

	if (!bmode && !fmode) {
		push_state(name, rnam, lzip, rzip);
		subtree = (name ? 1 : 0) | (rnam ? 2 : 0);
		scan_subdir(name, rnam, subtree);
	} else {
		unsigned *uv;
		char *cp; /* current path */
		unsigned *ti, *cu;
#ifdef HAVE_LIBAVLBST
		struct bst_node *n;
#else
		struct ptr_db_ent *n;
#endif
		if (!name)
			name = rnam;

		if (bmode || right_col)
			cp = rpath;
		else
			cp = lpath;

		if (right_col) {
			ti = &top_idx2;
			cu = &curs2;
		} else {
			ti = &top_idx;
			cu = &curs;
		}

		db_set_curs(cp, *ti, *cu);
		n = NULL; /* flag */

		if (*name == '/') {
			if (bmode) {
				if (!getcwd(rpath, sizeof rpath))
					printerr(strerror(errno),
					    "getcwd failed");

				cp = rpath;
			}

			ptr_db_add(&uz_path_db, strdup(name), strdup(cp));
		} else if (*name == '.' && name[1] == '.' && !name[2] &&
		    !ptr_db_srch(&uz_path_db, cp, (void **)&rnam,
		    (void **)&n)) {
			name = rnam; /* dat */
#ifdef HAVE_LIBAVLBST
			rnam = n->key.p;
#else
			rnam = n->key;
#endif
			ptr_db_del(&uz_path_db, n);
		} else
			/* DON'T REMOVE! (Cause currently unclear) */
			n = NULL;

		if (bmode && chdir(name) == -1) {
			printerr(strerror(errno),
			    "chdir \"%s\" failed", name);
			return;
		}

		if (fmode)
			name = strdup(name);
		else
			name = NULL;

		*ti = 0;
		*cu = 0;
		diff_db_free(right_col);
		scan_subdir(name, NULL, right_col ? 2 : 1);

		if (fmode)
			free(name);

		if (n) {
			size_t l;

			l = strlen(rnam);

			if (gl_mark && !strncmp(rnam, gl_mark, l))
				clr_mark();

			rnam[l - 2] = 0; /* remove "/[lr]" */
			rmtmpdirs(rnam); /* does a free() */
			free(name); /* dat */
		}

		if ((uv = db_get_curs(cp))) {
			*ti = *uv++;
			*cu = *uv;
		}
	}

	disp_list();
}

void
printerr(char *s2, char *s1, ...)
{
	va_list ap;

	if (qdiff) {
		va_start(ap, s1);
		vfprintf(stderr, s1, ap);
		va_end(ap);

		if (s2)
			fprintf(stderr, ": %s", s2);

		fputc('\n', stderr);
		return;
	}

	wstat_dirty = TRUE;
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

	filt_stat();
	wrefresh(wstat);
}

int
dialog(const char *quest, char *answ, char *fmt, ...)
{
	va_list ap;
	int c, c2;
	char *s;

	wstat_dirty = TRUE;

	if (fmt) {
		werase(wstat);
		wmove(wstat, 0, 0);
		va_start(ap, fmt);
		vwprintw(wstat, fmt, ap);
		va_end(ap);
	}

	mvwaddstr(wstat, 1, 0, quest);
	wrefresh(wstat);

	do {
		c = getch();
		for (s = answ; s && (c2 = *s); s++)
			if (c == c2)
				break;
	} while (s && !c2);

	werase(wstat);
	filt_stat();
	wrefresh(wstat);
	return c;
}

static void
ui_resize(void)
{
	set_win_dim();

	if (curs >= listh)
		curs = listh -1;

	if (bmode);
	else
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
	statw = COLS;
	listh = LINES - 2;
	listw = COLS;
	rlstx = COLS / 2 + midoffs;

	if (rlstx < 2)
		rlstx = 2;
	else if (rlstx > COLS - 1)
		rlstx = COLS - 1;

	rlstw = COLS - rlstx;
	llstw = rlstx - 1;
}
