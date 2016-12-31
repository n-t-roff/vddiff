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
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <regex.h>
#include <time.h>
#include <stdarg.h>
#include "compat.h"
#include "main.h"
#include "ui.h"
#include "diff.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"
#include "ui2.h"
#include "gq.h"
#include "tc.h"

struct scan_dir {
	char *s;
	short tree;
	struct scan_dir *next;
};

static struct filediff *alloc_diff(char *);
static void add_diff_dir(short);
static char *read_link(char *, off_t);
static size_t pthcut(char *, size_t);
static void ini_int(void);
static int is_diff_pth(const char *);

static struct filediff *diff;
static off_t lsiz1, lsiz2;

short followlinks;

bool one_scan;
static bool ign_diff_errs;

/* !0: Error */
int
build_diff_db(
    /* 1: Proc left dir
     * 2: Proc right dir
     * 3: Proc both dirs */
    int tree)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	struct scan_dir *dirs = NULL;
	int retval = 0;
	/* Used to show only dirs which contains diffs. Is set if any diff
	 * is found inside a dir. */
	short dir_diff = 0;
	bool file_err = FALSE;
	static time_t lpt, lpt2;

	if ((bmode || fmode) && !file_pattern) {
		if (scan) {
			return retval; /* scan useless in this case */
		}

		one_scan = FALSE;
	}

	if (one_scan) {
		one_scan = FALSE;

		if (recursive) {
			do_scan();
		}
	}

	if (!(tree & 1)) {
		goto right_tree;
	}

	if (bmode && !scan) {
		if (!getcwd(syspth[1], sizeof syspth[1])) {
			printerr(strerror(errno), "getcwd failed");
		}

		pthlen[1] = strlen(syspth[1]);

		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory \"%s\"", syspth[1]);
			lpt = lpt2;
		}
	} else if (!qdiff) {
		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory \"%s\"", syspth[0]);
			lpt = lpt2;
		}
	}

	ini_int();

#if defined(TRACE)
	fprintf(debug, "->build_diff_db tree(%d) opendir lp(%s)%s\n", tree,
	    syspth[0], scan ? " scan" : "");
#endif
	if (!(d = opendir(syspth[0]))) {
		if (!ign_diff_errs && dialog(ign_txt, NULL,
		    "opendir \"%s\": %s", syspth[0],
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

			syspth[0][pthlen[0]] = 0;
			printerr(strerror(errno), "readdir \"%s\"", syspth[0]);
			closedir(d);
			retval = -1;
			goto dir_scan_end;
		}

		name = ent->d_name;

		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		if (!(bmode || fmode)) {
			str_db_add(&name_db, strdup(name)
#ifdef HAVE_LIBAVLBST
			    , 0, NULL
#endif
			    );
		}

		pthcat(syspth[0], pthlen[0], name);

		/* Get link length. Redundant code but necessary,
		 * unfortunately. */

		if (followlinks && !scan && lstat(syspth[0], &stat1) != -1 &&
		    S_ISLNK(stat1.st_mode))
			lsiz1 = stat1.st_size;
		else
			lsiz1 = -1;

		file_err = FALSE;

		if (!followlinks || (i = stat(syspth[0], &stat1)) == -1)
			i = lstat(syspth[0], &stat1);

		if (i == -1) {
			if (errno != ENOENT) {
				if (!ign_diff_errs && dialog(ign_txt, NULL,
				    LOCFMT "stat \"%s\": %s" LOCVAR,
				    syspth[0], strerror(errno)) == 'i') {

					ign_diff_errs = TRUE;
				}

				file_err = TRUE;

				if (scan || qdiff)
					continue;
			}

			stat1.st_mode = 0;
		}

		if (tree & 2) {
			pthcat(syspth[1], pthlen[1], name);
		} else
			goto no_tree2;

		if (followlinks && !scan && lstat(syspth[1], &stat2) != -1 &&
		    S_ISLNK(stat2.st_mode))
			lsiz2 = stat2.st_size;
		else
			lsiz2 = -1;

		if (!followlinks || (i = stat(syspth[1], &stat2)) == -1)
			i = lstat(syspth[1], &stat2);

		if (i == -1) {
			if (errno != ENOENT) {
				if (!ign_diff_errs && dialog(ign_txt, NULL,
				    LOCFMT "stat \"%s\" failed: %s"
				    LOCVAR, syspth[1], strerror(errno)) == 'i') {

					ign_diff_errs = TRUE;
				}

				file_err = TRUE;

				if (scan || qdiff)
					continue;
			}

no_tree2:
			if (qdiff) {
				syspth[0][pthlen[0]] = 0;
				printf("Only in %s: %s\n", syspth[0],
				    name);
				continue;
			}

			stat2.st_mode = 0;
		}

		if (scan || qdiff) {
			if (S_ISDIR(stat1.st_mode) &&
			    (S_ISDIR(stat2.st_mode) || bmode || fmode)) {

				struct scan_dir *se;

				if (!scan) {
					/* Non-recursive qdiff */
					continue;
				}

				se = malloc(sizeof(struct scan_dir));
				se->s = strdup(name);
				se->tree = S_ISDIR(stat2.st_mode) ? 3 : 1;
				se->next = dirs;
				dirs = se;
				continue;
			}

			if (find_name) {
				if (regexec(&fn_re, name, 0, NULL, 0)) {
					continue;
				} else if (!gq_pattern) {
					dir_diff = 1;
					continue;
				}
			}

			if (gq_pattern) {
				diff = alloc_diff(name);
				diff->type[0] = stat1.st_mode;
				diff->type[1] = stat2.st_mode;
				diff->siz[0]  = stat1.st_size;
				diff->siz[1]  = stat2.st_size;

				if (!gq_proc(diff))
					dir_diff = 1;

				free_diff(diff);
				continue;
			}

			if (S_ISREG(stat1.st_mode) &&
			    S_ISREG(stat2.st_mode)) {
				if (cmp_file(syspth[0], stat1.st_size, syspth[1],
				    stat2.st_size, 0) == 1) {
					if (qdiff) {
						printf(
						    "Files %s and %s differ\n",
						    syspth[0], syspth[1]);
					} else {
						dir_diff = 1;
					}
				}
				continue;
			}

			if (S_ISLNK(stat1.st_mode) &&
			    S_ISLNK(stat2.st_mode)) {
				char *a, *b;

				if (!(a = read_link(syspth[0], stat1.st_size)))
					continue;

				if (!(b = read_link(syspth[1], stat2.st_size)))
					goto free_a;

				if (strcmp(a, b)) {
					if (qdiff)
						printf(
						    "Symbolic links "
						    "%s and %s differ\n",
						    syspth[0], syspth[1]);
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
					    "%s and %s\n", syspth[0], syspth[1]);
				else
					dir_diff = 1;
				continue;
			}

			continue;
		}

		diff = alloc_diff(name);

		if (file_err) {
			diff->diff = '-';
			diff_db_add(diff, 0);
			continue;
		}

		if ((diff->type[0] = stat1.st_mode)) {
			diff->luid  = stat1.st_uid;
			diff->lgid  = stat1.st_gid;
			diff->siz[0]  = stat1.st_size;
			diff->mtim[0] = stat1.st_mtim.tv_sec;
			diff->lrdev = stat1.st_rdev;

			if (S_ISLNK(stat1.st_mode))
				lsiz1 = stat1.st_size;

			if (lsiz1 >= 0)
				diff->llink = read_link(syspth[0], lsiz1);
		}

		if ((diff->type[1] = stat2.st_mode)) {
			diff->ruid  = stat2.st_uid;
			diff->rgid  = stat2.st_gid;
			diff->siz[1]  = stat2.st_size;
			diff->mtim[1] = stat2.st_mtim.tv_sec;
			diff->rrdev = stat2.st_rdev;

			if (S_ISLNK(stat2.st_mode))
				lsiz2 = stat2.st_size;

			if (lsiz2 >= 0)
				diff->rlink = read_link(syspth[1], lsiz2);
		}

		if ((diff->type[0] & S_IFMT) != (diff->type[1] & S_IFMT)) {

			diff_db_add(diff, 0);
			continue;

		} else if (stat1.st_ino == stat2.st_ino &&
		           stat1.st_dev == stat2.st_dev) {

			diff->diff = '=';
			diff_db_add(diff, 0);
			continue;

		} else if (S_ISREG(stat1.st_mode)) {

			switch (cmp_file(syspth[0], stat1.st_size, syspth[1],
			    stat2.st_size, 0)) {
			case -1:
				diff->diff = '-';
				goto db_add_file;
			case 1:
				diff->diff = '!';
				/* fall through */
			case 0:
db_add_file:
				diff_db_add(diff, 0);
				continue;
			}

		} else if (S_ISDIR(stat1.st_mode)) {

			diff_db_add(diff, 0);
			continue;

		} else if (S_ISLNK(stat1.st_mode)) {

			if (diff->llink && diff->rlink) {
				if (strcmp(diff->llink, diff->rlink))
					diff->diff = '!';
				diff_db_add(diff, 0);
				continue;
			}

		/* any other file type */
		} else {
			diff_db_add(diff, 0);
			continue;
		}

		free(diff);
	}

	closedir(d);
	syspth[0][pthlen[0]] = 0;

	/* Now already done here for diff mode to use syspth[0] instead of syspth[1].
	 * May be useless. */
	if (scan && dir_diff && !qdiff) {
		add_diff_dir(0);
		dir_diff = 0;
	}

	if (tree & 2)
		syspth[1][pthlen[1]] = 0;

right_tree:
	if (!(tree & 2) || bmode)
		goto build_list;

	if (scan && (real_diff || dir_diff))
		goto dir_scan_end;

	if (!qdiff) {
		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory \"%s\"", syspth[1]);
			lpt = lpt2;
		}
	}

	ini_int();

#if defined(TRACE)
	fprintf(debug, "  opendir rp(%s)%s\n", syspth[1], scan ? " scan" : "");
#endif
	if (!(d = opendir(syspth[1]))) {
		if (!ign_diff_errs && dialog(ign_txt, NULL,
		    "opendir \"%s\" failed: %s", syspth[1],
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
			printerr(strerror(errno), "readdir \"%s\" failed",
			    syspth[1]);
			closedir(d);
			retval = -1;
			goto dir_scan_end;
		}

		name = ent->d_name;

		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2]))) {
			continue;
		}

		if (!(bmode || fmode) && (tree & 1) && !str_db_srch(&name_db,
		    name
#ifdef HAVE_LIBAVLBST
		    , NULL
#endif
		    )) {
			continue;
		}

		if (qdiff) {
			syspth[1][pthlen[1]] = 0;
			printf("Only in %s: %s\n", syspth[1], name);
			continue;
		} else if (scan && !file_pattern) {
			dir_diff = 1;
			break;
		}

		pthcat(syspth[1], pthlen[1], name);

		if (followlinks && !scan && lstat(syspth[1], &stat2) != -1 &&
		    S_ISLNK(stat2.st_mode)) {
			lsiz2 = stat2.st_size;
		} else {
			lsiz2 = -1;
		}

		file_err = FALSE;

		if (!followlinks || (i = stat(syspth[1], &stat2)) == -1) {
			i = lstat(syspth[1], &stat2);
		}

		if (i == -1) {
			if (errno != ENOENT) {
				if (!ign_diff_errs && dialog(ign_txt, NULL,
				    LOCFMT "stat \"%s\" failed: %s"
				    LOCVAR, syspth[1], strerror(errno)) == 'i') {

					ign_diff_errs = TRUE;
				}

				file_err = TRUE;
			}

			stat2.st_mode = 0;
		}

		if (scan) {
			if (S_ISDIR(stat2.st_mode)) {
				struct scan_dir *se;

				se = malloc(sizeof(struct scan_dir));
				se->s = strdup(name);
				se->tree = 2;
				se->next = dirs;
				dirs = se;
				continue;
			}

			if (find_name) {
				if (regexec(&fn_re, name, 0, NULL, 0)) {
					/* No match */
					continue;
				} else if (
				    /* else *also* gq need to match */
				    !gq_pattern) {
					dir_diff = 1;
					continue;
				}
			}
		}

		diff = alloc_diff(name);
		diff->type[0] = 0;
		diff->type[1] = stat2.st_mode;

		if (file_err)
			diff->diff = '-';
		else {
			diff->ruid  = stat2.st_uid;
			diff->rgid  = stat2.st_gid;
			diff->siz[1]  = stat2.st_size;
			diff->mtim[1] = stat2.st_mtim.tv_sec;
			diff->rrdev = stat2.st_rdev;

			if (S_ISLNK(stat2.st_mode))
				lsiz2 = stat2.st_size;

			if (lsiz2 >= 0)
				diff->rlink = read_link(syspth[1], lsiz2);
		}

		if (scan) {
			if (gq_pattern && !gq_proc(diff)) {
				dir_diff = 1;
			}

			free_diff(diff);
			continue;
		}

		diff_db_add(diff, fmode ? 1 : 0);
	}

	closedir(d);

build_list:
	if (!scan)
		diff_db_sort(fmode && (tree & 2) ? 1 : 0);

dir_scan_end:
	free_names();

	if (!scan) {
		goto exit;
	}

	if (dir_diff && !qdiff) {
		add_diff_dir(1);
	}

	while (dirs) {
		size_t l1, l2 = 0 /* silence warning */;
		struct scan_dir *p;

		l1 = pthlen[0];
		l2 = pthlen[1];
		scan_subdir(dirs->s, NULL, dirs->tree);
		/* Not done in scan_subdirs(), since there are cases where
		 * scan_subdirs() must not reset the path */
		syspth[0][pthlen[0] = l1] = 0;
		syspth[1][pthlen[1] = l2] = 0;

		free(dirs->s);
		p = dirs;
		dirs = dirs->next;
		free(p);
	}

exit:
	nodelay(stdscr, FALSE);
#if defined(TRACE)
	fprintf(debug, "<-build_diff_db%s\n", scan ? " scan" : "");
#endif
	return retval;
}

static void
ini_int(void)
{
	mvwaddstr(wstat, 0, 0, "Type '%' to disable file compare");
	wrefresh(wstat);
	nodelay(stdscr, TRUE);
}

int
scan_subdir(char *name, char *rnam, int tree)
{
	int i;
#if defined(TRACE)
	TRCPTH;
	fprintf(debug, "->scan_subdir(%s,%s,%d) lp(%s) rp(%s)\n",
	    name, rnam, tree, trcpth[0], trcpth[1]);
#endif
	if (!rnam) {
		rnam = name;
	}

	if (tree & 1) {
		if (name)
			pthlen[0] = pthcat(syspth[0], pthlen[0], name);
		else
			syspth[0][pthlen[0]] = 0; /* -> syspth[0] = "." */
	}

	if (tree & 2) {
		if (rnam)
			pthlen[1] = pthcat(syspth[1], pthlen[1], rnam);
		else
			syspth[1][pthlen[1]] = 0; /* fmode_cp_pth() */
	}

	i = build_diff_db(tree);
#if defined(TRACE)
	fprintf(debug, "<-scan_subdir: %d\n", i);
#endif
	return i;
}

static void
add_diff_dir(
    /* Only fmode: 0: syspth[0], 1: syspth[1] */
    short side)
{
	char *path, *end, *rp = NULL;

	/* During scan bmode uses syspth[0] */
	syspth[0][pthlen[0]] = 0;
	syspth[1][pthlen[1]] = 0;
	path = side ? syspth[1] : syspth[0];
#if defined(TRACE)
	fprintf(debug, "->add_diff_dir(%s:%s) lp(%s) rp(%s)\n",
	    side ? "right" : "left", path, syspth[0], syspth[1]);
#endif

	if (!(rp = realpath(path, NULL))) {
		printerr(strerror(errno), LOCFMT "realpath \"%s\""
		    LOCVAR, path);
		goto ret0;
	}

	path = rp;
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

#if defined(TRACE)
		fprintf(debug, "  \"%s\" added\n", path);
#endif
		do {
			if (--end < path)
				goto ret;
		} while (*end != '/');

		if (end == path) {
			end[1] = 0;
		} else {
			*end = 0;
		}
	}

ret:
	free(rp);
ret0:
#if defined(TRACE)
	fprintf(debug, "<-add_diff_dir\n");
#endif
	return;
}

int
is_diff_dir(struct filediff *f)
{
	char *bp = NULL, *pth;
	size_t l;
	int v = 0;

	/* E.g. for file stat called independend from 'recursive' */
	if (!recursive) {
		goto ret0; /* No debug print */
	}
#if defined(TRACE)
	fprintf(debug, "->is_diff_dir(%s)\n", f->name);
#endif
	if (bmode) {
		pth = syspth[1];
		l = strlen(pth);
		bp = malloc(l + strlen(f->name) + 2);
		memcpy(bp, pth, l);
		pth = bp;
		pthcat(pth, l, f->name);
		v = is_diff_pth(pth);
		free(bp);
	} else {
		if (f->type[0]) {
			pth = syspth[0];
			l = pthlen[0];
			pthcat(pth, l, f->name);
			v = is_diff_pth(pth);
		}

		if (!v && f->type[1]) {
			pth = syspth[1];
			l = pthlen[1];
			pthcat(pth, l, f->name);
			v = is_diff_pth(pth);
		}

		pth[l] = 0;
	}

#if defined(TRACE)
	fprintf(debug, "<-is_diff_dir: %d\n", v);
#endif
ret0:
	return v;
}

static int
is_diff_pth(const char *p)
{
	char *rp = NULL;
	int v = 0;
#if defined(TRACE)
	fprintf(debug, "->is_diff_pth(%s)\n", p);
#endif
	/* Here since both path and name can be symlink */
	if (!(rp = realpath(p, NULL))) {
		printerr(strerror(errno), LOCFMT "realpath \"%s\""
		    LOCVAR, p);
		goto ret;
	}
#if defined(TRACE)
	fprintf(debug, "  realpath: \"%s\"\n", p);
#endif
	v = str_db_srch(&scan_db, rp
#ifdef HAVE_LIBAVLBST
	    , NULL
#endif
	    ) ? 0 : 1;
	free(rp);
ret:
#if defined(TRACE)
	fprintf(debug, "<-is_diff_pth: %d\n", v);
#endif
	return v;
}

static char *
read_link(char *path, off_t size)
{
	char *l = malloc(size + 1);

	if ((size = readlink(path, l, size)) == -1) {
		if (!ign_diff_errs && dialog(ign_txt, NULL,
		    "readlink \"%s\" failed: %s", path,
		    strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		free(l);
		return NULL;
	}

	l[size] = 0;
	return l;
}

/* Input: stat1, stat2, syspth[0], syspth[1]
 * Output:
 * -1  Error, don't make DB entry
 *  0  No diff
 *  1  Diff */

int
cmp_file(char *lpth, off_t lsiz, char *rpth, off_t rsiz,
    /* 1 (!0): force compare, no getch */
    unsigned md)
{
	int rv = 0, f1, f2;
	ssize_t l1, l2;

	if (!md) {
		if (dontcmp) {
			return 0;
		}

		if (getch() == '%') {
			dontcmp = TRUE;
			return 0;
		}
	}

	if (lsiz != rsiz)
		return 1;

	if (!lsiz)
		return 0;

	if ((f1 = open(lpth, O_RDONLY)) == -1) {
		if (!ign_diff_errs && dialog(ign_txt, NULL,
		    "open \"%s\": %s", lpth, strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		return -1;
	}

	if ((f2 = open(rpth, O_RDONLY)) == -1) {
		if (!ign_diff_errs && dialog(ign_txt, NULL,
		    "open \"%s\": %s", rpth, strerror(errno)) == 'i')
			ign_diff_errs = TRUE;

		rv = -1;
		goto close_f1;
	}

	while (1) {
		if ((l1 = read(f1, lbuf, sizeof lbuf)) == -1) {
			if (!ign_diff_errs && dialog(ign_txt, NULL,
			    "read \"%s\": %s", lpth,
			    strerror(errno)) == 'i')
				ign_diff_errs = TRUE;

			rv = -1;
			break;
		}

		if ((l2 = read(f2, rbuf, sizeof rbuf)) == -1) {
			if (!ign_diff_errs && dialog(ign_txt, NULL,
			    "read \"%s\": %s", rpth,
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
	p->fl = 0;
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

	if (*n == '.' && n[1] == '.' && !n[2])
		return pthcut(p, l);

	if (l + ln + 2 > PATHSIZ) {
		printerr(NULL, "Path buffer overflow");
		return l;
	}

	/* For archives push_state() sets l = 0 */
	/* ln = 0 for '#' in fmode */
	if (ln && l && p[l-1] != '/')
		p[l++] = '/';

	memcpy(p + l, n, ln + 1);
	return l + ln;
}

static size_t
pthcut(char *p, size_t l)
{
	if (l == 1)
		return l;

	while (l > 1 && p[--l] != '/');
	p[l] = 0;
	return l;
}

void
do_scan(void)
{
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	char *n;
#endif

#if defined(TRACE)
	fprintf(debug, "->do_scan lp(%s) rp(%s)\n", syspth[0], syspth[1]);
#endif
	while ((n = str_db_get_node(scan_db))) {
		str_db_del(&scan_db, n);
	}

	scan = 1;
	build_diff_db(bmode ? 1 : 3);
	scan = 0;
#if defined(TRACE)
	fprintf(debug, "<-do_scan\n");
#endif
}
