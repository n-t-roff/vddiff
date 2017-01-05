/*
Copyright (c) 2017, Carsten Kunze <carsten.kunze@arcor.de>

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

#include <ctype.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include "compat.h"
#include <wctype.h>
#include "ed.h"
#include "cplt.h"
#include "main.h"
#include "ui.h"

int
complet(char *s, int c)
{
	char *e, *b;

	if (1 || c != '\t' || !*s) {
		return 0;
	}

	e = s + strlen(s);

	while (e != s) {
		e--;

		if ((c = *e) == ' ' || c == '\t') {
			e++;
			break;
		}
	}

	if (!strlen(e)) {
		/* Last char was space */
		return 0;
	}

	if (!(b = pthexp(e))) {
		return 0;
	}

	ed_append(b);
	free(b);
	disp_edit();
	return EDCB_WR_BK;
}

char *
pthexp(char *p)
{
	char *b, *s, *s2;
	size_t n = 0;
	int c, c2;
	bool d;

	b = malloc(PATHSIZ);

	if (*p == '~') {
		if (!(s = getenv("HOME"))) {
			printerr("", "$HOME not set");
			goto err;
		}

		n = strlen(s);
		memcpy(b, s, n);
		p++;
	}

	while ((c = *p++)) {
		if (c == '$') {
			s2 = p;

			if (*p == '{') {
				d = TRUE;
				p++;
				s2++;
			}

			while ((c = *p) && (isalnum(c) || c == '_')) {
				p++;
			}

			if (p == s2) {
				c = '$';
			} else {
				*p = 0;

				if (!(s = getenv(s2))) {
					printerr("", "$%s not set", s2);
					goto err;
				}

				while ((c2 = *s++)) {
					b[n++] = c2;

					if (n == PATHSIZ) {
						goto err;
					}
				}

				*p++ = c;

				if (d && c == '}') {
					continue;
				}
			}
		}

		b[n++] = c;

		if (n == PATHSIZ) {
			goto err;
		}
	}

	b[n] = 0;
	return b;

err:
	free(b);
	return NULL;
}
