/*
Copyright (c) 2017, Carsten Kunze <carsten.kunze@arcor.de>

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
#include <signal.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "compat.h"
#include "ui.h"
#include "main.h"
#include "exec.h"
#include "info.h"
#include "dl.h"
#include "uzp.h"
#include "db.h"

static int info_proc(void);
static void info_wr_ddl(FILE *);

static const char info_name[] = ".vddiffinfo.new";
const char info_dir_txt[] = "dir";
const char info_ddir_txt[] = "diffdir";

char *info_pth;
static char *info_tpth;

void
info_load(void)
{
	FILE *fh;

	if (!(info_tpth = add_home_pth(info_name))) {
		return;
	}

	info_pth = strdup(info_tpth);
	info_pth[strlen(info_pth) - 4] = 0;

	if (stat(info_pth, &stat1) == -1) {
		if (errno == ENOENT) {
			return;
		}

		printerr(strerror(errno), "stat \"%s\"", info_pth);
		return;
	}

	if (!(fh = fopen(info_pth, "r"))) {
		printerr(strerror(errno), "fopen \"%s\"", info_pth);
		return;
	}

	while (fgets(lbuf, BUF_SIZE, fh)) {
		info_chomp(lbuf);

		if (!strcmp(lbuf, info_ddir_txt)) {
			dl_info_ddl(fh);
		} else {
			printerr(lbuf, "Invalid line in \"%s\"", info_pth);
		}
	}

	if (fclose(fh) == EOF) {
		printerr(strerror(errno), "fclose \"%s\"", info_pth);
	}
}

void
info_store(void)
{
	struct sigaction intr, quit;
	sigset_t smsk;
	pid_t pid;

	if (!info_pth) {
		return;
	}

	exec_set_sig(&intr, &quit, &smsk);

	switch ((pid = fork())) {
	case -1:
		printerr(strerror(errno), "fork");

	case 0:
		exec_res_sig(&intr, &quit, &smsk);
		_exit(info_proc());
	}

	exec_res_sig(&intr, &quit, &smsk);
}

static int
info_proc(void)
{
	int rv = 0;
	FILE *fh;
	bool wr;

	wr = ddl_num ? 1 : 0;

	if (!wr) {
		goto rm;
	} else if (!(fh = fopen(info_tpth, "w"))) {
		printerr(strerror(errno), "fopen \"%s\"", info_tpth);
		rv = 1;
		goto ret;
	}

	if (ddl_num) {
		info_wr_ddl(fh);
	}

	if (fclose(fh) == EOF) {
		printerr(strerror(errno), "fclose \"%s\"", info_tpth);
		rv = 1;
	}

rm:
	if (stat(info_pth, &stat1) == -1) {
		if (errno == ENOENT) {
			goto mv;
		}

		printerr(strerror(errno), "stat \"%s\"", info_pth);
		rv = 1;
		goto mv;
	}

	if (unlink(info_pth) == -1) {
		printerr(strerror(errno), "unlink \"%s\"", info_pth);
		rv = 1;
	}

mv:
	if (wr && rename(info_tpth, info_pth) == -1) {
		printerr(strerror(errno), "rename \"%s\", \"%s\"",
		    info_tpth, info_pth);
		rv = 1;
	}

ret:
	return rv;
}

static void
info_wr_ddl(FILE *fh)
{
	unsigned i;

	ddl_sort();

	for (i = 0; i < ddl_num; i++) {
		fprintf(fh, "%s\n%s\n%s\n", info_ddir_txt, ddl_list[i][0],
		    ddl_list[i][1]);
	}

	free(ddl_list);
	ddl_list = NULL;
}

void
info_chomp(char *s)
{
	size_t l;

	if ((l = strlen(s)) && s[l - 1] == '\n') {
		s[l - 1] = 0;
	}
}
