#include <exception>
#include <iostream>
#include <cstdlib>
#include "compat.h"
#include "test.h"
#include "fs_test.h"
#include "main.h"

bool printerr_called;

void test(void)
{
	fprintf(debug, "-> test\n");

    try {
        { FsTest test; test.run(); }
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
        exit(1);
    }
	fprintf(debug, "<- test\n");
}
