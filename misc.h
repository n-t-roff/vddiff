#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

int getuwidth(unsigned long);
char *msgrealpath(const char *);
bool str_eq_dotdot(const char *);
/*
 * Return base name from `buf`, leaving dir name in `buf`.
 * `buf` is modified, hence it is declared non-const.
 * `*bufsiz` is the length of the zero terminated C-string
 * in `buf`. It is updated to the dir name length.
 *
 * If `buf` does not contain a '/' `buf` is changed to ".".
 *
 * If `buf` is empty or an error occurs NULL is returned.
 *
 * The returned pointer needs to be freed with free(3).
 */
const char *buf_basename(char *const buf, size_t *bufsiz);
int do_cli_rm(int argc, char **argv);
/*
 * Copy syspth[0] into syspth[1] or, if syspth[1] is not a directory,
 * overwrite syspth[1] with syspth[0].
 * Expects (l)stat already do be done into gstat[i].
 *
 * opt:
 *   1  move instead of copy (remove source after copy operation)
 */
int do_cli_cp(int argc, char **argv, const unsigned opt);
/*
 * Return value:
 *   < 0: a < b: a is older than b
 *   = 0: a and b have equal time
 *   > 0: a > b: a is newer than b
 */
int cmp_timespec(const struct timespec a, const struct timespec b);
void get_uid_name(const uid_t uid, char *const buf, const size_t buf_size);
void get_gid_name(const gid_t gid, char *const buf, const size_t buf_size);
void add_skip_ext(char *const ext);

extern const char oom_msg[];
extern bool override_prev;

#ifdef __cplusplus
}
#endif
