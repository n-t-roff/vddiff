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
#include <regex.h>
#include "compat.h"
#include "ui.h"
#include "tc.h"
#include "main.h"
#include "ui2.h"
#include "fs.h"

static void close2cwins(void);

int llstw, rlstw, rlstx, midoffs;
WINDOW *wllst, *wmid, *wrlst;
bool twocols;
bool fmode;
bool right_col;
bool from_fmode;

void
open2cwins(void)
{
	if (!(wllst = new_scrl_win(listh, llstw, 0, 0)))
		return;

	if (!(wmid = newwin(listh, 1, 0, llstw))) {
		printf("newwin failed\n");
		return;
	}

	if (!(wrlst = new_scrl_win(listh, rlstw, 0, rlstx)))
		return;

	if (color)
		wbkgd(wmid, COLOR_PAIR(PAIR_CURSOR));
	else
		wbkgd(wmid, A_STANDOUT);

	wnoutrefresh(wmid);
}

static void
close2cwins(void)
{
	delwin(wllst);
	delwin(wmid);
	delwin(wrlst);
}

void
fmode_dmode(void)
{
	if (!fmode)
		return;

	fmode = FALSE;
	right_col = 0;
	from_fmode = TRUE;
	close2cwins();
}

void
dmode_fmode(void)
{
	fmode = TRUE;
	open2cwins();
	disp_fmode();
}

void
prt2chead(void)
{
	wmove(wstat, 1, 0);
	wclrtoeol(wstat);
	lpath[llen] = 0;
	putmbsra(wstat, lpath, llstw);
	standoutc(wstat);
	mvwaddch(wstat, 1, llstw, ' ');
	standendc(wstat);
	rpath[rlen] = 0;
	putmbsra(wstat, rpath, 0);
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

void
resize_fmode(int d)
{
	if (d)
		midoffs += d;
	else
		midoffs = 0;

	set_win_dim();

	if (fmode) {
		close2cwins();
		open2cwins();
	}

	disp_fmode();
}

void
disp_fmode(void)
{
	if (!fmode) {
		disp_list();
		return;
	}

	right_col = right_col ? FALSE : TRUE;
	disp_list();
	disp_curs(0);
	wnoutrefresh(getlstwin());
	touchwin(wmid);
	wnoutrefresh(wmid);
	right_col = right_col ? FALSE : TRUE;
	disp_list();
}

void
fmode_cp_pth(void)
{
	if (right_col) {
		lpath[llen] = 0;
		memcpy(rpath, lpath, llen + 1);
		rlen = llen;
	} else {
		rpath[rlen] = 0;
		memcpy(lpath, rpath, rlen + 1);
		llen = rlen;
	}

	enter_dir(NULL, NULL, FALSE, FALSE);
}
