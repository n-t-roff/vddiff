#include <cstring>
#include <cstdlib>
#include "compat.h"
#include "main.h"
#include "misc_test.h"
#include "misc.h"

void MiscTest::run() const
{
    fprintf(debug, "->misc_test\n");
    bufBaseNameTest();
    fprintf(debug, "<-misc_test\n");
}

void MiscTest::bufBaseNameTest() const {
    fprintf(debug, "->bufBaseNameTest\n");
    bufBaseNameTestCase(NULL);
    bufBaseNameTestCase("");
    bufBaseNameTestCase("/");
    bufBaseNameTestCase(".");
    bufBaseNameTestCase("..");
    bufBaseNameTestCase("usr");
    bufBaseNameTestCase("/usr");
    bufBaseNameTestCase("/usr/");
    bufBaseNameTestCase("./foo");
    bufBaseNameTestCase("foo/bar");
    bufBaseNameTestCase("/foo/bar");
    fprintf(debug, "<-bufBaseNameTest\n");
}

void MiscTest::bufBaseNameTestCase(const char *const dat) const {
    char *buf = dat ? strdup(dat) : const_cast<char *>(dat);
    size_t l = buf ? strlen(buf) : 0;
    const char *const base = buf_basename(buf, &l);
    free(const_cast<char *>(base));
    free(buf);
}
