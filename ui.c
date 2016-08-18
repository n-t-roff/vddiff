#include <stdarg.h>
#include "compat.h"
#include "ui.h"
#include "diff.h"

static unsigned listw,
     	        listh,
	        statw;

static WINDOW *wlist,
              *wstat;

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
	delwin(wstat);
	delwin(wlist);
}

void
printerr(char *s, ...)
{
	va_list ap;

	werase(wstat);
	wmove(wstat, 0, 0);
	va_start(ap, s);
	vwprintw(wstat, s, ap);
	va_end(ap);
	refresh();
	getch();
	werase(wstat);
}
