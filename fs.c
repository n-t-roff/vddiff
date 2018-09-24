/*
Copyright (c) 2016-2018, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
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
#include "misc.h"

struct str_list {
	char *s;
	struct str_list *next;
};

/* Case "dir not empty": -1: error, 0: empty, 1: not empty */
static int proc_dir(void);
/* Return value:
 *   -1: error */
static int rm_dir(void);
/* Return value:
 *   !0: error */
static int cp_file(void);
/* Return value:
 *    1: User response: "Don't overwrite"
 *   -1: error */
static int creatdir(void);
/* Return value:
 *    1: User response: "Don't overwrite"
 *   -1: error */
static int cp_link(void);
static int ask_for_perms(mode_t *);
static int fs_ro(void);
static void fs_fwrap(const char *, ...);
/*
 * Input:
 *   menu:
 *   op:
 *   typ:
 *   nam:
 *
 * Output:
 *   0: yes
 */
static int fs_deldialog(const char *menu, const char *op, const char *typ,
    const char *nam);
static int fs_testBreak(void);

time_t fs_t1, fs_t2;
char *pth1, *pth2;
static size_t len1, len2;

static enum {
    TREE_RM, /* delete */
    TREE_CP, /* copy */
    TREE_NOT_EMPTY
} tree_op;

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
bool fs_none;
bool fs_abort;
bool preserve_mtim;
bool preserve_all;

void
clr_fs_err(void) {
	fs_ign_errs = FALSE;
	fs_error = FALSE;
	fs_all = FALSE;
	fs_none = FALSE;
	fs_abort = FALSE;
}

void
fs_mkdir(short tree)
{
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
fs_rename(int tree, long u, int num,
    /* 1: Force */
    /* 2: Don't rebuild DB (for mmrk) */
    unsigned md)
{
	struct filediff *f;
    const char *s = NULL;
	size_t l;
	int ntr = 0;

	if (fs_ro() || !db_num[right_col]) {
		goto ret;
	}

	if (!force_multi && !(md & 1) && num > 1 && dialog(y_n_txt, NULL,
	    "Rename %d files?", num) != 'y')
		goto ret;

	while (num-- && u < (long)db_num[right_col]) {
		f = db_list[right_col][u++];

		if (str_eq_dotdot(f->name)) {
			goto ret;
		}

		if ((tree == 1 && !f->type[0]) ||
		    (tree == 2 && !f->type[1]))
			goto ret;

		if (tree == 3 && f->type[0] && f->type[1]) {
			tree = 1;
			ntr = 2;
		}

		if (ed_dialog("Enter new name (<ESC> to cancel):", f->name,
		    NULL, 0, NULL))
			goto ret;

ntr: /* next tree */
		if ((tree & 2) && f->type[1]) {
			pth1 = syspth[1];
			len1 = pthlen[1];
		} else {
			pth1 = syspth[0];
			len1 = pthlen[0];
		}

		l = len1;
		len1 = pthcat(pth1, len1, rbuf);
        if (!(s = strdup(pth1))) /* TODO: LSAN reports a leak--which can't be possible. Likely a LSAN bug. */
            goto exit;

		if (lstat(pth1, &gstat[0]) == -1) {
			if (errno != ENOENT)
                printerr(strerror(errno), LOCFMT "lstat \"%s\"" LOCVAR, pth1);
		} else {
            if (!force_fs &&
                    dialog(y_n_txt, NULL, "Delete existing %s \"%s\"?",
                           S_ISDIR(gstat[0].st_mode) ?
                           "directory" : "file", pth1) != 'y')
            {
				goto exit;
            }
			if (S_ISDIR(gstat[0].st_mode)) {
				tree_op = TREE_RM;
				proc_dir();
            } else {
				rm_file();
            }
		}

		len1 = l;
		pthcat(pth1, len1, f->name);

		if (rename(pth1, s) == -1) {
            printerr(strerror(errno), LOCFMT "rename \"%s\"" LOCVAR, pth1);
			goto exit;
		}

		if (ntr) {
			tree = ntr;
			ntr = 0;
            free(const_cast_ptr(s));
			goto ntr;
		}
	}

	if (!(md & 2)) {
		rebuild_db(0);
	}
exit:
    free(const_cast_ptr(s));
	syspth[0][pthlen[0]] = 0;

	if (!bmode)
		syspth[1][pthlen[1]] = 0;
ret:
	return;
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

		if (str_eq_dotdot(f->name)) {
			continue;
		}

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
			printerr(strerror(errno), "chmod \"%s\"", pth1);
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
        m |= (c & 0x7f) - '0';
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

	if (fs_ro() || !db_num[right_col]) {
		return;
	}

	have_owner = md & 4 ? TRUE : FALSE;

	if (!force_multi && !(md & 1) && num > 1 && dialog(y_n_txt, NULL,
	    "Change %s of %d files?", op ? "group" : "owner", num) != 'y')
		return;

	while (num-- && u < (long)db_num[right_col]) {
		f = db_list[right_col][u++];

		if (str_eq_dotdot(f->name)) {
			continue;
		}

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
            else if (!(gid = (gid_t)atoi(rbuf))) {
				printerr("", "Invalid group name \"%s\"", rbuf);
				return;
			}

            i = chown(pth1, (uid_t)-1, gid);
		} else {
			if ((pw = getpwnam(rbuf)))
				uid = pw->pw_uid;
            else if (!(uid = (uid_t)atoi(rbuf))) {
				printerr("", "Invalid user name \"%s\"", rbuf);
				return;
			}

            i = chown(pth1, uid, (gid_t)-1);
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

int fs_rm(int tree, const char *const txt, char *nam, long u, int n,
          unsigned md)
{
    struct filediff *f = NULL;
    const unsigned short m = n > 1; /* multiple files to process */
	int rv = 0;
    const char *fn = NULL;
    const char *const p0 = pth1; /* Save what is used by fs_cp() too */
    const char *s[2] = { NULL, NULL };
    const size_t l0 = len1; /* Save what is used by fs_cp() too */
    struct stat st = gstat[0]; /* Save what is used by fs_cp() too */
	int ntr = 0; /* next tree */
	bool chg = FALSE;
	bool empty_dir_;

	if (fs_ro()) {
		return 0;
	}

#if defined(TRACE)
	TRCPTH;
    fprintf(debug, "->fs_rm(tree=%d txt=\"%s\" nam=\"%s\" u=%ld n=%d md=%u) "
        "lp=\"%s\" rp=\"%s\"\n", tree, txt, nam, u, n, md, trcpth[0], trcpth[1]);
#endif
	/* Save what is used by fs_cp() too */
	s[0] = strdup(syspth[0]);
	s[1] = strdup(syspth[1]);

    if (!s[0] || !s[1]) {
        if (printerr(strerror(errno), LOCFMT "strdup()" LOCVAR))
            fputs(oom_msg, stderr);
        rv |= 2;
        goto free_mem;
    }

	/* case: Multiple files (not from fs_cp(), instead from <n>dd */

    if (!(force_fs && force_multi)
            && !(md & 1) /* force */
            && m) /* multiple files */
    {
        if (bmode) {
            pth1 = syspth[1];

        } else if ((fmode && tree == 3 && !right_col)
                   || tree == 1)
        {
            syspth[0][pthlen[0]] = 0;
            pth1 = syspth[0];

        } else if ((fmode && tree == 3 && right_col)
                   || tree == 2)
        {
            syspth[1][pthlen[1]] = 0;
            pth1 = syspth[1];
        } else {
            /* Case: "dd" in diff mode. Allowed only if file is present
             * on one side only. */
            pth1 = NULL;
        }

        if (pth1) {
            if (dialog(y_n_txt, NULL, "Really %s %d files in \"%s\"?",
                       txt ? txt : "delete", n, pth1)
                    != 'y')
            {
                rv |= 1;
                goto ret;
            }
        } else if (dialog(y_n_txt, NULL, "Really %s %d files?",
                          txt ? txt : "delete", n)
                   != 'y')
        {
            rv |= 1;
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
                if (!f) {
#if defined (TRACE) && defined (DEBUG)
                    fprintf(debug, LOCFMT "`f` == NULL !\n" LOCVAR);
                    exit(EXIT_STATUS_ERROR);
#else
                    rv |= 2;
                    goto ret;
#endif
                }
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

		if (str_eq_dotdot(fn)) {
			continue;
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
                printerr(strerror(errno), "lstat %s failed", pth1);
			continue;
		}

		empty_dir_ = FALSE;

        if (!(md & 1) && !m) { /* not force and not multi */
			const char *typ = NULL;

			if (S_ISDIR(gstat[0].st_mode)) {
				int v;

				tree_op = TREE_NOT_EMPTY;
				v = proc_dir();

				if (!v) {
					empty_dir_ = TRUE;
					typ = "directory ";
				} else if (v > 0) {
					typ = "non-empty directory ";
				}
			}

            if (fs_deldialog(tree < 1 || nam || ntr ? y_a_n_txt : y_n_txt, /* menu */
                             txt ? txt : "delete", /* operation text */
                             typ, /* e.g. "non-empty directory " */
                             pth1)) /* file name */
            {
                rv = 1;
                goto ret;
            }
		}

		chg = TRUE;

		if (empty_dir_) {
			rm_dir();
		} else if (S_ISDIR(gstat[0].st_mode)) {
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
            !chg) /* Nothing done */
    {
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
free_mem:
    free(const_cast_ptr(s[1]));
    free(const_cast_ptr(s[0]));
    pth1 = const_cast_ptr(p0);
	len1 = l0;
	gstat[0] = st;

	if (fs_error) {
		rv |= 2;
	}

	if (fs_ign_errs) {
		rv |= 4;
	}

#if defined(TRACE)
	fprintf(debug, "<-fs_rm: 0x%x\n", rv);
#endif
	return rv;
}

int fs_cp(int to, long u, int n, unsigned md, unsigned *sto_res_)
{
    struct filediff *f = NULL, *f2 = NULL;
    int i = 0;
	int r = 1;
    int eto = 0; /* Effective dest side */
	unsigned sto = 0; /* OR sum dest side */
    const char *tnam = NULL;
	static const char *const tmpnam_ = "." BIN ".X";
    bool m = FALSE;
	bool chg = FALSE;
	bool ofs = FALSE;

#if defined(TRACE)
	fprintf(debug, "->fs_cp(to=%d u=%ld n=%d md=0x%x)\n",
	    to, u, n, md);
#endif

	if (fs_ro() || !db_num[right_col]) {
#if defined(TRACE)
		fprintf(debug, "  r/o or no entry\n");
#endif
		goto ret0;
	}

	m = n > 1;

	if (!(force_fs && force_multi) && m && !(md & 4)) {
		if (dialog(y_n_txt, NULL,
		    "Really %s %d files?",
		    md &  2 ? "create symlink to" :
		    md & 16 ? "move"              :
		    md & 32 ? "exchange"          :
		              "copy"              ,
		    n) != 'y') {

			goto ret;
		}
	}

	if (bmode) {
		/* Case: Make a copy of a file with a new name
		 * (not just rename a file) */
		memcpy(syspth[0], syspth[1], pthlen[1]);
		pthlen[0] = pthlen[1];
	} else if (md & (16 | 32)) {
		syspth[0][pthlen[0]] = 0;
		syspth[1][pthlen[1]] = 0;

		if (fs_stat(syspth[0], &gstat[0], 0) == -1 ||
		    fs_stat(syspth[1], &gstat[1], 0) == -1) {
#if defined(TRACE)
			fprintf(debug, "  stat \"%s\", \"%s\" error\n",
			    syspth[0], syspth[1]);
#endif
			goto ret0;
		}

		ofs = gstat[0].st_dev == gstat[1].st_dev ? TRUE : FALSE;
	}

	for (; n-- && u < (long)db_num[right_col]; u++) {
		if (to) {
			eto = to;
		} else if (!(eto = fs_get_dst(u,
		    md & 8  ? 1 :
		    md & 32 ? 2 : 0))) {
#if defined(TRACE)
			fprintf(debug, "  dest can't be determined\n");
#endif
			continue;
		}

next_xchg:
		if (eto == 1) {
			sto |= 1;
			pth1 = syspth[1];
			len1 = pthlen[1];
			pth2 = syspth[0];
			len2 = pthlen[0];
		/* At first eto == 3 if md & 32 */
		} else {
			sto |= 2;
			pth1 = syspth[0];
			len1 = pthlen[0];
			pth2 = syspth[1];
			len2 = pthlen[1];
		}

		f = db_list[right_col][u];

        if (md & 128) {
            f2 = db_list[right_col ? 0 : 1][u];
        }

		if (str_eq_dotdot(f->name)) {
			continue;
		}

		pthcat(pth1, len1, f->name);
#if defined(TRACE)
		fprintf(debug, "  fs_cp src path(%s) f->name(%s) "
		    "right_col=%d u=%ld\n",
		    pth1, f->name, right_col, u);
#endif

		if (fs_stat(pth1, &gstat[0], 0) == -1) {
#if defined(TRACE)
			fprintf(debug, "  no source\n");
#endif
			continue;
		}

        tnam = ((md & 128) && f2)       ? f2->name :
               ((md &  32) && eto == 3) ? tmpnam_  :
                                          f->name;
tpth:
		pthcat(pth2, len2, tnam);
#if defined(TRACE)
		fprintf(debug, "  fs_cp dst path(%s)\n", pth2);
#endif
		i = fs_stat(pth2, &gstat[1], 0);

		if (i == -1) { /* from stat */
			if (errno != ENOENT) {
#if defined(TRACE)
				fprintf(debug, "  stat error\n");
#endif
				continue;
			}

			if ((md & (16 | 32)) && ofs) {
#if defined(TRACE)
				fprintf(debug, "  Rename \"%s\" -> \"%s\"\n",
				    pth1, pth2);
#endif
                if (!fs_error) {
                    if (rename(pth1, pth2) == -1) {
                        fs_fwrap("rename %s -> %s: %s",
                                 pth1, pth2, strerror(errno));
                    } else if (!wstat && verbose) {
                        printf("Rename \"%s\" -> \"%s\" done\n", pth1, pth2);
                    }
                }

				chg = TRUE;
				goto next;
			}
		} else if (gstat[0].st_ino == gstat[1].st_ino &&
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
			if (!fs_stat(pth2, &gstat[1], 0) &&
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

		if ((md & (16 | 32)) && !fs_error && !fs_none && !fs_abort) {
			if (fs_rm(-1, NULL, NULL, 0, 1,
			    4 | 3 | (fs_ign_errs ? 8 : 0)) & 4) {
				fs_ign_errs = TRUE;
			}
		}

		chg = TRUE;

next:
		if (md & 32) {
			char *s;

			if (eto == 3) {
				eto = 1;
				goto next_xchg;
			}

			pthcat(pth1, len1, tmpnam_);
			s = strdup(pth1);
			pthcat(pth1, len1, f->name);

			if (rename(s, pth1) == -1) {
				printerr(strerror(errno),
				    "rename %s -> %s", s, pth1);
			}

			free(s);
		}
	}

	if (chg && !(md & 1)) {
#if defined(TRACE)
		fprintf(debug, "  sto=%d\n", sto);
#endif
		rebuild_db(
		    !fmode || !sto || sto == 3 || (md & (16 | 32))
            ? 0 : (short)(sto << 1));
	}

	r = (fs_error    ? 1 : 0) |
	    (fs_ign_errs ? 2 : 0) ;

ret:
	if (bmode) {
		syspth[0][0] = '.';
		syspth[0][1] = 0;
		pthlen[0] = 1;
	}

ret0:
	if (sto_res_) {
		*sto_res_ = sto;
	}

#if defined(TRACE)
	fprintf(debug, "<-fs_cp: 0x%x\n", r);
#endif
    return r;
}

void
fs_cat(
    /* index */
    long u)
{
#if defined(TRACE)
	fprintf(debug, "->fs_cat(%ld)\n", u);
#endif
	if (!(bmode || fmode)) {
		printerr(NULL, "Append file not supported in diff mode");
		goto ret;
	}

	if (u >= (long)db_num[right_col]) {
		goto ret;
	}

    pth1 = const_cast_ptr(db_list[right_col][u]->name);

	if (str_eq_dotdot(pth1)) {
		goto ret;
	}

	if (!mark) {
		printerr(NULL, "Single file mark not set");
		goto ret;
	}

    pth2 = const_cast_ptr(mark->name ? mark->name :
                          bmode      ? gl_mark    :
                          mark_lnam  ? mark_lnam  :
                          mark_rnam  ? mark_rnam  :
                                       "<error>") ;

	if (fs_stat(pth1, &gstat[0], 1) == -1 ||
	    fs_stat(pth2, &gstat[1], 1)) {
		goto ret;
	}

	if (!S_ISREG(gstat[0].st_mode) ||
	    !S_ISREG(gstat[1].st_mode)) {
		printerr(NULL,
		    "Append file supported for regular files only");
		goto ret;
	}

	if (gstat[0].st_ino == gstat[1].st_ino &&
	    gstat[0].st_dev == gstat[1].st_dev) {
		printerr(NULL, "Append file not supported for same file");
		goto ret;
	}

	cp_reg(1);
	/* in fmode both sides can show same directory */
	rebuild_db(0);

ret:
#if defined(TRACE)
	fprintf(debug, "<-fs_cat\n");
#endif
	return;
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
    /* 2: rebuild left side only
     * 4: rebuild right side only */
    short mode)
{
    char *name = NULL;

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

	if (!(mode & 4)) {
		diff_db_free(0);
		build_diff_db(bmode || fmode ? 1 : subtree);
	}

	if (fmode && !(mode & 2)) {
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

static int proc_dir(void)
{
    DIR *d;
    struct dirent *ent;
    char *name;
    struct str_list *dirs = NULL;
    int rv = 0;

    if (tree_op == TREE_CP && creatdir()) {
        rv = -1;
        goto ret;
    }

    if (!(d = opendir(pth1))) {
        printerr(strerror(errno), "opendir %s failed", pth1);
        rv = -1;
        goto ret;
    }

    while (!fs_error && !fs_abort) {
        int i;

        errno = 0;

        if (!(ent = readdir(d))) {
            if (!errno) {
                break;
            }

            pth1[len1] = 0;
            printerr(strerror(errno), "readdir %s failed", pth1);
            rv = -1;
            break;
        }

        name = ent->d_name;

        if (*name == '.' && (!name[1] || (name[1] == '.' &&
                                          !name[2]))) {
            continue;
        }

        if (tree_op == TREE_NOT_EMPTY) {
            rv = 1;
            break;
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
                rv = -1;
                break;
            }
            continue; /* deleted after readdir */
        }

        if (S_ISDIR(gstat[0].st_mode)) {
            struct str_list *se = malloc(sizeof(struct str_list));
            se->s = strdup(name);
            se->next = dirs;
            dirs = se;
        } else if (tree_op == TREE_RM) {
            if (rm_file() < 0)
                rv = -1;
        } else {
            pthcat(pth2, len2, name);
            if (cp_file())
                rv = -1;
        }
    }

    closedir(d);
    pth1[len1] = 0;

    if (tree_op == TREE_NOT_EMPTY) {
        goto ret;
    }

    if (tree_op == TREE_CP) {
        pth2[len2] = 0;
    }

    while (dirs) {
        size_t l1, l2 = 0 /* silence warning */;
        struct str_list *p;

        if (!fs_error && !fs_abort) {
            l1 = len1;
            len1 = pthcat(pth1, len1, dirs->s);

            if (tree_op == TREE_CP) {
                l2 = len2;
                len2 = pthcat(pth2, len2, dirs->s);
            }

            if (proc_dir() < 0)
                rv = -1;
            pth1[(len1 = l1)] = 0;

            if (tree_op == TREE_CP)
                pth2[(len2 = l2)] = 0;
        }

        free(dirs->s);
        p = dirs;
        dirs = dirs->next;
        free(p);
    }

    if (tree_op == TREE_RM && rm_dir() < 0)
        rv = -1;
ret:
    if (exit_on_error && rv < 0)
        fs_abort = TRUE;
    return rv;
}

static int rm_dir(void)
{
#if defined(TRACE)
	fprintf(debug, "<>rm_dir(%s)\n", pth1);
#endif
    int return_value = 0;
    if (!fs_error) {
        if (rmdir(pth1) == -1) {
            fs_fwrap("rmdir \"%s\": %s", pth1, strerror(errno));
            return_value = -1;
        } else if (!wstat && verbose) {
            printf("Directory \"%s\" removed\n", pth1);
        }
    }
    return return_value;
}

int rm_file(void)
{
    int return_value = 0;
#if defined(TRACE) && (defined(TEST) || 1)
	fprintf(debug, "<>rm_file(path=%s)\n", pth1);
#endif

    if (wstat && (fs_t2 = time(NULL)) - fs_t1) {
		printerr(NULL, "Delete \"%s\"", pth1);
		fs_t1 = fs_t2;

		/* Test for ESC once a second */
		if (fs_testBreak()) {
            goto func_return;
		}
	}

    if (!fs_error) {
        if (unlink(pth1) == -1) {
            fs_fwrap("unlink \"%s\": %s", pth1, strerror(errno));
            return_value = -1;
        } else {
            if (!wstat && verbose) {
                printf("File \"%s\" removed\n", pth1);
            }

            if (cli_rm) { /* not cli_cp overwrite */
                tot_cmp_byte_count += gstat[0].st_size;
                ++tot_cmp_file_count;
            }
        }
    }
func_return:
    return return_value;
}

static int
cp_file(void)
{
    int rv = 0;

#if defined(TRACE)
    fprintf(debug, "->cp_file pth1=\"%s\" pth2=\"%s\"\n", pth1, pth2);
#endif

    if (wstat && (fs_t2 = time(NULL)) - fs_t1) {
		printerr(NULL, "Copy \"%s\" -> \"%s\"", pth1, pth2);
		fs_t1 = fs_t2;

		/* Test for ESC once a second */
		if (fs_testBreak()) {
            rv |= 1;
			goto ret;
		}
	}

	if (S_ISREG(gstat[0].st_mode)) {
        if (cp_reg(0) < 0)
            rv |= 1;
	} else if (S_ISLNK(gstat[0].st_mode)) {
        rv |= cp_link();
	} else {
        fs_fwrap("Unsupported file type 0%o", gstat[0].st_mode);
		printerr(NULL, "Not copied: \"%s\"", pth1);
        rv |= 1;
	}

ret:
    if (exit_on_error && rv)
        fs_abort = TRUE;
#if defined(TRACE)
	fprintf(debug, "<-cp_file: %d\n", rv);
#endif
	return rv;
}

/* !0: Break */

static int
fs_testBreak(void)
{
	int b = 0;

	mvwaddstr(wstat, 0, 0, "Type <ESC> to cancel");
	wrefresh(wstat);
	nodelay(stdscr, TRUE);

	if (getch() == 27 /* ESC */) {
		fs_abort = TRUE;
		b = 1;
	}

	nodelay(stdscr, FALSE);
	return b;
}

static int
creatdir(void)
{
    int return_value = 0;

    if (fs_stat(pth1, &gstat[0], 0) == -1) {
        return_value = -1;
        goto func_return;
    }

    if (!fs_stat(pth2, &gstat[1], 0)) {
        if (S_ISDIR(gstat[1].st_mode)) {
            /* Respect write protected dirs, don't make them
             * writeable */
            goto func_return;
        }

        if (fs_rm(0 /* tree */, "overwrite", NULL /* nam */,
                  0 /* u */, 1 /* n */, 4|2 /* md */) == 1) {
            return_value = 1;
            goto func_return;
        }
    }

    if (mkdir(pth2, (gstat[0].st_mode | 0100) & 07777) == -1
            && errno != EEXIST)
    {
        printerr(strerror(errno), "mkdir %s", pth2);
        return_value = -1;
        goto func_return;
    }
    if (preserve_all) {
        if (lchown(pth2, gstat[0].st_uid, gstat[0].st_gid) == -1) {
#if defined(TRACE)
            fprintf(debug, "  lchown(%s): %s\n", pth2, strerror(errno));
#endif
        }
        if (chmod(pth2, gstat[0].st_mode & 07777) == -1) {
#if defined(TRACE)
            fprintf(debug, "  chmod(%s): %s\n", pth2, strerror(errno));
#endif
        }
    }
    if (!wstat && verbose) {
        printf("Directory \"%s\" created\n", pth2);
    }
func_return:
    if (exit_on_error && return_value < 0)
        fs_abort = TRUE;
    return return_value;
}

static int
cp_link(void)
{
	int r = 0;
    bool equal = FALSE;
    char *const buf = malloc((size_t)gstat[0].st_size + 1);
    ssize_t l = readlink(pth1, buf, (size_t)gstat[0].st_size);
#if defined(TRACE)
    fprintf(debug, "->cp_link \"%s\" -> \"%s\"\n", pth1, pth2);
#endif

    if (l == -1) {
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

    if (!fs_stat(pth2, &gstat[1], 0)
            && S_ISLNK(gstat[1].st_mode)
            && l == gstat[1].st_size)
    {
        char *const buf2 = malloc((size_t)gstat[0].st_size + 1);
        ssize_t l2 = readlink(pth2, buf2, (size_t)gstat[1].st_size);

        if (l2 == -1) {
            printerr(strerror(errno), "readlink %s", pth2);
            r = -1;
            goto free_buf2;
        }

        if (l2 != l) {
            printerr("Unexpected link lenght", "readlink %s", pth2);
            r = -1;
            goto free_buf2;
        }

        buf2[l2] = 0;

        if (!strcmp(buf, buf2)) {
#if defined(TRACE)
            fprintf(debug, "  Equal symlinks: %s and %s\n", pth1, pth2);
#endif
            equal = TRUE;
        }

free_buf2:
        free(buf2);
    }

    if (!equal) {
        if (!fs_stat(pth2, &gstat[1], 0) &&
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
        if (preserve_all) {
#ifdef HAVE_FUTIMENS
            struct timespec ts[2];
            ts[0] = gstat[0].st_atim;
            ts[1] = gstat[0].st_mtim;
            if (utimensat(0, pth2, ts, AT_SYMLINK_NOFOLLOW) == -1) {
#if defined(TRACE)
                fprintf(debug, "  utimensat(%s): %s\n", pth2, strerror(errno));
#endif
            }
#endif /* HAVE_FUTIMENS */
            if (lchown(pth2, gstat[0].st_uid, gstat[0].st_gid) == -1) {
#if defined(TRACE)
                fprintf(debug, "  lchown(%s): %s\n", pth2, strerror(errno));
#endif
            }
        }
        tot_cmp_byte_count += l;
        ++tot_cmp_file_count;
        if (!wstat && verbose)
            printf("Symbolic link copy \"%s\" -> \"%s\" done\n", pth1, pth2);
    }
exit:
    free(buf);
    if (exit_on_error && r < 0)
        fs_abort = TRUE;
#if defined(TRACE)
    fprintf(debug, "<-cp_link: %d\n", r);
#endif
    return r;
}

static int cp_reg_check_overwrite(const unsigned mode)
{
    int rv = 0;
    if (overwrite_if_old &&
            /* gstat[0].st_mtim < gstat[1].st_mtim -> dest is newer */
            cmp_timespec(gstat[0].st_mtim, gstat[1].st_mtim) < 0)
    {
        rv = 2;
    } else if (!(mode & 1) && /* file contents are not relevant in append mode */
            !cmp_file(pth1, gstat[0].st_size,
                      pth2, gstat[1].st_size, 1))
    {
#if defined(TRACE)
        fprintf(debug, "  But equal: %s and %s\n", pth1, pth2);
#endif
        rv = 1;
    } else if (!(mode & 2) && /* not in "force" mode */
            fs_deldialog(y_a_n_txt, "overwrite", "file ", pth2))
    {
        rv = -2;
    }
    return rv;
}

static int cp_reg_prepare_overwrite(void)
{
    int rv = 0;
    bool mode_set = FALSE; /* mode set after access(2) error */
test:
    if (access(pth2, W_OK) != -1) {
        rv = 1000;
        goto ret;
    }
    if (errno != EACCES) {
        /* Unexpected system call error */
        printerr(strerror(errno), "access \"%s\"", pth2);
    }
    /* Access permission error -> try chmod(2) */
    if (!mode_set && !(gstat[1].st_mode & S_IWUSR)) {
        if (chmod(pth2, gstat[1].st_mode & S_IWUSR) == -1) {
            printerr(strerror(errno), LOCFMT "chmod \"%s\"" LOCVAR, pth2);
        } else {
            mode_set = TRUE;
            goto test;
        }
    }
    /* mode could not be set or didn't help -> try to remove target */
    if (unlink(pth2) == -1) {
        printerr(strerror(errno), LOCFMT "unlink \"%s\"" LOCVAR, pth2);
    }
ret:
    return rv;
}

static int cp_reg_over_reg(const unsigned mode)
{
    int rv = cp_reg_check_overwrite(mode);
    if (!rv)
        rv = cp_reg_prepare_overwrite();
    return rv;
}

static int cp_reg_to_existing(const unsigned mode)
{
    int rv = 0;
#if defined(TRACE)
    fprintf(debug, "  Already exists: %s\n", pth2);
#endif
    if (S_ISREG(gstat[1].st_mode)) {
        /* target file is a regular file
         * or a followed symlink to a regular file */
        rv = cp_reg_over_reg(mode);
    } else {
        /* target file is a not followed symlink,
         * a directory or anything else */

        /* Don't delete symlinks! They must be followed. */
        if (!followlinks &&
            fs_rm(0, /* tree: Use pth2, ignore nam and u. n must be 1. */
                  "overwrite", /* Dialog text */
                  NULL, /* nam: See `tree`. */
                  0, /* u: See `tree`. */
                  1, /* n: See `tree`. */
                  4|2) /* md: Don't reset error, don't rebuild DB. */
                == 1) /* == Cancel */
        {
            rv = -2;
        }
    }
    return rv;
}

static int cp_reg_copy_loop(const int f1, const int f2) {
    int rv = 0;
    while (1) {
        const ssize_t l1 = read(f1, lbuf, sizeof lbuf);
        if (l1 == -1) {
            printerr(strerror(errno), "read \"%s\"", pth1);
            rv = -1;
            break;
        }
        if (!l1)
            break;
        const ssize_t l2 = write(f2, lbuf, (size_t)l1);
        if (l2 == -1) {
            fs_fwrap("write \"%s\": %s", pth2, strerror(errno));
            rv = -1;
            break;
        }
        if (l2 != l1) {
            fs_fwrap("%s: \"%s\"", "Write error", pth2);
            rv = -1;
            break; /* error */
        }
        tot_cmp_byte_count += l1;
        /* Not an error: Signals the last iteration. */
        if (l1 < (ssize_t)(sizeof lbuf))
            break;
    }
    return rv;
}

static void cp_reg_set_attr(const int f2) {
    if (preserve_all || preserve_mtim) {
#ifdef HAVE_FUTIMENS
        struct timespec ts[2];
        ts[0] = gstat[0].st_atim;
        ts[1] = gstat[0].st_mtim;
        if (futimens(f2, ts) == -1) {
#if defined(TRACE)
            fprintf(debug, "  futimes(%s): %s\n", pth2, strerror(errno));
#endif
        }
#else
        struct utimbuf tb;
        tb.actime  = gstat[0].st_atime;
        tb.modtime = gstat[0].st_mtime;
        utime(pth2, &tb); /* error not checked */
#endif
    }
    if (preserve_all) {
        if (fchown(f2, gstat[0].st_uid, gstat[0].st_gid) == -1) {
#if defined(TRACE)
            fprintf(debug, "  fchown(%s): %s\n", pth2, strerror(errno));
#endif
        }
        if (fchmod(f2, gstat[0].st_mode & 07777) == -1) {
#if defined(TRACE)
            fprintf(debug, "  fchmod(%s): %s\n", pth2, strerror(errno));
#endif
        }
    }
}

int cp_reg(const unsigned mode) {
#if defined(TRACE)
    fprintf(debug, "->cp_reg(mode=%x) pth1=\"%s\" pth2=\"%s\"\n", mode, pth1, pth2);
#endif
    int rv = 0; /* return value. must only be set in code. Clearing rv may hide errors. */
    if (fs_stat(pth2, &gstat[1], 0)) {
        if (errno && exit_on_error) {
            rv = -1;
            goto ret;
        }
    } else { /* target file exists */
        if (dont_overwrite)
            goto ret;
        int i;
        switch ((i = cp_reg_to_existing(mode))) {
        case 1000:
            break;
        /* case 2: dest newer than src, copy not necessary */
        /* case 1: if (*src == *dest) no copy is done */
        /* case 0: don't overwrite */
        /* negative value: error */
        default:
            rv = i;
            goto ret;
        }
	} /* if (!fs_stat(pth2)) */
    const int fl = mode & 1 ? O_APPEND | O_WRONLY :
                              O_CREAT | O_TRUNC | O_WRONLY ;
    const int f2 = open(pth2, fl, gstat[0].st_mode & 07777);
    if (f2 == -1) {
        printerr(strerror(errno), LOCFMT "create \"%s\"" LOCVAR, pth2);
		rv = -1;
        goto ret;
	}
    if (gstat[0].st_size) {
        const int f1 = open(pth1, O_RDONLY);
        if (f1 == -1) {
            printerr(strerror(errno), "open \"%s\"", pth1);
            rv = -1;
            goto close2;
        }
        cp_reg_copy_loop(f1, f2);
        close(f1);
        ++tot_cmp_file_count;
    }
    cp_reg_set_attr(f2);
close2:
	close(f2);
    if (!rv && !wstat && verbose) {
        printf("File copy \"%s\" -> \"%s\" done\n", pth1, pth2);
    }
ret:
    if (exit_on_error && rv < 0)
        fs_abort = TRUE;
#if defined(TRACE)
	fprintf(debug, "<-cp_reg: %d\n", rv);
#endif
    return rv;
}

static void fs_fwrap(const char *f, ...)
{
    if (fs_ign_errs)
        return;
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
    if (exit_on_error)
        fs_abort = TRUE;
}

static int
fs_deldialog(const char *menu, const char *op, const char *typ,
    const char *nam)
{
    int retval = 0; /* "yes" */

#if defined(TRACE)
    fprintf(debug, "->fs_deldialog\n");
#endif

    if (fs_none || fs_abort) {
        retval = 1;
        goto ret;
	}

	if (force_fs || fs_all) {
        goto ret;
	}

    switch (dialog(menu, /* Answers as text */
                   NULL, /* Answer char array */
                   "Really %s %s\"%s\"?",
                   op, /* Operation, e.g. "delete" */
                   typ ? typ : "", /* File type, e.g. "symlink" */
                   nam)) /* File name */
    {
	case 'a':
		fs_all = TRUE;
		/* fall through */

	case 'y':
        goto ret;

	case '':
		fs_abort = 1;
        retval = 1;
        goto ret;

	case 'N':
		fs_none = TRUE;
		/* fall through */

	default:
        retval = 1;
	}

ret:
#if defined(TRACE)
    fprintf(debug, "<-fs_deldialog: %d\n", retval);
#endif
    return retval;
}

/* 0: Error
 * 1: Left tree
 * 2: Right tree */

int
fs_get_dst(long u,
    /* 1: auto-detect */
    /* 2: Exchange */
    unsigned m)
{
	struct filediff *f;
	int dst = 0;

	if (bmode) {
		dst = 2;
	} else if (fmode) {
		dst = m         ? 0 :
		      right_col ? 1 :
		                  2 ;
	} else {
		if (u >= (long)db_num[0]) {
			goto ret;
		}

		f = db_list[0][u];

		if (m == 2) {
			dst = f->diff == '!' ? 3 : 0;
		} else if (f->type[0]) {
			if (f->type[1]) {
                const int i = cmp_timespec(f->mtim[0], f->mtim[1]);
				if (!m || !S_ISREG(f->type[0]) ||
				          !S_ISREG(f->type[1])) {

					/* return 0 */

                } else if (i < 0) {
					dst = 1;

                } else if (i > 0) {
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
fs_any_dst(long u, int n, unsigned m)
{
	for (; n--; u++) {
		if (fs_get_dst(u, m)) {
			return 1;
		}
	}

	return 0;
}

int
fs_stat(const char *p, struct stat *s,
    /* 1: also report ENOENT */
    const unsigned mode)
{
    int i = -1;
    int saved_errno = 0;
#if defined(TRACE) && (defined(TEST) || 1)
	fprintf(debug, "->fs_stat(path=%s mode=%u) follow=%d\n",
		p, mode, followlinks);
#endif

	if (( followlinks && (i =  stat(p, s)) == -1) ||
        (!followlinks && (i = lstat(p, s)) == -1))
    {
		if (errno != ENOENT /* report any error that is not ENOENT */
		    || (mode & 1)) /* report even ENOENT when `mode & 1` */
		{
            saved_errno = errno;
            printerr(strerror(errno), LOCFMT "stat \"%s\""
			    LOCVAR, p);
            if (exit_on_error)
                fs_abort = TRUE;
		}
	}

#if defined(TRACE) && (defined(TEST) || 1)
	fprintf(debug, "<-fs_stat(): %d\n", i);
#endif
    errno = saved_errno;
	return i;
}
