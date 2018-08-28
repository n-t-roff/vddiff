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
/*
 * to:
 *   0: Auto-detect
 *   1: To left side
 *   2: To right side
 * u: initial index
 * n: number of files
 * md:
 *     1: don't rebuild DB
 *     2: Symlink instead of copying
 *     4: Force
 *     8: Sync (update, 'U')
 *    16: Move (remove source after copy)
 *    32: Exchange
 *    64 (0x40): Set fs_ign_errs
 *   128 (0x80): Use db_list[right_col ? 0 : 1][u]->name
 *               as new name
 *
 * Return value:
 *   1: General error
 *   2: fs_ign_errs set
 */
int fs_cp(int to, long u, int n, unsigned md, unsigned *sto_res);
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
