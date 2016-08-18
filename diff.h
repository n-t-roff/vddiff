enum diff { NO_DIFF, DIFF, SAME_INO };

struct filediff {
	char *name;
	char *llink, *rlink;
	mode_t ltype, rtype;
	enum diff diff;
};

int build_diff_db(void);
void follow(int);
