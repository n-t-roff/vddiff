#include "compat.h"
#include "test.h"
#include "fs_test.h"
#include "main.h"

bool printerr_called;

void test(void)
{
	fprintf(debug, "-> test\n");
	fs_test();
	fprintf(debug, "<- test\n");
}
