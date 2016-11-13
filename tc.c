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

#include "compat.h"
#include "ui.h"
#include "tc.h"

int llstw, rlstw, rlstx;
WINDOW *wllst, *wmid, *wrlst;
bool twocols;
bool fmode;

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
		wbkgd(wmid, COLOR_PAIR(PAIR_CURSOR));
	else
		wbkgd(wmid, A_STANDOUT);
}
