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

#define GQBUFSIZ (1024 * 1024)
#define GQBSEEK  (1024 * 4)

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "ui2.h"
#include "gq.h"

static char *gq_buf;
static regex_t gq_re;

int
gq_init(char *s)
{
	int fl;

	fl = REG_NOSUB;

	if (magic)
		fl |= REG_EXTENDED;
	if (!noic)
		fl |= REG_ICASE;

	if (regcomp(&gq_re, s, fl)) {
		printf("regcomp \"%s\" failed: %s", s, strerror(errno));
		return 1;
	}

	gq_buf = malloc(GQBUFSIZ + 1);
	file_pattern = 1;
	return 0;
}

int
gq_proc(struct filediff *f)
{
	size_t l;
	char *p;
	ssize_t n;
	int fh;
	int rv = 1; /* not found */

	if (S_ISREG(f->ltype) && f->lsiz) {
		p = lpath;
		l = llen;
	} else if (S_ISREG(f->rtype) && f->rsiz) {
		p = rpath;
		l = rlen;
	} else
		return rv;

	pthcat(p, llen, f->name);

	if ((fh = open(p, O_RDONLY)) == -1) {
		printerr(strerror(errno), "open \"%s\" failed", p);
		rv = -1;
		goto ret;
	}

	while (1) {
		if ((n = read(fh, gq_buf, GQBUFSIZ)) == -1) {
			printerr(strerror(errno), "read \"%s\" failed", p);
			rv = -1;
			break;
		}

		if (!n)
			break;

		gq_buf[n] = 0;

		if (!regexec(&gq_re, gq_buf, 0, NULL, 0)) {
			rv = 0;
			break;
		}

		if (n != GQBUFSIZ)
			break;

		if (lseek(fh, -GQBSEEK, SEEK_CUR) == -1) {
			printerr(strerror(errno), "lseek \"%s\" failed", p);
			rv = -1;
			break;
		}
	}

	if (close(fh) == -1) {
		printerr(strerror(errno), "close \"%s\" failed", p);
		rv = -1;
	}

ret:
	p[l] = 0;
	return rv;
}
