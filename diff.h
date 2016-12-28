/* File marked (for delete, copy, etc.) */
#define FDFL_MARK 1

struct filediff {
	char   *name;
	char   *llink, *rlink;
	mode_t  type[2];
	uid_t   luid,   ruid;
	gid_t   lgid,   rgid;
	off_t   siz[2];
	time_t  lmtim,  rmtim;
	dev_t   lrdev,  rrdev;
	unsigned fl;
	char    diff;
};

extern short followlinks;
extern bool one_scan;

int build_diff_db(int);
int scan_subdir(char *, char *, int);
int is_diff_dir(struct filediff *);
size_t pthcat(char *, size_t, char *);
int cmp_file(char *, off_t, char *, off_t, unsigned);
void free_diff(struct filediff *);
void do_scan(void);
