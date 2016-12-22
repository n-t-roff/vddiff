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

#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <regex.h>
#include <stdarg.h>
#include "compat.h"
#include "ui.h"
#include "exec.h"
#include "diff.h"
#include "uzp.h"
#include "db.h"
#include "main.h"
#include "tc.h"

static int mktmpdirs(void);
static enum uz_id check_ext(char *, int *);
static struct filediff *zcat(char *, struct filediff *, int, int);
static struct filediff *tar(char *, struct filediff *, int, int);
static struct filediff *unzip(struct filediff *, int, int);
static char *zpths(struct filediff *, struct filediff **, int, size_t *,
    int, int);

char *tmp_dir;
/* Path names used bei the UI. syspth[0] and syspth[1] are only used for file
 * system access. */
char *pthnam[2];
/* View path buffer size. Initially set by uz_init(). */
size_t pthsiz[2];
/* Number of bytes in syspth[0]/syspth[1], which belong to the temporary directory
 * only. */
size_t tpthlen[2];
/* Offset in view path to where real path + tpthlen is copied. */
size_t vpthofs[2];
static const char *tmpdirbase;

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

int
uz_init(void)
{
	int i;

	for (i = 0; i < (ssize_t)(sizeof(exttab)/sizeof(*exttab)); i++)
		uz_db_add(exttab + i);

	if (!(tmpdirbase = getenv("TMPDIR")))
		tmpdirbase = "/var/tmp";

	if (!(tmpdirbase = realpath(tmpdirbase, NULL))) {
		printerr(strerror(errno), LOCFMT
		    "realpath \"%s\"" LOCVAR, tmpdirbase);
		return 1;
	}

	pthnam[0] = malloc((pthsiz[0] = 4096));
	pthnam[1] = malloc((pthsiz[1] = 4096));
	return 0;
}

void
uz_exit(void)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	struct ptr_db_ent *n;
#endif
	char *key;
	struct bpth *dat;

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
		rmtmpdirs(key, TOOL_NOLIST);
		free(dat->pth);
		free(dat);
	}
}

const char *
gettmpdirbase(void)
{
	return tmpdirbase;
}

static int
mktmpdirs(void)
{
#ifndef HAVE_MKDTEMP
	return 1;
#else
	char *d1;
	char d2[] = TMPPREFIX "XXXXXX";
	size_t l;

	l = strlen(tmpdirbase);
	tmp_dir = malloc(l + sizeof(d2) + 2);

	memcpy(tmp_dir, tmpdirbase, l);
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
		rmtmpdirs(tmp_dir, TOOL_NOLIST);
		return 1;
	}

	tmp_dir[--l] = 'r';

	if (mkdir(d1, 0700) == -1) {
		printerr(strerror(errno),
		    "mkdir %s failed", tmp_dir);
		rmtmpdirs(tmp_dir, TOOL_NOLIST);
		return 1;
	}

	tmp_dir[l] = 0;
	return 0;
#endif /* HAVE_MKDTEMP */
}

void
rmtmpdirs(char *s, tool_flags_t tf)
{
	static char *cm[] = { "chmod", "-R" , "700", NULL, NULL };
	static char *rm[] = { "rm"   , "-rf", NULL , NULL };

#if defined(TRACE)
	fprintf(debug, "->rmtmpdirs(%s)\n", s);
#endif
	cm[3] = s;
	exec_cmd(cm, tf, NULL, NULL);
	rm[2] = s;
	exec_cmd(rm, tf, NULL, NULL);
	free(s); /* either tmp_dir or a DB entry */
#if defined(TRACE)
	fprintf(debug, "<-rmtmpdirs\n");
#endif
}

struct filediff *
unpack(struct filediff *f, int tree, char **tmp, int type)
{
	enum uz_id id;
	struct filediff *z;
	int i;
	char *s;

	if ((tree == 1 && !S_ISREG(f->type[0])) ||
	    (tree == 2 && !S_ISREG(bmode ? f->type[0] : f->type[1])))
		return NULL;

	s = f->name ? f->name : bmode ? gl_mark : tree == 1 ? mark_lnam :
	    mark_rnam;

	if ((id = check_ext(s, &i)) == UZ_NONE)
		return NULL;

	switch (id) {
	case UZ_TGZ:
	case UZ_TBZ:
	case UZ_TAR:
	case UZ_ZIP:
		if (!type)
			return NULL;

		/* fall through */
	default:
		;
	}

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
		rmtmpdirs(tmp_dir, TOOL_NOLIST);
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
		*--s = tolower((int)name[--l]);

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
	char *s, *s2;
	size_t l;
	struct filediff *z;
	struct stat st;

	l = 20;
	s = zpths(f, &z, tree, &l, i, 1);
	snprintf(s, l, "%s %s > %s", cmd, lbuf, rbuf);
	s2 = strdup(rbuf); /* altered by exec_cmd() */
	exec_cmd(&s, TOOL_SHELL, NULL, NULL);
	free(s);

	if (lstat(s2, &st) == -1) {
		if (errno == ENOENT)
			printerr("", "Unpacked file \"%s\" not found", s2);
		else
			printerr(strerror(errno), LOCFMT "lstat \"%s\""
			    LOCVAR, s2);
	} else if (tree == 1 || bmode)
		z->lsiz = st.st_size;
	else
		z->rsiz = st.st_size;

	free(s2);
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
	exec_cmd(av,
	    /* Causes a endwin() before the command. NetBSD tar has a lot of
	     * terminal output which is removed with this endwin(). See also
	     * ^L */
	    TOOL_TTY, NULL, NULL);
	return z;
}

static struct filediff *
unzip(struct filediff *f, int tree, int i)
{
	struct filediff *z;
	static char *av[] = { "unzip", "-qq", NULL, "-d", NULL, NULL };

	zpths(f, &z, tree, NULL, i, 0);
	av[2] = lbuf;
	av[4] = rbuf;
	exec_cmd(av, 0, NULL, NULL);
	return z;
}

static char *
zpths(struct filediff *f, struct filediff **z2, int tree, size_t *l2, int i,
    int fn)
{
	char *s, *s2;
	size_t l, l3;
	struct filediff *z;

	if (!fn)
		i = 0;

	s2 = f->name ? f->name : bmode ? gl_mark : tree == 1 ? mark_lnam :
	    mark_rnam;

	z = malloc(sizeof(struct filediff));
	*z2 = z;
	*z = *f;
	l = strlen(tmp_dir);
	s = malloc(l + 3 + i);
	memcpy(s, tmp_dir, l);

	if (!fn) {
		free(tmp_dir);
		tmp_dir = NULL;
	}

	s[l++] = tree == 1 ? 'l' : 'r';

	if (!fn) {
		l3 = 0;
	} else {
		s[l++] = '/';
		l3 = strlen(s2);

		while (l3 && s2[l3 - 1] != '/') l3--;
		memcpy(s + l, s2 + l3, i - l3);
	}

	s[l + i - l3] = 0;
	z->name = s;

	if (tree == 1 ||
	    /* In case of bmode separate unpacked files in directories "l"
	     * and "r", but use syspth[0]/type[0] */
	    bmode) {
		s = syspth[0];
		l = pthlen[0];

		if (!fn)
			z->type[0] = S_IFDIR | S_IRWXU;

		z->type[1] = 0;
	} else {
		s = syspth[1];
		l = pthlen[1];
		z->type[0] = 0;

		if (!fn)
			z->type[1] = S_IFDIR | S_IRWXU;
	}

	if (*s2 == '/')
		s = s2;
	else
		pthcat(s, l, s2);

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

void
setvpth(int i)
{
}
