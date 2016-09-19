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
#include <avlbst.h>
#include <ctype.h>
#include <search.h>
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"
#include "exec.h"

static void del_names(struct bst_node *);

#ifdef HAVE_LIBAVLBST
static int diff_cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void diff_db_delete(struct bst_node *);
#else
static int diff_cmp(const void *, const void *);
static void mk_list(const void *, const VISIT, const int);
#endif

enum sorting sorting;
unsigned db_num;
struct filediff **db_list;
short noequal, real_diff;
void *scan_db;
void *name_db;
void *curs_db;
void *ext_db;

static unsigned db_idx, tot_db_num;

#ifdef HAVE_LIBAVLBST
static struct bst diff_db = { NULL, diff_cmp };
#else
static void *diff_db;
#endif

void
db_init(void)
{
	scan_db = db_new(name_cmp);
	name_db = db_new(name_cmp);
	curs_db = db_new(name_cmp);
	ext_db = db_new(name_cmp);
}

void *
db_new(int (*compare)(union bst_val, union bst_val))
{
	struct bst *bst;

	bst = malloc(sizeof(struct bst));
	bst->root = NULL;
	bst->cmp = compare;
	return (void *)bst;
}

void
db_destroy(void *t)
{
	free(t);
}

void
str_db_add(void *db, char *s)
{
	avl_add(db, (union bst_val)(void *)strdup(s),
	    (union bst_val)(int)0);
}

void *
db_srch_str(void *db, char *s)
{
	struct bst_node *n;

	if (bst_srch(db, (union bst_val)(void *)s, &n))
		return NULL;
	else
		return n;
}

void
db_def_ext(char *ext, char *tool, int bg)
{
	struct bst_node *n;
	struct tool *t;
	char *s;
	int c;

	if (bst_srch(ext_db, (union bst_val)(void *)ext, &n)) {
		for (s = tool; (c = *s); s++)
			*s = tolower(c);

		t = malloc(sizeof(struct tool));
		*t->tool = tool;
		(t->tool)[1] = NULL;
		(t->tool)[2] = NULL;
		t->bg = bg;
		avl_add(ext_db, (union bst_val)(void *)ext,
		    (union bst_val)(void *)t);
	} else {
		free(ext);
		t = n->data.p;
		free(*t->tool);
		*t->tool = tool;
	}
}

void
db_set_curs(char *path, unsigned top_idx, unsigned curs)
{
	struct bst_node *n;
	unsigned *uv;

	if (!bst_srch(curs_db, (union bst_val)(void *)path, &n)) {
		uv = n->data.p;
	} else {
		uv = malloc(2 * sizeof(unsigned));
		avl_add(curs_db, (union bst_val)(void *)strdup(path),
		    (union bst_val)(void *)uv);
	}

	*uv++ = top_idx;
	*uv   = curs;
}

int
name_cmp(union bst_val a, union bst_val b)
{
	char *s1 = a.p,
	     *s2 = b.p;

	return strcmp(s1, s2);
}

void
free_names(void)
{
	del_names(((struct bst *)name_db)->root);
	((struct bst *)name_db)->root = NULL;
}

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

#define FREE_DIFF() \
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
	free(db_list);
	db_list = NULL;
#else
	struct filediff *f;

	while (diff_db != NULL) {
		f = *(struct filediff **)diff_db;
		tdelete(f, &diff_db, diff_cmp);
		FREE_DIFF();
	}
#endif
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
	FREE_DIFF();
	free(n);
}
#endif
