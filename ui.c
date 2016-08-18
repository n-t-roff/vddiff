#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "compat.h"
#include "avlbst.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"

static void ui_ctrl(void);
static void page_down(void);
static void page_up(void);
static void curs_down(void);
static void curs_up(void);
static void disp_curs(int);
static void disp_list(void);
static void disp_line(unsigned, unsigned);
static void push_state(void);
static void pop_state(void);
static void enter_dir(char *);

static unsigned listw, listh, statw;
static WINDOW *wlist, *wstat;
static unsigned top_idx, curs;
static struct ui_state *ui_stack;

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
	db_free();
	delwin(wstat);
	delwin(wlist);
}

static void
ui_ctrl(void)
{
	int c;
	struct filediff *f;

	while (1) {
		switch (c = getch()) {
		case 'q':
			return;
		case KEY_DOWN:
			curs_down(); break;
		case KEY_UP:
			curs_up(); break;
		case KEY_LEFT:
			pop_state(); break;
		case KEY_RIGHT:
			if (!db_num)
				break;
			f = db_list[top_idx + curs];
			if (f->ltype == f->rtype && S_ISDIR(f->ltype))
				enter_dir(f->name);
			break;
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
	int diff, type;
	mode_t mode;
	struct filediff *f = db_list[i];

	if (!f->ltype) {
		diff = '>';
		mode = f->rtype;
	} else if (!f->rtype) {
		diff = '<';
		mode = f->ltype;
	} else {
		diff = f->diff;
		mode = f->ltype;
	}

	if      (S_ISREG(mode)) type = ' ';
	else if (S_ISDIR(mode)) type = '/';
	else if (S_ISLNK(mode)) type = '@';
	else                    type = '?';

	mvwprintw(wlist, y, 0, "%c %c %s", diff, type, f->name);
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
	db_restore(st);
	free(st);
	disp_list();
}

static void
enter_dir(char *name)
{
	size_t l;

	push_state();

	lpath[llen++] = '/';
	rpath[rlen++] = '/';
	l = strlen(name);
	memcpy(lpath + llen, name, l + 1);
	memcpy(rpath + rlen, name, l + 1);
	llen += l;
	rlen += l;

	build_diff_db();
	disp_list();
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
