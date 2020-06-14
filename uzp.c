/*
Copyright (c) 2016-2018, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include "compat.h"
#include "ui.h"
#include "diff.h"
#include "uzp.h"
#include "db.h"
#include "main.h"
#include "tc.h"
#include "misc.h"
#include "fs.h"

struct pthofs {
	size_t sys;
	size_t view;
	char *vpth;
	struct pthofs *next;
};

static int mktmpdirs(void);
static enum uz_id check_ext(const char *, size_t *);
static struct filediff *zcat(const char *, const struct filediff *, int, size_t);
static struct filediff *tar(const char *const, const struct filediff *, int, size_t,
    unsigned);
static struct filediff *unzip(const struct filediff *, int, size_t, unsigned);
static char *zpths(const struct filediff *, struct filediff **, int, size_t *,
    size_t, int);

char *tmp_dir;
/* View path names used by the UI.
 * syspth[0] and syspth[1] are only used for file system access. */
char *path_display_name[2];
/* View path buffer size. Initially set by uz_init(). */
size_t path_display_buffer_size[2];
/* Number of bytes in syspth[0]/syspth[1], which belong to the temporary
 * directory only. */
size_t sys_path_tmp_len[2];
/* Offset in view path to where real path + tpthlen is copied. */
size_t path_display_name_offset[2];
static struct pthofs *pthofs[2];
const char *tmpdirbase;

static struct uz_ext exttab[] = {
	{ "bz2"    , UZ_BZ2 },
	{ "gz"     , UZ_GZ  },
    { "jar"    , UZ_ZIP },
    { "ods"    , UZ_ZIP },
	{ "odt"    , UZ_ZIP },
	{ "pptx"   , UZ_ZIP },
	{ "tar"    , UZ_TAR },
	{ "tar.bz2", UZ_TBZ },
	{ "tar.gz" , UZ_TGZ },
	{ "tar.xz" , UZ_TXZ },
	{ "tar.Z"  , UZ_TAR_Z },
	{ "tbz"    , UZ_TBZ },
	{ "tgz"    , UZ_TGZ },
	{ "txz"    , UZ_TXZ },
	{ "xz"     , UZ_XZ  },
	{ "Z"      , UZ_GZ  },
	{ "zip"    , UZ_ZIP },
	{ "xlsx"   , UZ_ZIP }
};

static struct uz_ext idtab[] = {
	{ "bz2"    , UZ_BZ2 },
	{ "gz"     , UZ_GZ  },
	{ "tar"    , UZ_TAR },
	{ "tar.Z"  , UZ_TAR_Z },
	{ "tbz"    , UZ_TBZ },
	{ "tgz"    , UZ_TGZ },
	{ "txz"    , UZ_TXZ },
	{ "xz"     , UZ_XZ  },
	{ "zip"    , UZ_ZIP }
};

int
uz_init(void)
{
	int i;
	const char *s;

#if defined(TRACE) && 1
    fprintf(debug, "<>uz_init())\n");
#endif

    for (i = 0; i < (ssize_t)(sizeof(exttab)/sizeof(*exttab)); i++) {
		uz_db_add(strdup(exttab[i].str), exttab[i].id);
	}

	if (!(s = getenv("TMPDIR"))) {
		s = "/var/tmp";
	}

	if (!(tmpdirbase = realpath(s, NULL))) {
		printerr(strerror(errno), LOCFMT
		    "realpath \"%s\"" LOCVAR, s);
		return 1;
	}

    path_display_name[0] = malloc((path_display_buffer_size[0] = 4096));
    path_display_name[1] = malloc((path_display_buffer_size[1] = 4096));
    path_display_name[0][0] = 0;
    path_display_name[1][0] = 0;
#if defined(TRACE) && 0 /* DANGER!!! Hides bugs! Keep 0! */
	*vpath[0] = 0;
	*vpath[1] = 0;
#endif
	return 0;
}

void
uz_add(char *ext, char *str)
{
	int i;
	enum uz_id id = UZ_NONE;

	for (i = 0; i < (ssize_t)(sizeof(idtab)/sizeof(*idtab)); i++) {
		if (!strcmp(str, idtab[i].str)) {
			id = idtab[i].id;
			break;
		}
	}

	if (UZ_NONE == id) {
		fprintf(stderr, "Invalid archive ID %s\n", str);
		exit(1);
	}

    /* uz_add can silently be used twice with the same argument since
     * it is used outside RC too. Hence ist is not distinguishable
     * if it had been in DB from RC or not. */
	if (UZ_NONE != uz_db_srch(ext)) {
		uz_db_del(ext);
	}

	uz_db_add(ext, id);
	free(str);
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

#if defined(TRACE) && 1
    fprintf(debug, "<>uz_exit())\n");
#endif

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

void rmtmpdirs(const char *const s)
{
#if defined(TRACE) && 1
    fprintf(debug, "->rmtmpdirs(%s) lpth=%s rpth=%s\n", s, syspth[0], syspth[1]);
#endif
    char *const syspth_copy = strdup(syspth[0]);
    memcpy(syspth[0], s, strlen(s) + 1);
    free(const_cast_ptr(s)); /* either tmp_dir or a DB entry */
    pth2 = syspth[0];
    fs_rm(0, /* tree */
          NULL, /* txt */
          NULL, /* nam */
          0, /* u */
          1, /* n */
          8|2|1); /* md */
    memcpy(syspth[0], syspth_copy, strlen(syspth_copy) + 1);
    free(syspth_copy);
#if defined(TRACE) && 1
	fprintf(debug, "<-rmtmpdirs\n");
#endif
}

struct filediff *
unpack(const struct filediff *f, int tree, char **tmp,
    /* 1: Also unpack files, not just archives */
    /* 2: Non-curses mode */
    /* 4: Always set tmpdir */
    /* 8: Check if viewer is set for extension. In this case the archive
          is not unpacked. */
    int type)
{
	enum uz_id id;
	struct filediff *z = NULL;
    const char *s;

#if defined(TRACE) && 1
	fprintf(debug, "->unpack(f->name=%s, tree=%d)\n", f->name, tree);
#endif

	if ((tree == 1 && !S_ISREG(f->type[0])) ||
	    (tree == 2 && !S_ISREG(bmode ? f->type[0] : f->type[1])))
		goto ret;

	s = f->name ? f->name : bmode ? gl_mark : tree == 1 ? mark_lnam :
	    mark_rnam;

	if ((type & 8) && check_ext_tool(s)) {
		goto ret;
	}
    size_t i;
	if ((id = check_ext(s, &i)) == UZ_NONE)
		goto ret;

	switch (id) {
	/* all archive types */
	case UZ_TGZ:
	case UZ_TBZ:
	case UZ_TAR:
	case UZ_TXZ:
	case UZ_ZIP:
	case UZ_TAR_Z:
		if (!(type & 1)) {
			goto ret;
		}

		/* fall through */
	default:
		;
	}

	if (mktmpdirs())
		goto ret;

	switch (id) {
	case UZ_GZ:
		z = zcat("zcat", f, tree, i);
		break;

	case UZ_BZ2:
		z = zcat("bzcat", f, tree, i);
		break;

	case UZ_XZ:
		z = zcat("xzcat", f, tree, i);
		break;

	case UZ_TGZ:
		z = tar("xzf", f, tree, i, type & 4 ? 1 : 0);
		break;

	case UZ_TBZ:
		z = tar("xjf", f, tree, i, type & 4 ? 1 : 0);
		break;

	case UZ_TAR:
		z = tar("xf", f, tree, i, type & 4 ? 1 : 0);
		break;

	case UZ_TXZ:
		z = tar("xJf", f, tree, i, type & 4 ? 1 : 0);
		break;

	case UZ_TAR_Z:
		z = tar("xZf", f, tree, i, type & 4 ? 1 : 0);
		break;

	case UZ_ZIP:
		z = unzip(f, tree, i, type & 4 ? 1 : 0);
		break;

	default:
        rmtmpdirs(tmp_dir);
		goto ret;
	}

	*tmp = tmp_dir;
ret:
#if defined(TRACE) && 1
	fprintf(debug, "<-unpack: z->name=%s *tmp=%s\n",
	    z ? z->name : "", *tmp);
#endif
	return z;
}

static enum uz_id
check_ext(const char *name, size_t *pos)
{
	size_t l;
	char *s;
	short skipped = 0;
	int c;
	enum uz_id id;
#if defined(TRACE)
    fprintf(debug, "<>check_ext(name=\"%s\")\n", name);
#endif

	l = strlen(name);
	s = lbuf + sizeof lbuf;
	*--s = 0;

	while (l) {
        *--s = (char)tolower((int)name[--l]);

		if (!skipped && *s == '.' &&
		    !str_db_srch(&skipext_db, s + 1, NULL)) {
			*s = 0;
			skipped = 1;
		}

		if (s == lbuf)
			break;
	}

    size_t i; /* Old compilers require -std=c99 for this
               * which causes further issues */
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
zcat(const char *cmd, const struct filediff *f, int tree, size_t i)
{
	struct filediff *z;
    size_t l = 20;
    char *s = zpths(f, &z, tree, &l, i, 3);
	snprintf(s, l, "%s %s > %s", cmd, lbuf, rbuf);
    const char *s2 = strdup(rbuf); /* altered by exec_cmd() */
	exec_cmd(&s, TOOL_SHELL, NULL, NULL);
	free(s);
    struct stat st;
    if (lstat(z->name, &st) == -1) {
		if (errno == ENOENT)
			printerr("", "Unpacked file \"%s\" not found", s2);
		else
			printerr(strerror(errno), LOCFMT "lstat \"%s\""
			    LOCVAR, s2);
	} else if (tree == 1 || bmode)
		z->siz[0] = st.st_size;
	else
		z->siz[1] = st.st_size;
    free(const_cast_ptr(s2));
	return z;
}

static struct filediff *
tar(const char *const opt, const struct filediff *f, int tree, size_t i,
    /* 1: set tmpdir */
    unsigned m)
{
	struct filediff *z;
	static const char *av[] = { "tar", NULL, NULL, "-C", NULL, NULL };

	zpths(f, &z, tree, NULL, i, m & 1 ? 2 : 0);
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
unzip(const struct filediff *f, int tree, size_t i,
    /* 1: set tmp_dir */
    unsigned m)
{
	struct filediff *z;
	static const char *av[] = { "unzip", "-qq", NULL, "-d", NULL, NULL };

	zpths(f, &z, tree, NULL, i, m & 1 ? 2 : 0);
	av[2] = lbuf;
	av[4] = rbuf;
    exec_cmd(av, TOOL_TTY, NULL, NULL);
	return z;
}

/* Sets lbuf (to original (packed) source file) and rbuf
 * (to temporary unpacked target file)! */

static char *
zpths(const struct filediff *f, struct filediff **z2, int tree, size_t *l2,
    size_t i,
    /* 1: is file, not dir */
    /* 2: keep tmpdir */
    int fn)
{
    char *s;
    const char *s2;
	size_t l, l3;
	struct filediff *z;

	if (!(fn & 1))
		i = 0;

	s2 = f->name ? f->name : bmode ? gl_mark : tree == 1 ? mark_lnam :
	    mark_rnam;

	z = malloc(sizeof(struct filediff));
	*z2 = z;
	*z = *f;
	l = strlen(tmp_dir);
	s = malloc(l + 3 + i);
	memcpy(s, tmp_dir, l);

	if (!(fn & 2)) {
		free(tmp_dir);
		tmp_dir = NULL;
	}

	s[l++] = tree == 1 ? 'l' : 'r';

	if (!(fn & 1)) {
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

		if (!(fn & 1))
			z->type[0] = S_IFDIR | S_IRWXU;

		z->type[1] = 0;
	} else {
		s = syspth[1];
		l = pthlen[1];
		z->type[0] = 0;

		if (!(fn & 1))
			z->type[1] = S_IFDIR | S_IRWXU;
	}

	if (*s2 == '/')
        s = const_cast_ptr(s2);
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

void set_path_display_name(const int i)
{
#   if defined(TRACE) && 1
    fprintf(debug, "->set_path_display_name(i=%d)\n", i);
    TRCVPTH;
#   endif
    if (i > 1)
    {
		/* 1 before 0 because of bmode */
        set_path_display_name(1);
        set_path_display_name(0);
        goto ret;
	}
    const int src = bmode ? 1 : i;
    if (sys_path_tmp_len[src] > pthlen[src] || path_display_name_offset[i] > path_display_buffer_size[i])
    {
#       ifdef DEBUG
        printerr("Path offset error", "set_path_display_name()");
#       endif
        goto ret;
	}
    const size_t l = pthlen[src] - sys_path_tmp_len[src];
    while (l >= path_display_buffer_size[i] - path_display_name_offset[i])
    {
        path_display_name[i] = realloc(path_display_name[i], path_display_buffer_size[i] <<= 1);
#       if defined(TRACE) && 1
        fprintf(debug, "  path_display_buffer_size[%d]=%zu\n", i, path_display_buffer_size[i]); // 0 <= i <= 1 !
#       endif
    }
#   if defined(TRACE) && 1
    fprintf(debug, "  memcpy(path_display_name[%d] + %zu, syspth[%d] + sys_path_tmp_len[%d]=%zu, %zu)\n",
            i, path_display_name_offset[i], src, src, sys_path_tmp_len[src], l);
#   endif
    memcpy(path_display_name[i] + path_display_name_offset[i], syspth[src] + sys_path_tmp_len[src], l);
    path_display_name[i][path_display_name_offset[i] + l] = 0;
    ret:
#   if defined(TRACE) && 1
    fprintf(debug, "<-set_path_display_name(i=%d)\n", i);
    TRCVPTH;
#   endif
    return;
}

void set_path_display_name_offset(const int mode, const char *const fn, const char *const tn)
{
#if defined(TRACE) && 1
    fprintf(debug, "->set_path_display_name_offset(mode=%d archive file name fn=\"%s\" temp dir name tn=\"%s\")\n", mode, fn, tn);
#endif
    const size_t l = strlen(fn);
    struct pthofs *const p = malloc(sizeof(*p));
    const int fl =  mode & ~3;
    const int  i = (mode & 3) > 1 ? 1 : mode & 3;
    const bool b = (mode & 3) > 1 ? FALSE : bmode && i ? TRUE : FALSE;

    p->sys = sys_path_tmp_len[i];
    p->view = path_display_name_offset[i];
    p->vpth = !(fl & 4) /* when started from main() vpath[i] is invalid */
            && *fn == '/' ? strdup(path_display_name[i]) : NULL;
	p->next = pthofs[i];
	pthofs[i] = p;
	/* If we are already in a archive and enter another archive,
	 * keep the full current vpath and add the archive name. */
    path_display_name_offset[i] = fl & 4 || *fn == '/' ? 0 :
                 path_display_name_offset[i] ? strlen(path_display_name[i]) : pthlen[bmode ? 1 : i];
    sys_path_tmp_len[i] = strlen(tn);

    while (path_display_buffer_size[i] < path_display_name_offset[i] + l + 3) {
        path_display_name[i] = realloc(path_display_name[i], path_display_buffer_size[i] <<= 1);
	}

	if (!((fl & 4) || *fn == '/')) {
        path_display_name[i][path_display_name_offset[i]++] = '/';
	}

    memcpy(path_display_name[i] + path_display_name_offset[i], fn, l);
    path_display_name[i][path_display_name_offset[i] += l] = '/';
    path_display_name[i][path_display_name_offset[i]] = 0;

	/* Nobody else sets vpath[0] in bmode. But this is needed for
	 * bmode -> dmode. */
	if (b) {
        path_display_name_offset[0] = path_display_name_offset[1];

        while (path_display_buffer_size[0] < path_display_name_offset[0] + l + 3) {
            path_display_name[0] = realloc(path_display_name[0], path_display_buffer_size[0] <<= 1);
		}

        memcpy(path_display_name[0], path_display_name[1], path_display_name_offset[0] + 1);
#if defined(TRACE) && 1
        fprintf(debug, "  vpthsz[0]=%zu vpath[0]=\"%s\"\n", path_display_buffer_size[0], path_display_name[0]);
#endif
	}

#if defined(TRACE) && 1
    fprintf(debug, "<-set_path_display_name_offset(i=%d) path_display_buffer_size[i]=%zu path_display_name[i]=\"%s\"\n",
            i, path_display_buffer_size[i], path_display_name[i]);
#endif
}

/* Called when archive is left */

void reset_path_offsets(int i)
{
	struct pthofs *p;

#if defined(TRACE) && 1
    fprintf(debug, "<>reset_path_offsets(col=%d)\n", i);
#endif
	p = pthofs[i];
	pthofs[i] = p->next;
    sys_path_tmp_len[i] = p->sys;
    path_display_name_offset[i] = p->view;

	if (p->vpth) {
        memcpy(path_display_name[i], p->vpth, strlen(p->vpth) + 1);
		free(p->vpth);
	}

	free(p);
}
