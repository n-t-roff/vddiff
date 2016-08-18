#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include "main.h"
#include "ui.h"
#include "diff.h"
#include "db.h"

static int cmp_link(void);
static int cmp_file(void);
static struct filediff *alloc_diff(char *);

static int (*xstat)(const char *, struct stat *) = lstat;
static struct filediff *diff;

int
build_diff_db(void)
{
	DIR *d;
	struct dirent *ent;
	char *name;
	size_t l;

	if (!(d = opendir(lpath))) {
		printerr("opendir(\"%s\") failed: %s", lpath,
		    strerror(errno));
		return -1;
	}

	while (1) {
		lpath[llen] = 0;
		rpath[rlen] = 0;

		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;
			printerr("readdir(\"%s\") failed: %s", lpath,
			    strerror(errno));
			closedir(d);
			return -1;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		lpath[llen++] = '/';
		rpath[rlen++] = '/';
		l = strlen(name);
		memcpy(lpath + llen--, name, l+1);
		memcpy(rpath + rlen--, name, l+1);

		if (xstat(lpath, &stat1) == -1) {
			if (errno != ENOENT) {
				printerr("stat(\"%s\") failed: %s", lpath,
				    strerror(errno));
				continue;
			}

			stat1.st_mode = 0;
		}

		if (xstat(rpath, &stat2) == -1) {
			if (errno != ENOENT) {
				printerr("lstat(\"%s\") failed: %s", rpath,
				    strerror(errno));
				continue;
			}

			if (!stat1.st_mode)
				continue; /* ignore two dead links */
			stat2.st_mode = 0;
		}

		diff = alloc_diff(name);
		diff->ltype = S_IFMT & stat1.st_mode;
		diff->rtype = S_IFMT & stat2.st_mode;

		if ((!diff->ltype && !diff->rtype) ||
		    diff->ltype != diff->rtype) {

			db_add(diff);
			continue;

		} else if (stat1.st_ino == stat2.st_ino) {

			diff->diff = SAME_INO;
			db_add(diff);
			continue;

		} else if (S_ISREG(stat1.st_mode)) {

			if (!cmp_file())
				continue;

		} else if (S_ISDIR(stat1.st_mode)) {

			db_add(diff);
			continue;

		} else if (S_ISLNK(stat1.st_mode)) {

			if (!cmp_link())
				continue;

		} else {
			db_add(diff);
			continue;
		}

		free(diff);
	}

	closedir(d);
	lpath[llen] = 0;
	rpath[rlen] = 0;

	if (!(d = opendir(rpath))) {
		printerr("opendir(\"%s\") failed: %s", lpath,
		    strerror(errno));
		return -1;
	}

	while (1) {
		errno = 0;
		if (!(ent = readdir(d))) {
			if (!errno)
				break;
			printerr("readdir(\"%s\") failed: %s", lpath,
			    strerror(errno));
			closedir(d);
			return -1;
		}

		name = ent->d_name;
		if (*name == '.' && (!name[1] || (name[1] == '.' &&
		    !name[2])))
			continue;

		if (!db_srch(name))
			continue;

		rpath[rlen++] = '/';
		l = strlen(name);
		memcpy(rpath + rlen--, name, l+1);

		if (xstat(rpath, &stat2) == -1) {
			if (errno != ENOENT) {
				printerr("lstat(\"%s\") failed: %s", rpath,
				    strerror(errno));
				continue;
			}
		}

		diff = alloc_diff(name);
		diff->ltype = 0;
		diff->rtype = S_IFMT & stat2.st_mode;
		db_add(diff);
	}

	closedir(d);

	return 0;
}

static int
cmp_link(void)
{
	ssize_t l1, l2;

	if ((l1 = readlink(lpath, lbuf, sizeof(lbuf) - 1)) == -1) {
		printerr("readlink(\"%s\") failed: %s", lpath,
		    strerror(errno));
		return -1;
	}

	if ((l2 = readlink(rpath, rbuf, sizeof(rbuf) - 1)) == -1) {
		printerr("readlink(\"%s\") failed: %s", rpath,
		    strerror(errno));
		return -1;
	}

	lbuf[l1] = 0;
	rbuf[l2] = 0;
	diff->llink = strdup(lbuf);
	diff->rlink = strdup(rbuf);

	if (l1 != l2 || memcmp(lbuf, rbuf, l1))
		diff->diff = DIFF;

	db_add(diff);
	diff = NULL;
	return 0;
}

static int
cmp_file(void)
{
	int rv = 0, f1, f2;
	ssize_t l1, l2;

	if (stat1.st_size != stat2.st_size) {
		diff->diff = DIFF;
		goto ret;
	}

	if (!stat1.st_size)
		goto ret;

	if ((f1 = open(lpath, O_RDONLY)) == -1) {
		printerr("open(\"%s\") failed: %s", lpath, strerror(errno));
		return -1;
	}

	if ((f2 = open(rpath, O_RDONLY)) == -1) {
		printerr("open(\"%s\") failed: %s", rpath, strerror(errno));
		rv = -1;
		goto close_f1;
	}

	while (1) {
		if ((l1 = read(f1, lbuf, sizeof lbuf)) == -1) {
			printerr("read(\"%s\") failed: %s", lpath,
			    strerror(errno));
			rv = -1;
			break;
		}

		if ((l2 = read(f2, rbuf, sizeof rbuf)) == -1) {
			printerr("read(\"%s\") failed: %s", rpath,
			    strerror(errno));
			rv = -1;
			break;
		}

		if (l1 != l2) {
			diff->diff = DIFF;
			break;
		}

		if (!l1)
			break;

		if (memcpy(lbuf, rbuf, l1)) {
			diff->diff = DIFF;
			break;
		}

		if (l1 < (ssize_t)(sizeof lbuf))
			break;
	}

	close(f2);
close_f1:
	close(f1);
ret:
	if (!rv) {
		db_add(diff);
		diff = NULL;
	}
	return rv;
}

static struct filediff *
alloc_diff(char *name)
{
	struct filediff *p = malloc(sizeof(struct filediff));
	p->name  = strdup(name);
	p->llink = NULL; /* to simply use free() later */
	p->rlink = NULL;
	p->diff  = NO_DIFF;
	return p;
}

void
follow(int f)
{
	xstat = f ? stat : lstat;
}
