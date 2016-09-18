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
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"
#include "exec.h"

static int cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void del_tree(struct bst_node *);
static void proc_subdirs(struct bst_node *);

enum sorting sorting;
unsigned db_num;
struct filediff **db_list;
short noequal, real_diff;

static struct bst db      = { NULL, cmp },
                  name_db = { NULL, name_cmp },
                  ext_db  = { NULL, name_cmp },
                  curs_db = { NULL, name_cmp };
static unsigned db_idx,
                tot_db_num;
static void *scan_db;

/* alloc space for empty DB tree */

void
db_init(void)
{
	scan_db = db_new(name_cmp);
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

/* free empty DB tree */

void
db_destroy(void *t)
{
	free(t);
}

void
db_scan_add(void *db, char *name)
{
	avl_add(db, (union bst_val)(void *)strdup(name),
	    (union bst_val)(int)0);
}

void
db_scan_walk(void *db)
{
	proc_subdirs(((struct bst *)db)->root);
}

static void
proc_subdirs(struct bst_node *n)
{
	size_t l1, l2;

	if (!n)
		return;

	proc_subdirs(n->left);
	proc_subdirs(n->right);

	l1 = llen;
	l2 = rlen;
	scan_subdir(n->key.p, NULL, 3);
	/* Not done in scan_subdirs(), since there are cases where
	 * scan_subdirs() must not reset the path */
	lpath[llen = l1] = 0;
	rpath[rlen = l2] = 0;

	free(n->key.p);
	free(n);
}

int
scan_db_find(char *path)
{
	return bst_srch(scan_db, (union bst_val)(void *)path, NULL);
}

void
scan_db_add(char *path)
{
	avl_add(scan_db, (union bst_val)(void *)strdup(path),
		    (union bst_val)(int)0);
}

void
db_def_ext(char *ext, char *tool, int bg)
{
	struct bst_node *n;
	struct tool *t;
	char *s;
	int c;

	if (bst_srch(&ext_db, (union bst_val)(void *)ext, &n)) {
		for (s = tool; (c = *s); s++)
			*s = tolower(c);

		t = malloc(sizeof(struct tool));
		*t->tool = tool;
		(t->tool)[1] = NULL;
		(t->tool)[2] = NULL;
		t->bg = bg;
		avl_add(&ext_db, (union bst_val)(void *)ext,
		    (union bst_val)(void *)t);
	} else {
		free(ext);
		t = n->data.p;
		free(*t->tool);
		*t->tool = tool;
	}
}

struct tool *
db_srch_ext(char *ext)
{
	struct bst_node *n;

	if (bst_srch(&ext_db, (union bst_val)(void *)ext, &n))
		return NULL;
	else
		return n->data.p;
}

void
db_set_curs(char *path, unsigned top_idx, unsigned curs)
{
	struct bst_node *n;
	unsigned *uv;

	if (!bst_srch(&curs_db, (union bst_val)(void *)path, &n)) {
		uv = n->data.p;
	} else {
		uv = malloc(2 * sizeof(unsigned));
		avl_add(&curs_db, (union bst_val)(void *)strdup(path),
		    (union bst_val)(void *)uv);
	}

	*uv++ = top_idx;
	*uv   = curs;
}

unsigned *
db_get_curs(char *path)
{
	struct bst_node *n;

	if (bst_srch(&curs_db, (union bst_val)(void *)path, &n))
		return NULL;
	else
		return n->data.p;
}

void
db_add(struct filediff *diff)
{
	avl_add(&db, (union bst_val)(void *)diff, (union bst_val)(int)0);
	tot_db_num++;
}

void
add_name(char *name)
{
	avl_add(&name_db, (union bst_val)(void *)strdup(name),
	    (union bst_val)(int)0);
}

int
srch_name(char *name)
{
	return bst_srch(&name_db, (union bst_val)(void *)name, NULL);
}

int
name_cmp(union bst_val a, union bst_val b)
{
	char *s1 = a.p,
	     *s2 = b.p;

	return strcmp(s1, s2);
}

#define IS_F_DIR(n) \
    /* both are dirs */ \
    (S_ISDIR(f##n->ltype) && S_ISDIR(f##n->rtype)) || \
    /* only left dir present */ \
    (S_ISDIR(f##n->ltype) && !f##n->rtype) || \
    /* only right dir present */ \
    (S_ISDIR(f##n->rtype) && !f##n->ltype)

static int
cmp(union bst_val a, union bst_val b)
{
	struct filediff *f1 = a.p,
	                *f2 = b.p;

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
db_sort(void)
{
	if (!tot_db_num)
		return;
	if (!db_list)
		db_list = malloc(tot_db_num * sizeof(struct filediff *));
	db_idx = 0;
	mk_list(db.root);
	db_num = db_idx;
}

static void
mk_list(struct bst_node *n)
{
	struct filediff *f;

	if (!n)
		return;
	mk_list(n->left);
	f = n->key.p;

	if (bmode ||
	    ((!noequal ||
	      f->diff == '!' || S_ISDIR(f->ltype) ||
	      (f->ltype & S_IFMT) != (f->rtype & S_IFMT)) &&
	     (!real_diff ||
	      f->diff == '!' || (S_ISDIR(f->ltype) && S_ISDIR(f->rtype) &&
	      is_diff_dir(f->name)))))
	{
		db_list[db_idx++] = f;
	}

	mk_list(n->right);
}

static void
del_tree(struct bst_node *n)
{
	struct filediff *f;

	if (!n)
		return;

	del_tree(n->left);
	del_tree(n->right);
	f = n->key.p;
	free(f->name);
	free(f->llink);
	free(f->rlink);
	free(f);
	free(n);
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

void
db_store(struct ui_state *st)
{
	st->bst  = db.root; db.root = NULL;
	st->num  = db_num ; db_num  = 0;
	st->list = db_list; db_list = NULL;
}

void
db_restore(struct ui_state *st)
{
	db_free();
	db.root = st->bst;
	db_num  = st->num;
	db_list = st->list;
}

void
db_free(void)
{
	del_tree(db.root); db.root = NULL;
	free(db_list)    ; db_list = NULL;
}

void
free_names(void)
{
	del_names(name_db.root); name_db.root = NULL;
}
