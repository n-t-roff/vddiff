#include <stdlib.h>
#include "compat.h"
#include "fs_test.h"
#include "main.h"
#include "fs.h"
#include "test.h"
#include "diff.h"

static void fs_stat_test_case(
		const int follow,
		const char *const path,
		const int mode,
		const int msg,
		const int ret_val)
{
	struct stat st;
	int v;

	fprintf(debug,
		"\tfs_stat(\"%s\", &st, %d) follow: %d msg: %d\n",
		path, mode, follow, msg);
	followlinks = follow;
	printerr_called = FALSE;
	v = fs_stat(path, &st, mode);
	fprintf(debug, "\t\t: %d msg: %d\n", v, printerr_called);
	if (printerr_called != msg || v != ret_val) exit(1);
}

static void fs_stat_test(void) {
	const char enoent[] = "Non-existing file";
	const char eacces[] = "/root/No access permissions";
	const char noerr[] = "TEST/Empty regular file";
	const char deadlink[] = "TEST/Dead link";
	const char symlink[] = "TEST/Symlink to empty regular file";

	fprintf(debug, "-> fs_stat_test\n");

	fs_stat_test_case(0, enoent, 1, 1, -1);
	fs_stat_test_case(0, enoent, 0, 0, -1);
	fs_stat_test_case(0, eacces, 0, 1, -1);
	fs_stat_test_case(0, noerr, 0, 0, 0);
	fs_stat_test_case(1, deadlink, 1, 1, -1);
	fs_stat_test_case(1, deadlink, 0, 0, -1);
	fs_stat_test_case(0, deadlink, 0, 0, 0);
	fs_stat_test_case(1, symlink, 0, 0, 0);

	fprintf(debug, "<- fs_stat_test\n");
}

void fs_test(void) {
	fprintf(debug, "-> fs_test\n");
	fs_stat_test();
	fprintf(debug, "<- fs_test\n");
}
