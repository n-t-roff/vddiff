struct filediff {
	char *name;
	char *llink, *rlink;
	mode_t ltype, rtype;
	char diff;
};

int build_diff_db(void);
void scan_subdir(char *);
void follow(int);
