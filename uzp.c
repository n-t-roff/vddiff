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
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
static struct filediff *unzip(struct filediff *, int, int);
static char *zpths(struct filediff *, struct filediff **, int, size_t *,
    int, int);

char *tmp_dir;

static struct uz_ext exttab[] = {
	{ "bz2"    , UZ_BZ2 },
	{ "gz"     , UZ_GZ  },
	{ "tar"    , UZ_TAR },
	{ "tar.bz2", UZ_TBZ },
	{ "tar.gz" , UZ_TGZ },
	{ "tbz"    , UZ_TBZ },
	{ "tgz"    , UZ_TGZ },
	{ "zip"    , UZ_ZIP }
};

void
uz_init(void)
{
	int i;

	for (i = 0; i < (ssize_t)(sizeof(exttab)/sizeof(*exttab)); i++)
		uz_db_add(exttab + i);
}

void
uz_exit(void)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	struct ptr_db_ent *n;
#endif
	char *key, *dat;

	while ((n = ptr_db_get_node(uz_path_db))) {
#ifdef HAVE_LIBAVLBST
		key = n->key.p;
		dat = n->data.p;
#else
		key = n->key;
		dat = n->dat;
#endif
		/* before rmtmpdirs(key) since DB needs key which is freed
		 * in rmtmpdirs() */
		ptr_db_del(&uz_path_db, n);
		key[strlen(key) - 2] = 0;
		rmtmpdirs(key);
		free(dat);
	}
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

	if (!(d1 = realpath(d1, NULL))) {
		printerr(strerror(errno), "realpath \"%s\" failed", d1);
		return 1;
	}

	l = strlen(d1);
	tmp_dir = malloc(l + sizeof(d2) + 2);

	memcpy(tmp_dir, d1, l);
	free(d1);
	memcpy(tmp_dir + l, d2, sizeof(d2));
	l += sizeof d2;

	if (!(d1 = mkdtemp(tmp_dir))) {
		printerr(strerror(errno), "mkdtemp \"%s\" failed", tmp_dir);
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
		rmtmpdirs(tmp_dir);
		return 1;
	}

	tmp_dir[--l] = 'r';

	if (mkdir(d1, 0700) == -1) {
		printerr(strerror(errno),
		    "mkdir %s failed", tmp_dir);
		rmtmpdirs(tmp_dir);
		return 1;
	}

	tmp_dir[l] = 0;
	return 0;
#endif /* HAVE_MKDTEMP */
}

void
rmtmpdirs(char *s)
{
	static char *av[] = { "rm", "-rf", NULL, NULL };

	av[2] = s;
	exec_cmd(av, 0, NULL, NULL);
	free(s); /* either tmp_dir or a DB entry */
}

struct filediff *
unpack(struct filediff *f, int tree, char **tmp)
{
	enum uz_id id;
	struct filediff *z;
	int i;

	if ((tree == 1 && !S_ISREG(f->ltype)) ||
	    (tree == 2 && !S_ISREG(f->rtype)))
		return NULL;

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
	case UZ_TGZ:
		z = tar("xzf", f, tree, i);
		break;
	case UZ_TBZ:
		z = tar("xjf", f, tree, i);
		break;
	case UZ_TAR:
		z = tar("xf", f, tree, i);
		break;
	case UZ_ZIP:
		z = unzip(f, tree, i);
		break;
	default:
		rmtmpdirs(tmp_dir);
		return NULL;
	}

	*tmp = tmp_dir;
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
	struct filediff *z;
	static char *av[] = { "tar", NULL, NULL, "-C", NULL, NULL };

	zpths(f, &z, tree, NULL, i, 0);
	av[1] = opt;
	av[2] = lbuf;
	av[4] = rbuf;
	exec_cmd(av, 0, NULL, NULL);
	return z;
}

static struct filediff *
unzip(struct filediff *f, int tree, int i)
{
	struct filediff *z;
	static char *av[] = { "unzip", NULL, "-d", NULL, NULL };

	zpths(f, &z, tree, NULL, i, 0);
	av[1] = lbuf;
	av[3] = rbuf;
	exec_cmd(av, 0, NULL, NULL);
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

	if (!fn) /* else needed in ui.c */
		free(tmp_dir);

	s[l++] = tree == 1 ? 'l' : 'r';

	if (fn) {
		s[l++] = '/';
		memcpy(s + l, f->name, i);
	}

	s[l + i] = 0;
	z->name = s;

	/* stat is done on the extracted file or directory and hence needs
	 * to be a lstat */
	if (!fn && lstat(s, &stat1) == -1) {
		if (errno != ENOENT)
			printerr(strerror(errno), "stat \"%s\" failed", s);
		else
			printerr("", "\"%s\" does not exist", s);
	}

	if (tree == 1) {
		s = lpath;
		l = llen;

		if (!fn) {
			z->ltype = stat1.st_mode;
			z->rtype = 0;
		}
	} else {
		s = rpath;
		l = rlen;

		if (!fn) {
			z->rtype = stat1.st_mode;
			z->ltype = 0;
		}
	}

	pthcat(s, l, f->name);

	if (l2) {
		shell_quote(lbuf, s, sizeof lbuf);
		shell_quote(rbuf, z->name, sizeof rbuf);
		l = *l2;
		l = strlen(lbuf) + strlen(rbuf) + l;
		*l2 = l;
		return malloc(l);
	} else {
		memcpy(lbuf, s, strlen(s) + 1);
		memcpy(rbuf, z->name, strlen(z->name) + 1);
		return NULL;
	}
}
