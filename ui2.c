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
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <regex.h>
#include "ed.h"
#include "ui.h"
#include "ui2.h"
#include "uzp.h"
#include "diff.h"
#include "db.h"

short noic, magic, nows, scale;
short regex;
struct history opt_hist;

static regex_t re_dat;

void
disp_regex(void)
{
	werase(wstat);
	mvwaddstr(wstat, 1, 0,
"[n] search forward, [N] search backward, [r] cancel search");
	wrefresh(wstat);
}

static char no_match_msg[] = "No match";

void
clr_regex(void)
{
	regex = 0;
	regfree(&re_dat);
	werase(wstat);
	wrefresh(wstat);
}

void
start_regex(char *pattern)
{
	int fl = REG_NOSUB;

	if (db_num < 2) {
		regex = 0;
		return;
	}

	if (magic)
		fl |= REG_EXTENDED;
	if (!noic)
		fl |= REG_ICASE;

	if (regcomp(&re_dat, pattern, fl)) {
		printerr(strerror(errno), "regcomp \"%s\" failed", pattern);
		regex = 0;
		return;
	}

	if (!regex_srch(0))
		disp_regex();
	else
		printerr(NULL, no_match_msg);
}

/* !0: Not found in *any* file */

int
regex_srch(
    /* -1: prev
     *  0: initial search
     *  1: next */
    int dir)
{
	unsigned i, j;
	struct filediff *f;

	/* does not make sense for one line */
	if (db_num < 2) {
		clr_regex();
		return 1;
	}

	i = j = top_idx + curs;

	while (1) {
		if (dir > 0) {
			if (++i >= db_num) {
				if (nows)
					goto no_match;
				i = 0;
			}
		} else if (dir < 0) {
			if (i)
				i--;
			else if (nows)
				goto no_match;
			else
				i = db_num - 1;
		}

		f = db_list[i];

		if (!regexec(&re_dat, f->name, 0, NULL, 0)) {
			center(i);
			return 0;
		}

		if (!dir)
			dir = 1;
		else if (i == j) {
			printerr(NULL, no_match_msg);
			clr_regex();
			return 1;
		}
	}

	return 0;

no_match:
	printerr(NULL, no_match_msg);
	return 0;
}

void
parsopt(char *buf)
{
	char *opt;
	short not;
	static char noic_str[]    = "noic\n";
	static char nomagic_str[] = "nomagic\n";
	static char nows_str[]    = "nows\n";

	if (!strcmp(buf, "set")) {
		werase(wlist);
		wmove(wlist, 0, 0);
		waddstr(wlist, noic  ? noic_str : noic_str + 2);
		waddstr(wlist, magic ? nomagic_str + 2 : nomagic_str);
		waddstr(wlist, nows  ? nows_str : nows_str + 2);
		anykey();
		return;
	}

	if (!strncmp(buf, "no", 2)) {
		opt = buf + 2;
		not = 1;
	} else {
		opt = buf;
		not = 0;
	}

	if (!strcmp(opt, "ic")) {
		noic = not;
	} else if (!strcmp(opt, "magic")) {
		magic = not ? 0 : 1;
	} else if (!strcmp(opt, "ws")) {
		nows = not;
	} else
		printerr(NULL, "Unknown option \"%s\"", buf);
}

void
anykey(void)
{
	wrefresh(wlist);
	printerr(NULL, "Press any key to continue");
	getch();
	disp_list();
}
