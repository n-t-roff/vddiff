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
#include <ctype.h>
#include <stdlib.h>
#include <regex.h>
#include <stdarg.h>
#include <signal.h>
#include "compat.h"
#include <wctype.h>
#include "ui.h"
#include "main.h"
#include "exec.h"
#include "ed.h"
#include "ui2.h"

#define LINESIZ (sizeof rbuf)

static void init_edit(void);
static int edit_line(int (*)(char *), struct history *);
static void linebuf_delch(void);
static void linebuf_insch(unsigned);
static void overful_del(void);
static void hist_add(struct history *);
static void hist_up(struct history *);
static void hist_down(struct history *);

short edit;
unsigned histsize = 100;

unsigned linelen;
static unsigned linepos, leftpos;

wchar_t *linebuf;
static wchar_t ws[2];
static cchar_t cc;

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(void);
#endif

void
ed_append(char *txt)
{
	size_t l;

	if (!edit)
		init_edit();

	l = strlen(txt);

	if (linelen + l >= LINESIZ) {
		printerr(NULL, "Line buffer overflow");
		return;
	}

	l = mbstowcs(linebuf + linelen, txt, l + 1);
	linelen += l;
	linepos = linelen;

	if (linelen + 1 > statw)
		leftpos = linelen + 1 - statw;
}

static void
init_edit(void)
{
	if (edit)
		clr_edit();

	edit = 1;
	linebuf = malloc(LINESIZ * sizeof(*linebuf));
	*linebuf = 0;
	linelen = linepos = leftpos = 0;
	*lbuf = 0;
	curs_set(1);
}

void
clr_edit(void)
{
	edit = 0;
	free(linebuf);
	curs_set(0);
	werase(wstat);
	filt_stat();
	wrefresh(wstat);
}

void
disp_edit(void)
{
	if (color)
		wattrset(wstat, COLOR_PAIR(PAIR_NORMAL));
	else
		wstandend(wstat);

	werase(wstat);
	mvwprintw(wstat, 0, 0, "%s", lbuf);
	filt_stat();
	wmove(wstat, 1, 0);
	putwcs(wstat, linebuf + leftpos, -1);
	wmove(wstat, 1, linepos - leftpos);
	wrefresh(wstat);
}

void
set_fkey(int i, char *s)
{
	int ek;
	size_t l;

	if (i < 1 || i > FKEY_NUM) {
		printf("Function key number must be in range 1-%d\n",
		    FKEY_NUM);
		exit(1);
	}

	i--; /* key 1-12, storage 0-11 */
	free(sh_str[i]);
	sh_str[i] = NULL;
	free(fkey_cmd[i]);
	fkey_cmd[i] = NULL;

	if (((ek = *s) == '$' || ek == '!' || ek == '#') &&
	    isspace((int)s[1])) {
		int c;
		char *p = s;

		while ((c = *++p) && isspace(c));

		if (!c)
			goto free; /* empty input */

		set_fkey_cmd(i, p, ek);

free:
		free(s);
		return;
	}

	/* wcslen(wcs) should be <= strlen(mbs) */
	l = strlen(s) + 1;
	sh_str[i] = malloc(l * sizeof(wchar_t));
	mbstowcs(sh_str[i], s, l);
	free(s);
}

int
ed_dialog(const char *msg,
    /* NULL: leave buffer as-is */
    char *ini, int (*callback)(char *), int keep_buf, struct history *hist)
{
	if (!edit)
		init_edit(); /* conditional, else rbuf is cleared! */

	memcpy(lbuf, msg, strlen(msg) + 1);

	if (ini) {
		linelen = 0; /* remove any existing text */
		ed_append(ini);
	}

	disp_edit();

	if (edit_line(callback, hist)) {
		clr_edit();
		return 1;
	}

	/* both rbuf and linebuf are used by ui.c */
	wcstombs(rbuf, linebuf, sizeof rbuf);

	if (!keep_buf)
		clr_edit();

	if (hist) {
		if (hist->have_ent) {
			hist->have_ent = 0;
			free(hist->top->line);
			hist->top->line = strdup(rbuf);
		} else
			hist_add(hist);
	}

	return 0;
}

static void
hist_add(struct history *hist)
{
	struct hist_ent *p;

	if (histsize < 2)
		return;

	if (hist->len < histsize)
		hist->len++;
	else {
		if ((p = hist->bot->prev))
			p->next = NULL;

		free(hist->bot);
		hist->bot = p;
	}

	p = malloc(sizeof(struct hist_ent));
	p->line = strdup(rbuf);
	p->prev = NULL;

	if ((p->next = hist->top))
		hist->top->prev = p;
	else
		hist->bot = p;

	hist->top = p;
}

static int
edit_line(int (*callback)(char *), struct history *hist)
{
	wint_t c;
	int i;
	size_t l;

	while (1) {
next_key:
		get_wch(&c);

		for (i = 0; i < FKEY_NUM; i++) {
			if (c != (wint_t)KEY_F(i + 1))
				continue;

			if (!sh_str[i])
				goto next_key;
			l = wcslen(sh_str[i]);
			if (linelen + l >= LINESIZ) {
				printerr(NULL, "Line buffer overflow");
				goto next_key;
			}

			linebuf_insch(l);
			wmemcpy(linebuf + linepos, sh_str[i], l);
			linepos += l;
			disp_edit();
			goto next_key;
		}

		switch (c) {
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			proc_mevent();
			break;
#endif
		case 27:
			return 1;
		case '\n':
			return 0;

		case KEY_HOME:
			if (leftpos) {
				leftpos = 0;
				wmove(wstat, 1, 0);
				putwcs(wstat, linebuf, -1);
			} else if (linepos) {
				wmove(wstat, 1, 0);
			} else {
				break;
			}

			linepos = 0;
			wrefresh(wstat);
			break;

		case KEY_LEFT:
			if (linepos - leftpos) {
				wmove(wstat, 1, --linepos - leftpos);
			} else if (leftpos) {
				--leftpos;
				*ws = linebuf[--linepos];
				setcchar(&cc, ws, 0, 0, NULL);
				wins_wch(wstat, &cc);
			} else {
				break;
			}

			wrefresh(wstat);
			break;

		case KEY_END:
			if (linepos == linelen) {
				break;
			}

			linepos = linelen;

			if (linepos - leftpos < statw - 1) {
				wmove(wstat, 1, linepos - leftpos);
			} else {
				leftpos = linepos - statw + 1;
				wmove(wstat, 1, 0);
				wclrtoeol(wstat);
				putwcs(wstat, linebuf + leftpos, -1);
				wmove(wstat, 1, statw - 1);
			}

			wrefresh(wstat);
			break;

		case KEY_RIGHT:
			if (linepos == linelen) {
				break;
			}

			if (linepos - leftpos < statw - 1) {
				wmove(wstat, 1, ++linepos - leftpos);
			} else {
				wmove(wstat, 1, 0);
				wdelch(wstat);
				wmove(wstat, 1, statw - 1);
				++leftpos;
				*ws = linebuf[++linepos];
				setcchar(&cc, ws, 0, 0, NULL);
				wins_wch(wstat, &cc);
			}

			wrefresh(wstat);
			break;

		/* Delete char left from cursor.
		 * Short string: Shifting right part of string to left.
		 * Long string: Shifting left part to the right. This has
		 *   the advantage, that most information is kept in the line
		 *   and it is seen what will be deleted. */

		case KEY_BACKSPACE:
		case CERASE:
backspace:
			if (!linepos)
				break;

			linepos--;
			wmove(wstat, 1, linepos - leftpos);
			goto del_char;

		case KEY_DC:
			if (linepos == linelen)
				break;

del_char:
			linebuf_delch();
			wdelch(wstat);

			if (leftpos) {
				overful_del();
			} else if (linelen >= statw) {
				wmove(wstat, 1, statw - 1);
				*ws = linebuf[statw - 1];
				setcchar(&cc, ws, 0, 0, NULL);
				wins_wch(wstat, &cc);
				wmove(wstat, 1, linepos - leftpos);
			}

			wrefresh(wstat);

			if (callback) {
				wcstombs(rbuf, linebuf, sizeof rbuf);
				callback(rbuf);
			}

			break;

		case KEY_UP:
			if (hist)
				hist_up(hist);
			break;
		case KEY_DOWN:
			if (hist)
				hist_down(hist);
			break;
		default:
			if (linelen + 1 >= LINESIZ)
				break;

			/* If cursor is at right window border,
			 * shift whole line to the left one char. */

			if (linepos - leftpos == statw - 1) {
				wmove(wstat, 1, 0);
				wdelch(wstat);
				leftpos++;
				wmove(wstat, 1, linepos - leftpos);
			}

			if (linepos == linelen) {
				linelen++;
				linebuf[linepos + 1] = 0;
			} else {
				linebuf_insch(1);
			}

			linebuf[linepos++] = c;

			*ws = c;
			setcchar(&cc, ws, 0, 0, NULL);

			if (linepos == linelen) {
				wadd_wch(wstat, &cc);
			} else {
				wins_wch(wstat, &cc);
				wmove(wstat, 1, linepos - leftpos);
			}

			wrefresh(wstat);

			if (callback) {
				int i2;

				wcstombs(rbuf, linebuf, sizeof rbuf);
				i2 = callback(rbuf);

				if (i2 & EDCB_FAIL)
					return 1;

				if (i2 & EDCB_RM_CB)
					callback = NULL;

				if (i2 & EDCB_IGN)
					goto backspace;
			}
		}
	}
}

static void
overful_del(void)
{
	wmove(wstat, 1, 0);
	*ws = linebuf[--leftpos];
	setcchar(&cc, ws, 0, 0, NULL);
	wins_wch(wstat, &cc);
	wmove(wstat, 1, linepos - leftpos);
}

static void
linebuf_delch(void)
{
	unsigned i, j;

	for (i = j = linepos; i < linelen; i = j)
		linebuf[i] = linebuf[++j];

	linelen--;
}

static void
linebuf_insch(unsigned num)
{
	int i, j;

	linelen += num;

	for (j = linelen, i = j - num; i >= (int)linepos; i--, j--)
		linebuf[j] = linebuf[i];
}

#ifdef NCURSES_MOUSE_VERSION
static void
proc_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED ||
	    mevent.bstate & BUTTON1_PRESSED) {
		if (mevent.y != LINES - 1)
			return;

		linepos = leftpos + mevent.x;
		wmove(wstat, 1, linepos - leftpos);
		wrefresh(wstat);
	}
}
#endif

static void
hist_up(struct history *hist)
{
	if (!histsize || !hist->top || (hist->have_ent && !hist->ent->next))
		return;

	if (!hist->have_ent || !hist->ent->prev)
		wcstombs(rbuf, linebuf, sizeof rbuf);

	if (!hist->have_ent) {
		hist->have_ent = 1;
		hist_add(hist);
		hist->ent = hist->top;
	} else if (!hist->ent->prev) {
		free(hist->top->line);
		hist->top->line = strdup(rbuf);
	}

	hist->ent = hist->ent->next;
	*linebuf = 0;
	linelen = 0;
	ed_append(hist->ent->line);
	disp_edit();
}

static void
hist_down(struct history *hist)
{
	if (!histsize || !hist->have_ent || !hist->ent->prev)
		return;

	hist->ent = hist->ent->prev;
	*linebuf = 0;
	linelen = 0;
	ed_append(hist->ent->line);
	disp_edit();
}
