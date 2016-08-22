#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "avlbst.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "db.h"

static int cmp(union bst_val, union bst_val);
static int name_cmp(union bst_val, union bst_val);
static void mk_list(struct bst_node *);
static void del_tree(struct bst_node *);

enum sorting sorting;
unsigned db_num;
struct filediff **db_list;
short noequal;

static struct bst db      = { NULL, cmp },
                  name_db = { NULL, name_cmp };
static unsigned db_idx,
                tot_db_num;

void
db_add(struct filediff *diff)
{
	avl_add(&db, (union bst_val)(void *)diff, (union bst_val)(int)0);
	tot_db_num++;
}

void
add_name(char *name)
{
	avl_add(&name_db, (union bst_val)(void *)strdup(name), (union bst_val)(int)0);
}

int
srch_name(char *name)
{
	return bst_srch(&name_db, (union bst_val)(void *)name, NULL);
}

static int
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

	if (!noequal ||
	    f->diff == '!' || S_ISDIR(f->ltype) || f->ltype != f->rtype)
		db_list[db_idx++] = f;

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
