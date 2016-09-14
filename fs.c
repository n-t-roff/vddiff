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
#ifndef HAVE_FUTIMENS
# include <utime.h>
#endif
#include "compat.h"
#include "main.h"
#include "diff.h"
#include "ui.h"
#include "db.h"
#include "fs.h"
#include "ed.h"

static void proc_dir(void);
static void proc_subdirs(struct bst_node *);
static void rm_file(void);
static void cp_file(void);
static void rebuild_db(void);
static int creatdir(void);
static void cp_link(void);
static void cp_reg(void);

static char *pth1, *pth2;
static size_t len1, len2;
static enum { TREE_RM, TREE_CP } tree_op;

void
fs_rename(int tree)
{
	struct filediff *f = db_list[top_idx + curs];
	char *s;

	/* "en" is not allowed if both files are present */
	if ((tree == 3 && f->ltype && f->rtype) ||
	    (tree == 1 && !f->ltype) ||
	    (tree == 2 && !f->rtype))
		return;

	if (ed_dialog("Enter new name (<ESC> to cancel):", f->name))
		return;

	if ((tree & 2) && f->rtype) {
		pth1 = rpath;
		len1 = rlen;
	} else {
		pth1 = lpath;
		len1 = llen;
	}

	pthcat(pth1, len1, rbuf);
	s = strdup(pth1);
	pthcat(pth1, len1, f->name);

	if (rename(pth1, s) == -1) {
		printerr(strerror(errno), "rename %s failed");
		goto exit;
	}

	rebuild_db();
exit:
	free(s);
	lpath[llen] = 0;
	rpath[rlen] = 0;
}

void
fs_rm(int tree, char *txt)
{
	struct filediff *f = db_list[top_idx + curs];

	/* "dd" is not allowed if both files are present */
	if (tree == 3 && f->ltype && f->rtype)
		return;
	if (!f->ltype)
		tree &= ~1;
	if (!f->rtype)
		tree &= ~2;
	if (tree & 1) {
		pth1 = lpath;
		len1 = llen;
	} else if (tree & 2) {
		pth1 = rpath;
		len1 = rlen;
	} else
		return;

	len1 = pthcat(pth1, len1, f->name);

	if (lstat(pth1, &stat1) == -1) {
		if (errno != ENOENT)
			printerr(strerror(errno), "lstat %s failed", pth1);
		goto cancel;
	}

	if (dialog("[y] yes, [other key] no", NULL,
	    "Really %s %s%s?", txt ? txt : "delete",
	    S_ISDIR(stat1.st_mode) ? "directory " : "", pth1) != 'y')
		goto cancel;

	if (S_ISDIR(stat1.st_mode)) {
		tree_op = TREE_RM;
		proc_dir();
	} else
		rm_file();

	if (txt)
		goto cancel; /* rebuild is done by others */

	rebuild_db();
	return;

cancel:
	lpath[llen] = 0;
	rpath[rlen] = 0;
}

void
fs_cp(int to)
{
	struct filediff *f = db_list[top_idx + curs];

	fs_rm(to, "overwrite");

	if (to == 1) {
		pth1 = rpath;
		len1 = rlen;
		pth2 = lpath;
		len2 = llen;
	} else {
		pth1 = lpath;
		len1 = llen;
		pth2 = rpath;
		len2 = rlen;
	}

	len1 = pthcat(pth1, len1, f->name);
	len2 = pthcat(pth2, len2, f->name);

	if (lstat(pth1, &stat1) == -1) {
		if (errno != ENOENT)
			printerr(strerror(errno), "lstat %s failed", pth1);
		goto cancel;
	}

	if (S_ISDIR(stat1.st_mode)) {
		tree_op = TREE_CP;
		proc_dir();
	} else
		cp_file();

	rebuild_db();
	return;

cancel:
	lpath[llen] = 0;
	rpath[rlen] = 0;
}

static void
rebuild_db(void)
{
	lpath[llen] = 0;
	rpath[rlen] = 0;
	db_free();
	build_diff_db(3);
	disp_list();
}

static void
proc_dir(void)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	struct bst dirs = { NULL, name_cmp };

	if (tree_op == TREE_CP) {
		if (creatdir())
			return;
	}

	if (!(d = opendir(pth1))) {
		printerr(strerror(errno), "opendir %s failed", pth1);
		return;
	}

	while (1) {
		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;

			pth1[len1] = 0;
			printerr(strerror(errno), "readdir %s failed", pth1);
			closedir(d);
			return;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		pthcat(pth1, len1, name);

		if (lstat(pth1, &stat1) == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno),
				    "lstat %s failed", pth1);
				goto closedir;
			}
			continue; /* deleted after readdir */
		}

		if (S_ISDIR(stat1.st_mode)) {
			avl_add(&dirs,
			    (union bst_val)(void *)strdup(name),
			    (union bst_val)(int)0);
		} else if (tree_op == TREE_RM)
			rm_file();
		else {
			pthcat(pth2, len2, name);
			cp_file();
		}
	}

closedir:
	closedir(d);
	pth1[len1] = 0;

	if (tree_op == TREE_CP)
		pth2[len2] = 0;

	proc_subdirs(dirs.root);

	if (tree_op == TREE_RM) {
		if (rmdir(pth1) == -1)
			printerr(strerror(errno),
			    "unlink %s failed", pth1);
	}
}

static void
proc_subdirs(struct bst_node *n)
{
	size_t l1, l2 = 0 /* silence warning */;

	if (!n)
		return;

	proc_subdirs(n->left);
	proc_subdirs(n->right);
	l1 = len1;
	len1 = pthcat(pth1, len1, n->key.p);

	if (tree_op == TREE_CP) {
		l2 = len2;
		len2 = pthcat(pth2, len2, n->key.p);
	}

	proc_dir();
	pth1[len1 = l1] = 0;

	if (tree_op == TREE_CP)
		pth2[len2 = l2] = 0;

	free(n->key.p);
	free(n);
}

static void
rm_file(void)
{
	if (unlink(pth1) == -1)
		printerr(strerror(errno),
		    "unlink %s failed", pth1);
}

static void
cp_file(void)
{
	if (S_ISREG(stat1.st_mode))
		cp_reg();
	else if (S_ISLNK(stat1.st_mode))
		cp_link();
	/* other file types are ignored */
}

static int
creatdir(void)
{
	if (lstat(pth1, &stat1) == -1) {
		if (errno != ENOENT) {
			printerr(strerror(errno),
			    "lstat %s failed", pth1);
		}
		return -1;
	}

	if (mkdir(pth2, stat1.st_mode & 07777) == -1) {
		printerr(strerror(errno), "mkdir %s failed", pth2);
		return -1;
	}

	return 0;
}

static void
cp_link(void)
{
	ssize_t l;
	char *buf = malloc(stat1.st_size + 1);

	if ((l = readlink(pth1, buf, stat1.st_size)) == -1) {
		printerr(strerror(errno), "readlink %s failed", pth1);
		goto exit;
	}

	if (l != stat1.st_size) {
		printerr("Unexpected link lenght", "readlink %s failed", pth1);
		goto exit;
	}

	buf[l] = 0;

	if (symlink(buf, pth2) == -1) {
		printerr(strerror(errno), "symlink %s failed", pth2);
		goto exit;
	}

	/* setting symlink time is not supported on all file systems */

exit:
	free(buf);
}

static void
cp_reg(void)
{
	int f1, f2;
	ssize_t l1, l2;
#ifdef HAVE_FUTIMENS
	struct timespec ts[2];
#else
	struct utimbuf tb;
#endif

	if ((f2 = open(pth2, O_WRONLY | O_CREAT, stat1.st_mode & 07777))
	    == -1) {
		printerr(strerror(errno), "create %s failed", pth2);
		return;
	}

	if (!stat1.st_size)
		goto setattr;

	if ((f1 = open(pth1, O_RDONLY)) == -1) {
		printerr(strerror(errno), "open %s failed", pth1);
		goto close2;
	}

	while (1) {
		if ((l1 = read(f1, lbuf, sizeof lbuf)) == -1) {
			printerr(strerror(errno), "read %s failed", pth1);
			break;
		}

		if (!l1)
			break;

		if ((l2 = write(f2, lbuf, l1)) == -1) {
			printerr(strerror(errno), "write %s failed", pth2);
			break;
		}

		if (l2 != l1)
			break; /* error */

		if (l1 < (ssize_t)(sizeof lbuf))
			break;
	}

	close(f1);

setattr:
#ifdef HAVE_FUTIMENS
	ts[0] = stat1.st_atim;
	ts[1] = stat1.st_mtim;
	futimens(f2, ts); /* error not checked */
#else
	tb.actime  = stat1.st_atime;
	tb.modtime = stat1.st_mtime;
	utime(pth2, &tb);
#endif

close2:
	close(f2);
}
