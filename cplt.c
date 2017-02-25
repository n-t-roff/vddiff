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

static int cpltstr(char *, const char **);

static const char *set_opts[] = {
	"all",
	"fkeys",
	"ic",
	"magic",
	"nofkeys",
	"noic",
	"nomagic",
	"norecursive",
	"nows",
	"recursive",
	"ws",
	NULL
};

static const char *ex_cmds[] = {
	"cd",
	"edit",
	"find",
	"grep",
	"marks",
	"nofind",
	"nogrep",
	"set",
	"view",
	NULL
};

int
complet(char *s, int c)
{
	char *e, *b, *d, *dn, *bn, *fn, *m = NULL;
	DIR *dh;
	struct dirent *de;
	size_t ld, lb, ln;
	int r = 0;
	bool co;
	bool ts; /* trailing slash */

	if (c != '\t' || !*s) {
		return 0;
	}

	e = s + strlen(s);
	ts = e[-1] == '/' ? TRUE : FALSE;

	while (e != s) {
		e--;

		if ((c = *e) == ' ' || c == '\t') {
			e++;
			break;
		}
	}

	if (e == s) {
		return cpltstr(e, ex_cmds);
	} else if (!strncmp(s, "se ", 3) || !strncmp(s, "set ", 4)) {
		return cpltstr(e, set_opts);
	} else if (strncmp(s, "cd ", 3)) {
		return 0;
	}

	if (!(b = pthexp(e))) {
		return 0;
	}

	/* Search for the last '/' in b and open this directory.
	 * If there is no '/' in b open ".". */
	d = strdup(b);

	/* On "foo/" dirname is "." and basename is "foo".
	 * But this code expects dirname as "foo" and basename as "". */
	if (ts) {
		dn = b;
		bn = "";
	} else {
		dn = dirname(b);
		bn = *d ? basename(d) : "";
	}

	ld = strlen(dn);
	lb = strlen(bn);
	memcpy(b, dn, ld+1);
	co = TRUE;

#if defined(TRACE) && 0
		fprintf(debug, "  cplt opendir \"%s\"\n", b);
#endif
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

		/* Don't use pthcat for "..", it destroys the path.
		 * "." and ".." are directories, stat(2) is not
		 * necessary. */
		if (!(*fn == '.' && (!fn[1] || (fn[1] == '.' && !fn[2])))) {
#if defined(TRACE) && 0
			fprintf(debug, "  cplt readdir \"%s\" b(%s)\n", fn, b);
#endif
			pthcat(b, ld, fn);
#if defined(TRACE) && 0
			fprintf(debug, "  cplt stat \"%s\"\n", b);
#endif

			if (stat(b, &gstat[0]) == -1) {
				switch (errno) {
				/* Case: Dead symlink */
				case EACCES:
				case ELOOP:
				case ENOENT:
#if defined(TRACE) /* keep! */
					fprintf(debug,
					    "  cplt stat \"%s\": %s\n",
					    b, strerror(errno));
#endif
					continue;

				default:
					printerr(strerror(errno),
					    "stat \"%s\"", b);
					continue;
				}
			}

			if (!S_ISDIR(gstat[0].st_mode)) {
				continue;
			}
		}

#if defined(TRACE) && 0
		fprintf(debug, "  cplt cmp \"%s\", \"%s\"\n", bn, fn);
#endif
		if (lb && strncmp(bn, fn, lb)) {
			continue;
		}

		/* Assumption: The first filename is already the correct one.
		 * For any later found name test how many characters match. */
		if (!m) {
			ln = strlen(fn);
			m  = strdup(fn);
#if defined(TRACE) && 0
			fprintf(debug, "  cplt 1st match \"%s\"\n", m);
#endif
			continue;
		}

		/* There is a second matching file */
		co = FALSE;

		do {
			if (!strncmp(fn, m, ln)) {
				/* does also fit but is longer than saved
				 * string */
#if defined(TRACE) && 0
				fprintf(debug,
				    "  cplt 2nd match \"%s\", %zu\n",
				    fn, ln);
#endif
				break;
			}
#if defined(TRACE) && 0
			fprintf(debug, "  cplt no 2nd match \"%s\", %zu\n",
			    fn, ln);
#endif
		} while (--ln > lb);

		/* Not the case when above loop is left with {break} */
		if (ln == lb) {
			break;
		}
	}

	if (closedir(dh) == -1) {
		b[ld] = 0;
		printerr(strerror(errno), "closedir \"%s\"", b);
	}

	if (!m) {
		goto free;
	}

	if (ln == lb) {
		if (ts) {
			goto free;
		}

		goto cplt;
	}

	m[ln] = 0;
	ed_append(m + lb);
	r = EDCB_WR_BK;

cplt:
	if (co) { /* complete, not part of name */
		ed_append("/");
		r = EDCB_WR_BK;
	}

free:
	free(m);
	free(d);
	free(b);
	disp_edit();
	return r;
}

static int
cpltstr(
    /* begin of last word of input */
    char *iw,
    /* string list */
    const char **lst)
{
	const char *cw; /* current word */
	char *mw = NULL; /* matched word */
	size_t iwl = strlen(iw); /* input word length */
	size_t mwl; /* matched word length */
	int r = 0;
	bool co = TRUE;

	while ((cw = *lst++)) {
		if (iwl && strncmp(cw, iw, iwl)) {
			continue;
		}

		if (!mw) {
			mwl = strlen(cw);
			mw  = strdup(cw);
			continue;
		}

		co = FALSE;

		do {
			if (!strncmp(cw, mw, mwl)) {
				break;
			}
		} while (--mwl > iwl);

		if (mwl == iwl) {
			break;
		}
	}

	if (!mw) {
		goto free;
	}

	if (mwl == iwl) {
		goto cplt;
	}

	mw[mwl] = 0;
	ed_append(mw + iwl);
	r = EDCB_WR_BK;

cplt:
	if (co) {
		ed_append(" ");
		r = EDCB_WR_BK;
	}

free:
	free(mw);
	disp_edit();
	return r;
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
