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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <search.h>
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"
#include "exec.h"

struct curs_pos {
	char *path;
	unsigned uv[2];
};

#ifdef HAVE_LIBAVLBST
static void *db_new(int (*)(union bst_val, union bst_val));
static int name_cmp(union bst_val, union bst_val);
static int diff_cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void diff_db_delete(struct bst_node *);
static void del_names(struct bst_node *);
#else
static int name_cmp(const void *, const void *);
static int diff_cmp(const void *, const void *);
static int curs_cmp(const void *, const void *);
static int ext_cmp(const void *, const void *);
static void mk_list(const void *, const VISIT, const int);
#endif

enum sorting sorting;
unsigned db_num;
struct filediff **db_list;
short noequal, real_diff;
void *scan_db;
void *name_db;

static void *curs_db;
static void *ext_db;
static unsigned db_idx, tot_db_num;

#ifdef HAVE_LIBAVLBST
static struct bst diff_db = { NULL, diff_cmp };
#else
static void *diff_db;
#endif

#ifdef HAVE_LIBAVLBST
void
db_init(void)
{
	scan_db = db_new(name_cmp);
	name_db = db_new(name_cmp);
	curs_db = db_new(name_cmp);
	ext_db  = db_new(name_cmp);
}

static void *
db_new(int (*compare)(union bst_val, union bst_val))
{
	struct bst *bst;

	bst = malloc(sizeof(struct bst));
	bst->root = NULL;
	bst->cmp = compare;
	return (void *)bst;
}
#endif

/********************
 * simple char * DB *
 ********************/

void
str_db_add(void **db, char *s)
{
#ifdef HAVE_LIBAVLBST
	avl_add(*db, (union bst_val)(void *)strdup(s),
	    (union bst_val)(int)0);
#else
	tsearch(strdup(s), db, name_cmp);
#endif
}

int
str_db_srch(void **db, char *s)
{
#ifdef HAVE_LIBAVLBST
	return bst_srch(*db, (union bst_val)(void *)s, NULL);
#else
	return tfind(s, db, name_cmp) ? 0 : 1;
#endif
}

static int
name_cmp(
#ifdef HAVE_LIBAVLBST
    union bst_val a, union bst_val b
#else
    const void *a, const void *b
#endif
    )
{
#ifdef HAVE_LIBAVLBST
	char *s1 = a.p,
	     *s2 = b.p;
#else
	char *s1 = (char *)a,
	     *s2 = (char *)b;
#endif

	return strcmp(s1, s2);
}

/**********
 * ext DB *
 **********/

void
db_def_ext(char *ext, char *tool, int bg)
{
	struct tool *t;
	char *s;
	int c;

#ifdef HAVE_LIBAVLBST
	struct bst_node *n;

	if (!bst_srch(ext_db, (union bst_val)(void *)ext, &n)) {
		t = n->data.p;
#else
	struct tool key;
	void *vp;

	key.ext = ext;

	if ((vp = tfind(&key, &ext_db, ext_cmp))) {
		t = *(struct tool **)vp;
#endif
		free(ext);
		free(*t->tool);
		*t->tool = tool;
	} else {
		for (s = tool; (c = *s); s++)
			*s = tolower(c);

		t = malloc(sizeof(struct tool));
		*t->tool = tool;
		(t->tool)[1] = NULL;
		(t->tool)[2] = NULL;
		t->bg = bg;
#ifdef HAVE_LIBAVLBST
		avl_add(ext_db, (union bst_val)(void *)ext,
		    (union bst_val)(void *)t);
#else
		t->ext = ext;
		tsearch(t, &ext_db, ext_cmp);
#endif
	}
}

struct tool *
db_srch_ext(char *ext)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;

	if (!bst_srch(ext_db, (union bst_val)(void *)ext, &n))
		return n->data.p;
#else
	struct tool key;
	void *vp;

	key.ext = ext;

	if ((vp = tfind(&key, &ext_db, ext_cmp)))
		return *(struct tool **)vp;
#endif
	else
		return NULL;
}

#ifndef HAVE_LIBAVLBST
static int
ext_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct tool *)a)->ext,
	    ((const struct tool *)b)->ext);
}
#endif

/***********
 * curs DB *
 ***********/

void
db_set_curs(char *path, unsigned top_idx, unsigned curs)
{
	unsigned *uv;

#ifdef HAVE_LIBAVLBST
	struct bst_node *n;

	if (!bst_srch(curs_db, (union bst_val)(void *)path, &n)) {
		uv = n->data.p;
	} else {
		uv = malloc(2 * sizeof(unsigned));
		avl_add(curs_db, (union bst_val)(void *)strdup(path),
		    (union bst_val)(void *)uv);
	}
#else
	struct curs_pos *cp, *cp2;
	void *vp;

	cp = malloc(sizeof(struct curs_pos));
	cp->path = strdup(path);
	vp = tsearch(cp, &curs_db, curs_cmp);
	cp2 = *(struct curs_pos **)vp;

	if (cp2 != cp) {
		free(cp->path);
		free(cp);
	}

	if (!cp2)
		return;

	uv = (unsigned *)&cp2->uv;
#endif

	*uv++ = top_idx;
	*uv   = curs;
}

unsigned *
db_get_curs(char *path)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;

	if (!bst_srch(curs_db, (union bst_val)(void *)path, &n))
		return n->data.p;
#else
	struct curs_pos *cp, key;
	void *vp;

	key.path = path;

	if ((vp = tfind(&key, &curs_db, curs_cmp))) {
		cp = *(struct curs_pos **)vp;
		return (unsigned *)&cp->uv;
	}
#endif
	else
		return NULL;
}

#ifndef HAVE_LIBAVLBST
static int
curs_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct curs_pos *)a)->path,
	    ((const struct curs_pos *)b)->path);
}
#endif

/***********
 * name DB *
 ***********/

void
free_names(void)
{
#ifdef HAVE_LIBAVLBST
	del_names(((struct bst *)name_db)->root);
	((struct bst *)name_db)->root = NULL;
#else
	char *s;

	while (name_db != NULL) {
		s = *(char **)name_db;
		tdelete(s, &name_db, name_cmp);
		free(s);
	}
#endif
}

#ifdef HAVE_LIBAVLBST
static void
del_names(struct bst_node *n)
{
	if (!n)
		return;

	del_names(n->left);
	del_names(n->right);
	free(n->key.p);
	free(n);
}
#endif

/***********
 * diff DB *
 ***********/

void
diff_db_store(struct ui_state *st)
{
#ifdef HAVE_LIBAVLBST
	st->bst  = diff_db.root; diff_db.root = NULL;
#else
	st->bst  = diff_db; diff_db = NULL;
#endif
	st->num  = db_num ; db_num  = 0;
	st->list = db_list; db_list = NULL;
}

void
diff_db_restore(struct ui_state *st)
{
	diff_db_free();
#ifdef HAVE_LIBAVLBST
	diff_db.root = st->bst;
#else
	diff_db = st->bst;
#endif
	db_num  = st->num;
	db_list = st->list;
}

void
diff_db_sort(void)
{
	if (!tot_db_num)
		return;
	if (!db_list)
		db_list = malloc(tot_db_num * sizeof(struct filediff *));
	db_idx = 0;
#ifdef HAVE_LIBAVLBST
	mk_list(diff_db.root);
#else
	twalk(diff_db, mk_list);
#endif
	db_num = db_idx;
}

#define PROC_DIFF_NODE() \
	do { \
	if (bmode || \
	    ((!noequal || \
	      f->diff == '!' || S_ISDIR(f->ltype) || \
	      (f->ltype & S_IFMT) != (f->rtype & S_IFMT)) && \
	     (!real_diff || \
	      f->diff == '!' || (S_ISDIR(f->ltype) && S_ISDIR(f->rtype) && \
	      is_diff_dir(f->name))))) \
	{ \
		db_list[db_idx++] = f; \
	} \
	} while (0)

#ifdef HAVE_LIBAVLBST
static void
mk_list(struct bst_node *n)
{
	struct filediff *f;

	if (!n)
		return;

	mk_list(n->left);
	f = n->key.p;
	PROC_DIFF_NODE();
	mk_list(n->right);
}
#else
static void
mk_list(const void *n, const VISIT which, const int depth)
{
	struct filediff *f;

	(void)depth;

	switch (which) {
	case postorder:
	case leaf:
		f = *(struct filediff **)n;
		PROC_DIFF_NODE();
		break;
	default:
		;
	}
}
#endif

#define IS_F_DIR(n) \
    /* both are dirs */ \
    (S_ISDIR(f##n->ltype) && S_ISDIR(f##n->rtype)) || \
    /* only left dir present */ \
    (S_ISDIR(f##n->ltype) && !f##n->rtype) || \
    /* only right dir present */ \
    (S_ISDIR(f##n->rtype) && !f##n->ltype)

static int
diff_cmp(
#ifdef HAVE_LIBAVLBST
    union bst_val a, union bst_val b
#else
    const void *a, const void *b
#endif
    )
{
	struct filediff
#ifdef HAVE_LIBAVLBST
	    *f1 = a.p,
	    *f2 = b.p;
#else
	    *f1 = (struct filediff *)a,
	    *f2 = (struct filediff *)b;
#endif

	if (sorting != SORTMIXED) {
		short f1_dir = IS_F_DIR(1),
		      f2_dir = IS_F_DIR(2);
		short dirsort = f1_dir && !f2_dir ? -1 :
		                f2_dir && !f1_dir ?  1 : 0;

		if (dirsort) {
			if (sorting == DIRSFIRST)
				return  dirsort;
			else
				return -dirsort;
		}
	}

	return strcmp(f1->name, f2->name);
}

void
diff_db_add(struct filediff *diff)
{
#ifdef HAVE_LIBAVLBST
	avl_add(&diff_db, (union bst_val)(void *)diff, (union bst_val)(int)0);
#else
	tsearch(diff, &diff_db, diff_cmp);
#endif
	tot_db_num++;
}

#define FREE_DIFF(f) \
	free(f->name); \
	free(f->llink); \
	free(f->rlink); \
	free(f);

void
diff_db_free(void)
{
#ifdef HAVE_LIBAVLBST
	diff_db_delete(diff_db.root);
	diff_db.root = NULL;
#else
	struct filediff *f;

	while (diff_db != NULL) {
		f = *(struct filediff **)diff_db;
		tdelete(f, &diff_db, diff_cmp);
		FREE_DIFF(f);
	}
#endif
	free(db_list);
	db_list = NULL;
}

#ifdef HAVE_LIBAVLBST
static void
diff_db_delete(struct bst_node *n)
{
	struct filediff *f;

	if (!n)
		return;

	diff_db_delete(n->left);
	diff_db_delete(n->right);
	f = n->key.p;
	FREE_DIFF(f);
	free(n);
}
#endif
