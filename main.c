#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "compat.h"
#include "main.h"
#include "y.tab.h"

char *prog;
size_t llen,
       rlen;
char lpath[PATHSIZ],
     rpath[PATHSIZ],
     lbuf[PATHSIZ],
     rbuf[PATHSIZ];
struct stat stat1, stat2;
enum sorting sorting;
char *difftool = "vim -d";

static void usage(void);
static void read_rc(void);

int
main(int argc, char **argv)
{
	prog = *argv++; argc--;
	if (argc != 2) {
		printf("Two arguments expected\n");
		usage();
	}

	read_rc();
	return 0;
}

static void
read_rc(void)
{
	static char rc_name[] = "/.vddiffrc";
	char *s, *rc_path;
	size_t l;
	extern FILE *yyin;

	if (!(s = getenv("HOME"))) {
		printf("HOME not set\n");
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
		goto free;
	}

	if (!(yyin = fopen(rc_path, "r"))) {
		printf("fopen(\"%s\") failed: %s\n", rc_path,
		    strerror(errno));
		goto free;
	}

	yyparse();

	if (fclose(yyin) == EOF) {
		printf("fclose(\"%s\") failed: %s\n", rc_path,
		    strerror(errno));
	}
free:
	free(rc_path);
}

static void
usage(void)
{
	printf("Usage: %s <directory_1> <directory_2>\n", prog);
	exit(1);
}
