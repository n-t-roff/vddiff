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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <avlbst.h>
#include "main.h"
#include "ui.h"
#include "diff.h"
#include "db.h"

static int cmp_link(void);
static int cmp_file(void);
static struct filediff *alloc_diff(char *);
static void proc_subdirs(struct bst_node *);
static void add_diff_dir(void);

static int (*xstat)(const char *, struct stat *) = lstat;
static struct filediff *diff;
static struct bst scan_db = { NULL, name_cmp };

/* 1: Proc left dir
 * 2: Proc right dir
 * 3: Proc both dirs */
int
build_diff_db(int tree)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	size_t l, l2;
	struct bst dirs = { NULL, name_cmp };
	short dir_diff = 0;

	if (!(tree & 1))
		goto right_tree;

	if (!(d = opendir(lpath))) {
		printerr(strerror(errno), "opendir %s failed", lpath);
		return -1;
	}

	while (1) {
		lpath[llen] = 0;
		rpath[rlen] = 0;

		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;
			printerr(strerror(errno), "readdir %s failed", lpath);
			closedir(d);
			return -1;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		add_name(name);
		l2 = llen;
		PTHSEP(lpath, l2);
		l = strlen(name);
		memcpy(lpath + l2, name, l+1);

		if (tree & 2) {
			l2 = rlen;
			PTHSEP(rpath, l2);
			memcpy(rpath + l2, name, l+1);
		}

		if (xstat(lpath, &stat1) == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno), "stat %s failed",
				   lpath);
				continue;
			}

			stat1.st_mode = 0;
		}

		if (!(tree & 2))
			goto no_tree2;

		if (xstat(rpath, &stat2) == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno), "stat %s failed",
				    rpath);
				continue;
			}

no_tree2:
			if (!stat1.st_mode)
				continue; /* ignore two dead links */
			stat2.st_mode = 0;
		}

		if (scan) {
			if (S_ISDIR(stat1.st_mode) &&
			    S_ISDIR(stat2.st_mode)) {
				avl_add(&dirs,
				    (union bst_val)(void *)strdup(name),
				    (union bst_val)(int)0);
				continue;
			}

			if (!*pwd || dir_diff)
				continue;

			if (S_ISREG(stat1.st_mode) &&
			    S_ISREG(stat2.st_mode)) {
				if (cmp_file() == 1)
					dir_diff = 1;
				continue;
			}

			if (S_ISLNK(stat1.st_mode) &&
			    S_ISLNK(stat2.st_mode)) {
				if (cmp_link() == 1)
					dir_diff = 1;
				continue;
			}

			if (real_diff)
				continue;

			if (!stat1.st_mode || !stat2.st_mode ||
			     stat1.st_mode !=  stat2.st_mode) {
				dir_diff = 1;
				continue;
			}

			continue;
		}

		diff = alloc_diff(name);
		diff->ltype = S_IFMT & stat1.st_mode;
		diff->rtype = S_IFMT & stat2.st_mode;

		if (diff->ltype != diff->rtype) {

			db_add(diff);
			continue;

		} else if (stat1.st_ino == stat2.st_ino) {

			diff->diff = '=';
			db_add(diff);
			continue;

		} else if (S_ISREG(stat1.st_mode)) {

			switch (cmp_file()) {
			case 1:
				diff->diff = '!';
				/* fall through */
			case 0:
				db_add(diff);
				continue;
			}

		} else if (S_ISDIR(stat1.st_mode)) {

			db_add(diff);
			continue;

		} else if (S_ISLNK(stat1.st_mode)) {

			switch (cmp_link()) {
			case 1:
				diff->diff = '!';
				/* fall through */
			case 0:
				diff->llink = strdup(lbuf);
				diff->rlink = strdup(rbuf);
				db_add(diff);
				continue;
			}

		/* any other file type */
		} else {
			db_add(diff);
			continue;
		}

		free(diff);
	}

	closedir(d);
	lpath[llen] = 0;
	rpath[rlen] = 0;

right_tree:
	if (!(tree & 2))
		goto build_list;

	if (scan && (real_diff || dir_diff))
		goto dir_scan_end;

	if (!(d = opendir(rpath))) {
		printerr(strerror(errno), "opendir %s failed", rpath);
		return -1;
	}

	while (1) {
		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;
			printerr(strerror(errno), "readdir %s failed", rpath);
			closedir(d);
			return -1;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		if (!srch_name(name))
			continue;

		if (scan) {
			dir_diff = 1;
			break;
		}

		l2 = rlen;
		PTHSEP(rpath, l2);
		l = strlen(name);
		memcpy(rpath + l2, name, l+1);

		if (xstat(rpath, &stat2) == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno), "lstat %s failed",
				    rpath);
				continue;
			}
		}

		diff = alloc_diff(name);
		diff->ltype = 0;
		diff->rtype = S_IFMT & stat2.st_mode;
		db_add(diff);
	}

	closedir(d);

build_list:
	if (!scan)
		db_sort();

dir_scan_end:
	free_names();

	if (!scan)
		return 0;

	if (dir_diff)
		add_diff_dir();

	proc_subdirs(dirs.root);
	return 0;
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
	scan_subdir(n->key.p, 3);
	/* Not done in scan_subdirs(), since there are cases where
	 * scan_subdirs() must not reset the path */
	lpath[llen = l1] = 0;
	rpath[rlen = l2] = 0;

	free(n->key.p);
	free(n);
}

void
scan_subdir(char *name, int tree)
{
	size_t l = strlen(name);

	if (tree & 1) {
		PTHSEP(lpath, llen);
		memcpy(lpath + llen, name, l + 1);
		llen += l;
	}

	if (tree & 2) {
		PTHSEP(rpath, rlen);
		memcpy(rpath + rlen, name, l + 1);
		rlen += l;
	}

	build_diff_db(tree);
}

static void
add_diff_dir(void)
{
	char *path, *end;

	if (!*pwd)
		return;

	path = strdup(PWD);
	end = path + strlen(path);

	while (1) {
		if (!bst_srch(&scan_db, (union bst_val)(void *)path, NULL))
			goto ret;

		avl_add(&scan_db, (union bst_val)(void *)strdup(path),
			    (union bst_val)(int)0);

		do {
			if (--end < path)
				goto ret;
		} while (*end != '/');

		*end = 0;
	}

ret:
	free(path);
}

int
is_diff_dir(char *name)
{
	char *s;
	size_t l1, l2;
	short v;

	if (!recursive)
		return 0;
	if (!*pwd)
		return bst_srch(&scan_db, (union bst_val)(void *)name, NULL) ?
		    0 : 1;
	l1 = strlen(PWD);
	l2 = strlen(name);
	s = malloc(l1 + l2 + 2);
	memcpy(s, PWD, l1);
	PTHSEP(s, l1);
	memcpy(s + l1, name, l2 + 1);
	v = bst_srch(&scan_db, (union bst_val)(void *)s, NULL);
	free(s);
	return v ? 0 : 1;
}

/* -1  Error, don't make DB entry
 *  0  No diff
 *  1  Diff */

static int
cmp_link(void)
{
	ssize_t l1, l2;

	if ((l1 = readlink(lpath, lbuf, sizeof(lbuf) - 1)) == -1) {
		printerr(strerror(errno), "readlink %s failed", lpath);
		return -1;
	}

	if ((l2 = readlink(rpath, rbuf, sizeof(rbuf) - 1)) == -1) {
		printerr(strerror(errno), "readlink %s failed", rpath);
		return -1;
	}

	lbuf[l1] = 0;
	rbuf[l2] = 0;

	if (l1 != l2 || memcmp(lbuf, rbuf, l1))
		return 1;

	return 0;
}

/* -1  Error, don't make DB entry
 *  0  No diff
 *  1  Diff */

static int
cmp_file(void)
{
	int rv = 0, f1, f2;
	ssize_t l1, l2;

	if (stat1.st_size != stat2.st_size)
		return 1;

	if (!stat1.st_size)
		return 0;

	if ((f1 = open(lpath, O_RDONLY)) == -1) {
		printerr(strerror(errno), "open %s failed", lpath);
		return -1;
	}

	if ((f2 = open(rpath, O_RDONLY)) == -1) {
		printerr(strerror(errno), "open %s failed", rpath);
		rv = -1;
		goto close_f1;
	}

	while (1) {
		if ((l1 = read(f1, lbuf, sizeof lbuf)) == -1) {
			printerr(strerror(errno), "read %s failed", lpath);
			rv = -1;
			break;
		}

		if ((l2 = read(f2, rbuf, sizeof rbuf)) == -1) {
			printerr(strerror(errno), "read %s failed", rpath);
			rv = -1;
			break;
		}

		if (l1 != l2) {
			rv = 1;
			break;
		}

		if (!l1)
			break;

		if (memcmp(lbuf, rbuf, l1)) {
			rv = 1;
			break;
		}

		if (l1 < (ssize_t)(sizeof lbuf))
			break;
	}

	close(f2);
close_f1:
	close(f1);
	return rv;
}

static struct filediff *
alloc_diff(char *name)
{
	struct filediff *p = malloc(sizeof(struct filediff));
	p->name  = strdup(name);
	p->llink = NULL; /* to simply use free() later */
	p->rlink = NULL;
	p->diff  = ' ';
	return p;
}

void
follow(int f)
{
	xstat = f ? stat : lstat;
}
