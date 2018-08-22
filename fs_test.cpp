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
        throw std::runtime_error{"fs_stat() failed"};
}

void FsTest::cpRegTest() const {
	fprintf(debug, "->cp_reg_test\n");

	/* Successfully copy large file */

    pth1 = const_cast<char *>("test");
    pth2 = const_cast<char *>("TEST/test");
    if (fs_stat(pth1, &gstat[0], 0))
        throw std::runtime_error{"stat test failed"};
    if (cp_reg(0)) // Copy file
        throw std::runtime_error{"cp_reg failed"};
    if (cp_reg(0) != 1) // Files are equal -> do nothing
        throw std::runtime_error{"cp_reg equal file failed"};

    pth1 = const_cast<char *>(empty);
    if (fs_stat(pth1, &gstat[0], 0))
        throw std::runtime_error{"stat empty failed"};
    if (cp_reg(1)) // Append empty file
        throw std::runtime_error{"cp_reg append failed"};
    if (cmp_file(pth1, gstat[0].st_size,
	             pth2, gstat[1].st_size, 1)) exit(1);

    pth1 = pth2;
    if (fs_stat(pth1, &gstat[0], 0))
        throw std::runtime_error{"stat existing TEST/test failed"};
    fs_t1 = time(NULL);
    rm_file();
    if (fs_stat(pth1, &gstat[0], 0) != -1)
        throw std::runtime_error{"stat removed TEST/test failed"};

	fprintf(debug, "<-cp_reg_test\n");
}
