#ifdef __cplusplus
extern "C" {
#endif

#include <sys/stat.h>

void clr_fs_err(void);
void fs_mkdir(short tree);
void fs_rename(int, long, int, unsigned);
void fs_chmod(int, long, int, unsigned);
void fs_chown(int, int, long, int, unsigned);
/*
 * Input:
 *   Global:
 *     syspth[]
 *     bmode
 *     fmode
 *
 *   Argument:
 *     tree:
 *        1: "dl", 2: "dr", 3: "dd" (detect which file exists)
 *        0: Use pth2, ignore nam and u. n must be 1.
 *       -1: Use pth1
 *     nam:
 *       File name. `fmode` only. Ignored for `tree <= 0` or `!fmode`.
 *       If `nam` is not NULL, `u` is not used. `n` must be 1.
 *     md:
 *       1: Force
 *       2: Don't rebuild DB (for mmrk and fs_cp())
 *       4: Don't reset 'fs_all'
 *       8: fs_ign_errs
 *
 * Return value:
 *   0: Ok
 *   1: Cancel
 *   2: fs_error
 *   4: fs_ign_errs
 */
int fs_rm(int tree, const char *const txt, char *nam, long u, int n,
          unsigned md);
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


extern bool fs_none; /* Don't delete or overwrite any file */
extern bool fs_abort; /* Abort operation */

/* global for software test: */

extern time_t fs_t1, fs_t2;
extern char *pth1, *pth2;

void rm_file(void);
/*
 * WARNING: Overwrites `lbuf` and (via cmp_file()) `rbuf`!
 *
 * Input:
 *   mode:
 *     1: append
 *     2: force (currently used by software test only)
 *
 * Output:
 *    1: Files are equal
 *    0: Successfully copied
 *   -1: System call failed
 *   -2: User abort
 */
int cp_reg(const unsigned mode);
int fs_stat(const char *, struct stat *, const unsigned);

#ifdef __cplusplus
}
#endif
