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
#include <stdlib.h>
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
#include "fs.h"
#include "main.h"

struct str_uint {
	char *s;
	unsigned int u;
};

static int srchcmp(const void *, const void *);

short noic, magic, nows, scale;
short regex;
struct history opt_hist;
struct history regex_hist;

static struct str_uint *srchmap;
static regex_t re_dat;
static unsigned srch_idx;

void
ui_srch(void)
{
	unsigned u;

	if (!db_num) {
		no_file();
		return;
	}

	srchmap = malloc(sizeof(struct str_uint) * db_num);

	for (u = 0; u < db_num; u++) {
		srchmap[u].s = db_list[u]->name;
		srchmap[u].u = u;
	}

	qsort(srchmap, db_num, sizeof(struct str_uint), srchcmp);
	srch_idx = 0;

	if (regex)
		clr_regex();

	ed_dialog("Type first characters of filename:",
	    "" /* remove existing */, srch_file, 0, NULL);
	free(srchmap);

	if (!regex)
		return;

	if (ed_dialog("Enter regular expression:",
	    "" /* remove existing */, NULL, 0, &regex_hist) ||
	    !*rbuf)
		regex = 0;
	else
		start_regex(rbuf);
}

static int
srchcmp(const void *p1, const void *p2)
{
	const struct str_uint *m1 = p1;
	const struct str_uint *m2 = p2;

	if (noic)
		return strcmp(m1->s, m2->s);
	else
		return strcasecmp(m1->s, m2->s);
}

int
srch_file(char *pattern)
{
	unsigned idx;
	int o, oo;
	size_t l;
	char *s;


	if (!*pattern || !db_num)
		return 0;

	if (*pattern == '/' && !pattern[1]) {
		regex = 1;
		return EDCB_FAIL;
	}

	if (srch_idx >= db_num)
		srch_idx = db_num - 1;

	idx = srch_idx;
	o = 0;
	l = strlen(pattern);

	while (1) {
		oo = o;
		s = srchmap[idx].s;

		if (noic)
			o = strncmp(s, pattern, l);
		else
			o = strncasecmp(s, pattern, l);

		if (!o) {
			center(srchmap[srch_idx = idx].u);
			break;
		} else if (o < 0) {
			if (oo > 0 || ++idx >= db_num)
				break;
		} else if (o > 0) {
			if (oo < 0 || !idx)
				break;

			idx--;
		}
	}

	return 0;
}

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
	if (db_num < 2)
		return 1;

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
