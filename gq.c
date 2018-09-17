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

#define GQBUFSIZ (1024 * 1024)
#define GQBSEEK  (1024 * 4)

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include "compat.h"
#include "diff.h"
#include "main.h"
#include "ui.h"
#include "ui2.h"
#include "gq.h"

struct gq_re {
	regex_t re;
	struct gq_re *next;
};

regex_t fn_re;
regex_t find_dir_name_regex;
static char *gq_buf;
static struct gq_re *gq_re;

bool file_pattern; /* TRUE for -F or -G */
bool find_name;
bool find_dir_name;
bool gq_pattern;
static bool ign_errs;
/*
 * Output:
 *   0: match
 *   1: no match
 */
static int gq_match_buf(const char *const buf);

int
fn_init(char *s)
{
	int fl;

	if (find_name) {
		fn_free();
	}

	fl = REG_NOSUB;

	if (magic) {
		fl |= REG_EXTENDED;
	}

	if (!noic) {
		fl |= REG_ICASE;
	}

	if (regcomp(&fn_re, s, fl)) {
		printerr(strerror(errno), "regcomp \"%s\"", s);
		return -1;
	}

	file_pattern = TRUE;
	find_name = TRUE;
	return 0;
}

int find_dir_name_init(const char *const s)
{
    int fl = REG_NOSUB;
#if defined(TRACE) && 1
    fprintf(debug, "<>find_dir_name_init(%s)\n", s);
#endif
    if (find_dir_name)
        find_dir_name_free();
    if (magic)
        fl |= REG_EXTENDED;
    if (!noic)
        fl |= REG_ICASE;
    if (regcomp(&find_dir_name_regex, s, fl)) {
        printerr(strerror(errno), "regcomp \"%s\"", s);
        return -1;
    }
    file_pattern = TRUE;
    find_dir_name = TRUE;
    return 0;
}

int
gq_init(char *s)
{
	int fl;
	struct gq_re *re;

	fl = REG_NOSUB | REG_NEWLINE;

	if (magic)
		fl |= REG_EXTENDED;
	if (!noic)
		fl |= REG_ICASE;

	re = malloc(sizeof(struct gq_re));
	re->next = gq_re;
	gq_re = re;

	if (regcomp(&re->re, s, fl)) {
		printerr(strerror(errno), "regcomp \"%s\"", s);
		return -1;
	}

	if (!gq_buf) {
		gq_buf = malloc(GQBUFSIZ + 1);
	}

	file_pattern = TRUE;
	gq_pattern = TRUE;
	return 0;
}

int
fn_free(void)
{
	if (!find_name) {
		return 1;
	}

	regfree(&fn_re);
	find_name = FALSE;

    if (!find_dir_name && !gq_pattern) {
		file_pattern = FALSE;
	}

	return 0;
}

int find_dir_name_free(void)
{
    if (!find_dir_name)
        return 1;
    regfree(&find_dir_name_regex);
    find_dir_name = FALSE;
    if (!find_name && !gq_pattern)
        file_pattern = FALSE;
    return 0;
}

/* Remove all patterns from list. */

int
gq_free(void)
{
	struct gq_re *p;

	if (!gq_pattern) {
		return 1;
	}

	while (gq_re) {
		p = gq_re->next;
		regfree(&gq_re->re);
		free(gq_re);
		gq_re = p;
	}

	gq_pattern = FALSE;

    if (!find_name && !find_dir_name) {
		file_pattern = FALSE;
	}

	return 0;
}

int
gq_proc(struct filediff *f)
{
	size_t l;
	char *p;
	ssize_t n;
	int fh;
	int rv = 1; /* not found */
    struct gq_re *re = gq_re;

	if (dontcmp) {
		return 0;
	}

    if (!cli_mode && getch() == '%') {
		dontcmp = TRUE;
		return 0;
	}

#if defined(TRACE) && 0
	fprintf(debug, "gq_proc(%s)", f->name);
#endif
	if (S_ISREG(f->type[0]) && f->siz[0]) {
		p = syspth[0];
		l = pthlen[0];
	} else if (S_ISREG(f->type[1]) && f->siz[1]) {
		p = syspth[1];
		l = pthlen[1];
	} else {
		goto ret2; /* Not "ret" since p and l are not set */
	}

	pthcat(p, l, f->name);

#if defined(TRACE) && 0
	fprintf(debug, " \"%s\"", p);
#endif
	if ((fh = open(p, O_RDONLY)) == -1) {

		if (!ign_errs && dialog(
		    "'i' ignore errors, <other key> continue",
		    NULL, "open \"%s\": %s", p, strerror(errno)) == 'i') {

			ign_errs = TRUE;
		}

		rv = -1;
		goto ret;
	}

	while (1) {
		if ((n = read(fh, gq_buf, GQBUFSIZ)) == -1) {
			printerr(strerror(errno), "read \"%s\"", p);
			rv = -1;
			break;
		}

		if (!n)
			break;
        tot_cmp_byte_count += n;
		gq_buf[n] = 0;

		if (!regexec(&re->re, gq_buf, 0, NULL, 0)) {
			if (!(re = re->next)) {
				rv = 0;
				break;
			} else {
				if (lseek(fh, 0, SEEK_SET) == -1) {
					printerr(strerror(errno),
					    "lseek \"%s\"", p);
					rv = -1;
					break;
				}

				continue;
			}
		}

		if (n != GQBUFSIZ)
			break;

		if (lseek(fh, -GQBSEEK, SEEK_CUR) == -1) {
            printerr(strerror(errno), LOCFMT "lseek \"%s\"" LOCVAR, p);
			rv = -1;
			break;
		}
	}

	if (close(fh) == -1) {
        printerr(strerror(errno), LOCFMT "close \"%s\"" LOCVAR, p);
		rv = -1;
	}

ret:
	p[l] = 0;
ret2:
#if defined(TRACE) && 0
	fprintf(debug, "->(%d)\n", rv);
#endif
	return rv;
}

int gq_proc_lines(const struct filediff *const f)
{
    FILE *fh;
    char *line = NULL; /* let getline() allocate buffer */
    size_t len = 0; /* let getline() allocate buffer */
    ssize_t nread;
    unsigned long line_num = 0;
    bool binary = FALSE;
    size_t pathlen;
    char *path;
    int return_value = 1; /* no match */

    if (S_ISREG(f->type[0]) && f->siz[0]) {
        path = syspth[0];
        pathlen = pthlen[0];
    } else if (S_ISREG(f->type[1]) && f->siz[1]) {
        path = syspth[1];
        pathlen = pthlen[1];
    } else {
        goto ret;
    }
    pthcat(path, pathlen, f->name);

    if (!(fh = fopen(path, "r"))) {
        printerr(strerror(errno), LOCFMT "fopen \"%s\"" LOCVAR, path);
        return_value = -1;
        goto cut_path;
    }
    while (1) { /* line loop */
        ssize_t i;
        errno = 0;

        if ((nread = getline(&line, &len, fh)) == -1) {
            if (errno)
                printerr(strerror(errno), LOCFMT "getline \"%s\"" LOCVAR, path);
            return_value = -1;
            break;
        }
        if (!nread)
            break;
        tot_cmp_byte_count += nread;
        /* test if buffer contains a 0 byte */
        for (i = 0; !binary && i < nread; i++)
            if (!line[i])
                binary = TRUE;
        if (!binary) {
            ++line_num;

            /* chomp */
            if (line[nread - 1] == '\n') {
                if (nread == 1)
                    continue;
                line[nread - 1] = 0;
            }
        }
        if (!gq_match_buf(line)) {
            return_value = 0;
            if (binary) {
                printf("Binary file %s matches\n", path);
                break;
            } else {
                printf("%s:%lu:%s\n", path, line_num, line);
            }
        }
    }
    free(line);
    fclose(fh);

cut_path:
    path[pathlen] = 0;
ret:
    return return_value;
}

static int gq_match_buf(const char *const buf)
{
    struct gq_re *re;
    int rv = 0; /* 0: match found */

    /* Pattern loop. All patterns need to match! */
    for (re = gq_re; re; re = re->next)
        if (regexec(&re->re, buf, 0, NULL, 0))
            rv = 1;
    return rv;
}
