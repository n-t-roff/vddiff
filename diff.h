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
	dev_t   lrdev,  rrdev;
	unsigned fl;
	char    diff;
};

extern short followlinks;
extern bool one_scan;

int build_diff_db(int);
int scan_subdir(char *, char *, int);
int is_diff_dir(struct filediff *);
size_t pthcat(char *, size_t, const char *);
int cmp_file(char *, off_t, char *, off_t, unsigned);
void free_diff(struct filediff *);
void do_scan(void);
