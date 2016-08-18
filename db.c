#include <string.h>
#include <stdlib.h>
#include <sys/types.h> /* for diff.h */
#include "avlbst.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"

static int cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void del_tree(struct bst_node *);

unsigned db_num;
struct filediff **db_list;

static struct bst db = { NULL, cmp };
static enum sorting db_sorting;
static unsigned db_idx;

void
db_add(struct filediff *diff)
{
	db_sorting = sorting;
	avl_add(&db, (union bst_val)(void *)diff, (union bst_val)(int)0);
	db_num++;
}

int
db_srch(char *name)
{
	static struct filediff d;

	d.name = name;
	db_sorting = SORTMIXED;
	return bst_srch(&db, (union bst_val)(void *)&d, NULL);
}

static int
cmp(union bst_val a, union bst_val b)
{
	return strcmp(((struct filediff *)(a.p))->name,
	    ((struct filediff *)(b.p))->name);
}

void
db_sort(void)
{
	if (!db_num)
		return;
	db_list = malloc(db_num * sizeof(struct filediff *));
	db_idx = 0;
	mk_list(db.root);
}

static void
mk_list(struct bst_node *n)
{
	if (!n)
		return;
	mk_list(n->left);
	db_list[db_idx++] = n->key.p;
	mk_list(n->right);
}

static void
del_tree(struct bst_node *n)
{
	struct filediff *f;

	if (!n)
		return;

	f = n->key.p;
	del_tree(n->left);
	del_tree(n->right);
	free(f->name);
	free(f->llink);
	free(f->rlink);
	free(f);
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
