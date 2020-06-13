/*
Copyright (c) 2016-2018, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#ifdef USE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#ifdef USE_SYS_MKDEV_H
# include <sys/mkdev.h>
#endif
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
#include "misc.h"
#include "gq.h"
#ifdef TEST
# include "test.h"
#endif
#include "fkeyListDisplay.h"
#include "MoveCursorToFile.h"

static void ui_ctrl(void);
static void page_down(void);
static void page_up(void);
static void curs_last(void);
static void curs_first(void);
static int last_line_is_disp(void);
static int first_line_is_top(void);
static void curs_up(void);

/**
 * @brief disp_marked_line
 * Display line with possible marking.
 * @param y
 * @param i
 * @param md [0]: 1: No cursor
 * @param w
 */
static void disp_marked_line(const unsigned y, const unsigned i, const unsigned md, WINDOW *const w);

/**
 * @brief disp_line
 * @param y Display line.
 * @param i DB index.
 * @param info 1: is cursor line.
 */
/* Used in: scroll_up(), scroll_down(), disp_curs(), disp_list(), clr_mark() */
static void disp_line(unsigned y, unsigned i, int info);
static void push_state(const char *, const char *, unsigned);
static void help(void);
static const char *type_name(mode_t);
static void ui_resize(void);
static void statcol(int);
static void file_stat(struct filediff *, struct filediff *);
static void set_file_info(struct filediff *, mode_t, int *, short *, int *);
/* Input
 *   mx: window width
 *   o: info
 *   ct: color_id
 *   d: diff
 *   i: tree--*not* col!
 * Modified
 *   lbuf */
static int disp_name(WINDOW *w, int y, int x, int mx, int o,
                     struct filediff *f, int t, short ct, char *l, int d,
                     int i);
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

wchar_t *sh_str[FKEY_MUX_NUM][FKEY_NUM];
char *fkey_cmd[FKEY_MUX_NUM][FKEY_NUM];
char *fkey_comment[FKEY_MUX_NUM][FKEY_NUM];
unsigned fkey_flags[FKEY_MUX_NUM][FKEY_NUM];
int fkey_set;

unsigned listw;
static unsigned help_top;
unsigned listh;
WINDOW *wlist;
WINDOW *wstat;
struct ui_state *ui_stack;
struct filediff *mark;
char *gl_mark, *mark_lnam, *mark_rnam;
static struct history sh_cmd_hist;

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(int *);
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
bool add_ns_mtim;
bool add_owner;
bool add_group;

int
build_ui(void)
{
    int return_value = 0;

#if defined(TRACE) && 1
    fprintf(debug, "->build_ui\n");
#endif

    if (!cli_mode) {
        srandom((unsigned)time(NULL));
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
            return_value |= 2;
            goto ret;
        }
    } /* if (!cli_mode) */

	/* Not in main since build_diff_db() uses printerr() */
	if (recursive) {
        return_value |= do_scan();

        if (cli_mode) {
            goto ret;
		}
    }

	if (bmode || fmode) {
		if (chdir(syspth[0]) == -1) {
			printerr(strerror(errno), "chdir \"%s\":", syspth[0]);
            return_value |= 2;
            goto ret;
		}

		if (bmode) {
			*syspth[0] = '.';
			syspth[0][1] = 0;
			pthlen[0] = 1;
		}
	}

    if (bmode) {
        /* case also used for option -S */
        return_value |= build_diff_db(1);
    } else if (fmode) {
		build_diff_db(1);
		build_diff_db(2);
    } else {
        /* case also used for qdiff */
        return_value |= build_diff_db(3);
    }

    if (cli_mode) {
        goto ret;
    }

    disp_fmode();
    if (moveCursorToFile)
    {
        center(findlistname(moveCursorToFileInst->getFileName(moveCursorToFileInst)));
    }
    if (applyFKey)
    {
        test_fkey(KEY_F(applyFKey), 1, top_idx[right_col] + curs[right_col]);
    }
    else
    {
        ui_ctrl();
    }

ret:
#if defined(TRACE) && 1
    fprintf(debug, "<-build_ui: %d\n", return_value);
#endif
    return return_value;
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
    int key[2] = { 0, 0 }, key2[2], c = 0, c2 = 0;
    unsigned num = 0;
    unsigned num2 = 0;
    long u = 0;
	struct filediff *f;
    bool ns = FALSE; /* num set */
    bool us = FALSE; /* u set */

	while (1) {
/* {continue} may be dangerous when a {for} loop is put around the statement
 * later. */
next_key:
		clr_fs_err();
		key[1] = *key;
		*key = c;

		if (!c) {
			num = 1;
			ns = FALSE;
			us = FALSE;
		}

		opt_flushinp();

		while ((c = getch()) == ERR) {
		}

#if defined(TRACE)
		if (isascii(c) && !iscntrl(c)) {
			fprintf(debug, "<>getch: '%c'\n", c);
		} else {
			fprintf(debug, "<>getch: 0x%x\n", c);
		}
#endif

		if (c == '') {
			printerr(NULL,
"Invalid escape sequence. Type <ENTER> to continue.");

			while (getch() != '\n') {
			}

			printerr(NULL, NULL);
			goto next_key;

		} else if (c == '.' && c2 && !*key) {
			if (!c2) {
				goto next_key;
			}

			c = c2;
			key[0] = key2[0];
			key[1] = key2[1];
			num = num2;
		}

		if (!us) {
			u = top_idx[right_col] + curs[right_col];
		}

		if (test_fkey(c, num, u)) {
			c = 0;

			if (nofkeys) { /* read-only mode active */
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
			proc_mevent(&c);
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
#if defined(TRACE)
            fprintf(debug, "  KEY_LEFT\n");
#endif
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
            {
                enter_dir("..", NULL, FALSE, FALSE, 0 LOCVAR);
            }
			else
            {
				pop_state(1);
            }
			break;

		case '\n':
#if defined(TRACE)
            fprintf(debug, "  [ENTER KEY]\n");
#endif
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
#if defined(TRACE)
            fprintf(debug, "  KEY_RIGHT\n");
#endif
            if (*key == '|') {
				c = *key;

				if (midoffs > COLS / 2)
					break;

				midoffs += 10;
				resize_fmode();
				break;
			}

			if (!db_num[right_col]) {
				c = 0;
				no_file();
				break;
			}

			action(3, c == '\n' ? 1 : 0);
			/* Don't clear {c} above this line, it is read there */
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
			c = 0;
			scroll_down(1, FALSE, -1);
			break;

		case CTRL('y'):
			c = 0;
			scroll_up(1, FALSE, -1);
			break;

		case CTRL('d'):
			c = 0;
			scroll_down(listh/2, TRUE, -1);
			break;

		case CTRL('u'):
			c = 0;
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
					c = 0;
					goto next_key;
				}

				goto save_st;

			} else if (key[1] == 'e') {
				if (*key == 'l') {
					if (ui_chmod(1, u, num)) {
						c = 0;
						goto next_key;
					}

					goto save_st;
				} else if (*key == 'r') {
					if (ui_chmod(2, u, num)) {
						c = 0;
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
            if (ns && num && num <= FKEY_MUX_NUM) {
				c = 0;
				fkey_set = num - 1;
                printerr(NULL, "Funktion key set %d selected", num);
				break;
			}

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

			switch (*key) {
			case 'A':
				add_hsize = TRUE;
				add_mode = TRUE;
				add_mtime = TRUE;
				add_owner = TRUE;
				add_group = TRUE;
				re_sort_list();
				disp_fmode();
				goto next_key;

			case 'D':
				dl_add();
				goto next_key;

			case 'R':
				add_bsize = FALSE;
				add_hsize = FALSE;
				add_mode = FALSE;
				add_mtime = FALSE;
				add_owner = FALSE;
				add_group = FALSE;
				disp_fmode();
				goto next_key;
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
                c = 0;
                regex_srch(1);
                break;

            } else if (*key == 'e') {
                ui_rename(3, u, num);
                goto save_st;
            } else if (*key == 'A') {
                c = 0;
                add_mtime = FALSE;
                add_ns_mtim = TRUE;
                disp_fmode();
                goto next_key;
            } else if (*key == 'R') {
                c = 0;
                add_ns_mtim = FALSE;
                disp_fmode();
                goto next_key;
            } else if (key[1] == 'e') {
                if (*key == 'l') {
                    ui_rename(1, u, num);
                    goto save_st;

                } else if (*key == 'r') {
                    ui_rename(2, u, num);
                    goto save_st;
                }
            }

            /* fall through */

		case '!':
		case 'c':
		case '&':
		case '^':
			if (bmode || fmode) {
				/* Don't clear {c} above this line! */
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
				nosingle = nosingle ? 0 : 3;
				break;

			case '^':
				excl_or = excl_or ? FALSE : TRUE;
				break;
			}

			/* Don't clear {c} above this line! */
			if (c != '&') { /* wait for 'l' or 'r' */
				c = 0;
			}

			re_sort_list();
			break;

		case 'E':
        case ',':
            switch (c) {
            case 'E':
                file_pattern = file_pattern ? FALSE : TRUE;
                break;
            case ',':
                nohidden = nohidden ? FALSE : TRUE;
                break;
            }
            c = 0; /* c is used in the switch statement above! */
            re_sort_list();
			break;

		case KEY_RESIZE:
			c = 0;
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
				c = 0;
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
                add_ns_mtim = FALSE;
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
				action(1, 2);
				break;
			}

			if (S_ISREG(f->type[1]) && !S_ISREG(f->type[0])) {
				c = 0;
				action(2, 2);
				break;
			}

			break;

		case 'l':
			switch (*key) {
			case 'd':
				if (ui_dd(1, u, num)) {
					c = 0;
					goto next_key;
				}

				goto save_st;

			case 's': /* "sl" */
				c = 0;
				open_sh(1);
				goto next_key;

			case 'v':
				c = 0;
				action(1, 2);
				goto next_key;

			case 'o':
				c = 0;
				action(1, 4);
				goto next_key;

			case 'e':
				goto next_key;

			case 'P':
				c = 0;
				fs_mkdir(1);
				goto save_st;

			case 'T': /* "Tl" */
				if (ui_mv(1, u, num)) {
					c = 0;
					goto next_key;
				}

				goto save_st;

			case '@': /* "@l" */
				if (ui_cp(1, u, num, 2)) {
					c = 0;
					goto next_key;
				}

				goto save_st;

			case 'D':
				c = 0;

				if (dl_list()) {
					return;
				}

				goto next_key;

			case '\'':
				c = 0;
				list_jmrks();
				goto next_key;

			case '&': /* "&l" */
				c = 0;
				nosingle = 1;
				re_sort_list();
				goto next_key;

            case 'S': /* "Sl" sort symlink */
                c = 0;

                if (sorting == SORT_SYMLINK) {
                    goto next_key;
                }

                sorting = SORT_SYMLINK;
                rebuild_db(1);
                goto next_key;
			}

			c = 0;
            disp_fkey_list();
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
					c = 0;
					goto next_key;
				}

				goto save_st;

			case 's': /* "sr" */
				c = 0;
				open_sh(2);
				goto next_key;

			case 'v':
				c = 0;
				action(2, 2);
				goto next_key;

			case 'o':
				c = 0;
				action(2, 4);
				goto next_key;

			case 'e':
				goto next_key;

			case 'P':
				c = 0;
				fs_mkdir(2);
				goto save_st;

			case 'T': /* "Tr" */
				if (ui_mv(2, u, num)) {
					c = 0;
					goto next_key;
				}

				goto save_st;

			case '@': /* "@r" */
				if (ui_cp(2, u, num, 2)) {
					c = 0;
					goto next_key;
				}

				goto save_st;

			case '&': /* "&r" */
				c = 0;
				nosingle = 2;
				re_sort_list();
				goto next_key;
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
			if (*key != '<') {
				break;
			}

			c = 0;

			if (bmode || (fmode && !right_col)) {
				break;
			}

			if (ui_cp(1, u, num, 0)) {
				goto next_key;
			}

			goto save_st;

		case '>':
			if (*key != '>') {
				break;
			}

			c = 0;

			if (bmode || (fmode && right_col)) {
				break;
			}

			if (ui_cp(2, u, num, 0)) {
				goto next_key;
			}

			goto save_st;

		case 'T':
			if (!bmode && !fmode && !fs_any_dst(u, num, 0)) {
				/* wait for key 'l' or 'r' */
				break;
			}

			if (ui_mv(0, u, num)) {
				c = 0;
				goto next_key;
			}

			goto save_st;

		case '@':
		case 'C':
		case 'U':
		case 'X':
		{
			unsigned m = 0;

			switch (c) {
			case '@':
				if (!bmode && !fmode && !fs_any_dst(u, num, 0))
				{
					/* wait for key 'l' or 'r' */
					goto next_key;
				}

				m |= 2;
				break;

			case 'U':
				m |= 8;
				break;

			case 'X':
				if (bmode || fmode || !fs_any_dst(u, num, 2))
				{
					c = 0;
					goto next_key;
				}

				m |= 32;
				break;
			}

			if (ui_cp(0, u, num, m)) {
				c = 0;
				goto next_key;
			}

			goto save_st;
		}

		case 'J':
			c = 0;
			fs_cat(u);
			break;

		case KEY_HOME:
			c = 0;
			curs_first();
			break;

		case 'G':
			if (ns) {
				c = 0;
				center(num ? num - 1 : 0);
				break;
			} else if (*key == 'V' || *key == KEY_IC) {
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
			    /* Must be NULL (instead of "")
			     * to not clear buffer.
			     * Else 'y' would not work. */
			    NULL,
			    NULL, 0,
			    &sh_cmd_hist) && *rbuf) {
                const char *s = rbuf;

				exec_cmd(&s, TOOL_WAIT | TOOL_NOLIST |
				    TOOL_TTY | TOOL_SHELL, NULL, NULL);
				/* exec_cmd() did likely create or
				 * delete files */
				rebuild_db(0);
			}

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
					c = 0;
					goto next_key;
				}

				goto save_st;

			} else if (key[1] == 'e') {
				if (*key == 'l') {
					if (ui_chown(1, 0, u, num)) {
						c = 0;
						goto next_key;
					}

					goto save_st;

				} else if (*key == 'r') {
					if (ui_chown(2, 0, u, num)) {
						c = 0;
						goto next_key;
					}

					goto save_st;
				}
            } else if (*key == 'S') {
                c = 0;
                if (sorting == SORT_OWNER)
                    break;
                sorting = SORT_OWNER;
                rebuild_db(1);
                break;
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
					c = 0;
					goto next_key;
				}

				goto save_st;

			} else if (key[1] == 'e') {
				if (*key == 'l') {
					if (ui_chown(1, 1, u, num)) {
						c = 0;
						goto next_key;
					}

					goto save_st;

				} else if (*key == 'r') {
					if (ui_chown(2, 1, u, num)) {
						c = 0;
						goto next_key;
					}

					goto save_st;
				}
            } else if (*key == 'S') {
                c = 0;
                if (sorting == SORT_GROUP)
                    break;
                sorting = SORT_GROUP;
                rebuild_db(1);
                break;
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
				action(1, 4);
				break;
			}

			if (S_ISREG(f->type[1]) && !S_ISREG(f->type[0])) {
				c = 0;
				action(2, 4);
				break;
			}

			break;

		case ':':
			c = 0;

			if (!ed_dialog("Enter command:",
			    /* "" instead of NULL to start
			     * with an empty buffer.
			     * 'y' need not be supported for ':'. */
			    "",
			    complet, 0,
			    &opt_hist)) {
				if (parsopt(rbuf) == 1)
					return;
			}

			break;

		case CTRL('l'):
#if defined(TRACE)
            fprintf(debug, "  CTRL('l')\n");
#endif
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

			/* Don't clear {c}, else {u} is overwritten */

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
				us = TRUE;
			}

			break;

		case '\t':
			c = 0;

			if (!fmode)
				break;

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
					push_scan_db(TRUE);
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

			/* Don't clear {c} above this line! */
			c = 0;
		}
	}
    /* while (1) loop -> not reached */
	return;

save_st:
	c2 = c;
	c = 0;
	key2[0] = key[0];
	key2[1] = key[1];
	num2 = num;
	goto next_key;
}

static const char *const helptxt[] = {
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
       "Sl		Sort files by symbolic link target"
       "Sm		Sort files by name only",
       "SS		Sort files by size only",
       "St		Sort files by modification time only",
       "Su		Sort files by owner name",
       "Sg		Sort files by group name",
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
       "&l		Hide files which are on left side only",
       "&r		Hide files which are on right side only",
       "^		Toggle display of files which are in both trees",
       "F		Toggle following symbolic links",
       "E		Toggle file name or file content filter",
       ",		Toggle display of hidden files",
       "Aa		Show mode, owner, group, size, and mtime",
       "Ah		Show scaled file size",
       "Ag		Show file group",
       "An		Show nanosecond precision mtime",
       "Ap		Show file mode",
       "As		Show file size",
       "At		Show modification time",
       "Au		Show file owner",
       "Ra		Remove mode, owner, group, size, and mtime column",
       "Rh		Remove scaled file size column",
       "Rg		Remove file group column",
       "Rn		Remove nanosecond precision mtime column",
       "Rp		Remove file mode column",
       "Rs		Remove file size column",
       "Rt		Remove modification time column",
       "Ru		Remove file owner column",
       "p		Show current relative work directory",
       "a		Show command line directory arguments",
       "f		Show full path",
       "<n>f		Switch to f-key set <n>",
       "[<n>]<<		Copy from second to first tree",
       "[<n>]>>		Copy from first to second tree",
       "'<<		Copy from second to first tree (range cursor...mark)",
       "'>>		Copy from first to second tree (range cursor...mark)",
       "[<n>]C		Copy to other side",
       "'C		Copy to other side (range cursor...mark)",
       "[<n>]U		Update file(s)",
       "'U		Update file(s) (range cursor...mark)",
       "[<n>]X		Exchange file(s)",
       "'X		Exchange file(s) (range cursor...mark)",
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
       "J		Append to marked file",
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
       "[<n>]<F1> - <F48>",
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
       ":!<shell command>",
       "		Enter shell command",
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
       ":set file_exec",
       "		bmode, fmode: Enable file execution",
       ":set nofile_exec",
       "		bmode, fmode: Disable file execution",
       ":set fkeys	Enable function keys",
       ":set nofkeys	Disable function keys",
       ":set ic		Case-insensitive match",
       ":set noic	Case-sensitive match",
       ":set loop	Set loop mode",
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
		opt_flushinp();

		switch (c = getch()) {
#if NCURSES_MOUSE_VERSION >= 2
		case KEY_MOUSE:
			help_mevent();
			break;
#endif
		case ':':
			keep_ungetch(c);
			/* fall-through */

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
proc_mevent(int *c)
{
	static bool mb;

	if (getmouse(&mevent) != OK)
		return;

	if (mb) {
		movemb(mevent.x);

		if (mevent.bstate & BUTTON1_RELEASED) {
			*c = 0;
			mb = FALSE;
			doresizecols();
		}

		return;
	}

	if (twocols && (mevent.bstate & BUTTON1_PRESSED) && mevent.x == llstw) {
		*c = 0;
		mb = TRUE;
		mousemask(REPORT_MOUSE_POSITION | BUTTON1_RELEASED, NULL);
		return;
	}

	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED ||
	    mevent.bstate & BUTTON1_PRESSED ||
	    mevent.bstate & BUTTON3_CLICKED ||
	    mevent.bstate & BUTTON3_PRESSED) {

		*c = 0;

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

			/* Not key_mmrk() since mouse button must not move
			 * cursor down! */
			tgl_mmrk(DB_LST_ITM);
			*c = 'V'; /* Fake key */

		} else if (mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
			action(3, 0);
		}

# if NCURSES_MOUSE_VERSION >= 2
	} else if (mevent.bstate & BUTTON4_PRESSED) {
		*c = 0;

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
		*c = 0;

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
    short tree,
/*
    0: <RIGHT> or double click: Enter directory
    1: <ENTER>: Do a compare
       ! IF !mode&1 MARKS ARE IGNORED !
    2: Used by 'v', "vl" and "vr":
       Unzip file but then view raw file using standard view tool.
    4: Used by 'o', "ol" and "or"
    8: Used by function key starting with "$ ".
       Ignore file type and file name extension, just use plain file name.
       (Don't enter directories!)
*/
    unsigned mode)
{
	struct filediff *f, *f1, *f2, *z1 = NULL, *z2 = NULL;
	char *t1 = NULL, *t2 = NULL;
	const char *err = NULL;
	static const char *const typerr = "Not a directory or regular file";
	static const char *const typdif = "Different file type";
	mode_t typ[2];
    const bool diff_act_ = !bmode && !fmode && tree == 3 && (mode & 1);
    const bool exec_act_ = file_exec && !(mode & 8) && !(mode & 2) &&
	    (bmode || fmode) && (mode & 1);
    const bool force_tool_ =
	    /* Always in 'o' (open) mode */
	    (mode & 4) ||
	    /* In browse mode also ENTER is sufficient */
	    ((bmode || fmode) && (mode & 1));

#if defined(TRACE)
	fprintf(debug, "->action(ignext=%d t=%d act=%u raw=%u) mark=%d\n",
	    (mode & 8), tree, (mode & 1), (mode & 2), mark ? 1 : 0);
#endif
	if (!db_num[right_col])
		goto out;

	f = f1 = f2 = db_list[right_col][top_idx[right_col] + curs[right_col]];

#if defined(TRACE)
	fprintf(debug, "  f1->name(%s)\n", f1->name);
#endif
	if (mark && (mode & 1)) {
		struct filediff *m;
		mode_t ltyp = 0, rtyp = 0;
        const char *lnam, *rnam, *mnam;

		m = mark;

		mnam = m->name ? m->name :
		       bmode ? gl_mark :
		       m->type[0] ? mark_lnam :
		       m->type[1] ? mark_rnam :
		       "<error>" ;

		if (!(mode & 8)) {
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
		if ((mode & 8) || (S_ISREG(ltyp) && S_ISREG(rtyp))) {
			tool(lnam, rnam, tree, (mode & 8) || (mode & 2) ? 1 : 0);

		} else if (S_ISDIR(ltyp) || S_ISDIR(rtyp)) {
			if (!S_ISDIR(ltyp)) {
				if (z2) {
					set_path_display_name_offset(1, f->name, z2->name);
				}

				enter_dir(NULL , rnam,
				          FALSE, z2 ? TRUE : FALSE,
				          tree LOCVAR);

			} else if (!S_ISDIR(rtyp)) {
				if (z1) {
					set_path_display_name_offset(0, mnam, z1->name);
				}

				enter_dir(lnam             , NULL,
				          z1 ? TRUE : FALSE, FALSE,
				          tree LOCVAR);
			} else {
				push_scan_db(TRUE);
                if (z1)
                {
					set_path_display_name_offset(0, mnam, z1->name);
				}
                if (z2)
                {
					/* 2: don't set vpath[0] */
					set_path_display_name_offset(2, f->name, z2->name);
				}
                /* Used in diff mode when both directories are present */
				enter_dir(lnam             , rnam,
				          z1 ? TRUE : FALSE, z2 ? TRUE : FALSE,
					  tree LOCVAR);
			}
		}

		goto ret;
	}

	if (!(mode & 8)) {
		if (f1->type[0] && (z1 = unpack(f1, 1, &t1,
		    force_tool_ ? 9 : 1)))
			f1 = z1;

		if (f2->type[1] && (z2 = unpack(f2, 2, &t2,
		    force_tool_ ? 9 : 1)))
			f2 = z2;
	}

	typ[0] = f1->type[0];
	typ[1] = f2->type[1];

	if (!typ[0] ||
	    (diff_act_ && S_ISDIR(typ[1]) && !S_ISDIR(typ[0])) ||
	    /* Tested here to support "or" */
	    tree == 2) {
		if (S_ISREG(typ[1]) || (mode & 8))
			tool(f2->name, NULL, 2,
			    exec_act_ && S_ISREG(typ[1]) &&
			      (typ[1] & S_IXUSR) ? 2 :
			    (mode & 8) || (mode & 2) ? 1 : 0);
		else if (S_ISDIR(typ[1])) {
			/* Used by fmode */

			if (z2) {
				set_path_display_name_offset(1, f->name, z2->name);
			}

            /* Used in diff mode when only right dir is present */
            enter_dir(diff_act_ ? f2->name : NULL, f2->name,
			          FALSE, z2 ? TRUE : FALSE, 0 LOCVAR);
		} else {
			err = typerr;
			goto ret;
		}
	} else if (!typ[1] ||
	    (diff_act_ && S_ISDIR(typ[0]) && !S_ISDIR(typ[1])) ||
	    tree == 1) {
		if (S_ISREG(typ[0]) || (mode & 8))
			tool(f1->name, NULL, 1,
			    exec_act_ && S_ISREG(typ[0]) &&
			      (typ[0] & S_IXUSR) ? 2 :
			    (mode & 8) || (mode & 2) ? 1 : 0);
		else if (S_ISDIR(typ[0])) {
			/* Used by bmode and fmode */

			if (z1) {
				set_path_display_name_offset(bmode ? 1 : 0, f->name, z1->name);
			}

            /* Used in diff mode when only left dir is present */
            enter_dir(f1->name, diff_act_ ? f1->name : NULL,
			          z1 ? TRUE : FALSE, FALSE, 0 LOCVAR);
		} else {
			err = typerr;
			goto ret;
		}
	} else if ((typ[0] & S_IFMT) == (typ[1] & S_IFMT)) {
		if ((mode & 8) || (mode & 2))
			tool(f1->name, f2->name, 3, 1);
		else if (S_ISREG(typ[0])) {
			if (f1->diff == '!')
				tool(f1->name, f2->name, 3, 0);
			else
				tool(f1->name, NULL, 1, 0);
        }
        else if (S_ISDIR(typ[0]))
        {
            if (z1 || z2)
            {
				push_scan_db(TRUE);
			}
            if (z1)
            {
				set_path_display_name_offset(0, f->name, z1->name);
			}
            if (z2)
            {
				set_path_display_name_offset(1, f->name, z2->name);
			}
			enter_dir(f1->name         , f2->name,
			          z1 ? TRUE : FALSE, z2 ? TRUE : FALSE, 0
				  LOCVAR);
        }
        else
        {
			err = typerr;
			goto ret;
		}
    }
    else
    {
		err = typdif;
    }
ret:
	if (z1)
    {
		free_zdir(z1, t1);
    }
	if (z2)
    {
		free_zdir(z2, t2);
    }
	if (err)
    {
		printerr(NULL, err);
    }
out:
#if defined(TRACE)
	fprintf(debug, "<-action\n");
#endif
	return;
}

static void
page_down(void)
{
#if defined(TRACE) && 0
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
#if defined(TRACE) && 0
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
#if defined(TRACE) && 0
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
#if defined(TRACE) && 0
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
#if defined(TRACE) && 0
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
#if defined(TRACE) && 0
	fprintf(debug, "<-last_line_is_disp\n");
#endif
	return r;
}

static int
first_line_is_top(void)
{
	int r = 0;
#if defined(TRACE) && 0
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
#if defined(TRACE) && 0
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
        disp_marked_line(y, i, 0, w);

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
            disp_marked_line(y, i, 0, w);
		}

	if (move_curs)
		disp_curs(1);

	refr_scr();

exit:
	right_col = ocol;
}

void disp_curs(int a)
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

void disp_list(unsigned md)
{
	unsigned y, i;
	WINDOW *w;

#if defined(TRACE)
	fprintf(debug, "->disp_list(%u) col=%d\n", md, right_col);
#endif
	w = getlstwin();

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
        y++, i++)
    {
        disp_marked_line(y, i, md, w);
	}

exit:
	refr_scr();
#if defined(TRACE)
	fprintf(debug, "<-disp_list\n");
#endif
	return;
}

static void disp_marked_line(const unsigned y, const unsigned i, const unsigned md, WINDOW *const w)
{
    bool cg = CHGAT_MRKS;
    if (i >= db_num[right_col])
    {
        standoutc(w);
        mvwaddch(w, y, llstw, ' ');
        standendc(w);
    }
    else if (md && y == curs[right_col])
    {
        disp_curs(1);
    }
    else if ((long)(top_idx[right_col] + y) == mark_idx[right_col])
    {
        if (!cg)
        {
            markc(w);
        }
        disp_line(y, i, 1);
        if (cg)
        {
            chgat_mark(w, y);
        }
    }
    else if ((db_list[right_col][top_idx[right_col] + y])->fl & FDFL_MMRK)
    {
        if (!cg)
        {
            mmrkc(w);
        }
        disp_line(y, i, 1);
        if (cg)
        {
            chgat_mmrk(w, y);
        }
    }
    else
    {
        disp_line(y, i, 0);
    }
}

static void disp_line(unsigned y, unsigned i, int info)
{
	int diff = 'E'; /* Internal error */
	int type[2] = { 'E', 'E' };
    mode_t mode[2] = { ~0U, ~0U }; /* Detect error */
	struct filediff *f;
	short color_id = 0;
	WINDOW *w;
    attr_t a = 0;
	int mx;
    short cp = 0;

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
#if defined(TRACE)
	fprintf(debug, "  col %d %c 0%o \"%s\"\n", right_col ? 1 : 0,
	    *type, twocols && !fmode ? f->type[0] : *mode, f->name);
#endif
	disp_name(w, y, 0, twocols && !fmode ? llstw : mx, info, f, *type,
	    color_id,
        right_col ? f->link[1] :
        twocols || f->link[0] ? f->link[0] : f->link[1],
	    diff, fmode ? right_col : twocols || f->type[0] ? 0 : 1);

    if (twocols && !fmode) {
prtc2:
        if (diff != '<') {
            (wattr_set)(w, a, cp, NULL);
            set_file_info(f, f->type[1], type+1, &color_id, &diff);
#if defined(TRACE)
            fprintf(debug, "  col R %c 0%o \"%s\"\n",
                    type[1], f->type[1], f->name);
#endif
            disp_name(w, y, rlstx, mx, info, f, type[1], color_id,
                    f->link[1], diff, 1);
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

        if (f->link[0]) {
			addmbs(wstat, " -> ", mx);
            addmbs(wstat, f->link[0], mx);
		}

		mvwaddstr(wstat, tc ? 0 : 1, tc ? rlstx : 2,
		    type_name(f->type[1]));

        if (f->link[1]) {
			addmbs(wstat, " -> ", 0);
            addmbs(wstat, f->link[1], 0);
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
		if (m & S_IXUSR)
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

static int disp_name(WINDOW *w, int y, int x, int mx, int o,
                     struct filediff *f, int t, short ct, char *l, int d,
                     int i)
{
	int j;
	int db;
    static const int ns_time_width = 28; /* "18-09-17 17:42:54.000000000"
                                          * ("%'09ld" does not work!?) */

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

    if (add_mtime)
		mx -= 13;
    else if (add_ns_mtim) {
        mx -= ns_time_width;
    }

	if (!o) {
		if (!color) {
			if (d != ' ') {
				wattron(w, A_BOLD);
			}
		} else {
			if (ct) {
				wattron(w, COLOR_PAIR(ct));

				if (!nobold) {
					wattron(w, A_BOLD);
				}
			} else {
				/* not attrset, else bold is off */
				wattron(w, COLOR_PAIR(PAIR_NORMAL));
			}
		}
	}

	if (twocols || bmode) {
		mvwprintw(w, y, x, "%c ", t);
	} else {
		mvwprintw(w, y, x, "%c %c ", d, t);
	}

	j = addmbs(w, f->name, mx);

    if (j == -1) {
        /* Likely mbstowcs(3) did fail */
        waddstr(w, f->name);
    }

	standendc(w);

    if (!j && l) {
		addmbs(w, " -> ", mx);
        putmbsra(w, l, mx);
	}

	if (add_mode) {
		mx += 5;
		snprintf(lbuf, sizeof lbuf, "%04o", (int)f->type[i] & 07777);
		wmove(w, y, mx - 4);
		addmbs(w, lbuf, 0);
	}

	if (add_owner) {
        get_uid_name(f->uid[i], lbuf, sizeof lbuf);
		wmove(w, y, mx + 1);
		addmbs(w, lbuf, 0);
		mx += usrlen[db];
	}

	if (add_group) {
        get_gid_name(f->gid[i], lbuf, sizeof lbuf);
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

            n = snprintf(lbuf, sizeof lbuf, "%3lu, %3lu",
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
        n = gettimestr(lbuf, sizeof lbuf, &f->mtim[i].tv_sec);
		wmove(w, y, mx - n);
		addmbs(w, lbuf, 0);
    } else if (add_ns_mtim) {
        struct tm tm;
        if (localtime_r(&f->mtim[i].tv_sec, &tm)) {
            const size_t n = snprintf(lbuf, sizeof(lbuf),
                         "%02d-%02d-%02d %02d:%02d:%02d.%09ld",
                         tm.tm_year % 100, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min, tm.tm_sec,
                         f->mtim[i].tv_nsec);
            mx += ns_time_width;
            wmove(w, y, mx - n);
            addmbs(w, lbuf, 0);
        }
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

static const char *
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
#   if defined(TRACE) && 1
    fprintf(debug, "->file_stat()\n");
#   endif
    int x, x2, w, w1, w2, yl, yr, lx1, lx2, mx1;
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
			set_path_display_name(2);
			putmbsra(wstat, path_display_name[1], 0);
		}
	} else if (dir_change) {
		set_path_display_name(2);
		wmove(wstat, 0, mark ? 0 : 2);
		putmbsra(wstat, path_display_name[0], mark ? mx1 : 0);
		wmove(wstat, mark ? 0 : 1, mark ? rlstx : 2);
		putmbsra(wstat, path_display_name[1], 0);
        goto ret;
	}

	if (S_ISLNK(ltyp)) {
		wmove(wstat, yl, x);
        addmbs(wstat, "-> ", mx1);
        putmbsra(wstat, f->link[0], mx1);
		ltyp = 0;
	}

	if (S_ISLNK(rtyp)) {
		wmove(wstat, yr, x2);
		addmbs(wstat, "-> ", 0);
        putmbsra(wstat, f2->link[1], 0);
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

    if (ltyp)
        get_uid_name(f->uid[0], lbuf, sizeof lbuf);
    if (rtyp)
        get_uid_name(f2->uid[1], rbuf, sizeof rbuf);
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

    if (ltyp)
        get_gid_name(f->gid[0], lbuf, sizeof lbuf);
    if (rtyp)
        get_gid_name(f2->gid[1], rbuf, sizeof rbuf);
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
        lx1 = x + gettimestr(lbuf, sizeof lbuf, &f->mtim[0].tv_sec);
		wmove(wstat, yl, x);
		addmbs(wstat, lbuf, mx1);
	}

	if (rtyp) {
        lx2 = x2 + gettimestr(rbuf, sizeof rbuf, &f2->mtim[1].tv_sec);
		wmove(wstat, yr, x2);
		addmbs(wstat, rbuf, 0);
	}

    if (ltyp && f->link[0]) {
		wmove(wstat, yl, lx1);
		addmbs(wstat, " -> ", mx1);
        putmbsra(wstat, f->link[0], mx1);
	}

    if (rtyp && f2->link[1]) {
		wmove(wstat, yr, lx2);
		addmbs(wstat, " -> ", 0);
        putmbsra(wstat, f2->link[1], 0);
	}
    ret: ;
#   if defined(TRACE) && 1
    fprintf(debug, "<-file_stat()\n");
#   endif
}

size_t getfilesize(char *buf, size_t bufsiz, off_t size,
    /* 1: scale */
    /* 2: don't group */
    unsigned md)
{
	const char *unit;
	float f;

	if (!md) {
		return snprintf(buf, bufsiz, "%'lld", (long long)size);

	} else if ((md & 2) || size < 1024) {
		return snprintf(buf, bufsiz, "%lld", (long long)size);

	} else {
		f = size / 1024.0;
		unit = "K";

		if (f > 999.9) {
			f /= 1024.0;
			unit = "M";
		}

		if (f > 999.9) {
			f /= 1024.0;
			unit = "G";
		}

		if (f > 999.9) {
			f /= 1024.0;
			unit = "T";
		}
	}

    if (f <= 9.9) {
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
	struct filediff *f;
#if defined(TRACE)
	fprintf(debug, "->set_mark\n");
#endif
	if (!db_num[right_col]) {
		goto ret;
	}

	f = db_list[right_col][top_idx[right_col] + curs[right_col]];

	if (str_eq_dotdot(f->name)) {
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
    if (nohidden    ) n++;

	if (n) {
		printerr(NULL,
		    "No file in list (key%s %s%s%s%s%sdisable%s filter%s).",
		    n > 1 ? "s" : "",
		    file_pattern ? "'E' " : "",
            nohidden     ? "',' " : "",
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
push_state(const char *name, const char *rnam,
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
    /* 2: Cleanup from dmode->fmode */
    /* 1: Normal mode
     * 0: Cleanup mode */
    short mode)
{
	struct ui_state *st = ui_stack;
	bool d2f;

#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "->pop_state(m=%d) lp(%s) rp(%s) fp(%s)"
	    " from_fmode=%d st=%p\n",
	    mode, trcpth[0], trcpth[1], fpath, from_fmode ? 1 : 0,
	    st);
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
#if defined(TRACE)
	fprintf(debug, "  d2f=%d\n", d2f ? 1 : 0);
#endif

	if (st->lzip || st->rzip) {
		/* In diff mode a new scan DB is only required when
		 * a compressed archive had been entered */
		pop_scan_db();
	}

	if (st->lzip) {
		if (mode == 1 && (
		      /* A global mark inside the archive */
		      (gl_mark && mark_lnam &&
		       !strncmp(syspth[0], mark_lnam, pthlen[0])) ||
		      /* A local mark -> can only be in archive
		       * since we are just leaving a archive */
		      (mark && !gl_mark)
		    ))
			clr_mark();

		st->lzip[strlen(st->lzip) - 2] = 0;
        rmtmpdirs(st->lzip);
		reset_path_offsets(0);
	}

	if (st->rzip) {
		if (mode == 1 && (
		      (gl_mark && mark_rnam &&
		       !strncmp(syspth[1], mark_rnam, pthlen[1])) ||
		      (mark && !gl_mark)
		    ))
			clr_mark();

		st->rzip[strlen(st->rzip) - 2] = 0;
        rmtmpdirs(st->rzip);
		reset_path_offsets(1);
	}

	if (mark) {
		if (mode == 1) {
			if (!gl_mark)
				mark_global();
		} else
			clr_mark();
	}

	pthlen[0] = st->llen;
	pthlen[1] = st->rlen;

	if (st->lpth) {
		if (mode == 1)
			memcpy(syspth[0], st->lpth, pthlen[0]);

		syspth[0][pthlen[0]] = 0;
		free(st->lpth);
	}

	if (st->rpth) {
		if (mode == 1)
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
		/* fpath would have been freed in restore_fmode().
		 * Since the following `goto` skips this function call,
		 * the path is freed here. */
		if (fpath) {
			free(fpath);
			fpath = NULL;
		}

		goto ret;
	}

	if (d2f) {
		if (from_fmode) {
			restore_fmode();
		} else {
			tgl2c(1);
		}
	} else if (mode == 1) {
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
enter_dir(const char *name, const char *rnam, bool lzip, bool rzip, short tree
#ifdef DEBUG
    , const char *const _file, const unsigned _line
#endif
    )
{
    int i = 0;
    unsigned *uv = NULL;
    char *current_path = NULL; /* current path */
    char *saved_path = NULL; /* saved path */
    struct bpth *bpth = NULL;
    size_t *lp = NULL;
#ifdef HAVE_LIBAVLBST
    struct bst_node *n = NULL;
#else
    struct ptr_db_ent *n = NULL;
#endif
	bool f2d = FALSE;

#ifdef TRACE
	TRCPTH;
	fprintf(debug, "->" LOCFMT
        "enter_dir(left_name(%s) right_name(%s) left_zip=%d right_zip=%d tree=%d) left_sys_path(%s) right_sys_path(%s)\n",
# ifdef DEBUG
	    _file, _line,
# endif
	    name, rnam, lzip, rzip, tree, trcpth[0], trcpth[1]);
#endif
	if (   (str_eq_dotdot(name) && rnam) /* enter_dir("..", NULL, ...) is used to change to the parent directory */
        || (str_eq_dotdot(rnam) && name)) /* Same goes for enter_dir(NULL, "..", ...) */
    {
		goto ret;
	}
    if (!twocols)
    {
		dir_change = TRUE;
	}
    if (name && rnam)
    {
        if (bmode || fmode)
        {
			clr_mark();
			name = strdup(name);
			rnam = strdup(rnam);
			f2d = TRUE;
		}
        if (bmode)
        {
			tgl2c(1);
        }
        else if (fmode)
        {
            if (tree == 1)
            {
				fpath = strdup(syspth[1]);
				fpath[pthlen[1]] = 0;
#if defined(TRACE)
				fprintf(debug,
				    "  enter_dir(): set fpath[1] = %s\n",
				    fpath);
#endif
				memcpy(syspth[1], syspth[0], pthlen[0]+1);
				pthlen[1] = pthlen[0];
			} else if (tree == 2) {
				fpath = strdup(syspth[0]);
				fpath[pthlen[0]] = 0;
#if defined(TRACE)
				fprintf(debug,
				    "  enter_dir(): set fpath[0] = %s\n",
				    fpath);
#endif
				memcpy(syspth[0], syspth[1], pthlen[1]+1);
				pthlen[0] = pthlen[1];
			}

			fmode_dmode();
		}
	}
	if (mark && !gl_mark && (!fmode ||
	    (!right_col && mark->type[0]) ||
        ( right_col && mark->type[1])))
    {
		mark_global();
	}
    if (!bmode && !fmode)
    {
		push_state(name, rnam,
		    (f2d ? 4 : 0) | (rzip ? 2 : 0) | (lzip ? 1 : 0));
		subtree = (name ? 1 : 0) | (rnam ? 2 : 0);
		scan_subdir(name, rnam, subtree);
        if (f2d)
        {
            free(const_cast_ptr(name));
            free(const_cast_ptr(rnam));
		}
		goto disp;
	}
    if (!name)
    {
		name = rnam;
		lzip = rzip;
	}
    if (bmode)
    {
        current_path = syspth[1];
    }
    else
    {
		syspth[right_col][pthlen[right_col]] = 0;
        current_path = syspth[right_col];
		lp = &pthlen[right_col];
        saved_path = strdup(current_path);
	}
    db_set_curs(right_col, current_path, top_idx[right_col], curs[right_col]);
	n = NULL; /* flag */
	/* Not in bmode since syspth[0] is always "." there */
    if (!lzip && !bmode && name && *name == '/')
    {
		*lp = 0;
        *current_path = 0;
	}
    if (lzip)
    {
        if (bmode)
        {
			if (!getcwd(syspth[1], sizeof syspth[1]))
            {
                printerr(strerror(errno), "getcwd failed");
            }
            current_path = syspth[1];
		}
		bpth = malloc(sizeof(struct bpth));
        bpth->pth = strdup(current_path);
        if (bmode)
        {
            bpth->col = -1;
        }
        else
        {
			bpth->col = right_col;
			*lp = 0;
            *current_path = 0;
		}
#       if defined(TRACE)
        fprintf(debug, "  ptr_db_add(uz_path_db key(%s) dat(%s))\n", name, current_path);
#       endif
        ptr_db_add(&uz_path_db, strdup(name), bpth);
    }
    else if (str_eq_dotdot(name) && !ptr_db_srch(&uz_path_db, current_path, (void **)&bpth, (void **)&n))
    {
		name = bpth->pth; /* dat */
#       ifdef HAVE_LIBAVLBST
        rnam = n->key.p;
#       else
        rnam = n->key;
#       endif
#       if defined(TRACE)
        fprintf(debug, "  ptr_db_srch(uz_path_db, key(%s)): name(%s) rnam(%s)\n", current_path, name, rnam);
#       endif
        const size_t rl = strlen(rnam);
        if (!bmode)
        {
            if (!strncmp(rnam, syspth[0], rl) || !strncmp(rnam, syspth[1], rl))
            {
                n = NULL;
            }
            const int col = bpth->col != -1 ? bpth->col : right_col;
            const size_t l = strlen(name);
            memcpy(syspth[col], name, l+1);
            pthlen[col] = l;

            if (n)
                free(const_cast_ptr(name));
            // else {
            //  TODO: reset path offset
            // }
            name = NULL;
        }
        if (n)
        {
            free(bpth);
            ptr_db_del(&uz_path_db, n);
        }
    }
    else
    {
        n = NULL; // n might be set by an unsuccessful search. Clear it.
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
        free(const_cast_ptr(name));

        if (i & 2) { /* build_diff_db() return an error */
			/* sp is absolut. So reset length to place sp at
			 * begin of string. */
			*lp = 0;
            scan_subdir(saved_path, NULL, right_col ? 2 : 1);
		} else {
			fmode_chdir();
		}

        free(saved_path);
	}

	if (n) {
        /* `rnam` is const but modified below */
        char *s = const_cast_ptr(rnam);

        size_t l = strlen(s);

        if (gl_mark && !strncmp(s, gl_mark, l))
			clr_mark();

        l -= 2;
        s[l] = 0; /* remove "/[lr]" */

        rmtmpdirs(s); /* does free(rnam) */
        reset_path_offsets(bmode || right_col ? 1 : 0);

		if (bmode)
            free(const_cast_ptr(name)); /* dat */
	}

	if (right_col) {
		syspth[1][pthlen[1]] = 0;
	} else if (fmode) {
		syspth[0][pthlen[0]] = 0;
	}

    if ((uv = db_get_curs(right_col, current_path))) {
		top_idx[right_col] = *uv++;
		curs[right_col] = *uv;
	}

disp:
	disp_list(1);

ret:
#ifdef TRACE
	TRCPTH;
	fprintf(debug, "<-enter_dir lp(%s) rp(%s)\n", trcpth[0], trcpth[1]);
#endif
	return;
}

int printerr(const char *s2, const char *s1, ...)
{
    va_list ap;
    char *buf;
    static const size_t bufsiz = 4096;
    int ret_val = 0;

#ifdef TEST
    printerr_called = TRUE;
#endif
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
        if (fprintf(stderr, "%s: ", prog) < 0)
            ret_val |= 1;

        if (s1) {
            va_start(ap, s1);
            if (vfprintf(stderr, s1, ap) < 0)
                ret_val |= 1;
            va_end(ap);
        }

        if (s2) {
            if (fprintf(stderr, ": %s", s2) < 0)
                ret_val |= 1;
        }

        if (fputc('\n', stderr) == EOF)
            ret_val |= 1;
        goto ret;
    }

    if (!(buf = malloc(bufsiz))) {
        fputs(oom_msg, stderr);
        ret_val |= 1;
        goto ret;
    }

    wstat_dirty = TRUE;
    if (werase(wstat) == ERR)
        ret_val |= 1;
    if (wmove(wstat, s2 ? 0 : 1, 0) == ERR)
        ret_val |= 1;

    if (s1) {
        va_start(ap, s1);
        if (vsnprintf(buf, bufsiz, s1, ap) < 0)
            ret_val |= 1;
        if (putmbs(wstat, buf, -1) < 0)
            ret_val |= 1;
        va_end(ap);
    }

    if (s2) {
        if (wmove(wstat, 1, 0) == ERR)
            ret_val |= 1;
        if (wclrtoeol(wstat) == ERR)
            ret_val |= 1;
        if (snprintf(buf, bufsiz, "%s", s2) < 0)
            ret_val |= 1;
        if (putmbs(wstat, buf, -1) < 0)
            ret_val |= 1;
        if (wrefresh(wstat) == ERR)
            ret_val |= 1;
        opt_flushinp();
        getch();
        werase(wstat);
    }

    filt_stat();
    if (wrefresh(wstat) == ERR)
        ret_val |= 1;
    free(buf);
ret:
    return ret_val;
}

static int cli_vdialog(const char *const quest,
                       const char *const fmt, va_list ap)
{
    int c = 0;
    if (fmt) {
        FILE *fp = nodialog ? stderr :
                              stdout ;
        if (nodialog)
            fprintf(fp, "%s: ", prog);
        vfprintf(fp, fmt, ap);
        fputc('\n', fp);
    }
    if (!nodialog) {
        fprintf(stdout, "%s: ", quest);
        c = fgetc(stdin);
        if (c != '\n') {
            while (fgetc(stdin) != '\n') {
            }
        }
    }
    return c;
}

int
dialog(const char *quest, const char *answ, const char *fmt, ...)
{
	va_list ap;
	int r;

#if defined(TRACE)
    fprintf(debug, "->dialog: quest=\"%s\" answ=\"%s\" ", quest, answ);
    if (fmt) {
        va_start(ap, fmt);
        vfprintf(debug, fmt, ap);
        va_end(ap);
    }
	fputc('\n', debug);
#endif

    if (fmt)
        va_start(ap, fmt);
    r = vdialog(quest, answ, fmt, ap);
    if (fmt)
        va_end(ap);

#if defined(TRACE)
    fprintf(debug, "<-dialog: '%c'\t(%d)\n", r, r);
#endif
    return r;
}

int
vdialog(const char *quest, const char *answ, const char *fmt, va_list ap)
{
    int c = 0;

    if (!wstat) {
        c = cli_vdialog(quest, fmt, ap);
    } else {
        int c2;
        const char *s;

        wstat_dirty = TRUE;

        if (fmt) {
            /* werase() must not be used if `fmt` is NULL.
             * This way it is possible to write something to `wstat`
             * and the call vdialog(). */
            werase(wstat);
            wmove(wstat, 0, 0);
            vw_printw(wstat, fmt, ap);
        }

        mvwprintw(wstat, 1, 0, "%s", quest);
        wrefresh(wstat);

        do {
            opt_flushinp();
            c = getch();
            for (s = answ; s && (c2 = *s); s++)
                if (c == c2)
                    break;
        } while (s && !c2);

        werase(wstat);
        filt_stat();
        wrefresh(wstat);
    }
    return c;
}

static void
ui_resize(void)
{
#if defined(TRACE)
	fprintf(debug, "->ui_resize\n");
#endif
	if (exec_nocurs) {
		/* In this case curses is not active.
		 * So don't even call set_win_dim() */
		goto ret;
	}

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

ret:
#if defined(TRACE)
	fprintf(debug, "<-ui_resize\n");
#endif
	return;
}

void
set_win_dim(void)
{
#if defined(TRACE)
	fprintf(debug,
"->set_win_dim statw=%d listh=%d listw=%d rlstx=%d rlstw=%d llstw=%d\n",
	    statw, listh, listw, rlstx, rlstw, llstw);
#endif
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
#if defined(TRACE)
	fprintf(debug,
"<-set_win_dim statw=%d listh=%d listw=%d rlstx=%d rlstw=%d llstw=%d\n",
	    statw, listh, listw, rlstx, rlstw, llstw);
#endif
}
