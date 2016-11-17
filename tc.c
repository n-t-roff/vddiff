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

#include <regex.h>
#include "compat.h"
#include "ui.h"
#include "tc.h"
#include "main.h"
#include "ui2.h"

int llstw, rlstw, rlstx, midoffs;
WINDOW *wllst, *wmid, *wrlst;
unsigned top_idx2, curs2;
bool twocols;
bool fmode;
bool right_col;

void
open2cwins(void)
{
	if (!(wllst = subwin(stdscr, listh, llstw, 0, 0))) {
		printf("subwin failed\n");
		return;
	}

	if (!(wmid = subwin(stdscr, listh, 1, 0, llstw))) {
		printf("subwin failed\n");
		return;
	}

	if (!(wrlst = subwin(stdscr, listh, rlstw, 0, rlstx))) {
		printf("subwin failed\n");
		return;
	}

	if (color)
		wbkgd(wmid, '|' | COLOR_PAIR(PAIR_CURSOR));
	else
		wbkgd(wmid, '|' | A_STANDOUT);
}

void
prt2chead(void)
{
	wmove(wstat, 1, 0);
	wclrtoeol(wstat);
	lpath[llen] = 0;
	putmbsra(wstat, lpath, llstw);
	standoutc(wstat);
	mvwaddch(wstat, 1, llstw, fmode ? '|' : ' ');
	standendc(wstat);
	rpath[rlen] = 0;
	putmbsra(wstat, rpath, 0);

	if (fmode)
		mvwchgat(wstat, 1, right_col ? rlstx : 0,
		    right_col ? rlstw : llstw,
		    (color ? 0 : A_REVERSE) | A_BOLD,
		    color ? PAIR_HEADLINE : 0, NULL);
}

WINDOW *
getlstwin(void)
{
	return right_col ? wrlst :
	       fmode     ? wllst :
	                   wlist ;
}

void
tgl2c(void)
{
	if (bmode || fmode)
		return;

	twocols = twocols ? FALSE : TRUE;
	disp_list();
}
