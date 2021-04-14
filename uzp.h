#ifndef UZP_H
#define UZP_H

#include <sys/types.h>
#include "exec.h"

#define TMPPREFIX "/." BIN "."

enum uz_id { UZ_BZ2, UZ_GZ, UZ_NONE, UZ_RAR, UZ_TAR, UZ_TAR_Z, UZ_TBZ, UZ_TGZ, UZ_TXZ, UZ_XZ, UZ_ZIP };

struct uz_ext {
	const char *str;
	enum uz_id id;
};

extern const char *tmpdirbase;
extern char *tmp_dir;
extern char *path_display_name[2];
extern size_t path_display_buffer_size[2];
extern size_t sys_path_tmp_len[2];
extern size_t path_display_name_offset[2];

/**
 * @brief unpack
 * @param f
 * @param tree
 * @param tmp
 * @param type
 *   1: Also unpack archives, not just files
 *   2: Non-curses mode
 *   4: Always set tmpdir
 *   8: Check if viewer is set for extension. In this case the archive is not unpacked.
 * @return
 */
struct filediff *unpack(const struct filediff *f, int tree, char **tmp, int type);
void rmtmpdirs(const char *const);
int uz_init(void);
void uz_add(char *, char *);
void uz_exit(void);
const char *gettmpdirbase(void);
/*
 * Called before output af path to UI
 * Input
 *   i: 0: syspth[0], 1: syspth[1], 2: both paths
 */
void set_path_display_name(const int i);
/*
 * Called when archive is entered
 * Input
 *   mode
 *     0/1: side, 2: side 1 only
 *     4: Started from main()
 *   fn: archive file name
 *   tn: temp dir name
 */
void set_path_display_name_offset(const int mode, const char *const, const char *const);
void reset_path_offsets(int);
void free_path_offsets(int i);
void copy_path_offsets(int i_from, int i_to);

#endif /* UZP_H */
