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

#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <libgen.h>
#include "compat.h"
#include <wctype.h>
#include "ed.h"
#include "cplt.h"
#include "main.h"
#include "ui.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "diff.h"

int
complet(char *s, int c)
{
	char *e, *b, *d, *dn, *bn, *fn, *m = NULL;
	DIR *dh;
	struct dirent *de;
	size_t ld, lb, ln;
	bool co;

	if (c != '\t' || !*s) {
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

	if (!(b = pthexp(e))) {
		return 0;
	}

	/* Search for the last '/' in b and open this directory.
	 * If there is no '/' in b open ".". */
	d = strdup(b);
	dn = dirname(b);
	bn = *d ? basename(d) : "";
	ld = strlen(dn);
	lb = strlen(bn);
	memcpy(b, dn, ld+1);
	co = TRUE;

	if (!(dh = opendir(b))) {
		printerr(strerror(errno), "opendir \"%s\"", b);
		goto free;
	}

	while (1) {
		errno = 0;

		if (!(de = readdir(dh))) {
			if (errno) {
				b[ld] = 0;
				printerr(strerror(errno),
				    "readdir \"%s\"", b);
			}

			break;
		}

		fn = de->d_name;

		if (*fn == '.' && (!fn[1] || (fn[1] == '.' && !fn[2]))) {
			continue;
		}

		pthcat(b, ld, fn);

		if (stat(b, &stat1) == -1) {
			if (errno == ENOENT) {
				continue;
			}

			printerr(strerror(errno), "stat \"%s\"", b);
			continue;
		}

		if (!S_ISDIR(stat1.st_mode)) {
			continue;
		}

		if (lb && strncmp(bn, fn, lb)) {
			continue;
		}

		/* Assumption: The first filename is already the correct one.
		 * For any later found name test how many characters match. */
		if (!m) {
			ln = strlen(fn);
			m = strdup(fn);
			continue;
		}

		while (ln > lb) {
			if (!strncmp(fn, m, ln)) {
				break;
			}

			ln--;
			co = FALSE;
		}

		if (ln == lb) {
			break;
		}
	}

	if (closedir(dh) == -1) {
		b[ld] = 0;
		printerr(strerror(errno), "closedir \"%s\"", b);
	}

	if (!m || ln == lb) {
		goto free;
	}

	m[ln] = 0;
	ed_append(m + lb);

	if (co) { /* complete, not part of name */
		ed_append("/");
	}

free:
	if (m) {
		free(m);
	}

	free(d);
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

	if (!*p) {
		*b = 0; /* "" expanded to "" */
		return b;
	}

	if (*p == '~') {
		p++;

		if (!*p || *p == '/') {
			if (!(s = getenv("HOME"))) {
				printerr("", "$HOME not set");
				goto err;
			}
		} else {
			struct passwd *pw;

			s2 = p;

			while ((c = *p) && c != '/') {
				p++;
			}

			*p = 0;
			errno = 0; /* getpwnam manpage */

			if (!(pw = getpwnam(s2))) {
				printerr(errno ? strerror(errno) :
				    "Not found", "getpwnam \"%s\"", s2);
				goto err;
			}

			s = pw->pw_dir;
			*p = c;
		}

		n = strlen(s);
		memcpy(b, s, n);
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
					printerr(NULL, "$%s not set", s2);
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
