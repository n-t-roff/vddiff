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

#define LINESIZ (sizeof rbuf)

static void init_edit(void);
static int edit_line(void (*)(char *));
static void linebuf_delch(void);
static void linebuf_insch(void);
static void overful_del(void);

short edit;

static unsigned linelen, linepos, leftpos;

#ifdef HAVE_NCURSESW_CURSES_H
static wchar_t *linebuf;
static wchar_t ws[2];
static cchar_t cc;
#else
static char *linebuf;
#endif

#ifdef NCURSES_MOUSE_VERSION
static void proc_mevent(void);

static MEVENT mevent;
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

#ifdef HAVE_NCURSESW_CURSES_H
	mbstowcs(linebuf + linelen, txt, l + 1);
#else
	memcpy(linebuf + linelen, txt, l + 1);
#endif
	linelen += l;
	linepos = linelen;
}

static void
init_edit(void)
{
	edit = 1;
#ifdef HAVE_NCURSESW_CURSES_H
	linebuf = malloc(LINESIZ * sizeof(*linebuf));
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
	mvwaddwstr(wstat, 1, 0, linebuf + leftpos);
#else
	mvwaddstr(wstat, 1, 0, linebuf + leftpos);
#endif
	wrefresh(wstat);
}

int
ed_dialog(char *msg, char *ini, void (*callback)(char *))
{
	if (edit)
		return 1;

	init_edit();
	memcpy(lbuf, msg, strlen(msg) + 1);

	if (ini)
		ed_append(ini);

	disp_edit();

	if (edit_line(callback)) {
		clr_edit();
		return 1;
	}

#ifdef HAVE_NCURSESW_CURSES_H
	wcstombs(rbuf, linebuf, sizeof rbuf);
#endif
	clr_edit();
	return 0;
}

static int
edit_line(void (*callback)(char *))
{
#ifdef HAVE_NCURSESW_CURSES_H
	wint_t c;
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
			if (!linepos)
				break;

			linepos = 0;
			wmove(wstat, 1, 0);
			wrefresh(wstat);
			break;
		case KEY_LEFT:
			if (!linepos)
				break;

			wmove(wstat, 1, --linepos - leftpos);
			wrefresh(wstat);
			break;
		case KEY_END:
			if (linepos == linelen)
				break;

			linepos = linelen;
			wmove(wstat, 1, linepos - leftpos);
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
			wmove(wstat, 1, linepos - leftpos);
			goto del_char;
		case KEY_DC:
			if (linepos == linelen)
				break;

del_char:
			linebuf_delch();
			wdelch(wstat);

			if (leftpos)
				overful_del();

			wrefresh(wstat);

			if (callback) {
#ifdef HAVE_NCURSESW_CURSES_H
				wcstombs(rbuf, linebuf, sizeof rbuf);
#endif
				callback(rbuf);
			}

			break;
		default:
			if (linelen + 1 >= LINESIZ)
				break;

			if (linelen >= statw - 1) {
				wmove(wstat, 1, 0);
				wdelch(wstat);
				wmove(wstat, 1, linepos - ++leftpos);
			}

			if (linepos == linelen++)
				linebuf[linepos + 1] = 0;
			else
				linebuf_insch();

			linebuf[linepos++] = c;

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
				winsch(wstat, c);
#endif
				wmove(wstat, 1, linepos - leftpos);
			}

			wrefresh(wstat);

			if (callback) {
#ifdef HAVE_NCURSESW_CURSES_H
				wcstombs(rbuf, linebuf, sizeof rbuf);
#endif
				callback(rbuf);
			}
		}
	}
}

static void
overful_del(void)
{
	wmove(wstat, 1, 0);
#ifdef HAVE_NCURSESW_CURSES_H
	*ws = linebuf[--leftpos];
	setcchar(&cc, ws, 0, 0, NULL);
	wins_wch(wstat, &cc);
#else
	winsch(wstat, linebuf[leftpos]);
#endif
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
linebuf_insch(void)
{
	unsigned i, j;

	for (i = j = linelen; i > linepos; i = j)
		linebuf[i] = linebuf[--j];
}

#ifdef NCURSES_MOUSE_VERSION
static void
proc_mevent(void)
{
	if (getmouse(&mevent) != OK)
		return;

	if (mevent.bstate & BUTTON1_CLICKED ||
	    mevent.bstate & BUTTON1_DOUBLE_CLICKED) {
		if (mevent.y != LINES - 1)
			return;

		linepos = leftpos + mevent.x;
		wmove(wstat, 1, linepos - leftpos);
		wrefresh(wstat);
	}
}
#endif
