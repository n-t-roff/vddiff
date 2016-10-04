enum uz_id { UZ_NONE, UZ_GZ, UZ_BZ2, UZ_TAR, UZ_TGZ, UZ_TBZ, UZ_ZIP };

struct uz_ext {
	char *str;
	enum uz_id id;
};

extern char *tmp_dir;

struct filediff *unpack(struct filediff *, int, char **, int);
void rmtmpdirs(char *);
void uz_init(void);
void uz_exit(void);
