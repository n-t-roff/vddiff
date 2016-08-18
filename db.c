#include <string.h>
#include <sys/types.h> /* for diff.h */
#include "avlbst.h"
#include "diff.h"
#include "main.h"

static int cmp(union bst_val, union bst_val);

struct bst db = { NULL, cmp };

static enum sorting db_sort;

void
db_add(struct filediff *diff)
{
	db_sort = sorting;
	avl_add(&db, (union bst_val)(void *)diff, (union bst_val)(int)0);
}

int
db_srch(char *name)
{
	static struct filediff d;

	d.name = name;
	db_sort = SORTMIXED;
	return bst_srch(&db, (union bst_val)(void *)&d, NULL);
}

static int
cmp(union bst_val a, union bst_val b)
{
	return strcmp(((struct filediff *)(a.p))->name,
	    ((struct filediff *)(b.p))->name);
}
