#include <stdarg.h>
#include "compat.h"
#include "ui.h"
#include "diff.h"
#include "db.h"

static void ui_ctrl(void);
static void page_down(void);
static void page_up(void);
static void curs_down(void);
static void curs_up(void);
static void disp_curs(int);
static void disp_list(void);
static void disp_line(unsigned, unsigned);

static unsigned listw, listh, statw;
static WINDOW *wlist, *wstat;
static unsigned top_idx, curs;

void
build_ui(void)
{
	listw = statw = COLS;
	listh = LINES - 3;

	if (!(wlist = subwin(stdscr, listh, listw, 0, 0))) {
		printf("subwin failed\n");
		return;
	}

	if (!(wstat = subwin(stdscr, 2, statw, LINES-2, 0))) {
		printf("subwin failed\n");
		return;
	}

	build_diff_db();
	disp_list();
	ui_ctrl();
	delwin(wstat);
	delwin(wlist);
}

static void
ui_ctrl(void)
{
	int c;

	while (1) {
		switch (c = getch()) {
		case 'q':
			return;
		case KEY_DOWN:
			curs_down(); break;
		case KEY_UP:
			curs_up(); break;
		case KEY_NPAGE:
			page_down(); break;
		case KEY_PPAGE:
			page_up(); break;
		default:
			printerr(NULL, "Invalid key %c pressed", c);
		}
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
curs_down(void)
{
	if (top_idx + curs + 1 >= db_num)
		return;

	if (curs + 1 >= listh) {
		page_down();
		return;
	}

	disp_curs(0);
	curs++;
	disp_curs(1);
	wrefresh(wlist);
}

static void
curs_up(void)
{
	if (!curs) {
		if (!top_idx)
			return;

		page_up();
		return;
	}

	disp_curs(0);
	curs--;
	disp_curs(1);
	wrefresh(wlist);
}

static void
disp_curs(int a)
{
	if (a)
		wattron(wlist, A_REVERSE);
	disp_line(curs, top_idx + curs);
	if (a)
		wattroff(wlist, A_REVERSE);
}

static void
disp_list(void)
{
	unsigned y, i;

	if (!db_num) {
		printerr(NULL, "No data");
		return;
	}

	werase(wlist);
	for (y = 0, i = top_idx; y < listh && i < db_num; y++, i++) {
		if (y == curs)
			disp_curs(1);
		else
			disp_line(y, i);
	}
	wrefresh(wlist);
}

static void
disp_line(unsigned y, unsigned i)
{
	mvwprintw(wlist, y, 0, "%s", db_list[i]->name);
}

void
printerr(char *s2, char *s1, ...)
{
	va_list ap;

	werase(wstat);
	wmove(wstat, 0, 0);
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
