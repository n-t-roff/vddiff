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
#include <regex.h>
#include <time.h>
#include "compat.h"
#include "main.h"
#include "ui.h"
#include "diff.h"
#include "uzp.h"
#include "exec.h"
#include "db.h"
#include "ui2.h"
#include "gq.h"

struct str_list {
	char *s;
	struct str_list *next;
};

static struct filediff *alloc_diff(char *);
static void add_diff_dir(void);
static char *read_link(char *, off_t);

static struct filediff *diff;
static off_t lsiz1, lsiz2;
static size_t bmode_ini_len;

short followlinks;

static bool ign_diff_errs;

/* 1: Proc left dir
 * 2: Proc right dir
 * 3: Proc both dirs */
int
build_diff_db(int tree)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	struct str_list *dirs = NULL;
	int retval = 0;
	short dir_diff = 0;
	bool file_err = FALSE;
	static time_t lpt, lpt2;

	if (!(tree & 1))
		goto right_tree;

	if (bmode && !scan) {
		if (!getcwd(rpath, sizeof rpath))
			printerr(strerror(errno), "getcwd failed");

		if (!bmode_ini_len)
			bmode_ini_len = strlen(rpath) + 1; /* + '/' */

		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory %s", rpath);
			lpt = lpt2;
		}
	} else if (!qdiff) {
		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory %s", lpath);
			lpt = lpt2;
		}
	}

	if (!(d = opendir(lpath))) {
		if (!ign_diff_errs && dialog(
		    "'i' ignore errors, <other key> continue",
		    NULL, "opendir \"%s\" failed: %s", lpath,
		    strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		retval = -1;
		goto dir_scan_end;
	}

	while (1) {
		int i;

		errno = 0;

		if (!(ent = readdir(d))) {
			if (!errno)
				break;

			lpath[llen] = 0;
			printerr(strerror(errno), "readdir \"%s\" failed",
			    lpath);
			closedir(d);
			retval = -1;
			goto dir_scan_end;
		}

		name = ent->d_name;

		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		str_db_add(&name_db, strdup(name)
#ifdef HAVE_LIBAVLBST
		    , 0, NULL
#endif
		    );
		pthcat(lpath, llen, name);

		/* Get link length. Redundant code but necessary,
		 * unfortunately. */

		if (followlinks && !scan && lstat(lpath, &stat1) != -1 &&
		    S_ISLNK(stat1.st_mode))
			lsiz1 = stat1.st_size;
		else
			lsiz1 = -1;

		file_err = FALSE;

		if (!followlinks || (i = stat(lpath, &stat1)) == -1)
			i = lstat(lpath, &stat1);

		if (i == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno),
				    "stat \"%s\" failed", lpath);
				file_err = TRUE;

				if (scan || qdiff)
					continue;
			}

			stat1.st_mode = 0;
		}

		if (tree & 2) {
			pthcat(rpath, rlen, name);
		} else
			goto no_tree2;

		if (followlinks && !scan && lstat(rpath, &stat2) != -1 &&
		    S_ISLNK(stat2.st_mode))
			lsiz2 = stat2.st_size;
		else
			lsiz2 = -1;

		if (!followlinks || (i = stat(rpath, &stat2)) == -1)
			i = lstat(rpath, &stat2);

		if (i == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno),
				    "stat \"%s\" failed", rpath);
				file_err = TRUE;

				if (scan || qdiff)
					continue;
			}

no_tree2:
			if (qdiff) {
				lpath[llen] = 0;
				printf("Only in %s: %s\n", lpath,
				    name);
				continue;
			}

			stat2.st_mode = 0;
		}

		if (scan || qdiff) {
			if (S_ISDIR(stat1.st_mode) &&
			    (S_ISDIR(stat2.st_mode) || bmode)) {
				if (!scan)
					/* Non-recursive qdiff */
					continue;

				struct str_list *se =
				    malloc(sizeof(struct str_list));
				se->s = strdup(name);
				se->next = dirs ? dirs : NULL;
				dirs = se;
				continue;
			}

			if (!qdiff && (!*pwd || dir_diff))
				continue;

			if (find_name) {
				if (regexec(&fn_re, name, 0, NULL, 0))
					continue;
				else if (!gq_pattern) {
					dir_diff = 1;
					continue;
				}
			}

			if (gq_pattern) {
				diff = alloc_diff(name);
				diff->ltype = stat1.st_mode;
				diff->rtype = stat2.st_mode;
				diff->lsiz  = stat1.st_size;
				diff->rsiz  = stat2.st_size;

				if (!gq_proc(diff))
					dir_diff = 1;

				free_diff(diff);
				continue;
			}

			if (S_ISREG(stat1.st_mode) &&
			    S_ISREG(stat2.st_mode)) {
				if (cmp_file(lpath, stat1.st_size, rpath,
				    stat2.st_size) == 1) {
					if (qdiff)
						printf(
						    "Files %s and %s differ\n",
						    lpath, rpath);
					else
						dir_diff = 1;
				}
				continue;
			}

			if (S_ISLNK(stat1.st_mode) &&
			    S_ISLNK(stat2.st_mode)) {
				char *a, *b;

				if (!(a = read_link(lpath, stat1.st_size)))
					continue;

				if (!(b = read_link(rpath, stat2.st_size)))
					goto free_a;

				if (strcmp(a, b)) {
					if (qdiff)
						printf(
						    "Symbolic links "
						    "%s and %s differ\n",
						    lpath, rpath);
					else
						dir_diff = 1;
				}

				free(b);

free_a:
				free(a);
				continue;
			}

			if (real_diff)
				continue;

			if (!stat1.st_mode || !stat2.st_mode ||
			     stat1.st_mode !=  stat2.st_mode) {
				if (qdiff)
					printf("Different file type: "
					    "%s and %s\n", lpath, rpath);
				else
					dir_diff = 1;
				continue;
			}

			continue;
		}

		diff = alloc_diff(name);

		if (file_err) {
			diff->diff = '-';
			diff_db_add(diff, FALSE);
			continue;
		}

		if ((diff->ltype = stat1.st_mode)) {
			diff->luid  = stat1.st_uid;
			diff->lgid  = stat1.st_gid;
			diff->lsiz  = stat1.st_size;
			diff->lmtim = stat1.st_mtim.tv_sec;
			diff->lrdev = stat1.st_rdev;

			if (S_ISLNK(stat1.st_mode))
				lsiz1 = stat1.st_size;

			if (lsiz1 >= 0)
				diff->llink = read_link(lpath, lsiz1);
		}

		if ((diff->rtype = stat2.st_mode)) {
			diff->ruid  = stat2.st_uid;
			diff->rgid  = stat2.st_gid;
			diff->rsiz  = stat2.st_size;
			diff->rmtim = stat2.st_mtim.tv_sec;
			diff->rrdev = stat2.st_rdev;

			if (S_ISLNK(stat2.st_mode))
				lsiz2 = stat2.st_size;

			if (lsiz2 >= 0)
				diff->rlink = read_link(rpath, lsiz2);
		}

		if ((diff->ltype & S_IFMT) != (diff->rtype & S_IFMT)) {

			diff_db_add(diff, FALSE);
			continue;

		} else if (stat1.st_ino == stat2.st_ino &&
		           stat1.st_dev == stat2.st_dev) {

			diff->diff = '=';
			diff_db_add(diff, FALSE);
			continue;

		} else if (S_ISREG(stat1.st_mode)) {

			switch (cmp_file(lpath, stat1.st_size, rpath,
			    stat2.st_size)) {
			case -1:
				diff->diff = '-';
				goto db_add_file;
			case 1:
				diff->diff = '!';
				/* fall through */
			case 0:
db_add_file:
				diff_db_add(diff, FALSE);
				continue;
			}

		} else if (S_ISDIR(stat1.st_mode)) {

			diff_db_add(diff, FALSE);
			continue;

		} else if (S_ISLNK(stat1.st_mode)) {

			if (diff->llink && diff->rlink) {
				if (strcmp(diff->llink, diff->rlink))
					diff->diff = '!';
				diff_db_add(diff, FALSE);
				continue;
			}

		/* any other file type */
		} else {
			diff_db_add(diff, FALSE);
			continue;
		}

		free(diff);
	}

	closedir(d);
	lpath[llen] = 0;

	if (tree & 2)
		rpath[rlen] = 0;

right_tree:
	if (!(tree & 2) || bmode)
		goto build_list;

	if (scan && (real_diff || dir_diff))
		goto dir_scan_end;

	if (!qdiff)
		printerr(NULL, "Reading directory %s", rpath);

	if (!(d = opendir(rpath))) {
		if (!ign_diff_errs && dialog(
		    "'i' ignore errors, <other key> continue",
		    NULL, "opendir \"%s\" failed: %s", rpath,
		    strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		retval = -1;
		goto dir_scan_end;
	}

	while (1) {
		int i;

		errno = 0;

		if (!(ent = readdir(d))) {
			if (!errno)
				break;
			printerr(strerror(errno), "readdir %s failed", rpath);
			closedir(d);
			retval = -1;
			goto dir_scan_end;
		}

		name = ent->d_name;

		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		if ((tree & 1) && !str_db_srch(&name_db, name
#ifdef HAVE_LIBAVLBST
		    , NULL
#endif
		    ))
			continue;

		if (qdiff) {
			rpath[rlen] = 0;
			printf("Only in %s: %s\n", rpath, name);
			continue;
		} else if (scan && !file_pattern) {
			dir_diff = 1;
			break;
		}

		pthcat(rpath, rlen, name);

		if (followlinks && !scan && lstat(rpath, &stat2) != -1 &&
		    S_ISLNK(stat2.st_mode))
			lsiz2 = stat2.st_size;
		else
			lsiz2 = -1;

		file_err = FALSE;

		if (!followlinks || (i = stat(rpath, &stat2)) == -1)
			i = lstat(rpath, &stat2);

		if (i == -1) {
			if (errno != ENOENT) {
				printerr(strerror(errno), "stat %s failed",
				    rpath);
				file_err = TRUE;
			}

			stat2.st_mode = 0;
		}

		if (scan && find_name) {
			if (!S_ISDIR(stat2.st_mode) &&
			    !regexec(&fn_re, name, 0, NULL, 0) &&
			    !gq_pattern) {
				dir_diff = 1;
				break;
			} else
				continue;
		}

		diff = alloc_diff(name);
		diff->ltype = 0;
		diff->rtype = stat2.st_mode;

		if (file_err)
			diff->diff = '-';
		else {
			diff->ruid  = stat2.st_uid;
			diff->rgid  = stat2.st_gid;
			diff->rsiz  = stat2.st_size;
			diff->rmtim = stat2.st_mtim.tv_sec;
			diff->rrdev = stat2.st_rdev;

			if (S_ISLNK(stat2.st_mode))
				lsiz2 = stat2.st_size;

			if (lsiz2 >= 0)
				diff->rlink = read_link(rpath, lsiz2);
		}

		if (scan && gq_pattern) {
			if (!gq_proc(diff)) {
				free_diff(diff);
				dir_diff = 1;
				break;
			}

			free_diff(diff);
			continue;
		}

		diff_db_add(diff, FALSE);
	}

	closedir(d);

build_list:
	if (!scan)
		diff_db_sort(FALSE);

dir_scan_end:
	free_names();

	if (!scan)
		return retval;

	if (dir_diff && !qdiff)
		add_diff_dir();

	while (dirs) {
		size_t l1, l2 = 0 /* silence warning */;
		struct str_list *p;

		l1 = llen;
		l2 = rlen;
		scan_subdir(dirs->s, NULL, 3);
		/* Not done in scan_subdirs(), since there are cases where
		 * scan_subdirs() must not reset the path */
		lpath[llen = l1] = 0;
		rpath[rlen = l2] = 0;

		free(dirs->s);
		p = dirs;
		dirs = dirs->next;
		free(p);
	}

	return retval;
}

void
scan_subdir(char *name, char *rnam, int tree)
{
	if (tree & 1) {
		if (name)
			llen = pthcat(lpath, llen, name);
		else
			lpath[llen] = 0; /* -> lpath = "." */
	}

	if (tree & 2)
		rlen = pthcat(rpath, rlen, rnam ? rnam : name);

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
#ifdef HAVE_LIBAVLBST
		struct bst_node *n;
		int i;

		if (!(i = str_db_srch(&scan_db, path, &n)))
			goto ret;

		str_db_add(&scan_db, strdup(path), i, n);
#else
		char *s, *s2;

		s = strdup(path);
		s2 = str_db_add(&scan_db, s);

		if (s2 != s) {
			free(s);
			goto ret;
		}
#endif

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
	size_t l1;
	short v;

	if (!recursive)
		return 0;

	if ((!bmode && !*pwd) ||
	    (bmode && strlen(rpath) < bmode_ini_len))
		return str_db_srch(&scan_db, name
#ifdef HAVE_LIBAVLBST
		    , NULL
#endif
		    ) ? 0 : 1;

	if (bmode) {
		l1 = strlen(rpath) - bmode_ini_len;
		s = malloc(l1 + strlen(name) + 2);
		memcpy(s, rpath + bmode_ini_len, l1);
	} else {
		l1 = strlen(PWD);
		s = malloc(l1 + strlen(name) + 2);
		memcpy(s, PWD, l1);
	}

	pthcat(s, l1, name);

	v = str_db_srch(&scan_db, s
#ifdef HAVE_LIBAVLBST
	    , NULL
#endif
	    ) ? 0 : 1;

	free(s);
	return v;
}

static char *
read_link(char *path, off_t size)
{
	char *l = malloc(size + 1);

	if ((size = readlink(path, l, size)) == -1) {
		if (!ign_diff_errs && dialog(
		    "'i' ignore errors, <other key> continue",
		    NULL, "readlink \"%s\" failed: %s", path,
		    strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		free(l);
		return NULL;
	}

	l[size] = 0;
	return l;
}

/* Input: stat1, stat2, lpath, rpath
 * Output:
 * -1  Error, don't make DB entry
 *  0  No diff
 *  1  Diff */

int
cmp_file(char *lpth, off_t lsiz, char *rpth, off_t rsiz)
{
	int rv = 0, f1, f2;
	ssize_t l1, l2;

	if (lsiz != rsiz)
		return 1;

	if (!lsiz)
		return 0;

	if ((f1 = open(lpth, O_RDONLY)) == -1) {
		if (!ign_diff_errs && dialog(
		    "'i' ignore errors, <other key> continue",
		    NULL, "open \"%s\" failed: %s", lpth,
		    strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		return -1;
	}

	if ((f2 = open(rpth, O_RDONLY)) == -1) {
		if (!ign_diff_errs && dialog(
		    "'i' ignore errors, <other key> continue",
		    NULL, "open \"%s\" failed: %s", rpth,
		    strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		rv = -1;
		goto close_f1;
	}

	while (1) {
		if ((l1 = read(f1, lbuf, sizeof lbuf)) == -1) {
			if (!ign_diff_errs && dialog(
			    "'i' ignore errors, <other key> continue",
			    NULL, "read \"%s\" failed: %s", lpth,
			    strerror(errno)) == 'i')
				ign_diff_errs = TRUE;

			rv = -1;
			break;
		}

		if ((l2 = read(f2, rbuf, sizeof rbuf)) == -1) {
			if (!ign_diff_errs && dialog(
			    "'i' ignore errors, <other key> continue",
			    NULL, "read \"%s\" failed: %s", rpth,
			    strerror(errno)) == 'i')
				ign_diff_errs = TRUE;

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
free_diff(struct filediff *f)
{
	free(f->name);
	free(f->llink);
	free(f->rlink);
	free(f);
}

size_t
pthcat(char *p, size_t l, char *n)
{
	size_t ln = strlen(n);

	if (l + ln + 2 > PATHSIZ) {
		printerr(NULL, "Path buffer overflow");
		return l;
	}

	/* For archives push_state() sets l = 0 */
	if (l && p[l-1] != '/')
		p[l++] = '/';

	memcpy(p + l, n, ln + 1);
	return l + ln;
}
