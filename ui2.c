/*
Copyright (c) 2016-2017, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <stdarg.h>
#include <signal.h>
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
#include "cplt.h"

const char y_n_txt[] = "'y' yes, 'n' no";
const char y_a_n_txt[] = "'y' yes, 'a' all, 'n' no, 'N' none, <ESC> cancel";
const char ign_txt[] = "'i' ignore errors, <ENTER> continue";
const char ign_esc_txt[] = "<ENTER> continue, <ESC> cancel, 'i' ignore errors";
const char any_txt[] = "Press any key to continue";
const char enter_regex_txt[] = "Enter regular expression (<ESC> to cancel):";
const char no_match_txt[] = "No match";

struct str_uint {
	char *s;
	unsigned int u;
};

static int srchcmp(const void *, const void *);
static char *getnextarg(char *, unsigned);
static void set_all(void);
static void sig_cont(int);
static void long_shuffle(long *, const long);

long mark_idx[2] = { -1, -1 };
long mmrkd[2];
static long prev_mmrk[2] = { -1, -1 };
static wchar_t wcbuf[BUF_SIZE];
static cchar_t ccbuf[BUF_SIZE];
short noic, magic, nows, scale;
short regex_mode;
unsigned short subtree = 3;
static struct str_uint *srchmap;
regex_t re_dat;
static unsigned srch_idx;
unsigned prev_pos[2];
unsigned jmrk[2][32];

bool file_pattern; /* TRUE for -F or -G */
bool excl_or;
bool file_exec;
bool nobold;
static bool sig_loop;
static bool loop_mode;
static bool rnd_mode;

int
test_fkey(
    /* key code */
    int c,
    /* "vi" multiplier number typed before f-key */
    unsigned short num,
    /* cursor position */
    long u)
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

		if (fkey_cmd[fkey_set][i]) {
			struct tool t;
			unsigned ti;
			unsigned act;
			static char *keys =
			    "<ENTER> execute, 'e' edit, 'n' no";

			t = viewtool;
			viewtool.tool = NULL;
			viewtool.args = NULL;
			/* set_tool() reused here to process
			 * embedded "$1" */
			set_tool(&viewtool, strdup(fkey_cmd[fkey_set][i]),
			    /* A rebuild_db() is done, so the list need not
			     * to be updated by the command itself */
			    TOOL_NOLIST |
			    (fkey_flags[fkey_set][i] & FKEY_WAIT ? TOOL_WAIT :
			    0));
			act = num > 1 ? 8 : 9;

			if ((force_exec || (fkey_flags[fkey_set][i] &
			    FKEY_FORCE)) && (force_multi || num <= 1)) {
				goto exec;
			}

			werase(wstat);
			mvwprintw(wstat, 0, 0, "Really execute \"%s\"",
			    fkey_cmd[fkey_set][i]);

			if (mmrkd[right_col]) {
				num = mmrkd[right_col];
			}

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

				if (mmrkd[right_col]) {
					unsigned cu;
					long ti2;
					bool jump = loop_mode && !rnd_mode;
					long *rnd_idx;

					disp_curs(0);
					cu = curs[right_col];
					curs[right_col] = 0;

#if defined(TRACE)
					fprintf(debug, "  test_fkey: ->while"
					    " (get_mmrk())\n");
#endif
					if (rnd_mode) {
						bool tloop = loop_mode;
						long j = 0;

						loop_mode = FALSE;
						rnd_idx = malloc(
						    sizeof(*rnd_idx) *
						    mmrkd[right_col]);

						while ((ti2 =
						    get_mmrk()) >= 0) {
#if defined(TRACE)
	fprintf(debug, "  rnd_idx[%ld]=%ld\n", j, ti2);
#endif
							rnd_idx[j++] = ti2;
						}

						loop_mode = tloop;

						do {
							long_shuffle(rnd_idx,
							    mmrkd[right_col]);

							for (j = 0;
							    j < mmrkd[right_col];
							    j++) {
#if defined(TRACE)
	fprintf(debug, "  rnd_idx[%ld]==%ld\n", j, rnd_idx[j]);
#endif
								top_idx[right_col] =
								    rnd_idx[j];
								action(3, act);
							}
						} while (loop_mode);

						free(rnd_idx);
					} else {
					    while ((ti2 = get_mmrk()) >= 0) {
						long relcp = u - ti2;

						if (jump) {
							if (relcp <= 0 ||
							    relcp >=
							    mmrkd[right_col]) {
								jump = FALSE;
							} else {
								continue;
							}
						}

						top_idx[right_col] = ti2;
						action(3, act);
					    }
					}
#if defined(TRACE)
					fprintf(debug, "  test_fkey: <-while"
					    " (get_mmrk())\n");
#endif

					curs[right_col] = cu;
					goto restore;
				}

#if defined(TRACE)
				fprintf(debug, "  test_fkey: ->%u x \"%s\"\n",
				    num, fkey_cmd[fkey_set][i]);
#endif
				while (num--) {
					action(3, act);
					top_idx[right_col]++; /* kludge */
				}
#if defined(TRACE)
				fprintf(debug, "  test_fkey: <-\"%s\"\n",
				    fkey_cmd[fkey_set][i]);
#endif

restore:
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

		if (fkey_cmd[fkey_set][i]) {
			if (fkey_flags[fkey_set][i] & FKEY_WAIT)
				ed_append("! ");
			else if (fkey_flags[fkey_set][i] & FKEY_FORCE)
				ed_append("# ");
			else
				ed_append("$ ");

			ed_append(fkey_cmd[fkey_set][i]);
		}

		if (ed_dialog(
		    "Type text to be saved for function key:",
		    NULL, NULL, 1, NULL))
			break;

		free(sh_str[fkey_set][i]);
		sh_str[fkey_set][i] = NULL;
		free(fkey_cmd[fkey_set][i]);
		fkey_cmd[fkey_set][i] = NULL;

		if (((ek = *rbuf) == '$' || ek == '!' || ek == '#') &&
		    isspace((int)rbuf[1])) {
			int c2;
			int j = 0;

			while ((c2 = rbuf[++j]) && isspace(c2));

			if (!c2)
				return 1; /* empty input */

			set_fkey_cmd(fkey_set, i, rbuf + j, ek);
			clr_edit();
			printerr(NULL, "%c %s saved for F%d",
			    FKEY_CMD_CHR(i), fkey_cmd[fkey_set][i], i + 1);
		} else {
			sh_str[fkey_set][i] = linebuf;
			linebuf = NULL; /* clr_edit(): free(linebuf) */
			clr_edit();
			printerr(NULL, "%ls"
			    " saved for F%d", sh_str[fkey_set][i], i + 1);
		}
		return 1;
	}

	return 0;
}

void
set_fkey_cmd(
    int j, /* set */
    int i, /* fkey number */
    char *s, int ek)
{
#if defined(TRACE)
	fprintf(debug, "<>set_fkey_cmd(%d '%c' \"%s\"\n", i, ek, s);
#endif
	fkey_cmd[j][i] = strdup(s);
	fkey_flags[j][i]
	    = ek == '!' ? FKEY_WAIT  : /* wait after command */
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

	if (regex_mode)
		clr_regex();

	ed_dialog("Type first characters of filename (<ESC> to cancel):",
	    "" /* remove existing */, srch_file, 0, NULL);
	free(srchmap);

	if (!regex_mode)
		return;

	if (ed_dialog(enter_regex_txt,
	    "" /* remove existing */, NULL, 0, &regex_hist) ||
	    !*rbuf)
		regex_mode = 0;
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
srch_file(char *pattern, int c)
{
	unsigned idx;
	int o, oo;
	size_t l;
	char *s;

	if (c || !*pattern || !db_num[right_col])
		return 0;

	if (*pattern == '/' && !pattern[1]) {
		regex_mode = 1;
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

void
clr_regex(void)
{
	regex_mode = 0;
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
		regex_mode = 0;
		return;
	}

	if (magic)
		fl |= REG_EXTENDED;
	if (!noic)
		fl |= REG_ICASE;

	if (regcomp(&re_dat, pattern, fl)) {
		printerr(strerror(errno), "regcomp \"%s\"", pattern);
		regex_mode = 0;
		return;
	}

	if (!regex_srch(0))
		disp_regex();
	else
		printerr(NULL, no_match_txt);
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
			printerr(NULL, no_match_txt);
			return 1;
		}
	}

	return 0;

no_match:
	printerr(NULL, no_match_txt);
	return 0;
}

/* 1: exit vddiff */

int
parsopt(char *buf)
{
	const char dmode_cd_txt[] = "cd not supported in diff mode";
	char *bufend;
	short not;
	short skip;
	bool need_arg;
	bool next_arg;

#if defined(TRACE)
	fprintf(debug, "<->parsopt(%s)\n", buf);
#endif
	bufend = buf + strlen(buf);

	while (1) {
		if (bufend == buf) {
			return 0;
		}

		if (*--bufend != ' ') {
			break;
		}

		*bufend = 0;
	}

	if (*buf == '!') {
		buf++;
		exec_cmd(&buf, TOOL_WAIT | TOOL_NOLIST |
		    TOOL_TTY | TOOL_SHELL, NULL, NULL);
		rebuild_db(0);
	}

	if (!strcmp(buf, "cd")) {
		char *s;

		if (!(bmode || fmode)) {
			printerr(NULL, dmode_cd_txt);
			return 0;
		}

cd_home:
		if (!(s = getenv("HOME"))) {
			printerr(NULL, "HOME not set\n");
			return 0;
		}

		enter_dir(s, NULL, FALSE, FALSE, 0 LOCVAR);
		return 0;
	}

	if (!strncmp(buf, "cd ", 3)) {
		char *s;

		if (!(bmode || fmode)) {
			printerr(NULL, dmode_cd_txt);
			return 0;
		}

		if (!(buf = getnextarg(buf + 3, 0))) {
			goto cd_home;
		}

		if (!(s = pthexp(buf))) {
			return 0;
		}

		enter_dir(s, NULL, FALSE, FALSE, 0 LOCVAR);
		free(s);
		return 0;
	}

	if (*buf == 'e' && (!buf[1] || !strcmp(buf+1, "dit"))) {
		readonly = FALSE;
		nofkeys = FALSE;
		return 0;
	}

	if (!strncmp(buf, "find ", 5)) {
		if (!(buf = getnextarg(buf + 5, 1))) {
			return 0;
		}

		if (!fn_init(buf)) {
			free_scan_db(TRUE); /* sets {one_scan} */
			rebuild_db(1);
		}

		return 0;
	}

	if (!strncmp(buf, "grep ", 5)) {
		if (!(buf = getnextarg(buf + 5, 1))) {
			return 0;
		}

		if (!gq_init(buf)) {
			free_scan_db(TRUE); /* sets {one_scan} */
			rebuild_db(1);
		}

		return 0;
	}

	if (!strcmp(buf, "marks")) {
		list_jmrks();
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

	if (strncmp(buf, "se " , (skip = 3)) &&
	    strncmp(buf, "set ", (skip = 4))) {
		goto unkn_opt;
	}

	need_arg = TRUE;
next_opt:
	if (!(buf = getnextarg(buf + skip, need_arg ? 1 : 0))) {
		return 0;
	}

	need_arg = FALSE;
	skip = 0;
	next_arg = FALSE;
#if defined(TRACE)
	fprintf(debug, "  set arg \"%s\"\n", buf);
#endif

	if (!strcmp(buf, "all")) {
		set_all();
		return 0;
	}

	if (!strncmp(buf, "no", 2)) {
		buf += 2;
		not = 1;
	} else {
		not = 0;
	}

	if (!strcmp(buf, "file_exec") ||
	    (!strncmp(buf, "file_exec ", (skip = 10)) &&
	    (next_arg = TRUE))) {
		file_exec = not ? FALSE : TRUE;
	} else if (!strcmp(buf, "fkeys") ||
	    (!strncmp(buf, "fkeys ", (skip = 6)) &&
	    (next_arg = TRUE))) {
		nofkeys = not ? TRUE : FALSE;
	} else if (!strcmp(buf, "ic") ||
	    (!strncmp(buf, "ic ", (skip = 3)) &&
	    (next_arg = TRUE))) {
		noic = not;
	} else if (!strcmp(buf, "loop") ||
	    (!strncmp(buf, "loop ", (skip = 5)) &&
	    (next_arg = TRUE))) {
		if (!sig_loop) {
			inst_sighdl(SIGCONT, sig_cont);
			sig_loop = TRUE;
		}

		loop_mode = not ? FALSE : TRUE;
	} else if (!strcmp(buf, "random") ||
	    (!strncmp(buf, "random ", (skip = 7)) &&
	    (next_arg = TRUE))) {
		rnd_mode = not ? FALSE : TRUE;
	} else if (!strcmp(buf, "magic")) {
		magic = not ? 0 : 1;
	} else if (!strcmp(buf, "recursive")) {
		recursive = not ? 0 : 1;
	} else if (!strcmp(buf, "ws")) {
		nows = not;
	} else if (*buf) {
unkn_opt:
		printerr(NULL, "Unknown option \"%s\"", buf);
	}

	if (next_arg) {
		goto next_opt;
	}

	return 0;
}

static char *
getnextarg(char *buf, unsigned m)
{
	while (*buf == ' ') {
		buf++;
	}

	if (!*buf) {
		if (m) {
			printerr(NULL, "Argument expected");
		}

		return NULL;
	}

	return buf;
}

static void
set_all(void)
{
	static char nofile_exec_str[] = "nofile_exec\n";
	static char nofkeys_str[]     = "nofkeys\n";
	static char noic_str[]        = "noic\n";
	static char noloop_str[]      = "noloop\n";
	static char nomagic_str[]     = "nomagic\n";
	static char norandom_str[]    = "norandom\n";
	static char norecurs_str[]    = "norecursive\n";
	static char nows_str[]        = "nows\n";

	werase(wlist);
	wattrset(wlist, A_NORMAL);
	wmove(wlist, 0, 0);
	waddstr(wlist, file_exec ? nofile_exec_str + 2 : nofile_exec_str);
	waddstr(wlist, nofkeys ? nofkeys_str : nofkeys_str + 2);
	waddstr(wlist, noic ? noic_str : noic_str + 2);
	waddstr(wlist, loop_mode ? noloop_str + 2 : noloop_str);
	waddstr(wlist, magic ? nomagic_str + 2 : nomagic_str);
	waddstr(wlist, rnd_mode ? norandom_str + 2 : norandom_str);
	waddstr(wlist, recursive ? norecurs_str + 2 : norecurs_str);
	waddstr(wlist, nows ? nows_str : nows_str + 2);

	if (anykey() == ':') {
		ungetch(':');
	}
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
	ml = m->type[0] && (f->type[1] || !m->type[1]);

	if ((z1 = unpack(m, ml ? 1 : 2, &t1, 0))) {
		m = z1;
	}

	/* check if other file needs to be unzipped */
	if ((z2 = unpack(f, f->type[1] ? 2 : 1, &t2, 0))) {
		f = z2;
	}

	if (ml) {
		olnam =
		lnam = m->name ? m->name   :
		       bmode   ? gl_mark   :
		                 mark_lnam ;

		if (*lnam != '/') {
			pthcat(syspth[0], pthlen[0], lnam);
			lnam = syspth[0];
		}

		if (chk_mark(lnam, 0))
			goto ret;

		ltyp = m->type[0];
		lsiz = m->siz[0];
		ornam = rnam = f->name;

		if (f->type[1]) {
			rtyp = f->type[1];
			rsiz = f->siz[1];

			if (*rnam != '/') {
				pthcat(syspth[1], pthlen[1], rnam);
				rnam = syspth[1];
			}
		} else {
			rtyp = f->type[0];
			rsiz = f->siz[0];

			if (*rnam != '/') {
				pthcat(syspth[0], pthlen[0], rnam);
				rnam = syspth[0];
			}
		}

	} else if (m->type[1]) {
		olnam = lnam = f->name;

		if (f->type[0]) {
			ltyp = f->type[0];
			lsiz = f->siz[0];

			if (*lnam != '/') {
				pthcat(syspth[0], pthlen[0], lnam);
				lnam = syspth[0];
			}
		} else {
			ltyp = f->type[1];
			lsiz = f->siz[1];

			if (*lnam != '/') {
				pthcat(syspth[1], pthlen[1], lnam);
				lnam = syspth[1];
			}
		}

		ornam =
		rnam = m->name ? m->name   :
		       bmode   ? gl_mark   :
		                 mark_rnam ;

		if (*rnam != '/') {
			pthcat(syspth[1], pthlen[1], rnam);
			rnam = syspth[1];
		}

		if (chk_mark(rnam, 0))
			goto ret;

		rtyp = m->type[1];
		rsiz = m->siz[1];
	} else {
		goto ret;
	}

	if (!S_ISREG(ltyp) || !S_ISREG(rtyp)) {
		printerr(NULL,
		    "Binary diff can only be used for regular files");
		goto ret;
	}

	printerr(NULL, "Comparing %s and %s", olnam, ornam);
	val = cmp_file(lnam, lsiz, rnam, rsiz, 1);

ret:
	switch (val) {
	case 0:
		printerr(any_txt, "Equal: %s and %s",
#if defined(DEBUG) && 0
		    lnam, rnam);
		(void)olnam;
		(void)ornam;
#else
		    olnam, ornam);
#endif
		break;
	case 1:
		printerr(any_txt, "Different: %s and %s",
#if defined(DEBUG) && 0
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
	TRCPTH;
	fprintf(debug, "<>chk_mark(%s,%d) lp(%s) rp(%s)\n",
	    file, tree, trcpth[0], trcpth[1]);
#endif
	rp = tree && /* f-key command */
	    !bmode && !strchr(file, '/');

	if (rp) {
		if (tree & 1) {
			pthcat(syspth[0], pthlen[0], file);
			file = syspth[0];
		} else if (tree & 2) {
			pthcat(syspth[1], pthlen[1], file);
			file = syspth[1];
		}
	}

	i = stat(file, &st);

	if (i == -1 && errno != ENOENT) {
		printerr(strerror(errno), LOCFMT "stat \"%s\"" LOCVAR, file);
	}

	if (rp) {
		if (tree & 1) {
			syspth[0][pthlen[0]] = 0;
		} else if (tree & 2) {
			syspth[1][pthlen[1]] = 0;
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

/* 1: Cursor moved down */

int
key_mmrk(void)
{
	long i1, i2;
	int r = 0;

	if ((i1 = mark_idx[right_col]) < 0) {
		/* mark_idx < 0 -> no mark set.
		 * -> Just apply to current line. */
		tgl_mmrk(DB_LST_ITM);

		if (curs_down() != 1) {
			r = 1;
		}
	} else {
		/* Mark set. -> Apply to range */
		i2 = DB_LST_IDX;

		if (i1 > i2) {
			i1 = i2;
			i2 = mark_idx[right_col];
		}

		for (; i1 <= i2; i1++) {
			tgl_mmrk(db_list[right_col][i1]);
		}

		clr_mark();
		disp_list(1);
	}

	return r;
}

void
tgl_mmrk(struct filediff *f)
{
	if (f->fl & FDFL_MMRK) {
		f->fl &= ~FDFL_MMRK;
		mmrkd[right_col]--;
	} else {
		f->fl |= FDFL_MMRK;
		mmrkd[right_col]++;
	}
}

long
get_mmrk(void)
{
start:
	while (++prev_mmrk[right_col] < (long)db_num[right_col]) {
		if ((db_list[right_col][prev_mmrk[right_col]])->fl &
		    FDFL_MMRK) {
			goto ret;
		}
	}

	prev_mmrk[right_col] = -1;

	if (loop_mode) {
		goto start;
	}
ret:
#if defined(TRACE)
	fprintf(debug, "<>get_mmrk(%d): %ld\n", right_col,
	    prev_mmrk[right_col]);
#endif
	return prev_mmrk[right_col];
}

void
mmrktobot(void)
{
	unsigned long i;

	for (i = DB_LST_IDX; i < db_num[right_col]; i++) {
		tgl_mmrk(db_list[right_col][i]);
	}

	disp_list(1);
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
		if (excl_or) {
			mvwaddch(wstat, 0, x--, '^');
		}

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
	if (color) {
		wattrset(w, COLOR_PAIR(PAIR_MARK));
	} else {
		wattrset(w, A_BOLD | A_UNDERLINE);
	}
}

void
mmrkc(WINDOW *w)
{
	if (color) {
		wattrset(w, COLOR_PAIR(PAIR_MMRK));
	} else {
		wattrset(w, A_UNDERLINE);
	}
}

void
chgat_mark(WINDOW *w, int y)
{
	mvwchgat(w, y, 0, -1,
	    color ? 0 : A_BOLD | A_UNDERLINE,
	    color ? PAIR_MARK : 0, NULL);
}

void
chgat_mmrk(WINDOW *w, int y)
{
	mvwchgat(w, y, 0, -1,
	    color ? 0 : A_UNDERLINE,
	    color ? PAIR_MMRK : 0, NULL);
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
	mvwchgat(w, y, 0, -1, get_curs_attr(), get_curs_pair(), NULL);
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
	mvwchgat(w, y, 0, -1, get_off_attr(), get_off_pair(), NULL);
}

attr_t
get_curs_attr(void)
{
	return color ? 0 : A_STANDOUT;
}

attr_t
get_off_attr(void)
{
	return 0;
}

short
get_curs_pair(void)
{
	return color ? PAIR_CURSOR : 0;
}

short
get_off_pair(void)
{
	return color ? PAIR_NORMAL : 0;
}

int
anykey(void)
{
	int c;

	wrefresh(wlist);
	printerr(NULL, any_txt);
	c = getch();
	disp_fmode();
	return c;
}

void
free_zdir(struct filediff *z, char *t)
{
#if defined(TRACE)
	fprintf(debug, "<>free_zdir(z->name=%s, t=%s)\n", z->name, t);
#endif
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

/* Number of wide chars */
ssize_t
mbstowchs(WINDOW *w, char *s)
{
	size_t l;

	if (!s) {
		s = "(NULL)";
	}

	l = mbstowcs(wcbuf, s, sizeof(wcbuf)/sizeof(*wcbuf));

	if (l == (size_t)-1) {
		/* Don't use printerr() here, it would use mbstowcs() again */
		werase(wstat);
		mvwprintw(wstat, 0, 0, "\"%s\":", s);
		mvwprintw(wstat, 1, 0, "mbstowcs(3) failed", s);
		wrefresh(wstat);
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

/* Number of wide chars */
ssize_t
putmbs(WINDOW *w, char *s,
    /* -1: unlimited */
    int n)
{
	ssize_t l;

	l = mbstowchs(w, s);

	/* Because of bug in older ncurses versions */
	if (n > l) {
		n = l;
	}

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
		return -2; /* Not really an error */

	if ((l = putmbs(w, s, n)) == -1)
		return -1;

	cx += l;

	if (cx > mx)
		cx = mx;

	wmove(w, cy, cx);
	return 0;
}

/* Number of wide chars */

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
		return -2; /* Not really an error */

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
	    | BUTTON3_CLICKED | BUTTON3_PRESSED
# if NCURSES_MOUSE_VERSION >= 2
	    | BUTTON4_PRESSED | BUTTON5_PRESSED
# endif
	    , NULL);
#endif
}

int
ui_cp(int t, long u, unsigned short num, unsigned md)
{
	unsigned sto;

	if (mmrkd[right_col]) {
		if (dialog(y_n_txt, NULL,
		    "Really copy %d files?",
		    mmrkd[right_col]) != 'y') {
			return 1;
		}

		while ((u = get_mmrk()) >= 0) {
			fs_cp(t, u, 1, md | 5, &sto);
		}

		rebuild_db(
		    !fmode || !sto || sto == 3 || (md & (16 | 32))
		    ? 0 : sto << 1);
		return 1;
	}

	fs_cp(t, u, num, md, NULL);
	return 0;
}

/* Only called on 'T' */

int
ui_mv(int dst, long u, unsigned short num)
{
	{
		bool same_dir;

		syspth[0][pthlen[0]] = 0;
		syspth[1][pthlen[1]] = 0;
		same_dir = !strcmp(syspth[0], syspth[1]);

		if (bmode || same_dir) {
			int multi = mmrkd[right_col];
			ui_rename(3, u, num);
			return multi;
		}
	}

	if (mmrkd[right_col]) {
		int fs_retval_ = 0;

		if (dialog(y_n_txt, NULL,
		    "Really move %d files?",
		    mmrkd[right_col]) != 'y') {
			return 1;
		}

		while ((u = get_mmrk()) >= 0 &&
		    !((fs_retval_ = fs_cp(dst, u, 1,
		    16 /* move */ | 5 | (fs_retval_ & 2 ? 0x40 : 0), NULL))
		    /* Don't break loop on "ignore error" */
		    & ~2))
		{
		}

		rebuild_db(0);
		return 1;
	}

	fs_cp(dst, u, num, 16 /* move */, NULL);
	return 0;
}

int
ui_dd(int t, long u, unsigned short num)
{
	if (mmrkd[right_col]) {
		int fs_retval_ = 0;

		if (dialog(y_n_txt, NULL,
		    "Really delete %d files?",
		    mmrkd[right_col]) != 'y') {
			return 1;
		}

		while ((u = get_mmrk()) >= 0 &&
		    !((fs_retval_ = fs_rm(t, NULL, NULL, u, 1,
		    (fs_retval_ & 4 ? 8 : 0) | 3)) & ~4))
		{
		}

		rebuild_db(0);
		return 1;
	}

	fs_rm(t, NULL, NULL, u, num, 0);
	return 0;
}

int
ui_rename(int t, long u, unsigned short num)
{
	int rv = 1;
#if defined(TRACE)
	fprintf(debug, "->ui_rename\n");
#endif
	if (mmrkd[right_col]) {
		if (dialog(y_n_txt, NULL,
		    "Really rename %d files?",
		    mmrkd[right_col]) != 'y') {
			goto ret;
		}

		while ((u = get_mmrk()) >= 0) {
#if defined(TRACE)
			fprintf(debug, "  u=%ld\n", u);
#endif
			fs_rename(t, u, 1, 3);
		}

		rebuild_db(0);
		goto ret;
	}

	fs_rename(t, u, num, 0);
	rv = 0;
ret:
#if defined(TRACE)
	fprintf(debug, "<-ui_rename\n");
#endif
	return rv;
}

int
ui_chmod(int t, long u, unsigned short num)
{
	unsigned i;

	if (mmrkd[right_col]) {
		if (dialog(y_n_txt, NULL,
		    "Really change permissions of %d files?",
		    mmrkd[right_col]) != 'y') {
			return 1;
		}

		for (i = 0; (u = get_mmrk()) >= 0; i = 4) {
			fs_chmod(t, u, 1, i | 3);
		}

		rebuild_db(0);
		return 1;
	}

	fs_chmod(t, u, num, 0);
	return 0;
}

int
ui_chown(int t, int op, long u, unsigned short num)
{
	unsigned i;

	if (mmrkd[right_col]) {
		if (dialog(y_n_txt, NULL,
		    "Really change %s of %d files?",
		    op ? "group" : "owner",
		    mmrkd[right_col]) != 'y') {
			return 1;
		}

		for (i = 0; (u = get_mmrk()) >= 0; i = 4) {
			fs_chown(t, op, u, 1, i | 3);
		}

		rebuild_db(0);
		return 1;
	}

	fs_chown(t, op, u, num, 0);
	return 0;
}

void
prt_ln_num(void)
{
	if (db_num[right_col]) {
		printerr(NULL, "File %lu of %lu",
		    DB_LST_IDX + 1, db_num[right_col]);
	} else {
		printerr(NULL, "No file");
	}
}

void
list_jmrks(void)
{
	int c;
	unsigned i, y, u;

	werase(wlist);
	werase(wstat);

	for (i = 0, y = 0; i < 32 && y < listh; i++) {
		if (!(u = jmrk[right_col][i]) || u >= db_num[right_col]) {
			continue;
		}

		mvwprintw(wlist, y, 0, "%u", i);
		wmove(wlist, y, 4);
		addmbs(wlist, db_list[right_col][u]->name, 0);
		y++;
	}

	wrefresh(wlist);

	if ((c = getch()) >= '0' && c <= '9') {
		ungetch(c);
	}

	disp_fmode();
}

static void
sig_cont(int sig)
{
	(void)sig;
#if defined(TRACE)
	fprintf(debug, "<>sig_cont\n");
#endif
	loop_mode = FALSE;
}

static void
long_shuffle(long *array, const long n_elem)
{
	long i;

	for (i = n_elem - 1; i >= 1; i--) {
		long j = random() % (i + 1);
		long t = array[j];

		array[j] = array[i];
		array[i] = t;
	}
}
