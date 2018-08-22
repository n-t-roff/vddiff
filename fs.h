#ifdef __cplusplus
extern "C" {
#endif

#include <sys/stat.h>

void clr_fs_err(void);
void fs_mkdir(short tree);
void fs_rename(int, long, int, unsigned);
void fs_chmod(int, long, int, unsigned);
void fs_chown(int, int, long, int, unsigned);
int fs_rm(int, const char *const, char *, long, int, unsigned);
int fs_cp(int, long, int, unsigned, unsigned *);
void fs_cat(long);
void rebuild_db(short);
int fs_get_dst(long, unsigned);
int fs_any_dst(long, int, unsigned);

/* global for software test: */

extern time_t fs_t1, fs_t2;
extern char *pth1, *pth2;

void rm_file(void);
int cp_reg(const unsigned);
int fs_stat(const char *, struct stat *, const unsigned);

#ifdef __cplusplus
}
#endif
