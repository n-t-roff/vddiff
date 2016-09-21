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
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include "ui.h"
#include "exec.h"
#include "diff.h"
#include "uzp.h"
#include "db.h"
#include "main.h"

static int mktmpdirs(void);
static enum uz_id check_ext(char *, int *);
static void gunzip(struct filediff *, struct filediff *, int);
static void bunzip2(struct filediff *, struct filediff *, int);
static char *zpths(struct filediff *, struct filediff *, int, size_t *);

static char *tmp_dir;

static struct uz_ext exttab[] = {
	{ "bz2"    , UZ_BZ2 },
	{ "gz"     , UZ_GZ  },
	{ "tar"    , UZ_TAR },
	{ "tar.bz2", UZ_TBZ },
	{ "tar.gz" , UZ_TGZ },
	{ "tbz"    , UZ_TBZ },
	{ "tgz"    , UZ_TGZ }
};

void
uz_init(void)
{
	int i;

	for (i = 0; i < (ssize_t)(sizeof(exttab)/sizeof(*exttab)); i++)
		uz_db_add(exttab + i);
}

static int
mktmpdirs(void)
{
#ifndef HAVE_MKDTEMP
	return 1;
#else
	char *d1;
	char d2[] = "/.vddiff.XXXXXX";
	size_t l;

	if (!(d1 = getenv("TMPDIR")))
		d1 = "/var/tmp";

	l = strlen(d1);
	tmp_dir = malloc(l + sizeof(d2) + 2);

	memcpy(tmp_dir, d1, l);
	memcpy(tmp_dir + l, d2, sizeof(d2));
	l += sizeof d2;

	if (!(d1 = mkdtemp(tmp_dir))) {
		printerr(strerror(errno),
		    "mkdtemp %s failed", tmp_dir);
		free(tmp_dir);
		tmp_dir = NULL;
		return 1;
	}

	tmp_dir[--l] = '/';
	tmp_dir[++l] = 'l';
	tmp_dir[++l] = 0;

	if (mkdir(d1, 0700) == -1) {
		printerr(strerror(errno),
		    "mkdir %s failed", tmp_dir);
		rmtmpdirs();
		return 1;
	}

	tmp_dir[--l] = 'r';

	if (mkdir(d1, 0700) == -1) {
		printerr(strerror(errno),
		    "mkdir %s failed", tmp_dir);
		rmtmpdirs();
		return 1;
	}

	tmp_dir[l] = 0;
	return 0;
#endif /* HAVE_MKDTEMP */
}

void
rmtmpdirs(void)
{
	static char *av[] = { "rm", "-rf", NULL, NULL };

	av[2] = tmp_dir;
	exec_cmd(av, 0, NULL, NULL);
	free(tmp_dir);
	tmp_dir = NULL;
}

struct filediff *
unzip(struct filediff *f, int tree)
{
	enum uz_id id;
	struct filediff *z;
	int i;
	char *s;
	size_t l;

	if ((id = check_ext(f->name, &i)) == UZ_NONE)
		return NULL;

	if (mktmpdirs())
		return NULL;

	z = malloc(sizeof(struct filediff));
	*z = *f;
	l = strlen(tmp_dir);
	s = malloc(l + 2 + i + 1);
	memcpy(s, tmp_dir, l);
	s[l++] = tree == 1 ? 'l' : 'r';
	s[l++] = '/';
	memcpy(s + l, f->name, i);
	s[l+i] = 0;
	z->name = s;

	switch (id) {
	case UZ_GZ:
		gunzip(f, z, tree);
		break;
	case UZ_BZ2:
		bunzip2(f, z, tree);
		break;
	default:
		;
	}

	return z;
}

static enum uz_id
check_ext(char *name, int *pos)
{
	size_t l;
	char *s;
	short skipped = 0;
	int c;
	enum uz_id id;
	int i;

	l = strlen(name);
	s = lbuf + sizeof lbuf;
	*--s = 0;

	while (l) {
		*--s = tolower(name[--l]);

		if (!skipped && *s == '.' &&
		    !str_db_srch(&skipext_db, s + 1)) {
			*s = 0;
			skipped = 1;
		}

		if (s == lbuf)
			break;
	}

	for (i = 0; (c = *s++); i++) {
		if (c == '.' && *s &&
		    ((id = uz_db_srch(s)) != UZ_NONE)) {
			*pos = i;
			return id;
		}
	}

	return UZ_NONE;
}

static void
gunzip(struct filediff *f, struct filediff *z, int tree)
{
	char *s;
	size_t l;

	l = 20;
	s = zpths(f, z, tree, &l);
	snprintf(s, l, "zcat %s > %s", lbuf, rbuf);
	sh_cmd(s, 0);
	free(s);
}

static void
bunzip2(struct filediff *f, struct filediff *z, int tree)
{
	char *s;
	size_t l;

	l = 20;
	s = zpths(f, z, tree, &l);
	snprintf(s, l, "bzcat %s > %s", lbuf, rbuf);
	sh_cmd(s, 0);
	free(s);
}

static char *
zpths(struct filediff *f, struct filediff *z, int tree, size_t *l2)
{
	char *s;
	size_t l;

	if (tree == 1) {
		s = lpath;
		l = llen;
	} else {
		s = rpath;
		l = rlen;
	}

	pthcat(s, l, f->name);
	shell_quote(lbuf, s, sizeof lbuf);
	shell_quote(rbuf, z->name, sizeof rbuf);
	l = *l2;
	l = strlen(lbuf) + strlen(rbuf) + l;
	*l2 = l;
	return malloc(l);
}
