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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <stdint.h>
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
#include "misc.h"

struct scan_dir {
	char *s;
	short tree;
	struct scan_dir *next;
};

static struct filediff *alloc_diff(const char *const);
static void add_diff_dir(short);
static size_t pthadd(char *, size_t, const char *);
static size_t pthcut(char *, size_t);
static void ini_int(void);
/* Returns file descriptor or -1 on error. */
static int dlg_open_ro(const char *const pth);
static ssize_t dlg_read(int fd, void *buf, size_t count,
                        const char* const pth);

static struct filediff *diff;
static off_t lsiz1, lsiz2;
static char *last_path;
off_t tot_cmp_byte_count;
long tot_cmp_file_count;

short followlinks;

bool one_scan;
bool dotdot;
static bool stopscan;
static bool ign_diff_errs;

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

#if defined(TRACE) && 1
	fprintf(debug, "->build_diff_db tree(%d)%s\n",
	    tree, scan ? " scan" : "");
#endif
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

		if (printwd) {
			save_last_path(syspth[1]);
		}

        if (!cli_mode && (lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory \"%s\"", syspth[1]);
			lpt = lpt2;
		}
    } else if (wstat) {
		if (printwd && fmode && !scan) {
			save_last_path(syspth[0]);
		}

		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory \"%s\"", syspth[0]);
			lpt = lpt2;
		}
	}

    if (!cli_mode) {
        ini_int();
    }

#if defined(TRACE) && 1
	fprintf(debug, "  opendir lp(%s)%s\n", syspth[0], scan ? " scan" : "");
#endif
	if (!(d = opendir(syspth[0]))) {
        if (!ign_diff_errs &&
                dialog(ign_txt, NULL, "opendir \"%s\": %s",
                       syspth[0], strerror(errno))
                == 'i')
        {
			ign_diff_errs = TRUE;
        }

        retval |= 2;
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
            retval |= 2;
			goto dir_scan_end;
		}

#if defined(TRACE) && 1
		fprintf(debug, "  readdir L \"%s\"\n", ent->d_name);
#endif
		name = ent->d_name;

		if (*name == '.' && (!name[1] ||
            (!((bmode || fmode) && (dotdot && !scan)) &&
		     name[1] == '.' && !name[2]))) {
			continue;
		}

		if (!(bmode || fmode)) {
			str_db_add(&name_db, strdup(name)
#ifdef HAVE_LIBAVLBST
			    , 0, NULL
#endif
			    );
		}

		pthadd(syspth[0], pthlen[0], name);
#if defined(TRACE) && 1
		fprintf(debug,
		    "  found L \"%s\" \"%s\" strlen=%zu pthlen=%zu\n",
		    ent->d_name, syspth[0], strlen(syspth[0]), pthlen[0]);
#endif

		/* Get link length. Redundant code but necessary,
		 * unfortunately. */

		if (followlinks && !scan && lstat(syspth[0], &gstat[0]) != -1 &&
		    S_ISLNK(gstat[0].st_mode))
			lsiz1 = gstat[0].st_size;
		else
			lsiz1 = -1;

		file_err = FALSE;

		if (!followlinks || (i = stat(syspth[0], &gstat[0])) == -1)
			i = lstat(syspth[0], &gstat[0]);

		if (i == -1) {
			if (errno != ENOENT) {
				if (!ign_diff_errs && dialog(ign_txt, NULL,
				    LOCFMT "stat \"%s\": %s" LOCVAR,
				    syspth[0], strerror(errno)) == 'i') {

					ign_diff_errs = TRUE;
				}

				file_err = TRUE;
                retval |= 2;

                if (scan || cli_mode)
					continue;
			}

			gstat[0].st_mode = 0;
		}

		if (tree & 2) {
#if defined(TRACE) && 1
            fprintf(debug, "  %s:%d\n", __FILE__, __LINE__);
#endif
            pthcat(syspth[1], pthlen[1], name);
		} else {
			goto no_tree2;
		}

		if (followlinks && !scan && lstat(syspth[1], &gstat[1]) != -1 &&
		    S_ISLNK(gstat[1].st_mode)) {
			lsiz2 = gstat[1].st_size;
		} else {
			lsiz2 = -1;
		}

		if (!followlinks || (i = stat(syspth[1], &gstat[1])) == -1)
			i = lstat(syspth[1], &gstat[1]);

		if (i == -1) {
			if (errno != ENOENT) {
				if (!ign_diff_errs && dialog(ign_txt, NULL,
                    LOCFMT "stat \"%s\": %s"
				    LOCVAR, syspth[1], strerror(errno)) == 'i') {

					ign_diff_errs = TRUE;
				}

				file_err = TRUE;
                retval |= 2;

                if (scan || cli_mode)
					continue;
			}

no_tree2:
            if (qdiff) {
                if (nosingle)
                    continue;
				syspth[0][pthlen[0]] = 0;
                printf("Only in %s: %s\n", syspth[0], name);
                retval |= 1;
				continue;
			}

			gstat[1].st_mode = 0;
		}

        if (scan || cli_mode) {
			if (stopscan ||
                (!cli_mode && (bmode || fmode) && file_pattern && getch() == '%')) {
				stopscan = TRUE;
				closedir(d);
				goto dir_scan_end;
			}

            if ( S_ISDIR(gstat[0].st_mode) &&
                (S_ISDIR(gstat[1].st_mode) || bmode || fmode))
            {
				struct scan_dir *se;

                if (find_dir_name) { /* -x */
                    if (find_dir_name_only)
                        ++tot_cmp_file_count;
                    if (!regexec(&find_dir_name_regex, name, 0, NULL, 0)) {
#if defined(TRACE) && 1
                        fprintf(debug, "  dir_diff: find_dir: %s\n", name);
#endif
                        dir_diff = 1;

                        if (cli_mode) { /* -Sx */
                            syspth[0][pthlen[0]] = 0;
                            printf("%s/%s\n", syspth[0], name);
                            retval |= 1;
                        }
                    }
                }

				if (!scan) {
					/* Non-recursive qdiff */
					continue;
				}

				se = malloc(sizeof(struct scan_dir));
				se->s = strdup(name);
				se->tree = S_ISDIR(gstat[1].st_mode) ? 3 : 1;
				se->next = dirs;
				dirs = se;
				continue;
			}

            if (find_name) { /* -F ("find(1)") */
                if (!gq_pattern)
                    ++tot_cmp_file_count;
				if (regexec(&fn_re, name, 0, NULL, 0)) {
                    /* no match */
					continue;
				} else if (!gq_pattern) {
                    /* match and not -G */
#if defined(TRACE) && 1
                    fprintf(debug, "  dir_diff: find: %s\n", name);
#endif
                    dir_diff = 1;

                    if (cli_mode) {
                        syspth[0][pthlen[0]] = 0;
                        printf("%s/%s\n", syspth[0], name);
                        retval |= 1;
                    }
					continue;
				}
			}

            if (gq_pattern) { /* -G ("grep(1)") */
                if (!file_grep(name)) {
#if defined(TRACE) && 1
                    fprintf(debug, "  dir_diff: grep: %s\n", name);
#endif
                    dir_diff = 1;
                    if (cli_mode)
                        retval |= 1;
                }
				continue;
			}
            if (bmode || fmode)
                continue;

            if (S_ISREG(gstat[0].st_mode) &&
                S_ISREG(gstat[1].st_mode))
            {
                if (cmp_file(syspth[0], gstat[0].st_size,
                             syspth[1], gstat[1].st_size, 0) == 1) {
                    if (qdiff) {
                        printf("Files %s and %s differ\n",
                               syspth[0], syspth[1]);
                        retval |= 1;
                    } else {
#if defined(TRACE) && 1
                        fprintf(debug, "  dir_diff: file diff: %s\n", name);
#endif
                        dir_diff = 1;
                    }
                }

                continue;
            }

			if (S_ISLNK(gstat[0].st_mode) &&
                S_ISLNK(gstat[1].st_mode))
            {
                char *a = NULL;
                char *b = NULL;

                int v = cmp_symlink(&a, &b);
                retval |= v;

                if (v == 1) {
                    if (qdiff) {
                        printf("Symbolic links differ: %s -> %s, %s -> %s\n",
                               syspth[0], a, syspth[1], b);
                    } else {
#if defined(TRACE) && 1
                        fprintf(debug, "  dir_diff: link diff: %s\n", name);
#endif
                        dir_diff = 1;
                    }
                }

                free(b);
                free(a);
                continue;
			}

			if (real_diff)
				continue;

            if (!gstat[0].st_mode || !gstat[1].st_mode ||
                 gstat[0].st_mode !=  gstat[1].st_mode)
            {
                if (qdiff) {
                    printf("Different file type: %s and %s\n",
                           syspth[0], syspth[1]);
                    retval |= 1;
                } else {
#if defined(TRACE) && 1
                    fprintf(debug, "  dir_diff: type diff: %s\n", name);
#endif
                    dir_diff = 1;
                }

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

		if ((diff->type[0] = gstat[0].st_mode)) {
#if defined(TRACE) && 1
			fprintf(debug, "  found L 0%o \"%s\"\n",
			    gstat[0].st_mode, syspth[0]);
#endif
			diff->uid[0] = gstat[0].st_uid;
			diff->gid[0] = gstat[0].st_gid;
			diff->siz[0] = gstat[0].st_size;
            diff->mtim[0] = gstat[0].st_mtim;
			diff->rdev[0] = gstat[0].st_rdev;

			if (S_ISLNK(gstat[0].st_mode))
				lsiz1 = gstat[0].st_size;

			if (lsiz1 >= 0)
				diff->llink = read_link(syspth[0], lsiz1);
		}

		if ((diff->type[1] = gstat[1].st_mode)) {
#if defined(TRACE) && 1
			fprintf(debug, "  found R 0%o \"%s\"\n",
			    gstat[1].st_mode, syspth[1]);
#endif
			diff->uid[1] = gstat[1].st_uid;
			diff->gid[1] = gstat[1].st_gid;
			diff->siz[1] = gstat[1].st_size;
            diff->mtim[1] = gstat[1].st_mtim;
			diff->rdev[1] = gstat[1].st_rdev;

			if (S_ISLNK(gstat[1].st_mode))
				lsiz2 = gstat[1].st_size;

			if (lsiz2 >= 0)
				diff->rlink = read_link(syspth[1], lsiz2);
		}

		if ((diff->type[0] & S_IFMT) != (diff->type[1] & S_IFMT)) {

			diff_db_add(diff, 0);
			continue;

		} else if (gstat[0].st_ino == gstat[1].st_ino &&
		           gstat[0].st_dev == gstat[1].st_dev) {

			diff->diff = '=';
			diff_db_add(diff, 0);
			continue;

		} else if (S_ISREG(gstat[0].st_mode)) {

			switch (cmp_file(syspth[0], gstat[0].st_size, syspth[1],
			    gstat[1].st_size, 0)) {
			case 1:
				diff->diff = '!';
				/* fall through */
			case 0:
db_add_file:
				diff_db_add(diff, 0);
				continue;
            default: /* 2 or 3 */
                diff->diff = '-';
                goto db_add_file;
            }

		} else if (S_ISDIR(gstat[0].st_mode)) {

			diff_db_add(diff, 0);
			continue;

		} else if (S_ISLNK(gstat[0].st_mode)) {

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
    } /* readdir() loop */

	closedir(d);
	syspth[0][pthlen[0]] = 0;

	/* Now already done here for diff mode to use syspth[0] instead of syspth[1].
	 * May be useless. */
    if (scan && dir_diff && !cli_mode) {
        add_diff_dir(0); /* left tree */
		dir_diff = 0;
	}

	if (tree & 2)
		syspth[1][pthlen[1]] = 0;

right_tree:
	if (!(tree & 2) || bmode)
		goto build_list;

	if (scan && (real_diff || dir_diff))
		goto dir_scan_end;

    if (!cli_mode) {
		if (printwd && fmode && !scan) {
			save_last_path(syspth[1]);
		}

		if ((lpt2 = time(NULL)) - lpt) {
			printerr(NULL, "Reading directory \"%s\"", syspth[1]);
			lpt = lpt2;
		}
	}

	ini_int();

#if defined(TRACE) && 1
	fprintf(debug, "  opendir rp(%s)%s\n", syspth[1], scan ? " scan" : "");
#endif
    if (!(d = opendir(syspth[1]))) {
        if (!ign_diff_errs &&
                dialog(ign_txt, NULL, "opendir \"%s\": %s",
                       syspth[1], strerror(errno))
                == 'i')
        {
            ign_diff_errs = TRUE;
        }

        retval |= 2;
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
            retval |= 2;
			goto dir_scan_end;
		}

#if defined(TRACE) && 1
		fprintf(debug, "  readdir R \"%s\"\n", ent->d_name);
#endif
		name = ent->d_name;

		if (*name == '.' && (!name[1] ||
            (!((bmode || fmode) && (dotdot && !scan)) &&
		     name[1] == '.' && !name[2]))) {
			continue;
		}

		if (!(bmode || fmode) && (tree & 1) && !str_db_srch(&name_db,
		    name, NULL)) {
			continue;
		}

        if (qdiff) {
            if (nosingle)
                continue;
			syspth[1][pthlen[1]] = 0;
			printf("Only in %s: %s\n", syspth[1], name);
            retval |= 1;
			continue;
		} else if (scan && !file_pattern) {
#if defined(TRACE) && 1
            fprintf(debug, "  dir_diff: One sided: %s\n", name);
#endif
            dir_diff = 1;
			break;
		}

		pthadd(syspth[1], pthlen[1], name);
#if defined(TRACE) && 1
		fprintf(debug,
		    "  found R \"%s\" \"%s\" strlen=%zu pthlen=%zu\n",
		    ent->d_name, syspth[1], strlen(syspth[1]), pthlen[1]);
#endif

		if (followlinks && !scan && lstat(syspth[1], &gstat[1]) != -1 &&
		    S_ISLNK(gstat[1].st_mode)) {
			lsiz2 = gstat[1].st_size;
		} else {
			lsiz2 = -1;
		}

		file_err = FALSE;

		if (!followlinks || (i = stat(syspth[1], &gstat[1])) == -1) {
			i = lstat(syspth[1], &gstat[1]);
		}

		if (i == -1) {
			if (errno != ENOENT) {
				if (!ign_diff_errs && dialog(ign_txt, NULL,
				    LOCFMT "stat \"%s\" failed: %s"
				    LOCVAR, syspth[1], strerror(errno)) == 'i') {

					ign_diff_errs = TRUE;
				}

				file_err = TRUE;
                retval |= 2;
			}

			gstat[1].st_mode = 0;
		}

		if (scan) {
			if (stopscan ||
			    ((bmode || fmode) && file_pattern && getch() == '%')) {
				stopscan = TRUE;
				closedir(d);
				goto dir_scan_end;
			}

			if (S_ISDIR(gstat[1].st_mode)) {
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
                    !gq_pattern)
                {
#if defined(TRACE) && 1
                    fprintf(debug, "  dir_diff: find[1]: %s\n", name);
#endif
                    dir_diff = 1;
					continue;
				}
			}
		}

		diff = alloc_diff(name);
		diff->type[0] = 0;
		diff->type[1] = gstat[1].st_mode;

		if (file_err)
			diff->diff = '-';
		else {
#if defined(TRACE) && 1
			fprintf(debug, "  found R 0%o \"%s\"\n",
			    gstat[1].st_mode, syspth[1]);
#endif
			diff->uid[1] = gstat[1].st_uid;
			diff->gid[1] = gstat[1].st_gid;
			diff->siz[1] = gstat[1].st_size;
            diff->mtim[1] = gstat[1].st_mtim;
			diff->rdev[1] = gstat[1].st_rdev;

			if (S_ISLNK(gstat[1].st_mode))
				lsiz2 = gstat[1].st_size;

			if (lsiz2 >= 0)
				diff->rlink = read_link(syspth[1], lsiz2);
		}

		if (scan) {
			if (gq_pattern && !gq_proc(diff)) {
#if defined(TRACE) && 1
                fprintf(debug, "  dir_diff: grep[1]: %s\n", name);
#endif
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
	free_strs(&name_db);

	if (!scan) {
		goto exit;
	}

    if (dir_diff && !cli_mode) {
        add_diff_dir(1); /* right tree */
	}

	while (dirs) {
		size_t l1, l2 = 0 /* silence warning */;
		struct scan_dir *p;

		l1 = pthlen[0];
		l2 = pthlen[1];
        retval |= scan_subdir(dirs->s, NULL, dirs->tree);
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
#if defined(TRACE) && 1
    fprintf(debug, "<-build_diff_db%s retval=%d\n", scan ? " scan" : "", retval);
#endif
	return retval;
}

int file_grep(const char *const name)
{
    int return_value = 1;
    ++tot_cmp_file_count;
    diff = alloc_diff(name);
    diff->type[0] = gstat[0].st_mode;
    diff->type[1] = gstat[1].st_mode;
    diff->siz[0]  = gstat[0].st_size;
    diff->siz[1]  = gstat[1].st_size;

    if (cli_mode && verbose) {
        /* -pSG -> print lines */
        return_value = gq_proc_lines(diff);
    } else if (!(return_value = gq_proc(diff)) && cli_mode) { /* -SG */
        syspth[0][pthlen[0]] = 0;
        printf("%s/%s\n", syspth[0], name);
    }
    free_diff(diff);
    return return_value;
}

static void
ini_int(void)
{
	const char *s = "Type '%' to disable file compare";
	const char *s2 = "Type '%' to stop find command";

	nodelay(stdscr, TRUE); /* compare() waits for key */

	if (bmode || fmode) {
		if (gq_pattern) {
			/* keep msg */
        } else if (recursive && (find_name || find_dir_name)) {
			s = s2;
		} else {
			return;
		}
	}

	mvwaddstr(wstat, 0, 0, s);
	wrefresh(wstat);
}

int
scan_subdir(const char *name, const char *rnam, int tree)
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
		if (name) {
			pthlen[0] = pthcat(syspth[0], pthlen[0], name);
		} else {
			syspth[0][pthlen[0]] = 0; /* -> syspth[0] = "." */
		}
	}

	if (tree & 2) {
		if (rnam) {
			pthlen[1] = pthcat(syspth[1], pthlen[1], rnam);
		} else {
			syspth[1][pthlen[1]] = 0; /* fmode_cp_pth() */
		}
	}

	i = build_diff_db(tree);
#if defined(TRACE)
	fprintf(debug, "<-scan_subdir: %d\n", i);
#endif
	return i;
}

/* For performance reasons and since the function is called
 * conditionally anyway, `printwd` is checked before the function
 * call and not inside the function. */

void
save_last_path(char *path)
{
#if defined(TRACE) && 0
	fprintf(debug, "<>save_last_path(%s)\n", path);
#endif

	if (last_path) {
		free(last_path);
	}

	last_path = strdup(path);
}

void
wr_last_path(void)
{
	int f;
	size_t l;

	if (!last_path) {
		return;
	}

	if ((f = open(printwd, O_WRONLY|O_CREAT|O_TRUNC, 0777)) == -1) {
		fprintf(stderr, "open \"%s\": %s\n", printwd,
		    strerror(errno));
		return;
	}

#if defined(TRACE) && 0
	fprintf(debug, "<>wr_last_path \"%s\" to \"%s\" (fh %d)\n",
	    last_path, printwd, f);
#endif

	l = strlen(last_path);
	errno = 0;

	if (write(f, last_path, l) != (ssize_t)l) {
		fprintf(stderr, "write \"%s\": %s\n", printwd,
		    errno ? strerror(errno) : "Unkown error");
	}

	if (close(f) == -1) {
		fprintf(stderr, "close \"%s\": %s\n", printwd,
		    strerror(errno));
	}
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
#if defined(TRACE) && 1
    fprintf(debug, "->add_diff_dir(side=%s path=%s) "
                   "syspth[0]=\"%s\" syspth[1]=\"%s\"\n",
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

#if defined(TRACE) && 1
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
#if defined(TRACE) && 1
	fprintf(debug, "<-add_diff_dir\n");
#endif
	return;
}

/*
 * Implementaion detail: Called from ui.c only!
 */

int
is_diff_dir(struct filediff *f)
{
    char *bp = NULL, *pth = NULL;
    size_t l = (size_t)-1;
	int v = 0;

	/* E.g. for file stat called independend from 'recursive' */
	/* or called in bmode with option -r (for later dir diffs) */
    if (!recursive || ((bmode || fmode) && !file_pattern)
            /* Don't apply for ".."! */
            || (dotdot && f->name[0] == '.' && f->name[1] == '.' && !f->name[2]))
    {
		goto ret0; /* No debug print */
	}
#if defined(TRACE) && 1
	fprintf(debug, "->is_diff_dir(%s)\n", f->name);
#endif
	if (bmode) {
		pth = syspth[1];
		l = strlen(pth);
		bp = malloc(l + strlen(f->name) + 2);
		memcpy(bp, pth, l);
		pth = bp;
#if defined(TRACE) && 1
        fprintf(debug, "  %s:%d\n", __FILE__, __LINE__);
#endif
        pthcat(pth, l, f->name);
		v = is_diff_pth(pth, 0);
		free(bp);
	} else {
		if (f->type[0]) {
			pth = syspth[0];
			l = pthlen[0];
#if defined(TRACE) && 1
            fprintf(debug, "  %s:%d\n", __FILE__, __LINE__);
#endif
            pthcat(pth, l, f->name);
			v = is_diff_pth(pth, 0);
		}

		if (!v && f->type[1]) {
			pth = syspth[1];
			l = pthlen[1];
#if defined(TRACE) && 1
            fprintf(debug, "  %s:%d\n", __FILE__, __LINE__);
#endif
            pthcat(pth, l, f->name);
			v = is_diff_pth(pth, 0);
		}

        if (pth && l != (size_t)-1)
            pth[l] = 0;
#if defined(DEBUG)
        else {
            fprintf(debug, LOCFMT "\n" LOCVAR);
            exit(EXIT_STATUS_ERROR);
        }
#endif
	}

#if defined(TRACE) && 1
	fprintf(debug, "<-is_diff_dir: %d\n", v);
#endif
ret0:
	return v;
}

int
is_diff_pth(const char *p,
    /* 1: Remove path */
    unsigned m)
{
	char *rp = NULL;
	int v = 0;
#ifdef HAVE_LIBAVLBST
	struct bst_node *n;
#else
	char *n;
#endif

#if defined(TRACE) && 0
	fprintf(debug, "->is_diff_pth(%s,%u)\n", p, m);
#endif
	/* Here since both path and name can be symlink */
	if (!(rp = realpath(p, NULL))) {
		printerr(strerror(errno), LOCFMT "realpath \"%s\""
		    LOCVAR, p);
		goto ret;
	}
#if defined(TRACE) && 0
	fprintf(debug, "  realpath: \"%s\"\n", p);
#endif
	v = str_db_srch(&scan_db, rp, &n) ? 0 : 1;

	if (m && v) {
#if defined(TRACE) && 0
		fprintf(debug, "  remove \"%s\"\n",
#ifdef HAVE_LIBAVLBST
		    rp
#else
		    n
#endif
		    );
#endif
		str_db_del(&scan_db, n);
	}

	free(rp);
ret:
#if defined(TRACE) && 0
	fprintf(debug, "<-is_diff_pth: %d\n", v);
#endif
	return v;
}

char *
read_link(char *path, off_t size)
{
    char *l = malloc(size + 1);

    if (!l) {
        if (printerr(strerror(errno),
                     LOCFMT "malloc(%zu)" LOCVAR, size + 1))
            fputs(oom_msg, stderr);
        return NULL;
    }

    if ((size = readlink(path, l, size)) == -1) {
        if (!ign_diff_errs &&
                dialog(ign_txt, NULL, "readlink \"%s\": %s",
                       path, strerror(errno))
                == 'i')
        {
            ign_diff_errs = TRUE;
        }

        free(l);
        return NULL;
    }

    l[size] = 0;
    return l;
}

int cmp_symlink(char **a, char **b) {
    int return_value = 0;
#if defined(TRACE) && (defined(TEST) || 1)
    fprintf(debug, "<>cmp_symlink()\n");
#endif

    if (gstat[0].st_size != gstat[1].st_size) {
        return_value |= 1;

    } else if (gstat[0].st_size) {
        if (!(*a = read_link(syspth[0], gstat[0].st_size))) {
            return_value |= 2;

        } else if (!(*b = read_link(syspth[1], gstat[1].st_size))) {
            return_value |= 2;
        } else {
            if (strcmp(*a, *b)) {
                return_value |= 1;
            } else {
                /* Count successfully compared links only. */
                ++tot_cmp_file_count;
                tot_cmp_byte_count += gstat[0].st_size;

                if (qdiff && verbose)
                    printf("Equal symbolic links: \"%s\" and \"%s\"\n",
                           syspth[0], syspth[1]);
            }
        }
    }

    return return_value;
}

int cmp_file(
	const char *const lpth,
	const off_t lsiz,
	const char *const rpth,
	const off_t rsiz,
    /* !0: force compare, no getch */
    const unsigned md)
{
	int rv = 0, f1 = -1, f2 = -1;
	ssize_t l1 = -1, l2 = -1;

#if defined(TRACE) && (defined(TEST) || 1)
	fprintf(debug, "->cmp_file(name1=%s size1=%ju name2=%s size2=%ju mode=%u)\n",
		lpth, (intmax_t)lsiz, rpth, (intmax_t)rsiz, md);
#endif

    if (lsiz != rsiz) {
        rv |= 1;
		goto ret;
	}

	if (!lsiz) {
		goto ret;
	}

	if (!md) {
		if (dontcmp) {
			goto ret;
		}

		if (getch() == '%') {
			dontcmp = TRUE;
			goto ret;
		}
	}

    if ((f1 = dlg_open_ro(lpth)) == -1) {
        rv |= 2;
		goto ret;
	}

    if ((f2 = dlg_open_ro(rpth)) == -1) {
        rv |= 2;
		goto close_f1;
	}

	while (1) {
        if ((l1 = dlg_read(f1, lbuf, sizeof lbuf, lpth)) == -1) {
            rv |= 2;
			break;
		}

        if ((l2 = dlg_read(f2, rbuf, sizeof rbuf, rpth)) == -1) {
            rv |= 2;
			break;
		}

		if (l1 != l2) {
            rv |= 1;
			break;
		}

		if (!l1)
			break;

		if (memcmp(lbuf, rbuf, l1)) {
            rv |= 1;
			break;
		}
        /* Count successfully compared bytes only. */
        if (qdiff)
            tot_cmp_byte_count += l1;
        if (l1 < (ssize_t)(sizeof lbuf))
            break;
    }

    /* Count really and successfully compared files only,
     * not zero size files, nor different files. */
    if (qdiff) {
        ++tot_cmp_file_count;

        if (verbose)
            printf("Equal files: \"%s\" and \"%s\"\n",
                   syspth[0], syspth[1]);
    }

    close(f2);
close_f1:
	close(f1);
ret:
#if defined(TRACE) && (defined(TEST) || 1)
	fprintf(debug, "<-cmp_file(): %d\n", rv);
#endif
	return rv;
}

static int dlg_open_ro(const char *const pth) {
    int fd;

    if ((fd = open(pth, O_RDONLY)) == -1) {
        if (!ign_diff_errs &&
                dialog(ign_txt, NULL, "open \"%s\": %s",
                       pth, strerror(errno))
                == 'i')
        {
            ign_diff_errs = TRUE;
        }
    }

    return fd;
}

static ssize_t dlg_read(int fd, void *buf, size_t count,
                        const char* const pth)
{
    ssize_t l;

    if ((l = read(fd, buf, count)) == -1) {
        if (!ign_diff_errs &&
                dialog(ign_txt, NULL, "read \"%s\": %s",
                       pth, strerror(errno))
                == 'i')
        {
            ign_diff_errs = TRUE;
        }
    }

    return l;
}

static struct filediff *
alloc_diff(const char *const name)
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
pthcat(char *p, size_t l, const char *n)
{
    size_t ret_val = 0;
#if defined(TRACE)
	{
		char *s = malloc(l + 1);
		memcpy(s, p, l);
		s[l] = 0;
        fprintf(debug, "->pthcat(\"%s\" param_len=%zu true_len=%zu + \"%s\")\n", s, l, strlen(s), n);
		free(s);
	}
#endif

    if (n[0] == '.') {
        if (!n[1]) {
            ret_val = l;
            goto ret;
        } else if (n[1] == '.' && !n[2]) {
            ret_val = pthcut(p, l);
            goto ret;
        }
    }

    ret_val = pthadd(p, l, n);
ret:
#if defined(TRACE)
    {
        char *s = malloc(ret_val + 1);
        memcpy(s, p, ret_val);
        s[ret_val] = 0;
        fprintf(debug, "<-pthcat: \"%s\" return_len=%zu true_len=%zu\n", s, ret_val, strlen(s));
        free(s);
    }
#endif
    return ret_val;
}

static size_t
pthadd(char *p, size_t l, const char *n)
{
    size_t rv = 0;
    size_t ln = strlen(n);

#if defined(TRACE)
    {
        char *s = malloc(l + 1);
        memcpy(s, p, l);
        s[l] = 0;
        fprintf(debug, "->pthadd(\"%s\" param_len=%zu true_len=%zu + \"%s\")\n", s, l, strlen(s), n);
        free(s);
    }
#endif

	if (l + ln + 2 > PATHSIZ) {
		printerr(NULL, "Path buffer overflow");
        rv = l;
        goto ret;
	}

	/* For archives push_state() sets l = 0 */
	/* ln = 0 for '#' in fmode */
	if (ln && l && p[l-1] != '/')
		p[l++] = '/';

	memcpy(p + l, n, ln + 1);
    rv = l + ln;
ret:
#if defined(TRACE)
    {
        char *s = malloc(rv + 1);
        memcpy(s, p, rv);
        s[rv] = 0;
        fprintf(debug, "<-pthadd: \"%s\" return_len=%zu true_len=%zu\n", s, rv, strlen(s));
        free(s);
    }
#endif
    return rv;
}

static size_t
pthcut(char *p, size_t l)
{
    size_t rv = 0;

#if defined(TRACE)
	{
		char *s = malloc(l + 1);
		memcpy(s, p, l);
		s[l] = 0;
		fprintf(debug, "->pthcut(%s, %zu)\n", s, l);
		free(s);
	}
#endif
    if (l == 1) {
        rv = l;
        goto ret;
    }

	while (l > 1 && p[--l] != '/');
	p[l] = 0;
    rv = l;
ret:
#if defined(TRACE)
    {
        char *s = malloc(rv + 1);
        memcpy(s, p, rv);
        s[rv] = 0;
        fprintf(debug, "<-pthcut: %s, %zu\n", s, rv);
        free(s);
    }
#endif
    return rv;
}

int
do_scan(void)
{
    int return_value = 0;

#if defined(TRACE) && 1
	fprintf(debug, "->do_scan lp(%s) rp(%s)\n", syspth[0], syspth[1]);
#endif
	scan = 1;
    return_value |= build_diff_db(bmode ? 1 : 3);
	stopscan = FALSE;
	scan = 0;
#if defined(TRACE) && 1
    fprintf(debug, "<-do_scan: %d\n", return_value);
#endif
    return return_value;
}
