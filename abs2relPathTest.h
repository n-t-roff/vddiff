#ifndef ABS2REL_PATH_TEST_H
#define ABS2REL_PATH_TEST_H

class Abs2RelPathTest
{
public:
    void run() const;

private:
    void test(const char *refPath, const char *absPath, const char *relPath) const;
};

#endif // ABS2REL_PATH_TEST_H
