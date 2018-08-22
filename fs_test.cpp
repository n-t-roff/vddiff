#include <exception>
#include <stdexcept>
#include <cstdlib>
#include <time.h>
#include "compat.h"
#include "fs_test.h"
#include "main.h"
#include "fs.h"
#include "test.h"
#include "diff.h"

void FsTest::run() const {
	fprintf(debug, "->fs_test\n");
	fsStatTest();
	cpRegTest();
	fprintf(debug, "<-fs_test\n");
}

void FsTest::fsStatTest() const {
	fprintf(debug, "->fs_stat_test\n");
	fsStatTestCase(0, enoent, 1, 1, -1);
	fsStatTestCase(0, enoent, 0, 0, -1);
	fsStatTestCase(0, eacces, 0, 1, -1);
	fsStatTestCase(0, empty, 0, 0, 0);
	fsStatTestCase(1, deadlink, 1, 1, -1);
	fsStatTestCase(1, deadlink, 0, 0, -1);
	fsStatTestCase(0, deadlink, 0, 0, 0);
	fsStatTestCase(1, symlink, 0, 0, 0);
	fprintf(debug, "<-fs_stat_test\n");
}

void FsTest::fsStatTestCase(
		const int follow,
		const char *const path,
		const int mode,
		const int msg,
		const int ret_val) const
{
	struct stat st;
	int v;

	followlinks = follow;
	printerr_called = FALSE;
	fprintf(debug, "\t-> msg: %d\n", msg);
	v = fs_stat(path, &st, mode);
	fprintf(debug, "\t<- msg: %d\n", printerr_called);
    if (printerr_called != msg || v != ret_val)
        FATAL_ERROR;
}

void FsTest::cpRegTest() const {
	fprintf(debug, "->cp_reg_test\n");
    copyLargeFile();
    fprintf(debug, "<-cp_reg_test\n");
}

void FsTest::copyLargeFile() const {
    fprintf(debug, "->copyLargeFile\n");

    // Successfully copy large file

    pth1 = const_cast<char *>("test");
    pth2 = const_cast<char *>("TEST/test");
    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cp_reg(0)) // Copy file
        FATAL_ERROR;
    if (cp_reg(0) != 1) // Files are equal -> do nothing
        FATAL_ERROR;

    // Append empty file

    pth1 = const_cast<char *>(empty);
    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cp_reg(3)) // Append empty file
        FATAL_ERROR;

    // Compare files after append

    pth1 = const_cast<char *>("test");
    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cmp_file(pth1, gstat[0].st_size,
                 pth2, gstat[1].st_size, 1))
        FATAL_ERROR;

    if (cp_reg(3)) // Append file to equal contents file
        FATAL_ERROR;

    // remove copy of file

    pth1 = pth2;
    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    fs_t1 = time(NULL);
    rm_file();
    if (fs_stat(pth1, &gstat[0], 0) != -1)
        FATAL_ERROR;

    fprintf(debug, "<-copyLargeFile\n");
}

void FsTest::appendFile() const {
    fprintf(debug, "->appendFile\n");
    fprintf(debug, "<-appendFile\n");
}
