#include <exception>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include "compat.h"
#include "fs_test.h"
#include "main.h"
#include "fs.h"
#include "test.h"
#include "diff.h"
#include "tc.h"
#include "exec.h"
#include "uzp.h"
#include "db.h"

void FsTest::run() const {
	fprintf(debug, "->fs_test\n");
	fsStatTest();
	cpRegTest();
    copyTree();
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
    copy2FollowLink();
    fprintf(debug, "<-cp_reg_test\n");
}

void FsTest::copyLargeFile() const {
    fprintf(debug, "->copyLargeFile\n");

    // Successfully copy large file

    memcpy(lbuf, "test", strlen("test") + 1);
    pth1 = lbuf;
    memcpy(rbuf, TEST_DIR "/test", strlen(TEST_DIR "/test") + 1);
    pth2 = rbuf;

    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cp_reg(0)) // Copy file
        FATAL_ERROR;

    memcpy(lbuf, "test", strlen("test") + 1);

    if (cp_reg(0) != 1) // Files are equal -> do nothing
        FATAL_ERROR;

    // Append empty file

    memcpy(lbuf, empty, strlen(empty) + 1);
    // cmp_file() had been called
    memcpy(rbuf, TEST_DIR "/test", strlen(TEST_DIR "/test") + 1);

    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cp_reg(3)) // Append empty file
        FATAL_ERROR;

    // Compare files after append

    memcpy(lbuf, "test", strlen("test") + 1);
    memcpy(rbuf, TEST_DIR "/test", strlen(TEST_DIR "/test") + 1);

    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cmp_file(pth1, gstat[0].st_size,
                 pth2, gstat[1].st_size, 1))
        FATAL_ERROR;

    memcpy(lbuf, "test", strlen("test") + 1);
    pth1 = lbuf;
    memcpy(rbuf, TEST_DIR "/test", strlen(TEST_DIR "/test") + 1);
    pth2 = rbuf;

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

void FsTest::copy2FollowLink() const {
    fprintf(debug, "->copy2FollowLink\n");

    // Copy file to dead symlink

    memcpy(lbuf, file1, strlen(file1) + 1);
    pth1 = lbuf;
    memcpy(rbuf, deadlink, strlen(deadlink) + 1);
    pth2 = rbuf;

    if (fs_stat(pth1, &gstat[0], 0))
        FATAL_ERROR;
    if (cp_reg(0)) // Copy file
        FATAL_ERROR;

    memcpy(lbuf, file1, strlen(file1) + 1);

    if (cp_reg(0) != 1) // Files are equal -> do nothing
        FATAL_ERROR;

    // Copy file to directory containing a symlink
    fprintf(debug, "<-copy2FollowLink\n");
}

void FsTest::appendFile() const {
    fprintf(debug, "->appendFile\n");
    fprintf(debug, "<-appendFile\n");
}

void FsTest::copyTree() const {
    fprintf(debug, "->copyTree\n");

    // copy project tree using fs_cp()

    const char src[] { ".." };
    pthlen[0] = strlen(src);
    memcpy(syspth[0], src, pthlen[0] + 1);

    const char dest[] { "/tmp" };
    pthlen[1] = strlen(dest);
    memcpy(syspth[1], dest, pthlen[1] + 1);

    filediff f;
    f.name = "vddiff";
    filediff *fl[] = { &f };
    db_list[0] = fl;
    db_list[1] = fl; // for fs_rm()
    db_num[0] = 1;
    db_num[1] = 1; // for fs_rm()

    followlinks = 0;
    right_col = 0;
    unsigned sto;
    bmode = FALSE;
    fmode = TRUE; // for fs_rm()

    if (fs_cp(2, // to right side
              0, // u
              1, // n
              1|4, // !rebuild|force
              &sto))
    {
        FATAL_ERROR;
    }

    // verify

    if (system("rlcmp . /tmp/vddiff"))
        FATAL_ERROR;

    // delete copy

    if (fs_rm(2, // tree: "dr"
              "delete", // txt
              nullptr, // nam
              0, // u
              1, // n
              1 | 2 | 4)) // force|!rebuild|!reset err
    {
        FATAL_ERROR;
    }

    // test if delete was successful

    if (fs_stat("/tmp/vddiff", &gstat[0], 0) != -1)
        FATAL_ERROR;
    if (errno != ENOENT)
        FATAL_ERROR;

    fprintf(debug, "<-copyTree\n");
}
