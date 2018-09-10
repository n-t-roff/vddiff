#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "compat.h"
#include "tc.h"
#include "ui.h"
#include "exec.h"
#include "uzp.h"
#include "misc.h"
#include "main.h"
#include "diff.h"
#include "db.h"
#include "fs.h"

const char oom_msg[] = "Out of memory\n";

int
getuwidth(unsigned long u)
{
	int w;

	if (u < 10) w = 1;
	else if (u < 100) w = 2;
	else if (u < 1000) w = 3;
	else if (u < 10000) w = 4;
	else if (u < 100000) w = 5;
	else w = 6;

	return w;
}

char *
msgrealpath(const char *p)
{
	char *s;

	if (!(s = realpath(p, NULL))) {
		printerr(strerror(errno), LOCFMT "realpath \"%s\"" LOCVAR, p);
	}

	return s;
}

bool
str_eq_dotdot(const char *s) {
	if (s && *s == '.' && s[1] == '.' && !s[2]) {
		return TRUE;
	}

	return FALSE;
}

const char *buf_basename(char *const buf, size_t *bufsiz) {
    size_t l = 0;
    char *base = NULL;

#if defined(TRACE)
    fprintf(debug, "->buf_basename(\"%s\" len=%zu)\n", buf, bufsiz ? *bufsiz : 0);
#endif

    /* Check for valid input */

    if (!buf || !*buf || !bufsiz || !(l = *bufsiz)) {
        return NULL;
    }

    /* remove trailing '/' (but not at *buf) */

    while (l > 1 && buf[l - 1] == '/') {
        buf[--l] = 0;
    }

    /* case buf == "/": Keep buf and return "/" */

    if (l == 1 && *buf == '/') {
        base = strdup(buf);
        goto ret;
    }

    /* search last '/' */

    while (l && buf[--l] != '/') {
    }

    if (buf[l] == '/') {
        base = strdup(buf + l + 1);

        if (l) {
            buf[l] = 0; /* cut at '/' */
        } else { /* file in root dir */
            buf[++l] = 0; /* cut after '/' */
        }
    } else { /* buf did not contain '/' */
        base = strdup(buf + l);
        buf[l++] = '.';
        buf[l] = 0;
    }

ret:
    *bufsiz = l;
#if defined(TRACE)
    fprintf(debug, "<-buf_basename dir=\"%s\" file=\"%s\"\n", buf, base);
#endif
    return base;
}

/* DEBUG CODE */

int do_cli_rm(int argc, char **argv) {
    int ret_val = 0;

    while (!ret_val &&
           !fs_none && /* Delete none */
           !fs_abort && /* Abort */
           argc)
    {
        get_arg(argv[0], 0);
        pth2 = syspth[0];
#if defined(TRACE)
        fprintf(debug, "<>do_cli_rm(\"%s\")\n", syspth[0]);
#endif
        if (fs_rm(0, /* tree */
                  "delete",
                  NULL, /* nam */
                  0, /* u */
                  1, /* n */
                  2) /* md */
                & ~1) /* delete "cancel" flag */
        {
            ret_val |= 1;
        }

        ++argv;
        --argc;
    }

    return ret_val;
}

int do_cli_cp(int argc, char **argv, const unsigned opt) {
    struct filediff f[2];
    struct filediff *pf[] = { &f[0], &f[1] };
    int ret_val = 0;
    unsigned md = 1|4; /* !rebuild|force */
    const char *const target = argv[argc - 1];

    if (opt & 1) {
        md |= 16; /* move instead of copy (remove source) */
    }
    get_arg(target, 1); /* set gstat[1] */

    if (argc > 2 && !S_ISDIR(gstat[1].st_mode)) {
        printerr(NULL, LOCFMT "Target \"%s\" is not a directory" LOCVAR, target);
        ret_val |= 1;
        goto ret;
    }
    while (!ret_val && argc >= 2) {
        get_arg(argv[0], 0);
        get_arg(target, 1); /* set gstat[1] on each iteration */
        f[0].name = NULL;
        f[1].name = NULL;
#if defined(TRACE)
        fprintf(debug, "<>do_cli_cp(\"%s\" -> \"%s\")\n", syspth[0], syspth[1]);
#endif
        if (!(f[0].name = buf_basename(syspth[0], &pthlen[0]))) {
            ret_val |= 1;
            goto abort;
        }

        if (!gstat[1].st_mode || /* target is to be created */
                (S_ISREG(gstat[0].st_mode) &&
                 S_ISREG(gstat[1].st_mode))) /* src and tgt are files */
        {
            if (!(f[1].name = buf_basename(syspth[1], &pthlen[1])))
            {
                ret_val |= 1;
                goto abort;
            }

            md |= 128;
        }

        db_list[0] = &pf[0];
        db_list[1] = &pf[1];
        db_num[0] = 1;
        db_num[1] = 1;
        right_col = 0;
        fmode = TRUE; /* for fs_rm() */

        if (fs_cp(2, /* to right side */
                  0, /* u */
                  1, /* n */
                  md,
                  NULL)) /* &sto */
        {
            ret_val |= 1;
        }

abort:
        free(f[1].name);
        free(f[0].name);
        ++argv;
        --argc;
    }
ret:
    return ret_val;
}

int cmp_timespec(const struct timespec a, const struct timespec b)
{
    if (a.tv_sec < b.tv_sec)
        return -1;
    else if (a.tv_sec > b.tv_sec)
        return 1;
    else if (a.tv_nsec < b.tv_nsec)
        return -1;
    else if (a.tv_nsec > b.tv_nsec)
        return 1;
    else
        return 0;
}

void get_uid_name(const uid_t uid, char *const buf, const size_t buf_size)
{
    const struct passwd *const pw = getpwuid(uid);
    if (pw)
        memcpy(buf, pw->pw_name, strlen(pw->pw_name) + 1);
    else
        snprintf(buf, buf_size, "%u", uid);
}

void get_gid_name(const gid_t gid, char *const buf, const size_t buf_size)
{
    const struct group *const gr = getgrgid(gid);
    if (gr)
        memcpy(buf, gr->gr_name, strlen(gr->gr_name) + 1);
    else
        snprintf(buf, buf_size, "%u", gid);
}
