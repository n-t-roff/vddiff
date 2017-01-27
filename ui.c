/*
Copyright (c) 2016-2017, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <signal.h>
#ifdef USE_SYS_MKDEV_H
# include <sys/mkdev.h>
#endif
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "fs.h"
#include "ed.h"
#include "ui2.h"
#include "tc.h"
#include "dl.h"
#include "cplt.h"

static void ui_ctrl(void);
static void page_down(void);
static void page_up(void);
static void curs_last(void);
static void curs_first(void);
static int last_line_is_disp(void);
static int first_line_is_top(void);
static void curs_up(void);
static void disp_line(unsigned, unsigned, int);
static void push_state(char *, char *, unsigned);
static void help(void);
static char *type_name(mode_t);
static void ui_resize(void);
static void statcol(int);
static void file_stat(struct filediff *, struct filediff *);
static void set_file_info(struct filediff *, mode_t, int *, short *, int *);
static int disp_name(WINDOW *, int, int, int, int, struct filediff *, int,
    short, char *, int, int);
static size_t getfilesize(char *, size_t, off_t, unsigned);
static size_t gettimestr(char *, size_t, time_t *);
static void disp_help(void);
static void help_pg_down(void);
static void help_pg_up(void);
static void help_down(short);
static void help_up(unsigned short);
static void set_mark(void);
static void disp_mark(void);
static void yank_name(int);
static void scroll_up(unsigned, bool, int);
static void scroll_down(unsigned, bool, int);
static int openwins(void);

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
      color_mmrk_fg   = COLOR_BLACK  ,
      color_mmrk_bg   = COLOR_YELLOW ,
      color_bg        = COLOR_BLACK  ;
unsigned top_idx[2], curs[2], statw;

wchar_t *sh_str[FKEY_NUM];
char *fkey_cmd[FKEY_NUM];
unsigned fkey_flags[FKEY_NUM];

static unsigned listw, help_top;
unsigned listh;
WINDOW *wlist;
WINDOW *wstat;
struct ui_state *ui_stack;
/* Line scroll enable. Else only full screen is scrolled */
struct filediff *mark;
char *gl_mark, *mark_lnam, *mark_rnam;
static struct history sh_cmd_hist;

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(void);
# if NCURSES_MOUSE_VERSION >= 2
static void help_mevent(void);
# endif

MEVENT mevent;
#endif

bool scrollen = TRUE;
static bool wstat_dirty;
static bool dir_change;
bool add_hsize; /* scaled size */
bool add_bsize;
bool add_mode;
bool add_mtime;
bool add_owner;
bool add_group;

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
		init_pair(PAIR_MMRK     , color_mmrk_fg  , color_mmrk_bg  );
		bkgd(   COLOR_PAIR(PAIR_NORMAL));
		bkgdset(COLOR_PAIR(PAIR_NORMAL));
	}

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	set_def_mouse_msk();
	refresh();

	if (openwins()) {
		return;
	}

do_diff:
	/* Not in main since build_diff_db() uses printerr() */
	if (recursive) {
		do_scan();

		if (qdiff) {
			return;
		}
	}

	if (bmode || fmode) {
		if (chdir(syspth[0]) == -1) {
			printerr(strerror(errno), "chdir \"%s\":", syspth[0]);
			goto exit;
		}

		if (bmode) {
			*syspth[0] = '.';
			syspth[0][1] = 0;
			pthlen[0] = 1;
		}
	}

	if (bmode)
		build_diff_db(1);
	else if (fmode) {
		build_diff_db(1);
		build_diff_db(2);
	} else
		build_diff_db(3);

	if (qdiff) {
		return;
	}

	disp_fmode();
	ui_ctrl();

	sig_term(0); /* remove tmp dirs */

exit:
	bkgd(A_NORMAL);
	erase();
	refresh();
	endwin();
}

static int
openwins(void)
{
	set_win_dim();

	if (!(wlist = new_scrl_win(listh, listw, 0, 0))) {
		return -1;
	}

	if (!(wstat = new_scrl_win(2, statw, LINES-2, 0))) {
		idlok(wstat, FALSE);
		scrollok(wstat, FALSE);
		return -1;
	}

	if (fmode) {
		open2cwins();
	}

	return 0;
}

static void
ui_ctrl(void)
{
	static struct history opt_hist;
	int key[2] = { 0, 0 }, key2[2], c = 0, c2 = 0, i;
	unsigned num, num2;
	long u;
	struct filediff *f;
	bool ns; /* num set */

	while (1) {
next_key:
		key[1] = *key;
		*key = c;

		if (!c) {
			num = 1;
			ns = FALSE;
			u = top_idx[right_col] + curs[right_col];
		}

		if ((c = getch()) == ERR)
			goto next_key;
#if defined(TRACE)
		if (isascii(c) && !iscntrl(c)) {
			fprintf(debug, "<>getch: '%c'\n", c);
		} else {
			fprintf(debug, "<>getch: 0x%x\n", c);
		}
#endif
		if (c == '.' && c2 && !*key) {
			if (!c2) {
				goto next_key;
			}

			c = c2;
			key[0] = key2[0];
			key[1] = key2[1];
			num = num2;
		}

		if (test_fkey(c, num)) {
			if (nofkeys) {
				goto next_key;
			}

			goto save_st;
		}

		if (c >= '0' && c <= '9') {
			num = ns ? num * 10 : 0;
			num += c - '0';
			ns = TRUE;
#if defined(TRACE)
			fprintf(debug, "  ui_ctrl: num := %u\n", num);
#endif
			goto next_key;
		}

		switch (c) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			c = 0;
			proc_mevent();
			break;
#endif
		case 'q':
			if (dialog(y_n_txt, NULL,
			    "Quit " BIN "?") != 'y')
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
				if (!top_idx[right_col]) {
					break;
				} else if (top_idx[right_col] >=
				    listh - 1 - curs[right_col]) {

					disp_curs(0);
					top_idx[right_col] -=
					    listh - 1 - curs[right_col];
					curs[right_col] = listh - 1;
				} else {
					disp_curs(0);
					curs[right_col] += top_idx[right_col];
					top_idx[right_col] = 0;
				}

				disp_list(1);
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
				resize_fmode();
				break;
			}

			c = 0;

			if (bmode || fmode)
				enter_dir("..", NULL, FALSE, FALSE, 0
				    LOCVAR);
			else
				pop_state(1);

			break;

		case '\n':
			if (*key == 'z') {
				if (!curs[right_col]) {
					c = 0;
					break;
				}

				disp_curs(0);
				top_idx[right_col] += curs[right_col];
				curs[right_col] = 0;
				disp_list(1);
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
				resize_fmode();
				break;
			}

			if (!db_num[right_col]) {
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
				resize_fmode();
				break;
			}

			if (!fmode)
				break;

			c = 0;
			fmode_cp_pth();
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
			scroll_down(1, FALSE, -1);
			break;
		case CTRL('y'):
			scroll_up(1, FALSE, -1);
			break;
		case CTRL('d'):
			scroll_down(listh/2, TRUE, -1);
			break;
		case CTRL('u'):
			scroll_up(listh/2, TRUE, -1);
			break;

		case 'h':
			switch (*key) {
			case 'A':
				c = 0;
				add_bsize = FALSE;
				add_hsize = TRUE;
				disp_fmode();
				goto next_key;

			case 'R':
				c = 0;
				add_hsize = FALSE;
				disp_fmode();
				goto next_key;
			}

			/* fall through */

		case '?':
			c = 0;
			help();
			break;

		case 'p':
			if (*key == 'A') {
				c = 0;
				add_mode = TRUE;
				disp_fmode();
				goto next_key;

			} else if (*key == 'R') {
				c = 0;
				add_mode = FALSE;
				disp_fmode();
				goto next_key;

			} else if (*key == 'e') {
				if (ui_chmod(3, u, num)) {
					goto next_key;
				}

				goto save_st;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					if (ui_chmod(1, u, num)) {
						goto next_key;
					}

					goto save_st;
				} else if (*key == 'r') {
					if (ui_chmod(2, u, num)) {
						goto next_key;
					}

					goto save_st;
				}
			}

			if (!bmode) {
				c = 0;
				syspth[0][pthlen[0]] = 0;
				syspth[1][pthlen[1]] = 0;

				if (!*pwd && !*rpwd)
					printerr(NULL, "At top directory");
				else if (!twocols &&
				    (!*pwd || !rpwd || !strcmp(PWD, RPWD)))
					printerr(NULL, "%s", *pwd ? PWD : RPWD);
				else {
					werase(wstat);
					statcol(0);
					stmbsra(PWD, RPWD);
					wrefresh(wstat);
				}

				break;
			}

			/* fall through */
		case 'f':
			if (!bmode) {
				c = 0;
				syspth[0][pthlen[0]] = 0;
				syspth[1][pthlen[1]] = 0;
				werase(wstat);
				statcol(0);
				stmbsra(syspth[0], syspth[1]);
				wrefresh(wstat);
				break;
			}

			/* fall through */
		case 'a':
			c = 0;

			if (*key == 'D') {
				dl_add();
				break;
			}

			if (bmode) {
				if (!getcwd(syspth[1], sizeof syspth[1])) {
					printerr(strerror(errno),
					    "getcwd failed");
					break;
				}

				werase(wstat);
				wmove(wstat, 1, 0);
				putmbsra(wstat, syspth[1], 0);
				wrefresh(wstat);
				break;
			}

			werase(wstat);
			statcol(0);
			stmbsra(arg[0], arg[1]);
			wrefresh(wstat);
			break;

		case 'n':
			if (regex_mode) {
				regex_srch(1);
				c = 0;
				break;
			} else if (*key == 'e') {
				fs_rename(3);
				goto save_st;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					fs_rename(1);
					goto save_st;
				} else if (*key == 'r') {
					fs_rename(2);
					goto save_st;
				}
			}

			/* fall through */

		case '!':
		case 'c':
		case '&':
		case '^':
			if (bmode || fmode) {
				c = 0;
				break;
			}

			switch (c) {
			case 'n':
			case '!':
				noequal = noequal ? 0 : 1;
				break;

			case 'c':
				real_diff = real_diff ? 0 : 1;
				break;

			case '&':
				nosingle = nosingle ? 0 : 1;
				break;

			case '^':
				excl_or = excl_or ? FALSE : TRUE;
				break;
			}

			re_sort_list();
			c = 0;
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
			if (bmode || fmode) {
				fs_mkdir(right_col ? 2 : 1);
				goto save_st;
			}

			break;
		case 'd':
			if (*key == 'S') {
				c = 0;

				if (sorting == DIRSFIRST) {
					break;
				}

				sorting = DIRSFIRST;
				rebuild_db(1);
				break;

			} else if (*key != 'd') {
				break;
			}

			/* fall through */

		case KEY_DC:
			if (ui_dd(3, u, num)) {
				break;
			}

			goto save_st;
		case 't':
			switch (*key) {
			case 'S':
				c = 0;

				if (sorting == SORTMTIME) {
					goto next_key;
				}

				sorting = SORTMTIME;
				rebuild_db(1);
				goto next_key;

			case 'A':
				c = 0;
				add_mtime = TRUE;
				disp_fmode();
				goto next_key;

			case 'R':
				c = 0;
				add_mtime = FALSE;
				disp_fmode();
				goto next_key;
			}

			break;
		case 'v':
			if (!db_num[right_col])
				break;

			f = db_list[right_col][top_idx[right_col] +
			    curs[right_col]];

			if (S_ISREG(f->type[0]) && !S_ISREG(f->type[1])) {
				c = 0;
				action(0, 1, 0, TRUE);
				break;
			}

			if (S_ISREG(f->type[1]) && !S_ISREG(f->type[0])) {
				c = 0;
				action(0, 2, 0, TRUE);
				break;
			}

			break;
		case 'l':
			switch (*key) {
			case 'd':
				if (ui_dd(1, u, num)) {
					goto next_key;
				}

				goto save_st;
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
				fs_mkdir(1);
				goto save_st;

			case 'T': /* "Tl" */
				if (ui_mv(1, u, num)) {
					goto next_key;
				}

				goto save_st;

			case '@': /* "@l" */
				if (ui_cp(1, u, num, 2)) {
					goto next_key;
				}

				goto save_st;

			case 'D':
				c = 0;
				dl_list();
				goto next_key;

			case '\'':
				c = 0;
				list_jmrks();
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

				mvwprintw(wlist, i, 5, "\"%ls\"", sh_str[i]);
			}

			anykey();
			break;
		case 'r':
#if defined(TRACE)
			fprintf(debug,
			    "  'r': *key(%c) mark=%p edit=%d regex=%d\n",
			    *key, mark, edit, regex_mode);
#endif
			switch (*key) {
			case 'd':
				if (ui_dd(2, u, num)) {
					goto next_key;
				}

				goto save_st;
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
				fs_mkdir(2);
				goto save_st;

			case 'T': /* "Tr" */
				if (ui_mv(2, u, num)) {
					goto next_key;
				}

				goto save_st;

			case '@': /* "@r" */
				if (ui_cp(2, u, num, 2)) {
					goto next_key;
				}

				goto save_st;

			default:
				;
			}

			if (mark) {
				c = 0;
				clr_mark();
			} else if (edit) {
				c = 0;
				clr_edit();
			} else if (regex_mode) {
				c = 0;
				clr_regex();
			}

			break;
		case '<':
			if (*key != '<')
				break;

			c = 0;

			if (fmode && !right_col) {
				break;
			}

			if (ui_cp(1, u, num, 0)) {
				goto next_key;
			}

			break;
		case '>':
			if (*key != '>')
				break;

			if (fmode && right_col) {
				break;
			}

			if (ui_cp(2, u, num, 0)) {
				goto next_key;
			}

			goto save_st;

		case 'T':
			if (!fs_get_dst(u, 1)) {
				/* diff mode */
				break;
			}

			if (ui_mv(0, u, num)) {
				goto next_key;
			}

			goto save_st;

		case '@':
		case 'C':
		case 'U':
		{
			unsigned m = 0;

			if (!fs_get_dst(u, 1)) {
				/* diff mode */
				break;
			}

			switch (c) {
			case '@': m |= 2; break;
			case 'U': m |= 8; break;
			}

			if (ui_cp(0, u, num, m)) {
				goto next_key;
			}

			goto save_st;
		}

		case KEY_HOME:
			c = 0;
			curs_first();
			break;

		case 'G':
			if (ns) {
				c = 0;
				center(num ? num - 1 : 0);
				break;
			} else if (*key == 'V') {
				c = 0;
				mmrktobot();
				break;
			}

			/* fall-through */

		case KEY_END:
			c = 0;
			curs_last();
			break;

		case 'm':
			if (ns) {
				c = 0;

				if (num > 31) {
					break;
				}

				jmrk[right_col][num] = DB_LST_IDX;
				break;

			} else if (*key == 'S') {
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
			if (*key == 'A') {
				c = 0;

				if (add_owner) {
					goto next_key;
				}

				add_owner = TRUE;
				re_sort_list();
				disp_fmode();
				goto next_key;

			} else if (*key == 'R') {
				c = 0;

				if (!add_owner) {
					goto next_key;
				}

				add_owner = FALSE;
				disp_fmode();
				goto next_key;

			} else if (*key == 'e') {
				if (ui_chown(3, 0, u, num)) {
					goto next_key;
				}

				goto save_st;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					if (ui_chown(1, 0, u, num)) {
						goto next_key;
					}

					goto save_st;
				} else if (*key == 'r') {
					if (ui_chown(2, 0, u, num)) {
						goto next_key;
					}

					goto save_st;
				}
			}

			c = 0;
			rebuild_db(0);
			break;
		case 'g':
			if (*key == 'A') {
				c = 0;

				if (add_group) {
					goto next_key;
				}

				add_group = TRUE;
				re_sort_list();
				disp_fmode();
				goto next_key;

			} else if (*key == 'R') {
				c = 0;

				if (!add_group) {
					goto next_key;
				}

				add_group = FALSE;
				disp_fmode();
				goto next_key;

			} else if (*key == 'e') {
				if (ui_chown(3, 1, u, num)) {
					goto next_key;
				}

				goto save_st;
			} else if (key[1] == 'e') {
				if (*key == 'l') {
					if (ui_chown(1, 1, u, num)) {
						goto next_key;
					}

					goto save_st;
				} else if (*key == 'r') {
					if (ui_chown(2, 1, u, num)) {
						goto next_key;
					}

					goto save_st;
				}
			}

			break;
		case 's':
			switch (*key) {
			case 'A':
				c = 0;

				if (add_bsize) {
					goto next_key;
				}

				add_hsize = FALSE;
				add_bsize = TRUE;
				re_sort_list();
				disp_fmode();
				goto next_key;

			case 'R':
				c = 0;

				if (!add_bsize) {
					goto next_key;
				}

				add_bsize = FALSE;
				disp_fmode();
				goto next_key;
			}

			/* Don't clear c here! Else sl and sr won't work! */
			open_sh(3);
			break;
		case 'z':
			break;
		case '.':
			if (*key == 'z') {
				c = 0;
				center(top_idx[right_col] + curs[right_col]);
			}

			break;
		case 'F':
			c = 0;
			followlinks = followlinks ? 0 : 1;
			rebuild_db(1);
			break;
		case 'H':
			c = 0;

			if (!curs[right_col])
				break;

			disp_curs(0);
			curs[right_col] = 0;
			disp_list(1);
			break;
		case 'M':
			c = 0;
			disp_curs(0);

			if (db_num[right_col] < top_idx[right_col] + listh)

				curs[right_col] = (db_num[right_col] -
				    top_idx[right_col]) / 2;
			else
				curs[right_col] = listh / 2;

			disp_list(1);
			break;
		case 'L':
			c = 0;
			disp_curs(0);

			if (db_num[right_col] < top_idx[right_col] + listh)

				curs[right_col] = db_num[right_col] -
				    top_idx[right_col] - 1;
			else
				curs[right_col] = listh - 1;

			disp_list(1);
			break;
		case 'o':
			if (!db_num[right_col])
				break;

			f = db_list[right_col][top_idx[right_col] +
			    curs[right_col]];

			if (S_ISREG(f->type[0]) && !S_ISREG(f->type[1])) {
				c = 0;
				action(0, 1, 0, FALSE);
				break;
			}

			if (S_ISREG(f->type[1]) && !S_ISREG(f->type[0])) {
				c = 0;
				action(0, 2, 0, FALSE);
				break;
			}

			break;
		case ':':
			c = 0;

			if (!ed_dialog("Enter command:",
			    NULL /* must be NULL !!! */, complet, 0,
			    &opt_hist)) {
				if (parsopt(rbuf) == 1)
					return;
			}

			break;
		case CTRL('l'):
			c = 0;
			rebuild_scr();
			break;
		case 'W':
			c = 0;
			wait_after_exec = wait_after_exec ? FALSE : TRUE;
			filt_stat();
			wrefresh(wstat);
			break;

		case '`':
		case '\'':
			if (*key == '\'') {
				c = 0;
				center(prev_pos[right_col]);
				break;
			} else if (ns) {
				c = 0;

				if (num < 32 && (u = jmrk[right_col][num])) {
					center(u);
				}

				break;
			}

			if (mark_idx[right_col] < 0) {
				break;
			}

			u = top_idx[right_col] + curs[right_col];

			if (u <= mark_idx[right_col]) {
				num = mark_idx[right_col] - u + 1;
			} else {
				disp_curs(0);
				num = u - mark_idx[right_col] + 1;
				u = mark_idx[right_col];
				/* After deleting files it is distracting
				 * when the cursor is left at some other
				 * unrelated file. So now the cursor is left
				 * at the first not deleted file. */
				if (u < (long)top_idx[right_col]) {
					top_idx[right_col] = u;
				}

				curs[right_col] = u - top_idx[right_col];
			}

			break;

		case '\t':
			if (!fmode)
				break;

			c = 0;
			disp_curs(0);
			wnoutrefresh(getlstwin());
			right_col = right_col ? 0 : 1;
			prt2chead(1);
			disp_curs(1);
			wnoutrefresh(getlstwin());
			wnoutrefresh(wstat);
			doupdate();
			fmode_chdir();
			break;
		case '|':
			if (!twocols)
				c = 0;

			break;
		case CTRL('w'):
			c = 0;
			tgl2c(0);
			break;
		case '#':
			c = 0;
			clr_mark();

			if (fmode || bmode) { /* FM -> diff */
				if (bmode) {
					tgl2c(1);
				} else {
					fmode_dmode();
					one_scan = TRUE;
					/* Use "", not NULL here! */
					enter_dir("", "", FALSE, FALSE,
					    0 LOCVAR);
				}
			} else { /* diff -> FM */
				if (twocols) {
					dmode_fmode(1);
				} else {
					tgl2c(1); /* diff -> bmode */
				}
			}

			break;

		case '%':
			c = 0;
			dontcmp = dontcmp ? FALSE : TRUE;

			if (!(bmode || fmode || dontcmp)) {
				rebuild_db(0);
			}

			filt_stat();
			wrefresh(wstat);
			break;

		case 'V':
		case KEY_IC:
			if (key_mmrk() != 1) {
				/* Already at bottom -> disable "VG" */
				c = 0;
			}
			break;

		case CTRL('g'):
			c = 0;
			prt_ln_num();
			break;

		case 'A': /* Add attribute column */
		case 'D': /* Display directory list */
		case 'R': /* Remove attribute column */
			break;

		case 'N':
			if (regex_mode) {
				c = 0;
				regex_srch(-1);
				break;
			}

			/* fall through */
		default:
			if (isascii(c) && !iscntrl(c)) {
				printerr(NULL,
				    "Invalid input '%c' (type 'h' for help).",
				    c);
			} else {
				printerr(NULL,
				    "Invalid character code 0x%x"
				    " (type 'h' for help).", c);
			}

			c = 0;
		}
	}

	return;

save_st:
	c2 = c;
	c = 0;
	key2[0] = key[0];
	key2[1] = key[1];
	num2 = num;
	goto next_key;
}

static char *helptxt[] = {
       "Type 'q' to quit help, scroll with <DOWN>, <UP>, <PAGE-DOWN>, and <PAGE-UP>.",
       "",
       "Q		Quit " BIN,
       "h, ?		Display help",
       "^L		Refresh display",
       "<TAB>		Toggle column",
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
       "<n>G		Go to line <n>",
       "|<LEFT>		In two-column mode: Enlarge right column",
       "|<RIGHT>	In two-column mode: Enlarge left column",
       "|=		In two-column mode: Make column widths equal",
       "^W		Toggle two-column mode",
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
       "^		Toggle display of files which are in both trees",
       "F		Toggle following symbolic links",
       "E		Toggle file name or file content filter",
       "Ah		Show scaled file size",
       "Ag		Show file group",
       "Ap		Show file mode",
       "As		Show file size",
       "At		Show modification time",
       "Au		Show file owner",
       "Rh		Remove scaled file size column",
       "Rg		Remove file group column",
       "Rp		Remove file mode column",
       "Rs		Remove file size column",
       "Rt		Remove modification time column",
       "Ru		Remove file owner column",
       "p		Show current relative work directory",
       "a		Show command line directory arguments",
       "f		Show full path",
       "[<n>]<<		Copy from second to first tree",
       "[<n>]>>		Copy from first to second tree",
       "'<<		Copy from second to first tree (range cursor...mark)",
       "'>>		Copy from first to second tree (range cursor...mark)",
       "[<n>]C		Copy to other side",
       "'C		Copy to other side (range cursor...mark)",
       "[<n>]dd		Delete file or directory",
       "[<n>]dl		Delete file or directory in first tree",
       "[<n>]dr		Delete file or directory in second tree",
       "'dd		Delete file or directory (range cursor...mark)",
       "'dl		Delete file or directory in first tree (range cursor...mark)",
       "'dr		Delete file or directory in second tree (range cursor...mark)",
       "[<n>]T		Move file or directory",
       "[<n>]Tl		Move file or directory to left tree",
       "[<n>]Tr		Move file or directory to right tree",
       "'T		Move file or directory (range cursor...mark)",
       "'Tl		Move file or directory to left tree (range cursor...mark)",
       "'Tr		Move file or directory to right tree (range cursor...mark)",
       "[<n>]@		Symlink in other tree to selected file or directory",
       "[<n>]@l		Symlink in left tree to file or directory in right tree",
       "[<n>]@r		Symlink in right tree to file or directory in left tree",
       "'@		Create symlink in other tree (range cursor...mark)",
       "'@l		Create symlink in left tree (range cursor...mark)",
       "'@r		Create symlink in right tree (range cursor...mark)",
       "en		Rename file",
       "eln		Rename left file",
       "ern		Rename right file",
       "[<n>]ep		Change file mode",
       "[<n>]elp	Change mode of left file",
       "[<n>]erp	Change mode of right file",
       "'ep		Change file mode (range cursor...mark)",
       "'elp		Change mode of left file (range cursor...mark)",
       "'erp		Change mode of right file (range cursor...mark)",
       "[<n>]eu		Change file owner",
       "[<n>]elu	Change owner of left file",
       "[<n>]eru	Change owner or right file",
       "'eu		Change file owner (range cursor...mark)",
       "'elu		Change owner of left file (range cursor...mark)",
       "'eru		Change owner or right file (range cursor...mark)",
       "[<n>]eg		Change file group",
       "[<n>]elg	Change group of left file",
       "[<n>]erg	Change group or right file",
       "'eg		Change file group (range cursor...mark)",
       "'elg		Change group of left file (range cursor...mark)",
       "'erg		Change group or right file (range cursor...mark)",
       "P		Create directory (bmode and fmode only)",
       "Pl		Create directory in left tree",
       "Pr		Create directory in right tree",
       ".		Repeat last command",
       "m		Mark file or directory",
       "V, <INSERT>	Mark multiple files",
       "VG		Toggle mark of all files from cursor to last line",
       "1GVG		Toggle mark of all files",
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
       ":cd		bmode, fmode: Change to home directory",
       ":cd <path>	bmode, fmode: Change to directory <path>",
       ":e, :edit	Enable write operations and function keys",
       ":find <pattern>",
       "		Display only filenames which match <pattern>",
       ":nofind		Remove filename pattern",
       ":grep <pattern>",
       "		Display only file which contain <pattern>",
       ":nogrep		Remove file content pattern",
       ":marks		List jump marks",
       ":q, :qa		Quit " BIN,
       ":set all	Display option values",
       ":set fkeys	Enable function keys",
       ":set nofkeys	Disable function keys",
       ":set ic		Case-insensitive match",
       ":set noic	Case-sensitive match",
       ":set magic	Use extended regular expressions",
       ":set nomaigc	Use basic regular expressions",
       ":set recursive",
       "		Use recursive diff, find or grep",
       ":set norecursive",
       "		Use non-recursive diff, find or grep",
       ":set ws		Wrap around top or bottom on filename search",
       ":set nows	Don't wrap around top or bottom on filename search",
       ":vie, :view	Set read-only mode, disable function keys",
       "#		Toggle between diff mode and browse mode",
       "=		In fmode: Copy current path from other column",
       "%		Toggle compare file contents",
       "Da		Add current directory to persistent list",
       "Dl		Show persistent directory list",
       "^G		Print cursor line number and number of files",
       "''		Jump to previous cursor position",
       "<n>m		Set jump mark",
       "<n>'		Jump to mark",
       "'l		List jump marks",
       "W		Toggle wait for <ENTER> after running external tool" };

#define HELP_NUM (sizeof(helptxt) / sizeof(*helptxt))

static void
help(void) {

	int c;

	help_top = 0;
	standendc(wlist);
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
		case CTRL('l'):
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

	werase(wlist);
	werase(wstat);

	for (y = 0, i = help_top; y < listh && i < HELP_NUM; y++, i++) {
		/* Does ignore tab char on NetBSD curses */
		/*mvwaddstr(wlist, y, 0, helptxt[i]);*/
		mvwprintw(wlist, y, 0, "%s", helptxt[i]);
	}

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

	for (i = n; i > 0; i--) {
		mvwprintw(wlist, listh - i, 0, "%s",
		    helptxt[help_top + listh - i]);
	}

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

	for (i = 0; i < n; i++) {
		mvwprintw(wlist, i, 0, "%s", helptxt[help_top + i]);
	}

	werase(wstat);
	wnoutrefresh(wlist);
	wrefresh(wstat);
}

#ifdef NCURSES_MOUSE_VERSION
static void
proc_mevent(void)
{
	static bool mb;

	if (getmouse(&mevent) != OK)
		return;

	if (mb) {
		movemb(mevent.x);

		if (mevent.bstate & BUTTON1_RELEASED) {
			mb = FALSE;
			doresizecols();
		}

		return;
	}

	if (twocols && (mevent.bstate & BUTTON1_PRESSED) && mevent.x == llstw) {
		mb = TRUE;
		mousemask(REPORT_MOUSE_POSITION | BUTTON1_RELEASED, NULL);
		return;
	}

	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED ||
	    mevent.bstate & BUTTON1_PRESSED ||
	    mevent.bstate & BUTTON3_CLICKED ||
	    mevent.bstate & BUTTON3_PRESSED) {

		if (( fmode && mevent.y >= (int)listh) ||
		    (!fmode && mevent.y >= (int)(db_num[0] - top_idx[0])))
			return;

		disp_curs(0);

		if (fmode) {
			if (mevent.x < llstw) {
				right_col = 0;
			} else if (mevent.x >= rlstx) {
				right_col = 1;
			}

			fmode_chdir();
		}

		if (mevent.y < (int)(db_num[right_col] - top_idx[right_col])) {
			curs[right_col] = mevent.y;
		}

		disp_curs(1);
		refr_scr();

		if (mevent.bstate & BUTTON3_CLICKED ||
		    mevent.bstate & BUTTON3_PRESSED) {
			tgl_mmrk(DB_LST_ITM);
		} else if (mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
			action(0, 3, 0, FALSE);
		}

# if NCURSES_MOUSE_VERSION >= 2
	} else if (mevent.bstate & BUTTON4_PRESSED) {
		if (fmode) {
			if (mevent.x < llstw) {
				scroll_up(3, FALSE, 0);
			} else if (mevent.x >= rlstx) {
				scroll_up(3, FALSE, 1);
			}
		} else {
			scroll_up(3, FALSE, -1);
		}
	} else if (mevent.bstate & BUTTON5_PRESSED) {
		if (fmode) {
			if (mevent.x < llstw) {
				scroll_down(3, FALSE, 0);
			} else if (mevent.x >= rlstx) {
				scroll_down(3, FALSE, 1);
			}
		} else {
			scroll_down(3, FALSE, -1);
		}
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
    /* 0: <RIGHT> or double click: Enter directory
     * 1: <ENTER>: Do a compare
     * ! IF ACT==0 MARKS ARE IGNORED ! */
    unsigned short act,
    /* Used by 'v', "vl" and "vr":
     * Unzip file but then view raw file using standard view tool. */
    bool raw_cont)
{
	struct filediff *f, *f1, *f2, *z1 = NULL, *z2 = NULL;
	char *t1 = NULL, *t2 = NULL;
	char *err = NULL;
	static char *typerr = "Not a directory or regular file";
	static char *typdif = "Different file type";

#if defined(TRACE)
	fprintf(debug, "->action(ignext=%d t=%d act=%u raw=%d)\n",
	    ign_ext, tree, act, raw_cont);
#endif
	if (!db_num[right_col])
		goto out;

	f = f1 = f2 = db_list[right_col][top_idx[right_col] + curs[right_col]];

#if defined(TRACE)
	fprintf(debug, "  f1->name(%s)\n", f1->name);
#endif
	if (mark && act) {
		struct filediff *m, *m0;
		mode_t ltyp = 0, rtyp = 0;
		char *lnam, *rnam;

		m0 = m = mark;

		if (!ign_ext) {
			/* check if mark needs to be unzipped */
			if ((z1 = unpack(m,
			    m->type[0] && (f1->type[1] || !m->type[1]) ? 1 : 2,
			    &t1, 1)))
				m = z1;

			/* check if other file needs to be unzipped */
			if ((z2 = unpack(f1, f1->type[1] ? 2 : 1, &t2, 1)))
				f1 = z2;
		}

		if (bmode) {
			/* Take all files from left side. */
			lnam = m->name ? m->name : gl_mark;

			if (chk_mark(lnam, 1))
				goto ret;

			ltyp = m->type[0];
			rnam = f1->name;
			rtyp = f1->type[0];

		} else if (m->type[0] && f1->type[1] && (tree & 2)) {
			lnam = m->name ? m->name : mark_lnam;

			if (chk_mark(lnam, 1))
				goto ret;

			ltyp = m->type[0];
			rnam = f1->name;
			rtyp = f1->type[1];

		} else if (f1->type[0] && m->type[1] && (tree & 1)) {
			lnam = f1->name;
			ltyp = f1->type[0];
			rnam = m->name ? m->name : mark_rnam;

			if (chk_mark(rnam, 2))
				goto ret;

			rtyp = m->type[1];

		/* for fmode: Both files on one side */

		} else if (m->type[0] && f1->type[0]) {
			lnam = m->name;

			if (chk_mark(lnam, 1))
				goto ret;

			ltyp = m->type[0];
			rnam = f1->name;
			rtyp = f1->type[0];
			tree = 1;

		} else if (m->type[1] && f1->type[1]) {
			lnam = m->name;

			if (chk_mark(lnam, 2))
				goto ret;

			ltyp = m->type[1];
			rnam = f1->name;
			rtyp = f1->type[1];
			tree = 2;

		} else {
			err = "Both files are in same directory";
			goto ret;
		}

		if ((ltyp & S_IFMT) != (rtyp & S_IFMT) &&
		    !S_ISDIR(ltyp) && !S_ISDIR(rtyp)) {

			err = typdif;
			goto ret;
		}

#if defined(TRACE)
		fprintf(debug, "  mark: lnam(%s) rnam(%s) t=%d\n",
		    lnam, rnam, tree);
#endif
		if (ign_ext || (S_ISREG(ltyp) && S_ISREG(rtyp))) {
			tool(lnam, rnam, tree, ign_ext || raw_cont);

		} else if (S_ISDIR(ltyp) || S_ISDIR(rtyp)) {
			if (!S_ISDIR(ltyp)) {
				if (z2) {
					setpthofs(1, f->name, z2->name);
				}

				enter_dir(NULL , rnam,
				          FALSE, z2 ? TRUE : FALSE,
				          tree LOCVAR);

			} else if (!S_ISDIR(rtyp)) {
				if (z1) {
					setpthofs(0, m0->name, z1->name);
				}

				enter_dir(lnam             , NULL,
				          z1 ? TRUE : FALSE, FALSE,
				          tree LOCVAR);
			} else {
				/* If t<n> == NULL tmpdir is not removed.
				 * It must not be removed before it is left
				 * with "cd .." later. */
				one_scan = TRUE;

				if (z1) {
					setpthofs(0, m0->name, z1->name);
				}

				if (z2) {
					/* 2: don't set vpath[0] */
					setpthofs(2, f->name, z2->name);
				}

				enter_dir(lnam             , rnam,
				          z1 ? TRUE : FALSE, z2 ? TRUE : FALSE,
					  tree LOCVAR);
			}
		}

		goto ret;
	}

	if (!ign_ext) {
		if (f1->type[0] && (z1 = unpack(f1, 1, &t1, 1)))
			f1 = z1;

		if (f2->type[1] && (z2 = unpack(f2, 2, &t2, 1)))
			f2 = z2;
	}

	if (!f1->type[0] ||
	    /* Tested here to support "or" */
	    tree == 2) {
		if (S_ISREG(f2->type[1]) || ign_ext)
			tool(f2->name, NULL, 2, ign_ext || raw_cont);
		else if (S_ISDIR(f2->type[1])) {
			/* Used by fmode */

			if (z2) {
				setpthofs(1, f->name, z2->name);
			}

			enter_dir(NULL , f2->name,
			          FALSE, z2 ? TRUE : FALSE, 0 LOCVAR);
		} else {
			err = typerr;
			goto ret;
		}
	} else if (!f2->type[1] || tree == 1) {
		if (S_ISREG(f1->type[0]) || ign_ext)
			tool(f1->name, NULL, 1, ign_ext || raw_cont);
		else if (S_ISDIR(f1->type[0])) {
			/* Used by bmode and fmode */

			if (z1) {
				setpthofs(bmode ? 1 : 0, f->name, z1->name);
			}

			enter_dir(f1->name         , NULL,
			          z1 ? TRUE : FALSE, FALSE, 0 LOCVAR);
		} else {
			err = typerr;
			goto ret;
		}
	} else if ((f1->type[0] & S_IFMT) == (f2->type[1] & S_IFMT)) {
		if (ign_ext || raw_cont)
			tool(f1->name, f2->name, 3, 1);
		else if (S_ISREG(f1->type[0])) {
			if (f1->diff == '!')
				tool(f1->name, f2->name, 3, 0);
			else
				tool(f1->name, NULL, 1, 0);
		} else if (S_ISDIR(f1->type[0])) {
			if (z1 || z2) {
				one_scan = TRUE;
			}

			if (z1) {
				setpthofs(0, f->name, z1->name);
			}

			if (z2) {
				setpthofs(1, f->name, z2->name);
			}

			enter_dir(f1->name         , f2->name,
			          z1 ? TRUE : FALSE, z2 ? TRUE : FALSE, 0
				  LOCVAR);
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
out:
#if defined(TRACE)
	fprintf(debug, "<-action\n");
#endif
	return;
}

static void
page_down(void)
{
#if defined(TRACE)
	fprintf(debug, "->page_down c=%u\n", curs[right_col]);
#endif
	if (last_line_is_disp()) {
		printerr(NULL, "At bottom");
		goto ret;
	}

	disp_curs(0);
	top_idx[right_col] += listh;
	curs[right_col] = 0;
	disp_list(1);

ret:
#if defined(TRACE)
	fprintf(debug, "<-page_down c=%u\n", curs[right_col]);
#endif
	return;
}

static void
curs_last(void)
{
	if (last_line_is_disp()) {
		return;
	}

	disp_curs(0);
	prev_pos[right_col] = DB_LST_IDX;
	top_idx[right_col] = db_num[right_col] - listh;
	curs[right_col] = listh - 1;
	disp_list(1);
}

static void
page_up(void)
{
#if defined(TRACE)
	fprintf(debug, "->page_up c=%u\n", curs[right_col]);
#endif
	if (first_line_is_top()) {
		printerr(NULL, "At top");
		goto ret;
	}

	disp_curs(0);

	if (top_idx[right_col] < listh) {
		top_idx[right_col] = 0;
		curs[right_col] -= top_idx[right_col];
	} else {
		top_idx[right_col] -= listh;
		curs[right_col] = listh - 1;
	}

	disp_list(1);

ret:
#if defined(TRACE)
	fprintf(debug, "->page_up c=%u\n", curs[right_col]);
#endif
	return;
}

static void
curs_first(void)
{
	if (first_line_is_top())
		return;

	prev_pos[right_col] = DB_LST_IDX;
	top_idx[right_col] = curs[right_col] = 0;
	disp_list(1);
}

static int
last_line_is_disp(void)
{
	int r = 0;
#if defined(TRACE)
	fprintf(debug, "->last_line_is_disp\n");
#endif
	if (db_num[right_col] - top_idx[right_col] <= listh) {
		/* last line is currently displayed */
		if (curs[right_col] != db_num[right_col] -
		    top_idx[right_col] - 1) {

			disp_curs(0);
			curs[right_col] = db_num[right_col] -
			    top_idx[right_col] - 1;
			disp_curs(1);
			refr_scr();
		}

		r = 1;
		goto ret;
	}

ret:
#if defined(TRACE)
	fprintf(debug, "<-last_line_is_disp\n");
#endif
	return r;
}

static int
first_line_is_top(void)
{
	int r = 0;
#if defined(TRACE)
	fprintf(debug, "->first_line_is_top\n");
#endif
	if (!top_idx[right_col]) {
		if (curs[right_col]) {
			disp_curs(0);
			curs[right_col] = 0;
			disp_curs(1);
			refr_scr();
		}

		r = 1;
		goto ret;
	}

ret:
#if defined(TRACE)
	fprintf(debug, "<-first_line_is_top\n");
#endif
	return r;
}

/* 1: Was already at bottom */

int
curs_down(void)
{
	int r = 0;

#if defined(TRACE) && 0
	fprintf(debug, "->curs_down c=%u\n", curs[right_col]);
#endif

	if (top_idx[right_col] + curs[right_col] + 1 >= db_num[right_col]) {
		printerr(NULL, "At bottom");
		r = 1;
		goto ret;
	}

	if (curs[right_col] + 1 >= listh) {
		if (scrollen) {
			disp_curs(0);
			wscrl(getlstwin(), 1);
			top_idx[right_col]++;
			disp_curs(1);
			refr_scr();
		} else
			page_down();

		goto ret;
	}

	disp_curs(0);
	curs[right_col]++;
	disp_curs(1);
	refr_scr();

ret:
#if defined(TRACE) && 0
	fprintf(debug, "<-curs_down c=%u\n", curs[right_col]);
#endif
	return r;
}

static void
curs_up(void)
{
#if defined(TRACE) && 0
	fprintf(debug, "->curs_up c=%u\n", curs[right_col]);
#endif
	if (!curs[right_col]) {
		if (!top_idx[right_col]) {
			printerr(NULL, "At top");
			goto ret;
		}

		if (scrollen) {
			disp_curs(0);
			wscrl(getlstwin(), -1);
			top_idx[right_col]--;
			disp_curs(1);
			refr_scr();
		} else
			page_up();

		goto ret;
	}

	disp_curs(0);
	curs[right_col]--;
	disp_curs(1);
	refr_scr();

ret:
#if defined(TRACE) && 0
	fprintf(debug, "<-curs_up c=%u\n", curs[right_col]);
#endif
	return;
}

static void
scroll_up(unsigned num, bool keepscrpos,
    /* -1: Use active column, 0: Force left column, 1: Force right column */
    int col)
{
	unsigned move_curs, y, i;
	WINDOW *w;
	int ocol;
	bool fake = FALSE;

	ocol = right_col;

	if (col == -1) {
		w = getlstwin();
	} else {
		if (right_col != col) {
			fake = TRUE;
			right_col = col;
		}

		w = right_col ? wrlst : wllst;
	}

	if (!top_idx[right_col]) {
		if (!curs[right_col] || keepscrpos) {
			printerr(NULL, "At top");
			goto exit;
		}

		if (!fake)
			disp_curs(0);

		if (curs[right_col] >= num)
			curs[right_col] -= num;
		else
			curs[right_col] = 0;

		if (!fake)
			disp_curs(1);

		refr_scr();
		goto exit;
	}

	if (top_idx[right_col] < num)
		num = top_idx[right_col];

	if (fake) {
		move_curs = 0;

	} else if (keepscrpos) {
		disp_curs(0);
		move_curs = 1;

	} else if (curs[right_col] + num >= listh) {
		disp_curs(0);
		curs[right_col] = listh - 1;
		move_curs = 1;
	} else {
		curs[right_col] += num;
		move_curs = 0;
	}

	wscrl(w, -((int)num));
	top_idx[right_col] -= num;

	for (y = 0, i = top_idx[right_col]; y < num; y++, i++)
		disp_line(y, i, 0);

	if (move_curs)
		disp_curs(1);

	refr_scr();

exit:
	right_col = ocol;
}

static void
scroll_down(unsigned num, bool keepscrpos, int col)
{
	unsigned move_curs, y, i, ti;
	WINDOW *w;
	int ocol;
	bool fake = FALSE;

	ocol = right_col;

	if (col == -1) {
		w = getlstwin();
	} else {
		if (right_col != col)
			fake = TRUE;

		right_col = col;
		w = col ? wrlst : wllst;
	}

	if (top_idx[right_col] >= db_num[right_col] - 1) {
		printerr(NULL, "At bottom");
		goto exit;
	}

	if (top_idx[right_col] + num >= db_num[right_col])
		num = db_num[right_col] - 1 - top_idx[right_col];

	ti = top_idx[right_col] + num;

	if (fake) {
		move_curs = 0;

	} else if (keepscrpos) {
		disp_curs(0);
		move_curs = 1;

		if (ti + curs[right_col] >= db_num[right_col])
			curs[right_col] = db_num[right_col] - 1 - ti;

	} else if (curs[right_col] < num) {
		disp_curs(0);
		curs[right_col] = 0;
		move_curs = 1;
	} else {
		curs[right_col] -= num;
		move_curs = 0;
	}

	wscrl(w, num);
	top_idx[right_col] = ti;

	for (y = listh - num, i = top_idx[right_col] + y;
	    y < listh && ((twocols && !fmode) || i < db_num[right_col]);
	    y++, i++)
		if (i >= db_num[right_col]) {
			standoutc(w);
			mvwaddch(w, y, llstw, ' ');
		} else {
			disp_line(y, i, 0);
		}

	if (move_curs)
		disp_curs(1);

	refr_scr();

exit:
	right_col = ocol;
}

void
disp_curs(
    /* 0: Remove cursor
     * 1: Normal cursor */
    int a)
{
	WINDOW *w;
	unsigned i, y, m;
	struct filediff *f;
	bool cg;

	w = getlstwin();
	y = curs[right_col];
	i = top_idx[right_col] + y;

	if (i >= db_num[right_col]) {
		return;
	}

	m = mark_idx[right_col];
	cg = CHGAT_MRKS;
	f = db_list[right_col][i];

#if defined(TRACE) && 0
	fprintf(debug, "->disp_curs(%i) i=%u c=%u n[%d]=%u \"%s\"\n",
	    a, i, y, right_col, db_num[right_col],
	    i < db_num[right_col] ? db_list[right_col][i]->name :
	    "index out of bounds");
#endif
	if (cg) {
		if (!a) {
			chgat_off(w, y);
		}
	} else if (a) {
		standoutc(w);

	} else if (i == m) {
		a = 1;
		markc(w);

	} else if (f->fl & FDFL_MMRK) {
		a = 1;
		mmrkc(w);
	}

	if (i < db_num[right_col]) {
		disp_line(y, i, a);
	}

	if (cg) {
		if (a) {
			chgat_curs(w, y);

		} else if (i == m) {
			chgat_mark(w, y);

		} else if (f->fl & FDFL_MMRK) {
			chgat_mmrk(w, y);
		}
	}
#if defined(TRACE) && 0
	fprintf(debug, "<-disp_curs c=%u\n", curs[right_col]);
#endif
}

void
disp_list(
    /* Reserverd for 32 mode flags
     * Value 0: No cursor! */
    unsigned md)
{
	unsigned y, i;
	WINDOW *w;
	bool cg;

#if defined(TRACE)
	fprintf(debug, "->disp_list(%u) col=%d\n", md, right_col);
#endif
	w = getlstwin();
	cg = CHGAT_MRKS;

	/* For the case that entries had been removed
	 * and page_down() */

	if (top_idx[right_col] >= db_num[right_col]) {

		top_idx[right_col] =
		    db_num[right_col] ? db_num[right_col] - 1 : 0;
	}

	if (top_idx[right_col] + curs[right_col] >= db_num[right_col]) {

		curs[right_col] = db_num[right_col] ? db_num[right_col] -
		    top_idx[right_col] - 1 : 0;
	}

	if (fmode && right_col) {
		/* Else glyphs are left in right column with ncursesw */
		wclear(w);
	} else {
		werase(w);
	}

	/* Else glyphs are left with NetBSD curses */
	wrefresh(w);

	if (!db_num[right_col]) {
		no_file();
		goto exit;
	}

	for (y = 0, i = top_idx[right_col];
	    y < listh && ((twocols && !fmode) || i < db_num[right_col]);
	    y++, i++) {
		if (i >= db_num[right_col]) {
			standoutc(w);
			mvwaddch(w, y, llstw, ' ');
			standendc(w);
		} else if (md && y == curs[right_col]) {
			disp_curs(1);
		} else if ((long)(top_idx[right_col] + y) ==
		    mark_idx[right_col]) {
			if (!cg) {
				markc(w);
			}

			disp_line(y, i, 1);

			if (cg) {
				chgat_mark(w, y);
			}
		} else if ((db_list[right_col][top_idx[right_col] + y])->fl
		    & FDFL_MMRK) {
			if (!cg) {
				mmrkc(w);
			}

			disp_line(y, i, 1);

			if (cg) {
				chgat_mmrk(w, y);
			}
		} else {
			disp_line(y, i, 0);
		}
	}

exit:
	refr_scr();
#if defined(TRACE)
	fprintf(debug, "<-disp_list\n");
#endif
	return;
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

#if defined(DEBUG)
	if (i >= db_num[right_col]) {
		standoutc(wstat);
		printerr("disp_line: i >= num", "");
		return;
	}
#endif

	w = getlstwin();
	f = db_list[right_col][i];
	mx = !fmode    ? (int)listw :
	     right_col ?      rlstw :
	                      llstw ;

	if (twocols && !fmode) {
		(wattr_get)(w, &a, &cp, NULL);
	}
#if defined(TRACE) && 0
	else {
		a = 0; cp = 0;
	}

	fprintf(debug,
	    "->disp_line(y=%u i=%u is_curs=%i) attr=0x%x color=%d\n",
	    y, i, info, a, cp);
#endif

	if (fmode || bmode) {
		goto no_diff;
	} else if (!f->type[0]) {
		diff = '>';
		*mode = f->type[1];
		color_id = PAIR_RIGHTONLY;
	} else if (!f->type[1]) {
		diff = '<';
		*mode = f->type[0];
		color_id = PAIR_LEFTONLY;
	} else if ((f->type[0] & S_IFMT) != (f->type[1] & S_IFMT)) {
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
		*mode = right_col ? f->type[1] : f->type[0];

		if (diff == '!')
			color_id = PAIR_DIFF;
		else if (diff == '-')
			color_id = PAIR_ERROR;
	}

	if (twocols && !fmode && diff == '>')
			goto prtc2;

	set_file_info(f, twocols && !fmode ? f->type[0] : *mode, type,
	    &color_id, &diff);
	disp_name(w, y, 0, twocols && !fmode ? llstw : mx, info, f, *type,
	    color_id,
	    right_col           ? f->rlink :
	    twocols || f->llink ? f->llink : f->rlink,
	    diff, fmode ? right_col : twocols || f->type[0] ? 0 : 1);

	if (twocols && !fmode) {
prtc2:
		if (diff != '<') {
			(wattr_set)(w, a, cp, NULL);
			set_file_info(f, f->type[1], type+1, &color_id, &diff);
			disp_name(w, y, rlstx, mx, info, f, type[1], color_id,
			    f->rlink, diff, 1);
		}

		standoutc(w);
		mvwaddch(w, y, llstw, diff);
		standendc(w);
	}

	if (!info) {
		goto ret;
	}

	werase(wstat);

	if (bmode && dir_change) {
	} else if (mark &&
	    /* fmode has only local marks which are highlighted anyway.
	     * Hence it is not necessary to display the mark in the
	     * status line. */
	    !fmode) {
		if (wstat_dirty) {
			disp_mark();
		}
	} else if (edit) {
		if (wstat_dirty)
			disp_edit();
		goto ret;
	} else if (regex_mode) {
		if (wstat_dirty)
			disp_regex();
		goto ret;
	}

	statcol(mark && !fmode ? 2 : 1);

	if (type[0] == '!' || diff == 'X') {
		bool tc = twocols || mark;

		mvwaddstr(wstat, 0, tc ? 0 : 2, type_name(f->type[0]));

		if (f->llink) {
			addmbs(wstat, " -> ", mx);
			addmbs(wstat, f->llink, mx);
		}

		mvwaddstr(wstat, tc ? 0 : 1, tc ? rlstx : 2,
		    type_name(f->type[1]));

		if (f->rlink) {
			addmbs(wstat, " -> ", 0);
			addmbs(wstat, f->rlink, 0);
		}
	} else if (fmode) {
		file_stat(
		    db_num[0] ? db_list[0][top_idx[0] + curs[0]] : NULL,
		    db_num[1] ? db_list[1][top_idx[1] + curs[1]] : NULL);
	} else {
		file_stat(f, f);
	}

	filt_stat();
	dir_change = FALSE;

ret:
#if defined(TRACE) && 0
	(wattr_get)(w, &a, &cp, NULL);
	fprintf(debug, "<-disp_line attr=0x%x color=%d\n", a, cp);
#endif
	return;
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

			if (is_diff_dir(f)) {
				*d = '!';
			}
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
    short ct, char *l, int d,
    /* tree--*not* col! */
    int i)
{
	int j;
	struct passwd *pw;
	struct group *gr;
	int db;

	db = fmode ? right_col : 0;

	if (add_mode) {
		mx -= 5;
	}

	if (add_owner) {
		mx -= usrlen[db];
	}

	if (add_group) {
		mx -= grplen[db];
	}

	if (add_hsize) {
		mx -= 5;
	} else if (add_bsize) {
		mx -= bsizlen[db];
	}

	if (add_mtime) {
		mx -= 13;
	}

	if (color && !o) {
		if (ct) {
			wattron(w, COLOR_PAIR(ct) | A_BOLD);
		} else {
			/* not attrset, else bold is off */
			wattron(w, COLOR_PAIR(PAIR_NORMAL));
		}
	}

	if (twocols || bmode) {
		mvwprintw(w, y, x, "%c ", t);
	} else {
		mvwprintw(w, y, x, "%c %c ", d, t);
	}

	j = addmbs(w, f->name, mx);
	standendc(w);

	if (j) {
		return 1;
	}

	if (l) {
		addmbs(w, " -> ", mx);
		putmbsra(w, l, mx);
	}

	if (add_mode) {
		mx += 5;
		snprintf(lbuf, sizeof lbuf, "%04o", f->type[i] & 07777);
		wmove(w, y, mx - 4);
		addmbs(w, lbuf, 0);
	}

	if (add_owner) {
		if ((pw = getpwuid(f->uid[i])))
			memcpy(lbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->uid[i]);

		wmove(w, y, mx + 1);
		addmbs(w, lbuf, 0);
		mx += usrlen[db];
	}

	if (add_group) {
		if ((gr = getgrgid(f->gid[i])))
			memcpy(lbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->gid[i]);

		wmove(w, y, mx + 1);
		addmbs(w, lbuf, 0);
		mx += grplen[db];
	}

	if (add_hsize) {
		size_t n;

		mx += 5;
		n = getfilesize(lbuf, sizeof lbuf, f->siz[i], 1);
		wmove(w, y, mx - n);
		addmbs(w, lbuf, 0);

	} else if (add_bsize) {
		size_t n;

		mx += bsizlen[db];

		if (S_ISCHR(f->type[i]) || S_ISBLK(f->type[i])) {

			n = snprintf(lbuf, sizeof lbuf, "%lu, %lu",
			    (unsigned long)major(f->rdev[i]),
			    (unsigned long)minor(f->rdev[i]));
		} else {
			n = getfilesize(lbuf, sizeof lbuf, f->siz[i], 2);
		}

		wmove(w, y, mx - n);
		addmbs(w, lbuf, 0);
	}

	if (add_mtime) {
		size_t n;

		mx += 13;
		n = gettimestr(lbuf, sizeof lbuf, &f->mtim[i]);
		wmove(w, y, mx - n);
		addmbs(w, lbuf, 0);
	}

	return 0;
}

static void
statcol(
    /* See prt2chead() */
    int m)
{
	if (bmode) {
		return;
	}

	if (twocols || (m & 2)) {
		prt2chead(m);
		return;
	}

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
	bool tc = twocols || mark;

	standendc(wstat);
	x  = tc || bmode ? 0 : 2;
	x2 = tc    ? rlstx :
	     bmode ? 0     : 2;
	yl = 0;
	yr = tc ? 0 : 1;
	ltyp = f  ? f->type[0]  : 0;
	rtyp = f2 ? f2->type[1] : 0;
	mx1 = tc ? llstw : 0;

	if (bmode) {
		if (!mark || dir_change) {
			wmove(wstat, 1, 0);
			setvpth(2);
			putmbsra(wstat, vpath[1], 0);
		}
	} else if (dir_change) {
		setvpth(2);
		wmove(wstat, 0, mark ? 0 : 2);
		putmbsra(wstat, vpath[0], mark ? mx1 : 0);
		wmove(wstat, mark ? 0 : 1, mark ? rlstx : 2);
		putmbsra(wstat, vpath[1], 0);
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

	if (tc && x >= llstw)
		ltyp = 0;

	if (tc && x2 >= COLS)
		rtyp = 0;

	if (ltyp) {
		if ((pw = getpwuid(f->uid[0])))
			memcpy(lbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->uid[0]);
	}

	if (rtyp) {
		if ((pw = getpwuid(f2->uid[1])))
			memcpy(rbuf, pw->pw_name, strlen(pw->pw_name) + 1);
		else
			snprintf(rbuf, sizeof rbuf, "%u", f2->uid[1]);
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

	if (tc) {
		x += w1;
		x2 += w2;
	} else {
		x += w1 > w2 ? w1 : w2;
		x2 = x;
	}

	if (tc && x >= llstw)
		ltyp = 0;

	if (tc && x2 >= COLS)
		rtyp = 0;

	if (ltyp) {
		if ((gr = getgrgid(f->gid[0])))
			memcpy(lbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(lbuf, sizeof lbuf, "%u", f->gid[0]);
	}

	if (rtyp) {
		if ((gr = getgrgid(f2->gid[1])))
			memcpy(rbuf, gr->gr_name, strlen(gr->gr_name) + 1);
		else
			snprintf(rbuf, sizeof rbuf, "%u", f2->gid[1]);
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

	if (tc) {
		x += w1;
		x2 += w2;
	} else {
		x += w1 > w2 ? w1 : w2;
		x2 = x;
	}

	if (tc && x >= llstw)
		ltyp = 0;

	if (tc && x2 >= COLS)
		rtyp = 0;

	if (ltyp && !S_ISDIR(ltyp)) {
		if (S_ISCHR(ltyp) || S_ISBLK(ltyp))
			w1 = snprintf(lbuf, sizeof lbuf, "%lu, %lu",
			    (unsigned long)major(f->rdev[0]),
			    (unsigned long)minor(f->rdev[0]));
		else
			w1 = getfilesize(lbuf, sizeof lbuf, f->siz[0],
			    scale || twocols ? 1 : 0);

		w1++;
	} else
		w1 = 0;

	if (rtyp && !S_ISDIR(rtyp)) {
		if (S_ISCHR(rtyp) || S_ISBLK(rtyp))
			w2 = snprintf(rbuf, sizeof rbuf, "%lu, %lu",
			    (unsigned long)major(f2->rdev[1]),
			    (unsigned long)minor(f2->rdev[1]));
		else
			w2 = getfilesize(rbuf, sizeof rbuf, f2->siz[1],
			    scale || twocols ? 1 : 0);

		w2++;
	} else
		w2 = 0;

	w = w1 > w2 ? w1 : w2;

	if (ltyp && !S_ISDIR(ltyp)) {
		wmove(wstat, yl, tc ? x : x + w - w1);
		addmbs(wstat, lbuf, mx1);

		if (tc)
			x += w1;
	}

	if (rtyp && !S_ISDIR(rtyp)) {
		wmove(wstat, yr, tc ? x2 : x2 + w - w2);
		addmbs(wstat, rbuf, 0);

		if (tc)
			x2 += w2;
	}

	if (!tc && (
	    (ltyp && !S_ISDIR(ltyp)) ||
	    (rtyp && !S_ISDIR(rtyp)))) {
		x += w;
		x2 = x;
	}

	if (tc && x >= llstw)
		ltyp = 0;

	if (tc && x2 >= COLS)
		rtyp = 0;

	if (ltyp) {
		lx1 = x + gettimestr(lbuf, sizeof lbuf, &f->mtim[0]);
		wmove(wstat, yl, x);
		addmbs(wstat, lbuf, mx1);
	}

	if (rtyp) {
		lx2 = x2 + gettimestr(rbuf, sizeof rbuf, &f2->mtim[1]);
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
getfilesize(char *buf, size_t bufsiz, off_t size,
    /* 1: scale */
    /* 2: don't group */
    unsigned md)
{
	char *unit;
	float f;

	if (!md) {
		return snprintf(buf, bufsiz, "%'lld", (long long)size);

	} else if ((md & 2) || size < 1024) {
		return snprintf(buf, bufsiz, "%lld", (long long)size);

	} else {
		f = size / 1024.0;
		unit = "K";

		if (f >= 1000) {
			f /= 1024.0;
			unit = "M";
		}

		if (f >= 1000) {
			f /= 1024.0;
			unit = "G";
		}

		if (f >= 1000) {
			f /= 1024.0;
			unit = "T";
		}
	}

	if (f < 10) {
		return snprintf(buf, bufsiz, "%.1f%s", f, unit);
	} else {
		return snprintf(buf, bufsiz, "%.0f%s", f+.5, unit);
	}
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
#if defined(TRACE)
	fprintf(debug, "->set_mark\n");
#endif
	if (!db_num[right_col]) {
		goto ret;
	}

	if (mark) {
		clr_mark();
	}

	mark_idx[right_col] = top_idx[right_col] + curs[right_col];
	mark = db_list[right_col][mark_idx[right_col]];
	disp_mark();

ret:
#if defined(TRACE)
	fprintf(debug, "<-set_mark\n");
#endif
	return;
}

static void
disp_mark(void)
{
	if (fmode)
		return;

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
	int rc;

#if defined(TRACE)
	fprintf(debug, "->clr_mark mi0=%ld mi1=%ld\n",
	    mark_idx[0], mark_idx[1]);
#endif
	rc = right_col;

	if (gl_mark) {
		free(gl_mark);
		free(mark_lnam);
		free(mark_rnam);
		free(mark);
		gl_mark = NULL;

	} else if (
	    mark_idx[0] >= (long)(top_idx[0]) &&
	    mark_idx[0] <  (long)(top_idx[0] + listh) &&
	    (mark_idx[0] != (long)(top_idx[0] + curs[0]) || rc)) {

		if (CHGAT_MRKS) {
			chgat_off(fmode ? wllst : wlist,
			    mark_idx[0] - top_idx[0]);
		}

		right_col = 0;
		disp_line(mark_idx[0] - top_idx[0], mark_idx[0], 0);
		wnoutrefresh(fmode ? wllst : wlist);

	} else if (fmode &&
	    mark_idx[1] >= (long)(top_idx[1]) &&
	    mark_idx[1] <  (long)(top_idx[1] + listh) &&
	    (mark_idx[1] != (long)(top_idx[1] + curs[1]) || !rc)) {

		chgat_off(wrlst, mark_idx[1] - top_idx[1]);

		right_col = 1;
		disp_line(mark_idx[1] - top_idx[1], mark_idx[1], 0);
		wnoutrefresh(wrlst);
	}

	right_col = rc;
	mark_idx[0] = -1;
	mark_idx[1] = -1;
	mark = NULL;
	werase(wstat);
	filt_stat();
	wnoutrefresh(wstat);
	doupdate();
#if defined(TRACE)
	fprintf(debug, "<-clr_mark\n");
#endif
}

void
mark_global(void)
{
	struct filediff *m;

#if defined(TRACE)
	fprintf(debug, "->mark_global(%s) c=%u\n",
	    mark->name, curs[right_col]);
#endif
	if (fmode) {
		clr_mark();
		goto ret;
	}

	mark_idx[right_col] = -1;
	m = malloc(sizeof(struct filediff));
	*m = *mark;
	mark = m;
	mark_lnam = NULL;
	mark_rnam = NULL;

	if (bmode) {
		pthcat(syspth[0], pthlen[0], m->name);

		if (!(gl_mark = realpath(syspth[0], NULL))) {
			/* Ignore error if file had just been deleted */
			if (errno != ENOENT)
				printerr(strerror(errno),
				    LOCFMT "realpath \"%s\"" LOCVAR, syspth[0]);
			syspth[0][pthlen[0]] = 0;
			goto error;
		}

		syspth[0][pthlen[0]] = 0;
	} else {
		if (!*syspth[0])
			m->type[0] = 0; /* tmp dir left */

		if (!*syspth[1])
			m->type[1] = 0;

		if (!m->type[0] && !m->type[1])
			goto error;

		if (m->type[0]) {
			pthcat(syspth[0], pthlen[0], m->name);
			gl_mark = strdup(syspth[0]);
			mark_lnam = strdup(syspth[0]);
			syspth[0][pthlen[0]] = 0;
		}

		if (m->type[1]) {
			pthcat(syspth[1], pthlen[1], m->name);

			if (!m->type[0])
				gl_mark = strdup(syspth[1]);

			mark_rnam = strdup(syspth[1]);
			syspth[1][pthlen[1]] = 0;
		}
	}

	m->name = NULL;
	goto ret;

error:
	free(m);
	clr_mark();
ret:
#if defined(TRACE)
	fprintf(debug, "<-mark_global(%s) c=%u\n", gl_mark, curs[right_col]);
#endif
	return;
}

#define YANK(x) \
	do { \
		if (f->type[x]) { \
			ed_append(" "); \
			/* here because 1st ed_append deletes lbuf */ \
			shell_quote(lbuf, syspth[x], sizeof lbuf); \
			ed_append(lbuf); \
			*lbuf = 0; \
		} \
	} while (0)

static void
yank_name(int reverse)
{
	struct filediff *f;

	if (!db_num[right_col])
		return;

	f = db_list[right_col][top_idx[right_col] + curs[right_col]];

	if (f->type[0])
		pthcat(syspth[0], pthlen[0], f->name);

	if (f->type[1])
		pthcat(syspth[1], pthlen[1], f->name);

	if (reverse)
		YANK(1);

	YANK(0);

	if (!reverse)
		YANK(1);

	disp_edit();
}

void
no_file(void)
{
	int n = 0;

	if (!(bmode || fmode)) {
		if (real_diff) n++;
		if (noequal  ) n++;
		if (nosingle ) n++;
		if (excl_or  ) n++;
	}

	if (file_pattern) n++;

	if (n) {
		printerr(NULL,
		    "No file in list (key%s %s%s%s%s%sdisable%s filter%s).",
		    n > 1 ? "s" : "",
		    file_pattern ? "'E' " : "",
		    real_diff    ? "'c' " : "",
		    noequal      ? "'!' " : "",
		    nosingle     ? "'&' " : "",
		    excl_or      ? "'^' " : "",
		    n > 1 ? "" : "s",
		    n > 1 ? "s" : "");

		if (!bmode && !fmode && recursive) {
			syspth[right_col][pthlen[right_col]] = 0;
			is_diff_pth(syspth[right_col], 1);
		}
	} else {
		printerr(NULL, "Directory is empty.");
	}
}

void
center(unsigned idx)
{
#if defined(TRACE)
	fprintf(debug, "<>center\n");
#endif
	disp_curs(0);
	prev_pos[right_col] = DB_LST_IDX;

	if (db_num[right_col] <= listh || idx <= listh / 2) {
		top_idx[right_col] = 0;
	} else if (db_num[right_col] - idx < listh / 2) {
		top_idx[right_col] = db_num[right_col] - listh;
	} else {
		top_idx[right_col] = idx - listh / 2;
	}

	curs[right_col] = idx - top_idx[right_col];
	disp_fmode();
}

static void
push_state(char *name, char *rnam,
    /* 1: lzip */
    /* 2: rzip */
    /* 4: don't push state */
    unsigned md)
{
	struct ui_state *st;

#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "->push_state(ln(%s) rn(%s) md=%u) lp(%s) rp(%s)\n",
	    name, rnam, md, trcpth[0], trcpth[1]);
#endif
	if (name && rnam && !*name && !*rnam) {
		/* from fmode */
		goto ret;
	}

	st = malloc(sizeof(struct ui_state));


	diff_db_store(st);
	st->llen = pthlen[0];
	st->rlen = pthlen[1];

	/* If an absolute path is given, save the current path for the
	 * time when this absolute path is left later */

	if (!(name && *name == '/'))
		st->lpth = NULL;
	else {
		st->lpth = strdup(syspth[0]);
		*syspth[0] = 0;
		pthlen[0] = 0;
		pwd = syspth[0];
	}

	if (!(rnam && *rnam == '/'))
		st->rpth = NULL;
	else {
		st->rpth = strdup(syspth[1]);
		*syspth[1] = 0;
		pthlen[1] = 0;
		rpwd = syspth[1];
	}

	/* Save name if temporary directory for removing it later */

	st->lzip = md & 1 ? strdup(name) : NULL;
	st->rzip = md & 2 ? strdup(rnam) : NULL;
	st->fl = md & 4 ? 1 : 0;
	st->top_idx = *top_idx;
	*top_idx = 0;
	st->curs = *curs;
	*curs = 0;
	st->tree = subtree;
	st->next = ui_stack;
	ui_stack = st;
ret:
#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "<-push_state lp(%s) rp(%s)\n", trcpth[0], trcpth[1]);
#endif
	return;
}

void
pop_state(
    /* 1: Normal mode
     * 0: Cleanup mode */
    short mode)
{
	struct ui_state *st = ui_stack;
	bool d2f;

#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "->pop_state(m=%d) lp(%s) rp(%s) fp(%s)\n",
	    mode, trcpth[0], trcpth[1], fpath);
#endif
	if (!st) {
		if (from_fmode) {
			restore_fmode();
		} else {
			printerr(NULL, "At top directory");
		}

		goto ret;
	}

	d2f = st->fl & 1 ? TRUE : FALSE;

	if (st->lzip) {
		if (mode && (
		      /* A global mark inside the archive */
		      (gl_mark && mark_lnam &&
		       !strncmp(syspth[0], mark_lnam, pthlen[0])) ||
		      /* A local mark -> can only be in archive
		       * since we are just leaving a archive */
		      (mark && !gl_mark)
		    ))
			clr_mark();

		st->lzip[strlen(st->lzip) - 2] = 0;
		rmtmpdirs(st->lzip, TOOL_NOLIST);
		respthofs(0);
	}

	if (st->rzip) {
		if (mode && (
		      (gl_mark && mark_rnam &&
		       !strncmp(syspth[1], mark_rnam, pthlen[1])) ||
		      (mark && !gl_mark)
		    ))
			clr_mark();

		st->rzip[strlen(st->rzip) - 2] = 0;
		rmtmpdirs(st->rzip, TOOL_NOLIST);
		respthofs(1);
	}

	if (mark) {
		if (mode) {
			if (!gl_mark)
				mark_global();
		} else
			clr_mark();
	}

	pthlen[0] = st->llen;
	pthlen[1] = st->rlen;

	if (st->lpth) {
		if (mode)
			memcpy(syspth[0], st->lpth, pthlen[0]);

		syspth[0][pthlen[0]] = 0;
		free(st->lpth);
	}

	if (st->rpth) {
		if (mode)
			memcpy(syspth[1], st->rpth, pthlen[1]);

		syspth[1][pthlen[1]] = 0;
		free(st->rpth);
	}

	*top_idx = st->top_idx;
	*curs    = st->curs;
	subtree  = st->tree;
	ui_stack = st->next;
	syspth[0][pthlen[0]] = 0; /* For 'p' (pwd) */
	diff_db_restore(st);
	free(st);

	if (!mode) {
		goto ret;
	}

	if (d2f) {
		if (from_fmode) {
			restore_fmode();
		} else {
			tgl2c(1);
		}
	} else if (mode) {
		if (!twocols) {
			dir_change = TRUE;
		}

		disp_list(1);
	}

ret:
#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "<-pop_state lp(%s) rp(%s) fp(%s)\n",
	    trcpth[0], trcpth[1], fpath);
#endif
	return;
}

void
enter_dir(char *name, char *rnam, bool lzip, bool rzip, short tree
#ifdef DEBUG
    , char *_file, unsigned _line
#endif
    )
{
	int i;
	unsigned *uv;
	char *cp /* current path */, *sp /* saved path */;
	struct bpth *bpth;
	size_t *lp;
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	struct ptr_db_ent *n;
#endif
	bool f2d = FALSE;

#ifdef TRACE
	TRCPTH;
	fprintf(debug, "->" LOCFMT
	    "enter_dir(ln(%s) rn(%s) lz=%d rz=%d t=%d) lp(%s) rp(%s)\n",
# ifdef DEBUG
	    _file, _line,
# endif
	    name, rnam, lzip, rzip, tree, trcpth[0], trcpth[1]);
#endif
	if (!twocols) {
		dir_change = TRUE;
	}

	if (name && rnam) {
		if (bmode || fmode) {

			clr_mark();
			name = strdup(name);
			rnam = strdup(rnam);
			f2d = TRUE;
		}

		if (bmode) {
			tgl2c(1);

		} else if (fmode) {
			if (tree == 1) {
				fpath = strdup(syspth[1]);
				fpath[pthlen[1]] = 0;
				memcpy(syspth[1], syspth[0], pthlen[0]+1);
				pthlen[1] = pthlen[0];
			} else if (tree == 2) {
				fpath = strdup(syspth[0]);
				fpath[pthlen[0]] = 0;
				memcpy(syspth[0], syspth[1], pthlen[1]+1);
				pthlen[0] = pthlen[1];
			}

			fmode_dmode();
		}
	}

	if (mark && !gl_mark && (!fmode ||
	    (!right_col && mark->type[0]) ||
	    ( right_col && mark->type[1]))) {

		mark_global();
	}

	if (!bmode && !fmode) {
		push_state(name, rnam,
		    (f2d ? 4 : 0) | (rzip ? 2 : 0) | (lzip ? 1 : 0));
		subtree = (name ? 1 : 0) | (rnam ? 2 : 0);
		scan_subdir(name, rnam, subtree);

		if (f2d) {
			free(name);
			free(rnam);
		}

		goto disp;
	}

	if (!name) {
		name = rnam;
		lzip = rzip;
	}

	if (bmode) {
		cp = syspth[1];
	} else {
		syspth[right_col][pthlen[right_col]] = 0;
		cp = syspth[right_col];
		lp = &pthlen[right_col];
		sp = strdup(cp);
	}

	db_set_curs(right_col, cp, top_idx[right_col], curs[right_col]);
	n = NULL; /* flag */

	/* Not in bmode since syspth[0] is always "." there */
	if (!lzip && !bmode && name && *name == '/') {
		*lp = 0;
		*cp = 0;
	}

	if (lzip) {
		if (bmode) {
			if (!getcwd(syspth[1], sizeof syspth[1]))
				printerr(strerror(errno),
				    "getcwd failed");

			cp = syspth[1];
		}

		bpth = malloc(sizeof(struct bpth));
		bpth->pth = strdup(cp);

		if (!bmode) {
			bpth->col = right_col;
			*lp = 0;
			*cp = 0;
		}

		ptr_db_add(&uz_path_db, strdup(name), bpth);

	} else if (name && *name == '.' && name[1] == '.' && !name[2] &&
	    !ptr_db_srch(&uz_path_db, cp, (void **)&bpth,
	    (void **)&n)) {

		name = bpth->pth; /* dat */

		if (!bmode) {
			size_t l;

			l = strlen(name);
			memcpy(syspth[bpth->col], name, l+1);
			pthlen[bpth->col] = l;
			free(name);
			name = NULL;
		}

		free(bpth);
#ifdef HAVE_LIBAVLBST
		rnam = n->key.p;
#else
		rnam = n->key;
#endif
		ptr_db_del(&uz_path_db, n);
	} else {
		/* DON'T REMOVE! (Cause currently unclear) */
		n = NULL;
	}

	if (bmode && chdir(name) == -1) {
		printerr(strerror(errno), "chdir \"%s\":", name);
	}

	if (fmode && name) {
		name = strdup(name);
	}

	top_idx[right_col] = 0;
	curs[right_col] = 0;
	diff_db_free(right_col);
	i = scan_subdir(fmode ? name : NULL, NULL, right_col ? 2 : 1);

	if (fmode) {
		free(name);

		if (i) {
			/* sp is absolut. So reset length to place sp at
			 * begin of string. */
			*lp = 0;
			scan_subdir(sp, NULL, right_col ? 2 : 1);
		} else {
			fmode_chdir();
		}

		free(sp);
	}

	if (n) {
		size_t l;

		l = strlen(rnam);

		if (gl_mark && !strncmp(rnam, gl_mark, l))
			clr_mark();

		rnam[l - 2] = 0; /* remove "/[lr]" */
		rmtmpdirs(rnam, TOOL_NOLIST); /* does a free() */
		respthofs(bmode || right_col ? 1 : 0);

		if (bmode)
			free(name); /* dat */
	}

	if (right_col) {
		syspth[1][pthlen[1]] = 0;
	} else if (fmode) {
		syspth[0][pthlen[0]] = 0;
	}

	if ((uv = db_get_curs(right_col, cp))) {
		top_idx[right_col] = *uv++;
		curs[right_col] = *uv;
	}

disp:
	disp_list(1);
#ifdef TRACE
	TRCPTH;
	fprintf(debug, "<-enter_dir lp(%s) rp(%s)\n", trcpth[0], trcpth[1]);
#endif
}

void
printerr(const char *s2, const char *s1, ...)
{
	va_list ap;

#if defined(TRACE)
	fputs("<>printerr: ", debug);
	if (s1) {
		va_start(ap, s1);
		vfprintf(debug, s1, ap);
		va_end(ap);
	}

	if (s2) {
		fprintf(debug, ": %s", s2);
	}

	fputc('\n', debug);
#endif
	if (!wstat) { /* curses not opened */
		if (s1) {
			va_start(ap, s1);
			vfprintf(stderr, s1, ap);
			va_end(ap);
		}

		if (s2) {
			fprintf(stderr, ": %s", s2);
		}

		fputc('\n', stderr);
		return;
	}

	wstat_dirty = TRUE;
	werase(wstat);
	wmove(wstat, s2 ? 0 : 1, 0);

	if (s1) {
		va_start(ap, s1);
		vwprintw(wstat, s1, ap);
		va_end(ap);
	}

	if (s2) {
		wmove(wstat, 1, 0);
		wclrtoeol(wstat);
		wprintw(wstat, "%s", s2);
		wrefresh(wstat);
		getch();
		werase(wstat);
	}

	filt_stat();
	wrefresh(wstat);
}

int
dialog(const char *quest, const char *answ, const char *fmt, ...)
{
	va_list ap;
	int r;

	if (fmt) {
		va_start(ap, fmt);
	}

	r = vdialog(quest, answ, fmt, ap);

	if (fmt) {
		va_end(ap);
	}

	return r;
}

int
vdialog(const char *quest, const char *answ, const char *fmt, va_list ap)
{
	int c, c2;
	const char *s;

	wstat_dirty = TRUE;

	if (fmt) {
		werase(wstat);
		wmove(wstat, 0, 0);
		vwprintw(wstat, fmt, ap);
	}

	mvwprintw(wstat, 1, 0, "%s", quest);
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
	if (fmode) {
		close2cwins();
	}

	delwin(wstat);
	delwin(wlist);
	refresh();
	openwins();

	if (curs[right_col] >= listh) {
		/* calls disp_fmode() */
		center(top_idx[right_col] + curs[right_col]);
	} else {
		disp_fmode();
	}
}

void
set_win_dim(void)
{
	statw = COLS;
	listh = LINES - 2;
	listw = COLS;
	rlstx = COLS / 2 + midoffs;

	if (rlstx < 2) {
		rlstx = 2;
	} else if (rlstx > COLS - 1) {
		rlstx = COLS - 1;
	}

	rlstw = COLS - rlstx;
	llstw = rlstx - 1;
}
