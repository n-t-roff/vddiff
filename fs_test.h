#ifndef FS_TEST_H_
#define FS_TEST_H_

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
    void copy2FollowLink() const;
    void appendFile() const;
    void copyTree() const;

	const char *const enoent { "Non-existing file" };
	const char *const eacces { "/root/No access permissions" };
    const char *const empty { TEST_DIR "/Empty regular file" };
    const char *const deadlink { TEST_DIR "/Dead link" };
    const char *const symlink { TEST_DIR "/Symlink to empty regular file" };
    const char *const file1 { TEST_DIR "/File 1" };
    const char *const file2 { TEST_DIR "/File 2" };
    const char *const file1copy { TEST_DIR "/Copy of file 1" };
};

#endif
