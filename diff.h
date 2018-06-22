/* File marked (for delete, copy, etc.) */
#define FDFL_MMRK 1

struct filediff {
	char   *name;
	char   *llink, *rlink;
	mode_t  type[2];
	uid_t   uid[2];
	gid_t   gid[2];
	off_t   siz[2];
	time_t  mtim[2];
	dev_t   rdev[2];
	unsigned fl;
	char    diff;
};

extern short followlinks;
extern bool one_scan;
extern bool dotdot;

int build_diff_db(int);
int scan_subdir(const char *, const char *, int);
int is_diff_dir(struct filediff *);
int is_diff_pth(const char *, unsigned);
size_t pthcat(char *, size_t, const char *);
int cmp_file(char *, off_t, char *, off_t, unsigned);
void free_diff(struct filediff *);
void do_scan(void);
void save_last_path(char *);
void wr_last_path(void);
