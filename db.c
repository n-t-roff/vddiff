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

unsigned db_num;
struct filediff **db_list;

static struct bst db      = { NULL, cmp },
                  name_db = { NULL, name_cmp };
static unsigned db_idx;

void
db_add(struct filediff *diff)
{
	avl_add(&db, (union bst_val)(void *)diff, (union bst_val)(int)0);
	db_num++;
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

static int
cmp(union bst_val a, union bst_val b)
{
	struct filediff *f1 = a.p,
	                *f2 = b.p;

	if (sorting == FILESFIRST) {
		if (!S_ISDIR(f1->ltype) && !S_ISDIR(f1->rtype) &&
		    (S_ISDIR(f2->ltype) ||  S_ISDIR(f2->rtype)))
			return 1;
		if (!S_ISDIR(f2->ltype) && !S_ISDIR(f2->rtype) &&
		    (S_ISDIR(f1->ltype) ||  S_ISDIR(f1->rtype)))
			return -1;
	} else if (sorting == DIRSFIRST) {
		if (!S_ISDIR(f1->ltype) && !S_ISDIR(f1->rtype) &&
		    (S_ISDIR(f2->ltype) ||  S_ISDIR(f2->rtype)))
			return -1;
		if (!S_ISDIR(f2->ltype) && !S_ISDIR(f2->rtype) &&
		    (S_ISDIR(f1->ltype) ||  S_ISDIR(f1->rtype)))
			return 1;
	}

	return strcmp(f1->name, f2->name);
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
