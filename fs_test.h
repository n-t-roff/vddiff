#ifndef FS_TEST_H_
#define FS_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

class FsTest
{
public:
	void run() const;

private:
	void fsStatTest() const;
	void fsStatTestCase(const int, const char *const, const int, const int,
		const int) const;
	void cpRegTest() const;
    void copyLargeFile() const;
    void appendFile() const;

	const char *const enoent { "Non-existing file" };
	const char *const eacces { "/root/No access permissions" };
	const char *const empty { "TEST/Empty regular file" };
	const char *const deadlink { "TEST/Dead link" };
	const char *const symlink { "TEST/Symlink to empty regular file" };
};

#ifdef __cplusplus
}
#endif

#endif
