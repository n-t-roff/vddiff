struct filediff {
	char *name;
	char *llink, *rlink;
	mode_t ltype, rtype;
	char diff;
};

int build_diff_db(int);
void scan_subdir(char *, int);
void follow(int);
int is_diff_dir(char *);
