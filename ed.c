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

#ifdef HAVE_NCURSESW_CURSES_H
# include <wctype.h>
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "ui.h"
#include "main.h"
#include "exec.h"

static void init_edit(void);
static int edit_line(void);
static void linebuf_delch(void);

short edit;

static unsigned linelen, linepos, leftpos;

#ifdef HAVE_NCURSESW_CURSES_H
static wchar_t *linebuf;
#else
static char *linebuf;
#endif

void
ed_append(char *txt)
{
	size_t l;

	if (!edit)
		init_edit();

	l = strlen(txt);

	if (linelen + l >= sizeof rbuf) {
		printerr(NULL, "Line buffer overflow");
		return;
	}

#ifdef HAVE_NCURSESW_CURSES_H
	mbstowcs(linebuf + linelen, txt, l);
#else
	memcpy(linebuf + linelen, txt, l);
#endif
	linelen += l;
	linepos = linelen;
}

static void
init_edit(void)
{
	edit = 1;
#ifdef HAVE_NCURSESW_CURSES_H
	linebuf = malloc(BUFSIZ * sizeof(*linebuf));
#else
	linebuf = rbuf;
#endif
	*linebuf = 0;
	linelen = linepos = leftpos = 0;
	*lbuf = 0;
	curs_set(1);
}

void
clr_edit(void)
{
	edit = 0;
#ifdef HAVE_NCURSESW_CURSES_H
	free(linebuf);
#endif
	curs_set(0);
	werase(wstat);
	wrefresh(wstat);
}

void
disp_edit(void)
{
	werase(wstat);
	mvwaddstr(wstat, 0, 0, lbuf);
#ifdef HAVE_NCURSESW_CURSES_H
	mvwaddwstr(wstat, 1, 0, linebuf);
#else
	mvwaddstr(wstat, 1, 0, linebuf);
#endif
	wrefresh(wstat);
}

void
enter_cmd(void)
{
	if (!edit)
		init_edit();

	snprintf(lbuf, sizeof lbuf,
       "Type command, <ENTER> to execute, <ESC> to cancel:");
	disp_edit();

	if (edit_line()) {
		clr_edit();
		return;
	}

#ifdef HAVE_NCURSESW_CURSES_H
	wcstombs(rbuf, linebuf, sizeof rbuf);
#endif
	clr_edit();
	sh_cmd(rbuf, 1);
}

static int
edit_line(void)
{
#ifdef HAVE_NCURSESW_CURSES_H
	wchar_t ws[] = { L'\0', L'\0' };
	wint_t c;
	cchar_t cc;
#else
	int c;
#endif

	while (1) {
#ifdef HAVE_NCURSESW_CURSES_H
		get_wch(&c);
#else
		c = getch();
#endif
		switch (c) {
		case 27:
			return 1;
		case '\n':
			return 0;
		case KEY_LEFT:
			if (!linepos)
				break;

			wmove(wstat, 1, --linepos - leftpos);
			wrefresh(wstat);
			break;
		case KEY_RIGHT:
			if (linepos == linelen)
				break;

			wmove(wstat, 1, ++linepos - leftpos);
			wrefresh(wstat);
			break;
		case KEY_BACKSPACE:
			if (!linepos)
				break;

			linepos--;
			linebuf_delch();
			wmove(wstat, 1, linepos - leftpos);
			wdelch(wstat);
			wrefresh(wstat);
			linelen--;
			break;
		case KEY_DC:
			if (linepos == linelen)
				break;

			if (linebuf[linepos])
				linebuf_delch();

			wdelch(wstat);
			wrefresh(wstat);
			linelen--;
			break;
		default:
			if (linelen >= statw - 1) {
				wmove(wstat, 1, 0);
				wdelch(wstat);
				wmove(wstat, 1, linepos - ++leftpos);
			}

			linebuf[linepos++] = c;

			if (++linelen == linepos)
				linebuf[linepos] = 0;
#ifdef HAVE_NCURSESW_CURSES_H
			*ws = c;
			setcchar(&cc, ws, 0, 0, NULL);
#endif

			if (linepos == linelen) {
#ifdef HAVE_NCURSESW_CURSES_H
				wadd_wch(wstat, &cc);
#else
				waddch(wstat, c);
#endif
			} else {
#ifdef HAVE_NCURSESW_CURSES_H
				wins_wch(wstat, &cc);
#else
				winschr(wstat, c);
#endif
				wmove(wstat, 1, linepos - leftpos);
			}

			wrefresh(wstat);
		}
	}
}

static void
linebuf_delch(void)
{
	unsigned i, j;

	for (i = j = linepos; i <= linelen; i = j)
		linebuf[i] = linebuf[++j];
}
