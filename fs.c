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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <avlbst.h>
#include "compat.h"
#include "main.h"
#include "diff.h"
#include "ui.h"
#include "db.h"
#include "fs.h"

static void proc_dir(void);
static void proc_subdirs(struct bst_node *);
static void rm_file(void);
static void cp_file(void);

static char *lp, *rp;
static size_t ll, rl;
static enum { TREE_RM, TREE_CP } tree_op;

void
fs_rm(int tree, char *txt)
{
	struct filediff *f = db_list[top_idx + curs];
	size_t ln;

	if (f->ltype)
		tree &= ~2; /* then not right */
	if (f->rtype)
		tree &= ~1; /* then not left */
	if (tree & 1) {
		lp = lpath;
		ll = llen;
	} else if (tree & 2) {
		lp = rpath;
		ll = rlen;
	} else
		return;
	PTHSEP(lp, ll);
	ln = strlen(f->name);
	memcpy(lp + ll, f->name, ln + 1);
	if (lstat(lp, &stat1) == -1) {
		if (errno != ENOENT) {
			printerr(strerror(errno),
			    "lstat %s failed", lp);
			return;
		}
	}
	if (dialog("[y] yes, [other key] no", NULL,
	    "Really %s %s?", txt ? txt : "delete", lp) != 'y')
		return;
	if (S_ISDIR(stat1.st_mode)) {
		tree_op = TREE_RM;
		proc_dir();
	} else
		rm_file();
}

void
fs_cp(int from, int to)
{
	struct filediff *f = db_list[top_idx + curs];

	fs_rm(to, "overwrite");
}

static void
proc_dir(void)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	size_t l;
	struct bst dirs = { NULL, name_cmp };

	if (!(d = opendir(lp))) {
		printerr(strerror(errno), "opendir %s failed", lp);
		return;
	}

	while (1) {
		lp[ll] = 0;

		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;
			printerr(strerror(errno), "readdir %s failed", lpath);
			closedir(d);
			return;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		PTHSEP(lp, ll);
		l = strlen(name);
		memcpy(lp + ll--, name, l+1);

		if (lstat(lp, &stat1) == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno),
				    "lstat %s failed", lp);
				return;
			}
		}

		if (S_ISDIR(stat1.st_mode)) {
			avl_add(&dirs,
			    (union bst_val)(void *)strdup(name),
			    (union bst_val)(int)0);
		} else if (tree_op == TREE_RM)
			rm_file();
		else
			cp_file();
	}

	closedir(d);
	lp[ll] = 0;
	proc_subdirs(dirs.root);

	if (tree_op == TREE_RM) {
		if (rmdir(lp) == -1)
			printerr(strerror(errno),
			    "unlink %s failed", lp);
	}
}

static void
proc_subdirs(struct bst_node *n)
{
	size_t l1, l2;

	if (!n)
		return;

	proc_subdirs(n->left);
	proc_subdirs(n->right);
	l1 = ll;
	l2 = rl;
	PTHSEP(lp, ll);
	PTHSEP(rp, rl);
	proc_dir();
	lp[ll = l1] = 0;
	rp[rl = l2] = 0;
	free(n->key.p);
	free(n);
}

static void
rm_file(void)
{
	if (unlink(lp) == -1)
		printerr(strerror(errno),
		    "unlink %s failed", lp);
}

static void
cp_file(void)
{
}
