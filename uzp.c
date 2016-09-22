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
static struct filediff *zcat(char *, struct filediff *, int, int);
static struct filediff *tar(char *, struct filediff *, int, int);
static char *zpths(struct filediff *, struct filediff **, int, size_t *,
    int, int);
static void push_path(char *, size_t, int);

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

	if ((id = check_ext(f->name, &i)) == UZ_NONE)
		return NULL;

	if (mktmpdirs())
		return NULL;

	switch (id) {
	case UZ_GZ:
		z = zcat("zcat", f, tree, i);
		break;
	case UZ_BZ2:
		z = zcat("bzcat", f, tree, i);
		break;
	case UZ_TBZ:
		z = tar("xjf", f, tree, i);
		break;
	default:
		rmtmpdirs();
		return NULL;
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
		    !str_db_srch(&skipext_db, s + 1
#ifdef HAVE_LIBAVLBST
		    , NULL
#endif
		    )) {
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

static struct filediff *
zcat(char *cmd, struct filediff *f, int tree, int i)
{
	char *s;
	size_t l;
	struct filediff *z;

	l = 20;
	s = zpths(f, &z, tree, &l, i, 1);
	snprintf(s, l, "%s %s > %s", cmd, lbuf, rbuf);
	sh_cmd(s, 0);
	free(s);
	return z;
}

static struct filediff *
tar(char *opt, struct filediff *f, int tree, int i)
{
	char *s;
	size_t l;
	struct filediff *z;

	l = 20;
	s = zpths(f, &z, tree, &l, i, 0);
	snprintf(s, l, "tar %s %s -C %s", opt, lbuf, rbuf);
	sh_cmd(s, 0);
	free(s);
	return z;
}

static char *
zpths(struct filediff *f, struct filediff **z2, int tree, size_t *l2, int i,
    int fn)
{
	char *s;
	size_t l;
	struct filediff *z;

	if (!fn)
		i = 0;

	z = malloc(sizeof(struct filediff));
	*z2 = z;
	*z = *f;
	l = strlen(tmp_dir);
	s = malloc(l + 3 + i);
	memcpy(s, tmp_dir, l);
	s[l++] = tree == 1 ? 'l' : 'r';

	if (fn) {
		s[l++] = '/';
		memcpy(s + l, f->name, i);
	}

	s[l + i] = 0;
	z->name = s;

	if (lstat(s, &stat1) == -1) {
		if (errno != ENOENT) {
			printerr(strerror(errno), "stat %s failed",
			   lpath);
		}
	}

	if (tree == 1) {
		s = lpath;
		l = llen;
		z->ltype = stat1.st_mode;
	} else {
		s = rpath;
		l = rlen;
		z->rtype = stat1.st_mode;
	}

	pthcat(s, l, f->name);
	shell_quote(lbuf, s, sizeof lbuf);
	shell_quote(rbuf, z->name, sizeof rbuf);
	push_path(s, l, tree);
	l = *l2;
	l = strlen(lbuf) + strlen(rbuf) + l;
	*l2 = l;
	return malloc(l);
}

struct path_stack {
	char *path;
	size_t len;
	short tree;
	struct path_stack *next;
};

static struct path_stack *path_stack;

static void
push_path(char *path, size_t len, int tree)
{
	struct path_stack *p;

	path[len] = 0;
	p = malloc(sizeof(struct path_stack));
	p->path = strdup(path);
	p->len  = len;
	p->tree = tree;
	p->next = path_stack;
	path_stack = p;
	*path = 0;

	if (tree == 1)
		llen = 0;
	else
		rlen = 0;
}

void
pop_path(void)
{
	struct path_stack *p;
	char *s;

	if (!(p = path_stack))
		return;

	path_stack = p->next;

	if (p->tree == 1) {
		s = lpath;
		llen = p->len;
	} else {
		s = rpath;
		rlen = p->len;
	}

	memcpy(s, p->path, p->len + 1);
	free(p->path);
	free(p);
}
