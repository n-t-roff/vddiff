#include <stdlib.h>
#include <time.h>
#include "compat.h"
#include "fs_test.h"
#include "main.h"
#include "fs.h"
#include "test.h"
#include "diff.h"

static const char enoent[] = "Non-existing file";
static const char eacces[] = "/root/No access permissions";
static const char empty[] = "TEST/Empty regular file";
static const char deadlink[] = "TEST/Dead link";
static const char symlink[] = "TEST/Symlink to empty regular file";

static void fs_stat_test_case(
		const int follow,
		const char *const path,
		const int mode,
		const int msg,
		const int ret_val)
{
	struct stat st;
	int v;

	followlinks = follow;
	printerr_called = FALSE;
	fprintf(debug, "\t-> msg: %d\n", msg);
	v = fs_stat(path, &st, mode);
	fprintf(debug, "\t<- msg: %d\n", printerr_called);
	if (printerr_called != msg || v != ret_val) exit(1);
}

static void fs_stat_test(void) {
	fprintf(debug, "->fs_stat_test\n");

	fs_stat_test_case(0, enoent, 1, 1, -1);
	fs_stat_test_case(0, enoent, 0, 0, -1);
	fs_stat_test_case(0, eacces, 0, 1, -1);
	fs_stat_test_case(0, empty, 0, 0, 0);
	fs_stat_test_case(1, deadlink, 1, 1, -1);
	fs_stat_test_case(1, deadlink, 0, 0, -1);
	fs_stat_test_case(0, deadlink, 0, 0, 0);
	fs_stat_test_case(1, symlink, 0, 0, 0);

	fprintf(debug, "<-fs_stat_test\n");
}

static void cp_reg_test(void) {
	fprintf(debug, "->cp_reg_test\n");

	/* Successfully copy large file */

	pth1 = "test";
	pth2 = "TEST/test";
	if (fs_stat(pth1, &gstat[0], 0)) exit(1);
	if (cp_reg(0)) exit(1); /* Copy file */
	if (cp_reg(0) != 1) exit(1); /* Files are equal -> do nothing */

	pth1 = empty;
	if (fs_stat(pth1, &gstat[0], 0)) exit(1);
	if (cp_reg(1)) exit(1); /* Append empty file */
	if (cmp_file(pth1, gstat[0].st_size,
	             pth2, gstat[1].st_size, 1)) exit(1);

	pth1 = pth2;
	if (fs_stat(pth1, &gstat[0], 0)) exit(1);
	fs_t1 = time(NULL);
	rm_file();
	if (fs_stat(pth1, &gstat[0], 0) != -1) exit(1);

	fprintf(debug, "<-cp_reg_test\n");
}

void fs_test(void) {
	fprintf(debug, "->fs_test\n");
	fs_stat_test();
	cp_reg_test();
	fprintf(debug, "<-fs_test\n");
}
