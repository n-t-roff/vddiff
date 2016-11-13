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
#include <regex.h>
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "ui2.h"
#include "uzp.h"
#include "exec.h"
#include "db.h"
#include "gq.h"

#ifdef HAVE_LIBAVLBST
static void *db_new(int (*)(union bst_val, union bst_val));
static int name_cmp(union bst_val, union bst_val);
static int diff_cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void diff_db_delete(struct bst_node *);
static void del_names(struct bst_node *);
#else
struct curs_pos {
	char *path;
	unsigned uv[2];
};

static int name_cmp(const void *, const void *);
static int diff_cmp(const void *, const void *);
static int curs_cmp(const void *, const void *);
static int ext_cmp(const void *, const void *);
static int uz_cmp(const void *, const void *);
static int ptr_db_cmp(const void *, const void *);
static void mk_list(const void *, const VISIT, const int);
#endif

enum sorting sorting;
unsigned db_num, db2_num;
struct filediff **db_list, **db2_list;
static struct filediff **cur_list;
short noequal, real_diff;
void *scan_db;
void *name_db;
void *skipext_db;
void *uz_path_db;
void *alias_db;

static void *curs_db;
static void *ext_db;
static void *uz_ext_db;
static unsigned db_idx, tot_db_num, tot_db2_num;

#ifdef HAVE_LIBAVLBST
static struct bst diff_db = { NULL, diff_cmp };
static struct bst diff_db2 = { NULL, diff_cmp };
#else
static void *diff_db;
static void *diff_db2;
#endif

#ifdef HAVE_LIBAVLBST
void
db_init(void)
{
	scan_db    = db_new(name_cmp);
	name_db    = db_new(name_cmp);
	curs_db    = db_new(name_cmp);
	ext_db     = db_new(name_cmp);
	uz_ext_db  = db_new(name_cmp);
	skipext_db = db_new(name_cmp);
	uz_path_db = db_new(name_cmp);
	alias_db   = db_new(name_cmp);
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

/**************
 * ptr DB *
 **************/

/* 0: Node found */

int
ptr_db_add(void **db, char *key, void *dat)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
	int br;

	if (!(br = bst_srch(*db, (union bst_val)(void *)key, &n)))
		return 0; /* was already in DB */

	avl_add_at(*db, (union bst_val)(void *)key,
	    (union bst_val)(void *)dat, br, n);
	return 1; /* was not in DB, now added */
#else
	struct ptr_db_ent *pe;
	void *vp;

	pe = malloc(sizeof(struct ptr_db_ent));
	pe->key = key;
	pe->dat = dat;
	vp = tsearch(pe, db, ptr_db_cmp);

	if (*(struct ptr_db_ent **)vp != pe) {
		free(pe);
		return 0; /* free mem */
	} else
		return 1; /* don't free mem */
#endif
}

/* 0: Node found */

int
ptr_db_srch(void **db, char *key, void **dat, void **n)
{
#ifdef HAVE_LIBAVLBST
	int i;
	struct bst_node *n1;

	if (!n)
		n = (void **)&n1;

	if (!(i = bst_srch(*db, (union bst_val)(void *)key,
	    (struct bst_node **)n)) && dat)
		*dat = (*(struct bst_node **)n)->data.p;

	return i;
#else
	struct ptr_db_ent pe;
	void *vp;

	pe.key = key;

	vp = tfind(&pe, db, ptr_db_cmp);

	if (vp) {
		if (dat)
			*dat = (*(struct ptr_db_ent **)vp)->dat;

		if (n)
			*n = *(struct ptr_db_ent **)vp;

		return 0;
	} else
		return 1;
#endif
}

void
ptr_db_del(void **db, void *node)
{
#ifdef HAVE_LIBAVLBST
	avl_del_node(*db, node);
#else
	tdelete(node, db, ptr_db_cmp);
	free(node);
#endif
}

void *
ptr_db_get_node(void *db)
{
#ifdef HAVE_LIBAVLBST
	return ((struct bst *)db)->root;
#else
	return db ? *(struct ptr_db_ent **)db : NULL;
#endif
}

#ifndef HAVE_LIBAVLBST
static int
ptr_db_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct ptr_db_ent *)a)->key,
	    ((const struct ptr_db_ent *)b)->key);
}
#endif

/********************
 * simple char * DB *
 ********************/

#ifdef HAVE_LIBAVLBST
void
str_db_add(void **db, char *s, int br, struct bst_node *n)
{
	avl_add_at(*db, (union bst_val)(void *)s,
	    (union bst_val)(int)0, br, n);
}
#else
char *
str_db_add(void **db, char *s)
{
	void *vp;

	vp = tsearch(s, db, name_cmp);
	return *(char **)vp;
}
#endif

#ifdef HAVE_LIBAVLBST
int
str_db_srch(void **db, char *s, struct bst_node **n)
{
	return bst_srch(*db, (union bst_val)(void *)s, n);
}
#else
int
str_db_srch(void **db, char *s)
{
	return tfind(s, db, name_cmp) ? 0 : 1;
}
#endif

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
	const char *s1 = a,
	           *s2 = b;
#endif

	return strcmp(s1, s2);
}

/****************
 * unzip ext DB *
 ****************/

void
uz_db_add(struct uz_ext *p)
{
#ifdef HAVE_LIBAVLBST
	avl_add(uz_ext_db, (union bst_val)(void *)p->str,
	    (union bst_val)(void *)p);
#else
	tsearch(p, &uz_ext_db, uz_cmp);
#endif
}

enum uz_id
uz_db_srch(char *str)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;

	if (!bst_srch(uz_ext_db, (union bst_val)(void *)str, &n))
		return ((struct uz_ext *)n->data.p)->id;
#else
	struct uz_ext key;
	void *vp;

	key.str = str;

	if ((vp = tfind(&key, &uz_ext_db, uz_cmp)))
		return (*(struct uz_ext **)vp)->id;
#endif
	else
		return UZ_NONE;
}

#ifndef HAVE_LIBAVLBST
static int
uz_cmp(const void *a, const void *b)
{
	return strcmp(
	    ((const struct uz_ext *)a)->str,
	    ((const struct uz_ext *)b)->str);
}
#endif

/**********
 * ext DB *
 **********/

void
db_def_ext(char *ext, char *_tool, tool_flags_t flags)
{
	struct tool *t;
#ifndef HAVE_LIBAVLBST
	struct tool key;
#endif

	str_tolower(ext);
#ifdef HAVE_LIBAVLBST
	if (!bst_srch(ext_db, (union bst_val)(void *)ext, NULL)) {
#else
	key.ext = ext;

	if (tfind(&key, &ext_db, ext_cmp)) {
#endif
		printf("Error: Tool for extension \"%s\" set twice\n", ext);
		exit(1);
	} else {
		char *s;

		t = malloc(sizeof(struct tool));
		t->tool = NULL; /* set_tool makes a free() */
		t->args = NULL;

		if (!ptr_db_srch(&alias_db, _tool, (void **)&s, NULL)) {
			free(_tool);
			_tool = strdup(s);
		}

		set_tool(t, _tool, flags);
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
db_set_curs(char *path, unsigned _top_idx, unsigned _curs)
{
	unsigned *uv;

#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
	int br;

	if (!(br = bst_srch(curs_db, (union bst_val)(void *)path, &n))) {
		uv = n->data.p;
	} else {
		uv = malloc(2 * sizeof(unsigned));
		avl_add_at(curs_db, (union bst_val)(void *)strdup(path),
		    (union bst_val)(void *)uv, br, n);
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

	*uv++ = _top_idx;
	*uv   = _curs;
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

	while (name_db) {
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
	diff_db_free(FALSE);
#ifdef HAVE_LIBAVLBST
	diff_db.root = st->bst;
#else
	diff_db = st->bst;
#endif
	db_num  = st->num;
	db_list = st->list;
}

void
diff_db_sort(bool b)
{
	if (b) {
		if (!tot_db2_num)
			return;

		if (!db2_list)
			cur_list = db2_list =
			    malloc(tot_db2_num * sizeof(struct filediff *));

		db_idx = 0;
#ifdef HAVE_LIBAVLBST
		mk_list(diff_db2.root);
#else
		twalk(diff_db2, mk_list);
#endif
		db_num = db_idx;
	} else {
		if (!tot_db_num)
			return;

		if (!db_list)
			cur_list = db_list =
			    malloc(tot_db_num * sizeof(struct filediff *));

		db_idx = 0;
#ifdef HAVE_LIBAVLBST
		mk_list(diff_db.root);
#else
		twalk(diff_db, mk_list);
#endif
		db_num = db_idx;
	}
}

#define PROC_DIFF_NODE() \
	do { \
	if ((!file_pattern || \
	     ((S_ISDIR(f->ltype) || S_ISDIR(f->rtype)) && \
	      (!recursive || is_diff_dir(f->name))) || \
	     ((!find_name || !regexec(&fn_re, f->name, 0, NULL, 0)) && \
	      (!gq_pattern || !gq_proc(f)))) && \
	    \
	    (bmode || \
	     ((!noequal || \
	       f->diff == '!' || S_ISDIR(f->ltype) || \
	       (f->ltype & S_IFMT) != (f->rtype & S_IFMT)) && \
	      \
	      (!real_diff || \
	       f->diff == '!' || (S_ISDIR(f->ltype) && S_ISDIR(f->rtype) && \
	       is_diff_dir(f->name))) && \
	      \
	      (!nosingle || \
	       (f->ltype && f->rtype))))) \
	{ \
		cur_list[db_idx++] = f; \
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
		f = *(struct filediff * const *)n;
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
	const struct filediff
#ifdef HAVE_LIBAVLBST
	    *f1 = a.p,
	    *f2 = b.p;
#else
	    *f1 = a,
	    *f2 = b;
#endif

	if (sorting == SORTMTIME) {
		time_t t1, t2;

		t1 = f1->ltype ? f1->lmtim : f1->rmtim;
		t2 = f2->ltype ? f2->lmtim : f2->rmtim;

		if      (t1 < t2) return -1;
		else if (t1 > t2) return  1;

	} else if (sorting == SORTSIZE) {
		off_t t1, t2;
		short f1_dir = IS_F_DIR(1),
		      f2_dir = IS_F_DIR(2);
		short dirsort = f1_dir && !f2_dir ? -1 :
		                f2_dir && !f1_dir ?  1 : 0;

		if (dirsort)
			return dirsort;

		t1 = f1->ltype ? f1->lsiz : f1->rsiz;
		t2 = f2->ltype ? f2->lsiz : f2->rsiz;

		if      (t1 < t2) return -1;
		else if (t1 > t2) return  1;

	} else if (sorting != SORTMIXED) {
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
diff_db_add(struct filediff *diff, bool b)
{
#ifdef HAVE_LIBAVLBST
	avl_add(b ? &diff_db2 : &diff_db,
	    (union bst_val)(void *)diff, (union bst_val)(int)0);
#else
	tsearch(diff, b ? &diff_db2 : &diff_db, diff_cmp);
#endif
	if (b)
		tot_db2_num++;
	else
		tot_db_num++;
}

void
diff_db_free(bool b)
{
#ifndef HAVE_LIBAVLBST
	struct filediff *f;
#endif
	if (b) {
#ifdef HAVE_LIBAVLBST
		diff_db_delete(diff_db2.root);
		diff_db2.root = NULL;
#else
		while (diff_db2 != NULL) {
			f = *(struct filediff **)diff_db2;
			tdelete(f, &diff_db2, diff_cmp);
			free_diff(f);
		}
#endif
		free(db2_list);
		db2_list = NULL;
		db2_num = 0;
	} else {
#ifdef HAVE_LIBAVLBST
		diff_db_delete(diff_db.root);
		diff_db.root = NULL;
#else
		while (diff_db != NULL) {
			f = *(struct filediff **)diff_db;
			tdelete(f, &diff_db, diff_cmp);
			free_diff(f);
		}
#endif
		free(db_list);
		db_list = NULL;
		db_num = 0;
	}
}

/* In the libavlbst case the nodes are not really deleted, just the memory
 * is freed after both subtrees had been visited.  This is much faster than
 * rebalancing the tree for each delete.  It is not dangerous since the tree
 * structure does not change due to the node memory free. */

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
	free_diff(f);
	free(n);
}
#endif

/********
 * misc *
 ********/

char *
str_tolower(char *in)
{
	int c;
	char *s = in;

	while ((c = *s))
		*s++ = tolower(c);

	return in;
}
