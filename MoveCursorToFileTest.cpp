#include "MoveCursorToFileTest.h"
#include "MoveCursorToFile.h"
#include <iostream>

void MoveCursorToFileTest::run() const
{
    test(".");
    test("/");
    test("foo");
    test("foo/bar");
    test("/foo/bar");
}

void MoveCursorToFileTest::test(const char *pathName) const
{
    struct MoveCursorToFile *obj = newMoveCursorToFile(pathName);
    std::cout << pathName << ": Path(" << obj->getPathName(obj) << ") File(" << obj->getFileName(obj) << ")\n";
    obj->destroy(obj);
}
