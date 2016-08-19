#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "compat.h"
#include "main.h"
#include "y.tab.h"
#include "ui.h"
#include "exec.h"

char *prog;
size_t llen,
       rlen;
char lpath[PATHSIZ],
     rpath[PATHSIZ],
     lbuf[PATHSIZ],
     rbuf[PATHSIZ];
struct stat stat1, stat2;

static void check_args(char **);
static int read_rc(void);
static void usage(void);

int
main(int argc, char **argv)
{
	prog = *argv++; argc--;
	if (argc != 2) {
		printf("Two arguments expected\n");
		usage();
	}

	set_difftool("vim -dR");
	check_args(argv);
	if (read_rc())
		return 1;
	build_ui();
	return 0;
}

static int
read_rc(void)
{
	static char rc_name[] = "/.vddiffrc";
	char *s, *rc_path;
	size_t l;
	int rv = 0;
	extern FILE *yyin;

	if (!(s = getenv("HOME"))) {
		printf("HOME not set\n");
		return 1;
	} else {
		l = strlen(s);
		rc_path = malloc(l + sizeof rc_name);
		memcpy(rc_path, s, l);
		memcpy(rc_path + l, rc_name, sizeof rc_name);
	}

	if (stat(rc_path, &stat1) == -1) {
		if (errno == ENOENT)
			goto free;
		printf("stat(\"%s\") failed: %s\n", rc_path,
		    strerror(errno));
		rv = 1;
		goto free;
	}

	if (!(yyin = fopen(rc_path, "r"))) {
		printf("fopen(\"%s\") failed: %s\n", rc_path,
		    strerror(errno));
		rv = 1;
		goto free;
	}

	rv = yyparse();

	if (fclose(yyin) == EOF) {
		printf("fclose(\"%s\") failed: %s\n", rc_path,
		    strerror(errno));
	}
free:
	free(rc_path);
	return rv;
}

static void
check_args(char **argv)
{
	char *s;
	ino_t ino;

	if (stat(s = *argv++, &stat1) == -1) {
		printf("stat(\"%s\") failed: %s\n", s, strerror(errno));
		usage();
	}

	if (!S_ISDIR(stat1.st_mode)) {
		printf("\"%s\" is not a directory\n", s);
		usage();
	}

	llen = strlen(s);
	memcpy(lpath, s, llen + 1);
	ino = stat1.st_ino;

	if (stat(s = *argv++, &stat1) == -1) {
		printf("stat(\"%s\") failed: %s\n", s, strerror(errno));
		usage();
	}

	if (stat1.st_ino == ino) {
		printf("\"%s\" and \"%s\" are the same directory\n",
		    lpath, s);
		exit(0);
	}

	if (!S_ISDIR(stat1.st_mode)) {
		printf("\"%s\" is not a directory\n", s);
		usage();
	}

	rlen = strlen(s);
	memcpy(rpath, s, rlen + 1);
}

static void
usage(void)
{
	printf("Usage: %s <directory_1> <directory_2>\n", prog);
	exit(1);
}
