#include "abs2relPathTest.h"
#include "abs2relPath.h"
#include "test.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

void Abs2RelPathTest::run() const
{

    test("/a/b/c/d/e/f", "/a/b/c/g/h/i", "../../g/h/i");
    test("/abc/def/ghi/jkl", "/abc/def/mno/qrs", "../mno/qrs");
}

void Abs2RelPathTest::test(const char *const refPath, const char *const absPath, const char *const relPath) const
{
    char *result = abs2relPath(absPath, refPath);
    if (strcmp(result, relPath))
    {
        std::cerr << "Ref \"" << refPath << "\" abs \"" << absPath << "\" expected \"" << relPath << "\" got \"" << result << "\"" << std::endl;
        FATAL_ERROR;
    }
    free(result);
}
