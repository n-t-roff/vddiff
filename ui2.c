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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>
#include <ctype.h>
#include "compat.h"
#include "ed.h"
#include "ui.h"
#include "ui2.h"
#include "exec.h"
#include "uzp.h"
#include "diff.h"
#include "fs.h"
#include "main.h"
#include "db.h"
#include "tc.h"
#include "gq.h"

const char y_n_txt[] = "'y' yes, 'n' no";
const char y_a_n_txt[] = "'y' yes, 'a' all, 'n' no";
const char ign_txt[] = "'i' ignore errors, <ENTER> continue";
const char ign_esc_txt[] = "<ENTER> continue, <ESC> cancel, 'i' ignore errors";
const char any_txt[] = "Press any key to continue";

struct str_uint {
	char *s;
	unsigned int u;
};

static int srchcmp(const void *, const void *);
static char *getnextarg(char *);
static void set_all(void);

long mark_idx[2] = { -1, -1 };
static wchar_t wcbuf[BUF_SIZE];
static cchar_t ccbuf[BUF_SIZE];
short noic, magic, nows, scale;
short regex;
unsigned short subtree = 3;

static struct str_uint *srchmap;
static regex_t re_dat;
static unsigned srch_idx;

bool file_pattern; /* TRUE for -F or -G */

int
test_fkey(int c, unsigned short num)
{
	int i;
	int ek;

	for (i = 0; i < FKEY_NUM; i++) {
		if (c != KEY_F(i + 1))
			continue;

		if (nofkeys) {
			printerr(NULL,
			    "Type \":set fkeys\" to enable function keys");
			return 1;
		}

		if (fkey_cmd[i]) {
			struct tool t;
			unsigned ti;
			unsigned short act;
			static char *keys =
			    "<ENTER> execute, 'e' edit, 'n' no";

			t = viewtool;
			viewtool.tool = NULL;
			viewtool.args = NULL;
			/* set_tool() reused here to process
			 * embedded "$1" */
			set_tool(&viewtool, strdup(fkey_cmd[i]),
			    /* A rebuild_db() is done, so the list need not
			     * to be updated by the command itself */
			    TOOL_NOLIST |
			    (fkey_flags[i] & FKEY_WAIT ? TOOL_WAIT : 0));
			act = num > 1 ? 0 : 1;

			if ((force_exec || (fkey_flags[i] & FKEY_FORCE)) &&
			    (force_multi || num <= 1)) {
				goto exec;
			}

			werase(wstat);
			mvwprintw(wstat, 0, 0, "Really execute \"%s\"",
			    fkey_cmd[i]);

			if (num > 1) {
				wprintw(wstat, " for %d files", num);
			}

			/* Mark makes no sense for multiple file operation */
			waddstr(wstat, "?");

			switch (dialog(keys, NULL, NULL)) {
			case 'e':
				free_tool(&viewtool);
				viewtool = t;
				goto edit_fkey;

			case '\n':
exec:
				ti = top_idx[right_col];

#if defined(TRACE)
				fprintf(debug, "test_fkey: ->%u x \"%s\"\n",
				    num, fkey_cmd[i]);
#endif
				while (num--) {
					action(1, 3, act, FALSE);
					top_idx[right_col]++; /* kludge */
				}
#if defined(TRACE)
				fprintf(debug, "test_fkey: <-\"%s\"\n",
				    fkey_cmd[i]);
#endif

				top_idx[right_col] = ti;
				/* action() did likely create or delete files.
				 * Calls mark_global() since 'mark' pointer
				 * gets invalid. (Rebuild means at first free
				 * all filestat pointers. 'mark' is one of
				 * them.) */
				rebuild_db(0);

				if (gl_mark) {
					chk_mark(gl_mark, 0);
				}

				break;

			default:
				disp_fmode(); /* since curs pos did chg */
			}

			free_tool(&viewtool);
			viewtool = t;
			return 1;
		}

edit_fkey:
		linelen = 0; /* Remove existing text */

		if (fkey_cmd[i]) {
			if (fkey_flags[i] & FKEY_WAIT)
				ed_append("! ");
			else if (fkey_flags[i] & FKEY_FORCE)
				ed_append("# ");
			else
				ed_append("$ ");

			ed_append(fkey_cmd[i]);
		}

		if (ed_dialog(
		    "Type text to be saved for function key:",
		    NULL, NULL, 1, NULL))
			break;

		free(sh_str[i]);
		sh_str[i] = NULL;
		free(fkey_cmd[i]);
		fkey_cmd[i] = NULL;

		if (((ek = *rbuf) == '$' || ek == '!' || ek == '#') &&
		    isspace((int)rbuf[1])) {
			int c2;
			int j = 0;

			while ((c2 = rbuf[++j]) && isspace(c2));

			if (!c2)
				return 1; /* empty input */

			set_fkey_cmd(i, rbuf + j, ek);
			clr_edit();
			printerr(NULL, "%c %s saved for F%d",
			    FKEY_CMD_CHR(i), fkey_cmd[i], i + 1);
		} else {
			sh_str[i] = linebuf;
			linebuf = NULL; /* clr_edit(): free(linebuf) */
			clr_edit();
			printerr(NULL, "%ls"
			    " saved for F%d", sh_str[i], i + 1);
		}
		return 1;
	}

	return 0;
}

void
set_fkey_cmd(int i, char *s, int ek)
{
	fkey_cmd[i] = strdup(s);
	fkey_flags[i] = ek == '!' ? FKEY_WAIT  : /* wait after command */
			ek == '#' ? FKEY_FORCE : /* don't wait before command */
				    0 ;
}

void
ui_srch(void)
{
	static struct history regex_hist;
	unsigned u;

	if (!db_num[right_col]) {
		no_file();
		return;
	}

	srchmap = malloc(sizeof(struct str_uint) * db_num[right_col]);

	for (u = 0; u < db_num[right_col]; u++) {
		srchmap[u].s = db_list[right_col][u]->name;
		srchmap[u].u = u;
	}

	qsort(srchmap, db_num[right_col], sizeof(struct str_uint), srchcmp);
	srch_idx = 0;

	if (regex)
		clr_regex();

	ed_dialog("Type first characters of filename (<ESC> to cancel):",
	    "" /* remove existing */, srch_file, 0, NULL);
	free(srchmap);

	if (!regex)
		return;

	if (ed_dialog("Enter regular expression (<ESC> to cancel):",
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


	if (!*pattern || !db_num[right_col])
		return 0;

	if (*pattern == '/' && !pattern[1]) {
		regex = 1;
		return EDCB_FAIL;
	}

	if (srch_idx >= db_num[right_col])
		srch_idx = db_num[right_col] - 1;

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
			if (oo > 0 || ++idx >= db_num[right_col])
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
	filt_stat();
	mvwaddstr(wstat, 1, 0,
"'n' search forward, 'N' search backward, 'r' cancel search");
	wrefresh(wstat);
}

static char no_match_msg[] = "No match";

void
clr_regex(void)
{
	regex = 0;
	regfree(&re_dat);
	werase(wstat);
	filt_stat();
	wrefresh(wstat);
}

void
start_regex(char *pattern)
{
	int fl = REG_NOSUB;

	if (db_num[right_col] < 2) {
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
	if (db_num[right_col] < 2)
		return 1;

	i = j = top_idx[right_col] + curs[right_col];

	while (1) {
		if (dir > 0) {
			if (++i >= db_num[right_col]) {
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
				i = db_num[right_col] - 1;
		}

		f = db_list[right_col][i];

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

/* 1: exit vddiff */

int
parsopt(char *buf)
{
	char *opt;
	short not;
	short skip;

#if defined(TRACE)
	fprintf(debug, "<->parsopt(%s)\n", buf);
#endif
	if (!strcmp(buf, "cd")) {
		char *s;

		if (!(bmode || fmode)) {
			printerr(NULL, "cd not supported in diff mode");
			return 0;
		}

		if (!(s = getenv("HOME"))) {
			printerr(NULL, "HOME not set\n");
			return 0;
		}

		enter_dir(s, NULL, FALSE, FALSE, 0);
		return 0;
	}

	if (!strncmp(buf, "cd ", 3)) {
		if (!(bmode || fmode)) {
			printerr(NULL, "cd not supported in diff mode");
			return 0;
		}

		if (!(buf = getnextarg(buf + 3))) {
			return 0;
		}

		enter_dir(buf, NULL, FALSE, FALSE, 0);
		return 0;
	}

	if (*buf == 'e' && (!buf[1] || !strcmp(buf+1, "dit"))) {
		readonly = FALSE;
		nofkeys = FALSE;
		return 0;
	}

	if (!strncmp(buf, "find ", 5)) {
		if (!(buf = getnextarg(buf + 5))) {
			return 0;
		}

		if (!fn_init(buf)) {
			one_scan = TRUE;
			rebuild_db(1);
		}

		return 0;
	}

	if (!strncmp(buf, "grep ", 5)) {
		if (!(buf = getnextarg(buf + 5))) {
			return 0;
		}

		if (!gq_init(buf)) {
			one_scan = TRUE;
			rebuild_db(1);
		}

		return 0;
	}

	if (!strcmp(buf, "nofind")) {
		if (!fn_free()) {
			rebuild_db(1);
		}

		return 0;
	}

	if (!strcmp(buf, "nogrep")) {
		if (!gq_free()) {
			rebuild_db(1);
		}

		return 0;
	}

	if (!strcmp(buf, "q") || !strcmp(buf, "qa")) {
		return 1;
	}

	if (!strncmp(buf, "se", 2) && (!buf[2] || (buf[2] == 't' && !buf[3]))) {
		set_all();
		return 0;
	}

	if (!strncmp(buf, "vie", 3) && (!buf[3] ||
	    (buf[3] == 'w' && !buf[4]))) {
		readonly = TRUE;
		nofkeys = TRUE;
		return 0;
	}

	/* only bool options below */

	if (!strncmp(buf, "se " , (skip = 3)) ||
	    !strncmp(buf, "set ", (skip = 4))) {
		if (!(buf = getnextarg(buf + skip))) {
			return 0;
		}

		if (!strcmp(buf, "all")) {
			set_all();
			return 0;
		}
	}

	if (!strncmp(buf, "no", 2)) {
		opt = buf + 2;
		not = 1;
	} else {
		opt = buf;
		not = 0;
	}

	if (!strcmp(opt, "fkeys")) {
		nofkeys = not ? TRUE : FALSE;
	} else if (!strcmp(opt, "ic")) {
		noic = not;
	} else if (!strcmp(opt, "magic")) {
		magic = not ? 0 : 1;
	} else if (!strcmp(opt, "recursive")) {
		recursive = not ? 0 : 1;
	} else if (!strcmp(opt, "ws")) {
		nows = not;
	} else {
		printerr(NULL, "Unknown option \"%s\"", buf);
	}

	return 0;
}

static char *
getnextarg(char *buf)
{
	while (*buf == ' ') {
		buf++;
	}

	if (!*buf) {
		printerr(NULL, "Argument expected");
		return NULL;
	}

	return buf;
}

static void
set_all(void)
{
	static char nofkeys_str[] = "nofkeys\n";
	static char noic_str[] = "noic\n";
	static char nomagic_str[] = "nomagic\n";
	static char norecurs_str[] = "norecursive\n";
	static char nows_str[] = "nows\n";

	werase(wlist);
	wattrset(wlist, A_NORMAL);
	wmove(wlist, 0, 0);
	waddstr(wlist, nofkeys ? nofkeys_str : nofkeys_str + 2);
	waddstr(wlist, noic ? noic_str : noic_str + 2);
	waddstr(wlist, magic ? nomagic_str + 2 : nomagic_str);
	waddstr(wlist, recursive ? norecurs_str + 2 : norecurs_str);
	waddstr(wlist, nows ? nows_str : nows_str + 2);
	anykey();
}

void
bindiff(void)
{
	struct filediff *f, *z1 = NULL, *z2 = NULL;
	char *t1 = NULL, *t2 = NULL;
	struct filediff *m = mark;
	mode_t ltyp = 0, rtyp = 0;
	char *lnam, *rnam, *olnam, *ornam;
	off_t lsiz, rsiz;
	int val = -1;
	bool ml;

	if (!db_num[right_col] || !mark)
		return;

	f = db_list[right_col][top_idx[right_col] + curs[right_col]];

	/* check if mark needs to be unzipped */
	ml = m->ltype && (f->rtype || !m->rtype);

	if ((z1 = unpack(m, ml ? 1 : 2, &t1, 0))) {
		m = z1;
	}

	/* check if other file needs to be unzipped */
	if ((z2 = unpack(f, f->rtype ? 2 : 1, &t2, 0))) {
		f = z2;
	}

	if (ml) {
		olnam =
		lnam = m->name ? m->name   :
		       bmode   ? gl_mark   :
		                 mark_lnam ;

		if (*lnam != '/') {
			pthcat(lpath, llen, lnam);
			lnam = lpath;
		}

		if (chk_mark(lnam, 0))
			goto ret;

		ltyp = m->ltype;
		lsiz = m->lsiz;
		ornam = rnam = f->name;

		if (f->rtype) {
			rtyp = f->rtype;
			rsiz = f->rsiz;

			if (*rnam != '/') {
				pthcat(rpath, rlen, rnam);
				rnam = rpath;
			}
		} else {
			rtyp = f->ltype;
			rsiz = f->lsiz;

			if (*rnam != '/') {
				pthcat(lpath, llen, rnam);
				rnam = lpath;
			}
		}

	} else if (m->rtype) {
		olnam = lnam = f->name;

		if (f->ltype) {
			ltyp = f->ltype;
			lsiz = f->lsiz;

			if (*lnam != '/') {
				pthcat(lpath, llen, lnam);
				lnam = lpath;
			}
		} else {
			ltyp = f->rtype;
			lsiz = f->rsiz;

			if (*lnam != '/') {
				pthcat(rpath, rlen, lnam);
				lnam = rpath;
			}
		}

		ornam =
		rnam = m->name ? m->name   :
		       bmode   ? gl_mark   :
		                 mark_rnam ;

		if (*rnam != '/') {
			pthcat(rpath, rlen, rnam);
			rnam = rpath;
		}

		if (chk_mark(rnam, 0))
			goto ret;

		rtyp = m->rtype;
		rsiz = m->rsiz;
	} else {
		goto ret;
	}

	if (!S_ISREG(ltyp) || !S_ISREG(rtyp)) {
		printerr(NULL,
		    "Binary diff can only be used for regular files");
		goto ret;
	}

	val = cmp_file(lnam, lsiz, rnam, rsiz, 1);

ret:
	switch (val) {
	case 0:
		printerr(any_txt, "Equal: %s and %s",
#if defined(DEBUG)
		    lnam, rnam);
		(void)olnam;
		(void)ornam;
#else
		    olnam, ornam);
#endif
		break;
	case 1:
		printerr(any_txt, "Different: %s and %s",
#if defined(DEBUG)
		    lnam, rnam);
#else
		    olnam, ornam);
#endif
		break;
	default:
		/* Error message is already output by cmp_file() */
		;
	}

	if (z1)
		free_zdir(z1, t1);

	if (z2)
		free_zdir(z2, t2);
}

int
chk_mark(char *file,
    /* Can be 0 for global mark */
    short tree)
{
	struct stat st;
	int i;
	bool rp;

#if defined(TRACE)
	/* Don't trim paths! (bindiff()) */
	TPTH;
	fprintf(debug, "<>chk_mark(%s,%d) lp(%s) rp(%s)\n",
	    file, tree, tlpth, trpth);
#endif
	rp = tree && /* f-key command */
	    !bmode && !strchr(file, '/');

	if (rp) {
		if (tree & 1) {
			pthcat(lpath, llen, file);
			file = lpath;
		} else if (tree & 2) {
			pthcat(rpath, rlen, file);
			file = rpath;
		}
	}

	i = stat(file, &st);

	if (i == -1 && errno != ENOENT) {
		printerr(strerror(errno), LOCFMT "stat \"%s\"" LOCVAR, file);
	}

	if (rp) {
		if (tree & 1) {
			lpath[llen] = 0;
		} else if (tree & 2) {
			rpath[rlen] = 0;
		}
	}

	if (i != -1) {
		return 0;
	}

	clr_mark();
	return -1;
}

char *
saveselname(void)
{
	unsigned ui;
	struct filediff *f;

	ui = top_idx[right_col] + curs[right_col];

	if (ui >= db_num[right_col]) {
		return NULL;
	}

	f = db_list[right_col][ui];
	return strdup(f->name);
}

unsigned
findlistname(char *name)
{
	unsigned u;
	struct filediff *f;

	for (u = 0; u < db_num[right_col]; u++) {
		f = db_list[right_col][u];

		if (!strcmp(name, f->name)) {
			return u;
		}
	}

	return 0;
}

void
re_sort_list(void)
{
	char *name;

	nodelay(stdscr, TRUE);

	if (fmode) {
		right_col = right_col ? 0 : 1;
		diff_db_sort(right_col);
		disp_list(0);
		right_col = right_col ? 0 : 1;
	}

	name = saveselname();
	diff_db_sort(right_col);

	if (name) {
		center(findlistname(name));
		free(name);
	} else {
		top_idx[right_col] = 0;
		curs[right_col] = 0;
		disp_list(1);
	}

	nodelay(stdscr, FALSE);
}

void
filt_stat(void)
{
	unsigned x;

	x = COLS - 1;
	standoutc(wstat);

	if (file_pattern) {
		mvwaddch(wstat, 0, x--, 'E');
	}

	if (wait_after_exec) {
		mvwaddch(wstat, 0, x--, 'W');
	}

	if (followlinks) {
		mvwaddch(wstat, 0, x--, 'F');
	}

	if (dontcmp) {
		mvwaddch(wstat, 0, x--, '%');
	}

	if (!(bmode || fmode)) {
		if (nosingle) {
			mvwaddch(wstat, 0, x--, '&');
		}

		if (noequal) {
			mvwaddch(wstat, 0, x--, '!');
		}

		if (real_diff) {
			mvwaddch(wstat, 0, x--, 'c');
		}
	}

	standendc(wstat);
	mvwaddch(wstat, 0, x--, ' ');
}

void
markc(WINDOW *w)
{
	if (color)
		wattrset(w, COLOR_PAIR(PAIR_MARK));
	else
		wattrset(w, A_BOLD | A_UNDERLINE);
}

void
chgat_mark(WINDOW *w, int y)
{
	mvwchgat(w, y, 0, -1,
	    color ? 0 : A_BOLD | A_UNDERLINE,
	    color ? PAIR_MARK : 0, NULL);
}

void
standoutc(WINDOW *w)
{
	if (color) {
		wattrset(w, COLOR_PAIR(PAIR_CURSOR));
	} else {
		wattrset(w, A_REVERSE);
	}
}

void
chgat_curs(WINDOW *w, int y)
{
	mvwchgat(w, y, 0, -1,
	    color ? 0 : A_STANDOUT,
	    color ? PAIR_CURSOR : 0, NULL);
}

void
standendc(WINDOW *w)
{
	if (color) {
		wattrset(w, COLOR_PAIR(PAIR_NORMAL));
	} else {
		wattrset(w, A_NORMAL);
	}
}

void
chgat_off(WINDOW *w, int y)
{
	mvwchgat(w, y, 0, -1, 0, color ? PAIR_NORMAL : 0, NULL);
}

void
anykey(void)
{
	wrefresh(wlist);
	printerr(NULL, any_txt);
	getch();
	disp_fmode();
}

void
free_zdir(struct filediff *z, char *t)
{
	free(z->name);
	free(z);

	if (t) {
		/* Not called for archives, only for compressed files.
		 * Hence don't use TOOL_NOLIST since */
		rmtmpdirs(t, TOOL_UDSCR);
	}
}

void
refr_scr(void)
{
	if (fmode) {
		wnoutrefresh(wllst);
		wnoutrefresh(wmid);
		wnoutrefresh(wrlst);
	} else
		wnoutrefresh(wlist);

	wnoutrefresh(wstat);
	doupdate();
}

void
rebuild_scr(void)
{
	endwin();
	refresh();

	if (fmode) {
		touchwin(wllst);
		touchwin(wmid);
		touchwin(wrlst);
	} else {
		touchwin(wlist);
	}

	touchwin(wstat);
	refr_scr();
}

ssize_t
mbstowchs(WINDOW *w, char *s)
{
	size_t l;

	l = mbstowcs(wcbuf, s, sizeof(wcbuf)/sizeof(*wcbuf));

	if (l == (size_t)-1) {
		printerr(NULL, "mbstowcs \"%s\" failed", s);
		return -1;

	} else if (l == sizeof(wcbuf)/sizeof(*wcbuf)) {
		wcbuf[sizeof(wcbuf)/sizeof(*wcbuf) - 1] = 0;
	}

	wcs2ccs(w, wcbuf);
	return l;
}

void
wcs2ccs(WINDOW *w, wchar_t *wc)
{
	attr_t a;
	short cp;
	cchar_t *cc;
	static wchar_t ws[2];

	(wattr_get)(w, &a, &cp, NULL);
	cc = ccbuf;

	do {
		*ws = *wc++;
		setcchar(cc++, ws, a, cp, NULL);
	} while (*ws);
}

void
putwcs(WINDOW *w, wchar_t *s,
    /* -1: unlimited */
    int n)
{
	wcs2ccs(w, s);
	wadd_wchnstr(w, ccbuf, n);
}

ssize_t
putmbs(WINDOW *w, char *s,
    /* -1: unlimited */
    int n)
{
	ssize_t l;

	l = mbstowchs(w, s);
	wadd_wchnstr(w, ccbuf, n);
	return l;
}

int
addmbs(WINDOW *w, char *s, int mx)
{
	int cy, cx, my;
	ssize_t l;
	int n;

	getyx(w, cy, cx);

	if (!mx) {
		n = -1; /* wadd_wchnstr does the work */
		getmaxyx(w, my, mx);
	} else
		n = mx - cx;

	(void)my;

	if (cx >= mx)
		return 1;

	if ((l = putmbs(w, s, n)) == -1)
		return -1;

	cx += l;

	if (cx > mx)
		cx = mx;

	wmove(w, cy, cx);
	return 0;
}

ssize_t
putmbsra(WINDOW *w, char *s, int mx)
{
	int cy, cx, my;
	ssize_t l;
	cchar_t *cs;

	getyx(w, cy, cx);

	if (!mx)
		getmaxyx(w, my, mx);

	(void)cy;
	(void)my;

	if (cx >= mx)
		return 1;

	if ((l = mbstowchs(w, s)) == -1)
		return -1;

	cs = ccbuf;

	if (l > mx - cx)
		cs += l - (mx - cx);

	wadd_wchstr(w, cs);
	return l;
}

WINDOW *
new_scrl_win(int h, int w, int y, int x)
{
	WINDOW *win;

	if (!(win = newwin(h, w, y, x))) {
		printf("newwin failed\n");
		return NULL;
	}

	if (scrollen) {
		idlok(win, TRUE);
		scrollok(win, TRUE);
	}

	if (color) {
		wbkgd(   win, COLOR_PAIR(PAIR_NORMAL));
		wbkgdset(win, COLOR_PAIR(PAIR_NORMAL));
	}

	touchwin(win);
	wnoutrefresh(win);
	return win;
}

void
set_def_mouse_msk(void)
{
#ifdef NCURSES_MOUSE_VERSION
	mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_PRESSED
# if NCURSES_MOUSE_VERSION >= 2
	    | BUTTON4_PRESSED | BUTTON5_PRESSED
# endif
	    , NULL);
#endif
}
