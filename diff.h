#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/* File marked (for delete, copy, etc.) */
#define FDFL_MMRK 1

struct filediff {
    const char *name;
	char   *llink, *rlink;
	mode_t  type[2];
	uid_t   uid[2];
	gid_t   gid[2];
	off_t   siz[2];
    struct timespec mtim[2];
	dev_t   rdev[2];
	unsigned fl;
	char    diff;
};

extern off_t tot_cmp_byte_count;
extern long tot_cmp_file_count;
extern short followlinks;
extern bool one_scan;
extern bool dotdot;
/*
 * Returns a compination of:
 *   1 difference found
 *   2 on error
 *   0 else
 */
int build_diff_db(int);
/*
 * Returns a compination of:
 *   1 difference found
 *   2 on error
 *   0 else
 */
int scan_subdir(const char *, const char *, int);
/* Input:
 *   Parameter:
 *     `name`: File name (without path)
 *   Global:
 *     syspth[0]
 *     pthlen[0]
 *     gstat[i]
 * Output:
 *   stdout:
 *     if (cli_mode)
 *       matching line
 *   Return value:
 *      1: no pattern match
 *      0: pattern match
 *     -1: error */
int file_grep(const char *const name);
int is_diff_dir(struct filediff *);
int is_diff_pth(const char *, unsigned);
size_t pthcat(char *, size_t, const char *);

/* WARNING: Overwrites `lbuf` and `rbuf`!
 *
 * Input: gstat[0], gstat[1], syspth[0], syspth[1]
 * Output: Combination of:
 *   2  Error, don't make DB entry
 *   0  No diff
 *   1  Diff */

int cmp_file(const char *const, const off_t, const char *const, const off_t,
	const unsigned);
/*
 * Input:
 *   syspth[0]
 *   syspth[1]
 *   gstat[0]
 *   gstat[1]
 *
 * Output: Combination of:
 *   0  No difference
 *   1  Difference
 *   2  Error
 *
 * Usage example:
 *
 *   if ((lstat("...", &gstat[0]) == -1 && errno != ENOENT) ||
 *       (lstat("...", &gstat[1]) == -1 && errno != ENOENT))
 *   {
 *     ... handle error ...
 *   } else {
 *     char *a = NULL;
 *     char *b = NULL;
 *     cmp_symlink();
 *     free(b);
 *     free(a);
 *   }
 */
int cmp_symlink(char **, char **);
void free_diff(struct filediff *);
char *read_link(char *, off_t);

/* Returns a compination of:
 *   1 difference found
 *   2 on error
 *   0 else */

int do_scan(void);
void save_last_path(char *);
void wr_last_path(void);

#ifdef __cplusplus
}
#endif
