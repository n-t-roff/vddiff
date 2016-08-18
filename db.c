#include <string.h>
#include <stdlib.h>
#include <sys/types.h> /* for diff.h */
#include "avlbst.h"
#include "diff.h"
#include "main.h"

static int cmp(union bst_val, union bst_val);
static void db_trav(struct bst_node *);

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
	db_list = malloc(db_num * sizeof(struct filediff *));
	db_trav(db.root);
}

static void
db_trav(struct bst_node *n)
{
	if (!n)
		return;
	db_trav(n->left);
	db_list[db_idx++] = n->key.p;
	db_trav(n->right);
}
