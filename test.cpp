#include <exception>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "compat.h"
#include "test.h"
#include "fs_test.h"
#include "main.h"
#include "fs.h"
#include "diff.h"
#include "misc_test.h"
#include "abs2relPathTest.h"

bool printerr_called;

static void rmTestDir()
{
    fprintf(debug, "->rmTestDir\n");
    followlinks = 0;
    struct stat st;

    if (!fs_stat(TEST_DIR, &st, 0)) {
        // TEST_DIR exists, remove it

        memcpy(lbuf, TEST_DIR, strlen(TEST_DIR) + 1);
        pth1 = lbuf;

        if (fs_rm(-1, // tree: Use pth1
                  "", // txt
                  nullptr, // nam
                  0, // u
                  1, // n
                  1 | 2 | 4)) // force|!rebuild|!reset err
        {
            FATAL_ERROR;
        }

        if (fs_stat(TEST_DIR, &st, 0) != -1)
            FATAL_ERROR;
    }

    if (errno != ENOENT)
        FATAL_ERROR;

    fprintf(debug, "<-rmTestDir\n");
}

void test(void)
try {
    fprintf(debug, "->test\n");
    rmTestDir();

    if (system("./gen_test_dat.sh " TEST_DIR))
        FATAL_ERROR;

    { FsTest          test; test.run(); }
    { MiscTest        test; test.run(); }
    { Abs2RelPathTest test; test.run(); }

    rmTestDir();
    fprintf(debug, "<-test\n");
} catch (std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    exit(1);
}
