/*
Copyright (c) 2016-2017, Carsten Kunze <carsten.kunze@arcor.de>

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

#include <stdarg.h>
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
#include <regex.h>
#include <time.h>
#include <signal.h>
#ifndef HAVE_FUTIMENS
# include <utime.h>
#endif
#include "compat.h"
#include "main.h"
#include "diff.h"
#include "ui.h"
#include "ui2.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "fs.h"
#include "ed.h"
#include "tc.h"

struct str_list {
	char *s;
	struct str_list *next;
};

static void proc_dir(void);
static void rm_file(void);
static int cp_file(void);
static int creatdir(void);
static int cp_link(void);
static int cp_reg(void);
static int ask_for_perms(mode_t *);
static int fs_ro(void);
static void fs_fwrap(const char *, ...);
static int fs_stat(const char *, struct stat *);
static int fs_deldialog(const char *, const char *, const char *,
    const char *);

static time_t fs_t1, fs_t2;
static char *pth1, *pth2;
static size_t len1, len2;
static enum { TREE_RM, TREE_CP } tree_op;
/* Ignores all syscall errors (continues on return value -1) */
/* Set by fs_fwrap() on key 'i' */
static bool fs_ign_errs;
/* File system operation did fail. Stop further processing of recursive
 * operation. */
static bool fs_error;
/* Overwrite *all* ? */
/* Reset at start of each fs_rm() and fs_cp() */
/* Has the same meaning as force_fs */
static bool fs_all;
static bool fs_none;

void
fs_mkdir(short tree)
{
	fs_error = FALSE;
	fs_ign_errs = FALSE;

	if (fs_ro()) {
		return;
	}

	if (ed_dialog("Enter name of directory to create (<ESC> to cancel):",
	    "", NULL, 0, NULL))
		return;

	if (tree & 1) {
		pth1 = syspth[0];
		len1 = pthlen[0];
	} else {
		pth1 = syspth[1];
		len1 = pthlen[1];
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
	int ntr = 0;

	fs_error = FALSE;
	fs_ign_errs = FALSE;

	if (fs_ro() || !db_num[right_col]) {
		return;
	}

	f = db_list[right_col][top_idx[right_col] + curs[right_col]];

	if ((tree == 1 && !f->type[0]) ||
	    (tree == 2 && !f->type[1]))
		return;

	if (tree == 3 && f->type[0] && f->type[1]) {
		tree = 1;
		ntr = 2;
	}

	if (ed_dialog("Enter new name (<ESC> to cancel):", f->name, NULL, 0,
	    NULL))
		return;

ntr:
	if ((tree & 2) && f->type[1]) {
		pth1 = syspth[1];
		len1 = pthlen[1];
	} else {
		pth1 = syspth[0];
		len1 = pthlen[0];
	}

	l = len1;
	len1 = pthcat(pth1, len1, rbuf);
	s = strdup(pth1);

	if (lstat(pth1, &gstat[0]) == -1) {
		if (errno != ENOENT)
			printerr(strerror(errno), "lstat \"%s\" failed", pth1);
	} else {
		if (!force_fs && dialog(y_n_txt, NULL,
		    "Delete existing %s \"%s\"?", S_ISDIR(gstat[0].st_mode) ?
		    "directory" : "file", pth1) != 'y')
			goto exit;

		if (S_ISDIR(gstat[0].st_mode)) {
			tree_op = TREE_RM;
			proc_dir();
		} else
			rm_file();
	}

	len1 = l;
	pthcat(pth1, len1, f->name);

	if (rename(pth1, s) == -1) {
		printerr(strerror(errno), "rename \"%s\" failed");
		goto exit;
	}

	if (ntr) {
		tree = ntr;
		ntr = 0;
		free(s);
		goto ntr;
	}

	rebuild_db(0);
exit:
	free(s);
	syspth[0][pthlen[0]] = 0;

	if (!bmode)
		syspth[1][pthlen[1]] = 0;
}

void
fs_chmod(int tree, long u, int num,
    /* 1: Force */
    /* 2: Don't rebuild DB (for mmrk) */
    /* 4: Reuse previous mode */
    unsigned md)
{
	struct filediff *f;
	static mode_t m;
	int ntr = 0;
	bool have_mode;

	fs_error = FALSE;
	fs_ign_errs = FALSE;

#if defined(TRACE)
	fprintf(debug, "->fs_chmod(t=%i u=%li n=%i) c=%u\n",
	    tree, u, num, curs[right_col]);
#endif
	if (fs_ro() || !db_num[right_col]) {
		goto ret;
	}

	have_mode = md & 4 ? TRUE : FALSE;

	if (!force_multi && !(md & 1) && num > 1 && dialog(y_n_txt, NULL,
	    "Change mode of %d files?", num) != 'y')
		goto ret;

	while (num-- && u < (long)db_num[right_col]) {
		f = db_list[right_col][u++];

		if ((tree == 1 && !f->type[0]) ||
		    (tree == 2 && !f->type[1]))
			continue;

		if (tree == 3 && f->type[0] && f->type[1]) {
			tree = 1;
			ntr = 2;
		}

ntr:
		if ((tree & 2) && f->type[1]) {
			if (S_ISLNK(f->type[1]))
				continue;

			pth1 = syspth[1];
			len1 = pthlen[1];

			if (!have_mode)
				m = f->type[1];
		} else {
			if (S_ISLNK(f->type[0]))
				continue;

			pth1 = syspth[0];
			len1 = pthlen[0];

			if (!have_mode)
				m = f->type[0];
		}

		if (!have_mode) {
			if (ask_for_perms(&m))
				goto ret;

			have_mode = TRUE;
		}

		pthcat(pth1, len1, f->name);

		if (chmod(pth1, m) == -1) {
			printerr(strerror(errno), "chmod \"%s\"");
			goto exit;
		}

		if (ntr) {
			tree = ntr;

			if (ntr == 3) {
				ntr = 0;
			} else {
				ntr = 3;
				goto ntr;
			}
		}
	}

	if (!(md & 2)) {
		rebuild_db(0);
	}
exit:
	syspth[0][pthlen[0]] = 0;

	if (!bmode)
		syspth[1][pthlen[1]] = 0;

ret:
#if defined(TRACE)
	fprintf(debug, "<-fs_chmod c=%u\n", curs[right_col]);
#endif
	return;
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
fs_chown(int tree, int op, long u, int num,
    /* 1: Force */
    /* 2: Don't rebuild DB (for mmrk) */
    /* 4: Reuse previous ID */
    unsigned md)
{
	struct filediff *f;
	static struct history owner_hist, group_hist;
	int i;
	struct passwd *pw;
	struct group *gr;
	static uid_t uid;
	static gid_t gid;
	int ntr = 0;
	bool have_owner;

	fs_error = FALSE;
	fs_ign_errs = FALSE;

	if (fs_ro() || !db_num[right_col]) {
		return;
	}

	have_owner = md & 4 ? TRUE : FALSE;

	if (!force_multi && !(md & 1) && num > 1 && dialog(y_n_txt, NULL,
	    "Change %s of %d files?", op ? "group" : "owner", num) != 'y')
		return;

	while (num-- && u < (long)db_num[right_col]) {
		f = db_list[right_col][u++];

		if ((tree == 1 && !f->type[0]) ||
		    (tree == 2 && !f->type[1]))
			continue;

		if (tree == 3 && f->type[0] && f->type[1]) {
			tree = 1;
			ntr = 2;
		}

ntr:
		if ((tree & 2) && f->type[1]) {
			if (S_ISLNK(f->type[1]))
				continue;

			pth1 = syspth[1];
			len1 = pthlen[1];
		} else {
			if (S_ISLNK(f->type[0]))
				continue;

			pth1 = syspth[0];
			len1 = pthlen[0];
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

		if (ntr) {
			tree = ntr;

			if (ntr == 3) {
				ntr = 0;
			} else {
				ntr = 3;
				goto ntr;
			}
		}
	}

	if (!(md & 2)) {
		rebuild_db(0);
	}
exit:
	syspth[0][pthlen[0]] = 0;

	if (!bmode)
		syspth[1][pthlen[1]] = 0;
}

/* 0: Ok
 * 1: Cancel */

int
fs_rm(
    /* 1: "dl", 2: "dr", 3: "dd" (detect which file exists)
     * 0: Use pth2, ignore nam and u. n must be 1. */
    /* -1: Use pth1 */
    int tree, char *txt,
    /* File name. If nam is given, u is not used. n must be 1. */
    char *nam, long u, int n,
    /* 1: Force */
    /* 2: Don't rebuild DB (for mmrk and fs_cp()) */
    /* 4: Don't reset 'fs_all' */
    unsigned md)
{
	struct filediff *f;
	unsigned short m;
	int rv = 0;
	char *fn;
	char *p0, *s[2];
	size_t l0;
	struct stat st;
	int ntr = 0; /* next tree */
	bool chg = FALSE;

	fs_error = FALSE;
	fs_ign_errs = FALSE;

	if (!(md & 4)) {
		fs_all = FALSE;
		fs_none = FALSE;
	}

	if (fs_ro()) {
		return 0;
	}

#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "->fs_rm(tree=%d txt(%s) nam(%s) u=%ld n=%d md=%u) "
	    "lp(%s) rp(%s)\n", tree, txt, nam, u, n, md, trcpth[0], trcpth[1]);
#endif
	/* Save what is used by fs_cp() too */
	p0 = pth1;
	l0 = len1;
	s[0] = strdup(syspth[0]);
	s[1] = strdup(syspth[1]);
	st = gstat[0];
	m = n > 1;

	/* case: Multiple files (not from fs_cp(), instead from <n>dd */

	if (!(force_fs && force_multi) && !(md & 1) && m) {
		if (bmode) {
			pth1 = syspth[1];

		} else if ((fmode && tree == 3 && !right_col)
		    || tree == 1) {

			syspth[0][pthlen[0]] = 0;
			pth1 = syspth[0];

		} else if ((fmode && tree == 3 && right_col)
		    ||tree == 2) {

			syspth[1][pthlen[1]] = 0;
			pth1 = syspth[1];
		} else {
			pth1 = NULL;
		}

		if (pth1) {
			if (dialog(y_n_txt,
			    NULL, "Really %s %d files in \"%s\"?",
			    txt ? txt : "delete", n, pth1) != 'y') {

				rv = 1;
				goto ret;
			}
		} else if (dialog(y_n_txt, NULL, "Really %s %d files?",
		    txt ? txt : "delete", n) != 'y') {

			rv = 1;
			goto ret;
		}
	}

	for (; n; n--, u++) {

		/* u is ignored if nam != NULL or tree == 0 */
		if (!fmode && !nam && tree > 0) {
			if (u >= (long)db_num[0]) {
				continue;
			}

			f = db_list[0][u];
			fn = f->name;
		}

		if (tree == 3) {
			if (bmode) {
				tree = 1;
			} else if (fmode) {
				if (u >= (long)db_num[right_col]) {
					continue;
				}

				f = db_list[right_col][u];
				fn = f->name;
				tree = right_col ? 2 : 1;
			} else {
				/* "dd" is not allowed
				 * if both files are present */
				if (f->type[0] && f->type[1]) {
					ntr = 2;
					tree = 1;
				}

				if (!f->type[0]) {
					tree &= ~1;
				}

				if (!f->type[1]) {
					tree &= ~2;
				}
			}
		} else if (tree > 0 && fmode) {
			if (nam) {
				fn = nam;
			} else {
				int col = tree == 1 ? 0 : 1;

				if (u >= (long)db_num[col]) {
					continue;
				}

				f = db_list[col][u];
				fn = f->name;
			}
		}

ntr:
		if (tree == 1) {
			pth1 = syspth[0];
			len1 = pthlen[0];
		} else if (tree == 2) {
			pth1 = syspth[1];
			len1 = pthlen[1];
		} else if (!tree) {
			pth1 = pth2;
			len1 = strlen(pth2);
		} else { /* tree < 0 */
			len1 = strlen(pth1);
		}

		if (tree > 0) {
			len1 = pthcat(pth1, len1, fn);
		}

#if defined(TRACE)
		fprintf(debug, "  force_fs=%d md=%u m=%u n=%d \"%s\"\n",
		    force_fs ? 1 : 0, md, m, n, pth1);
#endif
		if (lstat(pth1, &gstat[0]) == -1) {
			if (errno != ENOENT)
				printerr(strerror(errno), "lstat %s failed",
				    pth1);
			continue;
		}

		if (!(md & 1) && !m) {
			if (fs_deldialog(
			    tree < 1 || nam || ntr ? y_a_n_txt : y_n_txt,
			    txt ? txt : "delete",
			    S_ISDIR(gstat[0].st_mode) ? "directory " : NULL,
			    pth1)) {
				rv = 1;
				goto ret;
			}
		}

		chg = TRUE;

		if (S_ISDIR(gstat[0].st_mode)) {
			tree_op = TREE_RM;
			proc_dir();
		} else {
			rm_file();
		}

		if (ntr) {
			tree = ntr;

			if (ntr == 3) {
				ntr = 0;
			} else {
				ntr = 3;
				goto ntr;
			}
		}
	}

	if (txt || /* rebuild is done by others */
	    !chg) { /* Nothing done */
		goto ret;
	}

	if (!(md & 2)) {
		rebuild_db(0);
	}

	if (gl_mark) {
		chk_mark(gl_mark, 0);
	}

ret:
	memcpy(syspth[0], s[0], strlen(s[0]) + 1);
	memcpy(syspth[1], s[1], strlen(s[1]) + 1);
	free(s[1]);
	free(s[0]);
	pth1 = p0;
	len1 = l0;
	gstat[0] = st;
#if defined(TRACE)
	fprintf(debug, "<-fs_rm\n");
#endif
	return rv;
}

int /* !0: Error */
fs_cp(
    /* 0: Auto-detect */
    int to,
    long u, /* initial index */
    int n,
    /* 1: don't rebuild DB */
    /* 2: Symlink instead of copying */
    /* 4: Force */
    /* 8: Sync (update, 'U') */
    /* 16: Move (remove source after copy) */
    unsigned md)
{
	struct filediff *f;
	int i;
	int r = 1;
	int eto;
	char *tnam;
	bool m;
	bool chg = FALSE;

	fs_error = FALSE;
	fs_ign_errs = FALSE;
	fs_all = FALSE;
	fs_none = FALSE;

	if (fs_ro() || !db_num[right_col]) {
		return 1;
	}

#if defined(TRACE)
	fprintf(debug, "->fs_cp(to=%d u=%ld n=%d md=%u)\n",
	    to, u, n, md);
#endif
	m = n > 1;

	if (!(force_fs && force_multi) && m && !(md & 4)) {
		if (bmode) {
			/* Multiple files don't make sense in bmode */
			goto ret;
		}

		if (dialog(y_n_txt, NULL,
		    "Really %s %d files?",
		    md &  2 ? "create symlink to" :
		    md & 16 ? "move"              :
		              "copy"              ,
		    n) != 'y') {

			goto ret;
		}
	}

	if (bmode) {
		memcpy(syspth[0], syspth[1], pthlen[1]);
		pthlen[0] = pthlen[1];
	}

	for (; n-- && u < (long)db_num[right_col]; u++) {
		if (to) {
			eto = to;
		} else if (!(eto = fs_get_dst(u, md & 8 ? 1 : 0))) {
			continue;
		}

		if (eto == 1) {
			pth1 = syspth[1];
			len1 = pthlen[1];
			pth2 = syspth[0];
			len2 = pthlen[0];
		} else {
			pth1 = syspth[0];
			len1 = pthlen[0];
			pth2 = syspth[1];
			len2 = pthlen[1];
		}

		f = db_list[right_col][u];
		pthcat(pth1, len1, f->name);
#if defined(TRACE)
		fprintf(debug, "  fs_cp path(%s)\n", pth1);
#endif

		if (fs_stat(pth1, &gstat[0]) == -1) {
			continue;
		}

		tnam = f->name;
tpth:
		pthcat(pth2, len2, tnam);
		i = fs_stat(pth2, &gstat[1]);

		if (!i && /* from stat */
		    gstat[0].st_ino == gstat[1].st_ino &&
		    gstat[0].st_dev == gstat[1].st_dev) {
			if (ed_dialog("Enter new name (<ESC> to cancel):",
			    tnam, NULL, 0, NULL) || !*rbuf) {
				continue;
			}

			tnam = rbuf;
			goto tpth;
		}

		len1 = pthcat(pth1, len1, f->name);
		len2 = pthcat(pth2, len2, tnam);
#if defined(TRACE)
		fprintf(debug, "  Copy \"%s\" -> \"%s\"\n", pth1, pth2);
#endif
		if (md & 2) {
			if (!fs_stat(pth2, &gstat[1]) &&
			    fs_rm(0 /* tree */, "overwrite", NULL /* nam */,
			    0 /* u */, 1 /* n */, 4|2 /* md */) == 1) {
				goto ret;
			}

			if (symlink(pth1, pth2) == -1) {
				printerr(strerror(errno), "symlink %s -> %s",
				    pth2, pth1);
				continue;
			}
		} else if (S_ISDIR(gstat[0].st_mode)) {
			tree_op = TREE_CP;
			proc_dir();
		} else {
			if (cp_file()) {
				continue;
			}
		}

		if (!fs_error && (md & 16)) {
			fs_rm(-1, NULL, NULL, 0, 1, 4|3);
		}

		chg = TRUE;
	}

	if (chg && !(md & 1)) {
		rebuild_db(0);
	}

	r = 0;

ret:
	if (bmode) {
		syspth[0][0] = '.';
		syspth[0][1] = 0;
		pthlen[0] = 1;
	}

#if defined(TRACE)
	fprintf(debug, "<-fs_cp\n");
#endif
	return r;
}

static int
fs_ro(void)
{
	if (!readonly) {
		return 0;
	}

	printerr(NULL, "Type \":e\" to disable read-only mode");
	return 1;
}

/* top_idx and curs must kept unchanged for "//" */

void
rebuild_db(
    /* 0: keep top_idx and curs unchanged
     *    (for filesystem operations)
     * 1: keep selected name unchanged
     *    (for changing the list sort mode) */
    short mode)
{
	char *name;

#if defined(TRACE)
	fprintf(debug, "->rebuild_db(%d) c=%u\n", mode, curs[right_col]);
#endif
	syspth[0][pthlen[0]] = 0;

	if (!bmode) {
		syspth[1][pthlen[1]] = 0;
	}

	if (mode) {
		name = saveselname();
	}

	/* pointer is freed in next line */
	if (mark && !gl_mark) {
		mark_global();
	}

	diff_db_free(0);
	build_diff_db(bmode || fmode ? 1 : subtree);

	if (fmode) {
		diff_db_free(1);
		build_diff_db(2);
	}

	if (mode && name) {
		center(findlistname(name));
		free(name);
	} else {
		disp_fmode();
	}
#if defined(TRACE)
	fprintf(debug, "<-rebuild_db\n");
#endif
}

static void
proc_dir(void)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	struct str_list *dirs = NULL;

	if (tree_op == TREE_CP && creatdir()) {
		return;
	}

	if (!(d = opendir(pth1))) {
		printerr(strerror(errno), "opendir %s failed", pth1);
		return;
	}

	while (!fs_error) {
		int i;

		errno = 0;

		if (!(ent = readdir(d))) {
			if (!errno) {
				break;
			}

			pth1[len1] = 0;
			printerr(strerror(errno), "readdir %s failed", pth1);
			closedir(d);
			return;
		}

		name = ent->d_name;

		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2]))) {
			continue;
		}

		pthcat(pth1, len1, name);

		/* fs_rm does never follow links! */
		if (followlinks && tree_op != TREE_RM) {
			i =  stat(pth1, &gstat[0]);
		} else {
			i = lstat(pth1, &gstat[0]);
		}

		if (i == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno),
				    LOCFMT "stat %s" LOCVAR, pth1);
				goto closedir;
			}

			continue; /* deleted after readdir */
		}

		if (S_ISDIR(gstat[0].st_mode)) {
			struct str_list *se = malloc(sizeof(struct str_list));
			se->s = strdup(name);
			se->next = dirs;
			dirs = se;
		} else if (tree_op == TREE_RM) {
			rm_file();
		} else {
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

		if (!fs_error) {
			l1 = len1;
			len1 = pthcat(pth1, len1, dirs->s);

			if (tree_op == TREE_CP) {
				l2 = len2;
				len2 = pthcat(pth2, len2, dirs->s);
			}

			proc_dir();
			pth1[len1 = l1] = 0;

			if (tree_op == TREE_CP)
				pth2[len2 = l2] = 0;
		}

		free(dirs->s);
		p = dirs;
		dirs = dirs->next;
		free(p);
	}

	if (!fs_error && tree_op == TREE_RM &&
	    rmdir(pth1) == -1 && !fs_ign_errs) {

		fs_fwrap("rmdir \"%s\": %s", pth1, strerror(errno));
	}
}

static void
rm_file(void)
{
	if ((fs_t2 = time(NULL)) - fs_t1) {
		printerr(NULL, "Delete \"%s\"", pth1);
		fs_t1 = fs_t2;
	}

	if (!fs_error && unlink(pth1) == -1 && !fs_ign_errs) {
		fs_fwrap("unlink \"%s\": %s", pth1, strerror(errno));
	}
}

/* !0: Error */

static int
cp_file(void)
{
	if ((fs_t2 = time(NULL)) - fs_t1) {
		printerr(NULL, "Copy \"%s\" -> \"%s\"", pth1, pth2);
		fs_t1 = fs_t2;
	}

	if (S_ISREG(gstat[0].st_mode)) {
		return cp_reg();
	} else if (S_ISLNK(gstat[0].st_mode)) {
		return cp_link();
	} else {
		printerr(NULL, "Not copied: \"%s\"", pth1);
		return 0; /* Not an error */
	}
}

static int
creatdir(void)
{
	if (fs_stat(pth1, &gstat[0]) == -1) {
		return -1;
	}

	if (!fs_stat(pth2, &gstat[1])) {
		if (S_ISDIR(gstat[1].st_mode)) {
			/* Respect write protected dirs, don't make them
			 * writeable */
			return 0;
		}

		if (fs_rm(0 /* tree */, "overwrite", NULL /* nam */,
		    0 /* u */, 1 /* n */, 4|2 /* md */) == 1) {
			return -1;
		}
	}

	if (mkdir(pth2, (gstat[0].st_mode | 0100) & 07777) == -1
	    && errno != EEXIST) {
		printerr(strerror(errno), "mkdir %s", pth2);
		return -1;
	}

	return 0;
}

static int
cp_link(void)
{
	ssize_t l;
	char *buf;
	int r = 0;

	buf = malloc(gstat[0].st_size + 1);

	if ((l = readlink(pth1, buf, gstat[0].st_size)) == -1) {
		printerr(strerror(errno), "readlink %s", pth1);
		r = -1;
		goto exit;
	}

	if (l != gstat[0].st_size) {
		printerr("Unexpected link lenght", "readlink %s", pth1);
		r = -1;
		goto exit;
	}

	buf[l] = 0;

	if (!fs_stat(pth2, &gstat[1]) &&
	    fs_rm(0 /* tree */, "overwrite", NULL /* nam */,
	    0 /* u */, 1 /* n */, 4|2 /* md */) == 1) {
		r = 1;
		goto exit;
	}

	if (symlink(buf, pth2) == -1) {
		printerr(strerror(errno), "symlink %s", pth2);
		r = -1;
		goto exit;
	}

	/* setting symlink time is not supported on all file systems */

exit:
	free(buf);
	return r;
}

/* !0: Error */

static int
cp_reg(void)
{
	int f1, f2;
	ssize_t l1, l2;
#ifdef HAVE_FUTIMENS
	struct timespec ts[2];
#else
	struct utimbuf tb;
#endif

#if defined(TRACE)
	fprintf(debug, "<>cp_reg \"%s\" -> \"%s\"\n", pth1, pth2);
#endif
	if (!fs_stat(pth2, &gstat[1])) {
		if (S_ISREG(gstat[1].st_mode)) {
			bool ms = FALSE;

			if (!cmp_file(pth1, gstat[0].st_size,
			              pth2, gstat[1].st_size, 1)) {
				return 0;
			}

			if (fs_deldialog(y_a_n_txt, "overwrite", "file ",
			    pth2)) {
				return 1;
			}
test:
			if (!access(pth2, W_OK)) {
				goto copy;
			}

			if (errno != EACCES) {
				printerr(strerror(errno),
				    "access \"%s\"", pth2);
			}

			if (!ms && !(gstat[1].st_mode & S_IWUSR)) {
				if (chmod(pth2, gstat[1].st_mode & S_IWUSR) == -1)
				{
					printerr(strerror(errno),
					    "chmod \"%s\"", pth2);
				} else {
					ms = TRUE;
					goto test;
				}
			}

			if (unlink(pth2) == -1) {
				printerr(strerror(errno), "unlink \"%s\"",
				    pth2);
			}
		} else {
			/* Don't delete symlinks! They must be followed. */
			if (!followlinks &&
			    fs_rm(0 /* tree */, "overwrite", NULL /* nam */,
			    0 /* u */, 1 /* n */, 4|2 /* md */) == 1) {
				return 1;
			}
		}
	}

copy:
	if ((f2 = open(pth2, O_CREAT | O_TRUNC | O_WRONLY,
	    gstat[0].st_mode & 07777)) == -1) {
		printerr(strerror(errno), "create \"%s\"", pth2);
		return -1;
	}

	if (!gstat[0].st_size)
		goto setattr;

	if ((f1 = open(pth1, O_RDONLY)) == -1) {
		printerr(strerror(errno), "open \"%s\"", pth1);
		goto close2;
	}

	while (1) {
		if ((l1 = read(f1, lbuf, sizeof lbuf)) == -1) {
			printerr(strerror(errno), "read \"%s\"", pth1);
			break;
		}

		if (!l1)
			break;

		if ((l2 = write(f2, lbuf, l1)) == -1 && !fs_ign_errs) {
			fs_fwrap("write \"%s\"", pth2, strerror(errno));
			break;
		}

		if (l2 != l1) {
			fs_fwrap("\"%s\"", pth2, "write error");
			break; /* error */
		}

		if (l1 < (ssize_t)(sizeof lbuf))
			break;
	}

	close(f1);

setattr:
#ifdef HAVE_FUTIMENS
	ts[0] = gstat[0].st_atim;
	ts[1] = gstat[0].st_mtim;
	futimens(f2, ts); /* error not checked */
#else
	tb.actime  = gstat[0].st_atime;
	tb.modtime = gstat[0].st_mtime;
	utime(pth2, &tb);
#endif

close2:
	close(f2);
	return 0;
}

static void
fs_fwrap(const char *f, ...)
{
	va_list a;

	va_start(a, f);

	switch (vdialog(ign_esc_txt, "\ni", f, a)) {
	case '':
		fs_error = 1;
		break;
	case 'i':
		fs_ign_errs = TRUE;
		break;
	}

	va_end(a);
}

/* 0: yes */

static int
fs_deldialog(const char *menu, const char *op, const char *typ,
    const char *nam)
{
	if (fs_none) {
		return 1;
	}

	if (force_fs || fs_all) {
		return 0;
	}

	switch (dialog(menu, NULL,
	    "Really %s %s\"%s\"?", op, typ ? typ : "", nam)) {
	case 'a':
		fs_all = TRUE;
		/* fall through */

	case 'y':
		return 0;

	case 'N':
		fs_none = TRUE;
		/* fall through */

	default:
		return 1;
	}
}

/* 0: Error
 * 1: Left tree
 * 2: Right tree */

int
fs_get_dst(long u,
    /* 1: auto-detect */
    unsigned m)
{
	struct filediff *f;
	int dst = 0;

	if (bmode) {
		dst = 2;
	} else if (fmode) {
		dst = m ? 0 : right_col ? 1 : 2;
	} else {
		if (u >= (long)db_num[0]) {
			goto ret;
		}

		f = db_list[0][u];

		if (f->type[0]) {
			if (f->type[1]) {
				if (!m || !S_ISREG(f->type[0]) ||
				          !S_ISREG(f->type[1])) {

					/* return 0 */

				} else if (f->mtim[0] < f->mtim[1]) {
					dst = 1;

				} else if (f->mtim[0] > f->mtim[1]) {
					dst = 2;
				}

				goto ret;
			}

			dst = 2;
		} else {
			dst = 1;
		}
	}

ret:
#if defined(TRACE)
	fprintf(debug, "<>fs_get_dst(u=%ld m=%u): %d\n", u, m, dst);
#endif
	return dst;
}

int /* 0: false, !0: true */
fs_any_dst(long u, int n)
{
	for (; n--; u++) {
		if (fs_get_dst(u, 0)) {
			return 1;
		}
	}

	return 0;
}

static int
fs_stat(const char *p, struct stat *s)
{
	int i;

	if (( followlinks && (i =  stat(p, s)) == -1) ||
	    (!followlinks && (i = lstat(p, s)) == -1)) {
		if (errno != ENOENT) {
			printerr(strerror(errno), LOCFMT "stat \"%s\""
			    LOCVAR, p);
		}
	}

	return i;
}
