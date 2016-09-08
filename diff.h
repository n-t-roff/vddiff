struct filediff {
	char   *name;
	char   *llink, *rlink;
	mode_t  ltype,  rtype;
	uid_t   luid,   ruid;
	gid_t   lgid,   rgid;
	off_t   lsiz,   rsiz;
	time_t  lmtim,  rmtim;
	char    diff;
};

int build_diff_db(int);
void scan_subdir(char *, int);
void follow(int);
int is_diff_dir(char *);
size_t pthcat(char *, size_t, char *);
