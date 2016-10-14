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
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef HAVE_FUTIMENS
# include <utime.h>
#endif
#include "compat.h"
#include "main.h"
#include "diff.h"
#include "ui.h"
#include "ui2.h"
#include "uzp.h"
#include "db.h"
#include "fs.h"
#include "ed.h"

struct str_list {
	char *s;
	struct str_list *next;
};

static int proc_dir(int);
static int rm_file(void);
static void cp_file(void);
static int creatdir(int);
static void cp_link(void);
static void cp_reg(void);
static int ask_for_perms(mode_t *);

static char *pth1, *pth2;
static size_t len1, len2;
static enum { TREE_RM, TREE_CP } tree_op;
static bool ign_rm_errs;

void
fs_mkdir(short tree)
{
	if (ed_dialog("Enter name of directory to create (<ESC> to cancel):",
	    "", NULL, 0, NULL))
		return;

	if (tree & 1) {
		pth1 = lpath;
		len1 = llen;
	} else {
		pth1 = rpath;
		len1 = rlen;
	}

	pthcat(pth1, len1, rbuf);

	if (mkdir(pth1, 0777) == -1) {
		printerr(strerror(errno), "mkdir \"%s\" failed", pth1);
		goto exit;
	}

	rebuild_db(0);

exit:
	pth1[len1] = 0;
}

void
fs_rename(int tree)
{
	struct filediff *f;
	char *s;
	size_t l;

	if (!db_num)
		return;

	f = db_list[top_idx + curs];

	/* "en" is not allowed if both files are present */
	if ((tree == 3 && f->ltype && f->rtype) ||
	    (tree == 1 && !f->ltype) ||
	    (tree == 2 && !f->rtype))
		return;

	if (ed_dialog("Enter new name (<ESC> to cancel):", f->name, NULL, 0,
	    NULL))
		return;

	if ((tree & 2) && f->rtype) {
		pth1 = rpath;
		len1 = rlen;
	} else {
		pth1 = lpath;
		len1 = llen;
	}

	l = len1;
	len1 = pthcat(pth1, len1, rbuf);
	s = strdup(pth1);

	if (lstat(pth1, &stat1) == -1) {
		if (errno != ENOENT)
			printerr(strerror(errno), "lstat \"%s\" failed", pth1);
	} else {
		if (dialog(y_n_txt, NULL,
		    "Delete existing %s %s?", S_ISDIR(stat1.st_mode) ?
		    "directory" : "file", pth1) != 'y')
			goto exit;

		if (S_ISDIR(stat1.st_mode)) {
			tree_op = TREE_RM;
			proc_dir(0);
		} else
			rm_file();
	}

	len1 = l;
	pthcat(pth1, len1, f->name);

	if (rename(pth1, s) == -1) {
		printerr(strerror(errno), "rename %s failed");
		goto exit;
	}

	rebuild_db(0);
exit:
	free(s);
	lpath[llen] = 0;

	if (!bmode)
		rpath[rlen] = 0;
}

void
fs_chmod(int tree, int num)
{
	struct filediff *f;
	mode_t m;
	unsigned short u;
	bool have_mode = FALSE;

	if (!db_num)
		return;

	if (num > 1 && dialog(y_n_txt, NULL,
	    "Change mode of %d files?", num) != 'y')
		return;

	u = top_idx + curs;

	while (num-- && u < db_num) {
		f = db_list[u++];

		/* "en" is not allowed if both files are present */
		if ((tree == 3 && f->ltype && f->rtype) ||
		    (tree == 1 && !f->ltype) ||
		    (tree == 2 && !f->rtype))
			continue;

		if ((tree & 2) && f->rtype) {
			if (S_ISLNK(f->rtype))
				continue;

			pth1 = rpath;
			len1 = rlen;

			if (!have_mode)
				m = f->rtype;
		} else {
			if (S_ISLNK(f->ltype))
				continue;

			pth1 = lpath;
			len1 = llen;

			if (!have_mode)
				m = f->ltype;
		}

		if (!have_mode) {
			if (ask_for_perms(&m))
				return;

			have_mode = TRUE;
		}

		pthcat(pth1, len1, f->name);

		if (chmod(pth1, m) == -1) {
			printerr(strerror(errno), "chmod %s failed");
			goto exit;
		}
	}

	rebuild_db(0);
exit:
	lpath[llen] = 0;

	if (!bmode)
		rpath[rlen] = 0;
}

static int
ask_for_perms(mode_t *mode)
{
	mode_t m;
	char *s;
	int i, c;

	snprintf(lbuf, sizeof lbuf, "%04o",
	    (unsigned)*mode & 07777);
	s = strdup(lbuf);

	if (ed_dialog("Enter new permissions (<ESC> to cancel):", s, NULL, 0,
	    NULL)) {
		free(s);
		return 1;
	}

	free(s);

	for (m = 0, i = 0; ; i++) {
		if (!(c = rbuf[i])) {
			if (!i) {
				printerr(NULL, "No input");
				return 1;
			}

			break;
		}

		if (c < '0' || c > '7') {
			printerr(NULL, "Digit '%s' out of range", c);
			return 1;
		}

		if (i > 3) {
			printerr(NULL, "Input has more than 4 digits");
			return 1;
		}

		m <<= 3;
		m |= c - '0';
	}

	*mode = m;
	return 0;
}

void
fs_chown(int tree, int op, int num)
{
	struct filediff *f;
	static struct history owner_hist, group_hist;
	int i;
	struct passwd *pw;
	struct group *gr;
	uid_t uid;
	gid_t gid;
	unsigned short u;
	bool have_owner = FALSE;

	if (!db_num)
		return;

	if (num > 1 && dialog(y_n_txt, NULL,
	    "Change %s of %d files?", op ? "group" : "owner", num) != 'y')
		return;

	u = top_idx + curs;

	while (num-- && u < db_num) {
		f = db_list[u++];

		/* "en" is not allowed if both files are present */
		if ((tree == 3 && f->ltype && f->rtype) ||
		    (tree == 1 && !f->ltype) ||
		    (tree == 2 && !f->rtype))
			continue;

		if ((tree & 2) && f->rtype) {
			if (S_ISLNK(f->rtype))
				continue;

			pth1 = rpath;
			len1 = rlen;
		} else {
			if (S_ISLNK(f->ltype))
				continue;

			pth1 = lpath;
			len1 = llen;
		}

		pthcat(pth1, len1, f->name);

		if (!have_owner && ed_dialog(op ?
		    "Enter new group (<ESC> to cancel):" :
		    "Enter new owner (<ESC> to cancel):", "", NULL, 0,
		    op ? &group_hist : &owner_hist)) {
			return;
		}

		have_owner = TRUE;

		if (op) {
			if ((gr = getgrnam(rbuf)))
				gid = gr->gr_gid;
			else if (!(gid = atoi(rbuf))) {
				printerr("", "Invalid group name \"%s\"", rbuf);
				return;
			}

			i = chown(pth1, -1, gid);
		} else {
			if ((pw = getpwnam(rbuf)))
				uid = pw->pw_uid;
			else if (!(uid = atoi(rbuf))) {
				printerr("", "Invalid user name \"%s\"", rbuf);
				return;
			}

			i = chown(pth1, uid, -1);
		}

		if (i == -1) {
			printerr(strerror(errno), "chown \"%s\", \"%s\" failed",
			    pth1, rbuf);
			goto exit;
		}
	}

	rebuild_db(0);
exit:
	lpath[llen] = 0;

	if (!bmode)
		rpath[rlen] = 0;
}

void
fs_rm(int tree, char *txt, int num)
{
	struct filediff *f;
	unsigned short u, m;

	if (!db_num)
		return;

	m = num > 1;

	if (m && dialog(y_n_txt, NULL,
	    "Really %s %d files?", txt ? txt : "delete", num) != 'y')
		return;

	u = top_idx + curs;

	while (num-- && u < db_num) {
		f = db_list[u++];

		/* "dd" is not allowed if both files are present */
		if (tree == 3 && f->ltype && f->rtype)
			continue;

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
			continue;

		len1 = pthcat(pth1, len1, f->name);

		if (lstat(pth1, &stat1) == -1) {
			if (errno != ENOENT)
				printerr(strerror(errno), "lstat %s failed",
				    pth1);
			continue;
		}

		if (!m && dialog(y_n_txt, NULL,
		    "Really %s %s%s?", txt ? txt : "delete",
		    S_ISDIR(stat1.st_mode) ? "directory " : "", pth1) != 'y')
			goto cancel;

		printerr(NULL, "Deleting %s%s", S_ISDIR(stat1.st_mode) ?
		    "directory " : "", pth1);

		if (S_ISDIR(stat1.st_mode)) {
			tree_op = TREE_RM;
			proc_dir(0);
		} else
			rm_file();
	}

	if (txt)
		goto cancel; /* rebuild is done by others */

	ign_rm_errs = FALSE;
	rebuild_db(0);
	return;

cancel:
	lpath[llen] = 0;

	if (!bmode)
		rpath[rlen] = 0;
}

void
fs_cp(int to, int folw, int num)
{
	struct filediff *f;
	struct stat st;
	unsigned short ti = top_idx;

	if (!db_num)
		return;

	if (num > 1 && dialog(y_n_txt, NULL,
	    "Really copy %d files?", num) != 'y')
		return;

	for (; num-- && top_idx + curs < db_num; top_idx++) {
		if (to == 1) {
			pth1 = rpath;
			len1 = rlen;
		} else {
			pth1 = lpath;
			len1 = llen;
		}

		f = db_list[top_idx + curs];
		pthcat(pth1, len1, f->name);

		if (( folw &&  stat(pth1, &st) == -1) ||
		    (!folw && lstat(pth1, &st) == -1)) {
			if (errno != ENOENT)
				printerr(strerror(errno), "stat %s failed",
				    pth1);
			continue;
		}

		/* After stat src to avoid removing dest if there is a problem
		 * with src */
		fs_rm(to, "overwrite", 1);

		/* fs_rm() did change pths and stat1 */

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
		stat1 = st;

		printerr(NULL, "Copy %s%s -> %s", S_ISDIR(stat1.st_mode) ?
		    "directory " : "", pth1, pth2);

		if (S_ISDIR(stat1.st_mode)) {
			tree_op = TREE_CP;
			proc_dir(folw);
		} else
			cp_file();
	}

	top_idx = ti;
	rebuild_db(0);
	return;
}

/* top_idx and curs must kept unchanged for "//" */

void
rebuild_db(
    /* 0: keep top_idx and curs unchanged
     * 1: keep selected name unchanged */
    short mode)
{
	char *name;

	lpath[llen] = 0;

	if (!bmode)
		rpath[rlen] = 0;

	if (mode)
		name = saveselname();

	/* pointer is freed in next line */
	if (mark && !gl_mark)
		mark_global();

	diff_db_free();
	build_diff_db(bmode ? 1 : 3);

	if (mode && name) {
		center(findlistname(name));
		free(name);
	} else
		disp_list();
}

static int
proc_dir(int folw)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	struct str_list *dirs = NULL;
	short err = 0;

	if (tree_op == TREE_CP && creatdir(folw))
		return err;

	if (!(d = opendir(pth1))) {
		printerr(strerror(errno), "opendir %s failed", pth1);
		return err;
	}

	while (!err) {
		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;

			pth1[len1] = 0;
			printerr(strerror(errno), "readdir %s failed", pth1);
			closedir(d);
			return err;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		pthcat(pth1, len1, name);

		if (( folw &&  stat(pth1, &stat1) == -1) ||
		    (!folw && lstat(pth1, &stat1) == -1)) {
			if (errno != ENOENT) {
				printerr(strerror(errno),
				    "stat %s failed", pth1);
				goto closedir;
			}
			continue; /* deleted after readdir */
		}

		if (S_ISDIR(stat1.st_mode)) {
			struct str_list *se = malloc(sizeof(struct str_list));
			se->s = strdup(name);
			se->next = dirs ? dirs : NULL;
			dirs = se;
		} else if (tree_op == TREE_RM)
			err |= rm_file();
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

	while (dirs) {
		size_t l1, l2 = 0 /* silence warning */;
		struct str_list *p;

		if (!err) {
			l1 = len1;
			len1 = pthcat(pth1, len1, dirs->s);

			if (tree_op == TREE_CP) {
				l2 = len2;
				len2 = pthcat(pth2, len2, dirs->s);
			}

			err |= proc_dir(folw);

			pth1[len1 = l1] = 0;

			if (tree_op == TREE_CP)
				pth2[len2 = l2] = 0;
		}

		free(dirs->s);
		p = dirs;
		dirs = dirs->next;
		free(p);
	}

	if (!err && tree_op == TREE_RM && rmdir(pth1) == -1 && !ign_rm_errs)
		switch (dialog(
		    "<ENTER> continue, <ESC> cancel, 'i' ignore errors",
		    "\ni", "rmdir \"%s\" failed: %s", pth1,
		    strerror(errno))) {
		case '':
			err = 1;
			break;
		case 'i':
			ign_rm_errs = TRUE;
			break;
		}

	return err;
}

static int
rm_file(void)
{
	if (unlink(pth1) == -1 && !ign_rm_errs)
		switch (dialog(
		    "<ENTER> continue, <ESC> cancel, 'i' ignore errors",
		    "\ni", "unlink \"%s\" failed: %s", pth1,
		    strerror(errno))) {
		case '':
			return 1;
		case 'i':
			ign_rm_errs = TRUE;
			break;
		}

	return 0;
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
creatdir(int folw)
{
	if (( folw &&  stat(pth1, &stat1) == -1) ||
	    (!folw && lstat(pth1, &stat1) == -1)) {
		if (errno != ENOENT) {
			printerr(strerror(errno),
			    "stat %s failed", pth1);
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
