enum uz_id { UZ_NONE, UZ_GZ, UZ_BZ2, UZ_TAR, UZ_TGZ, UZ_TBZ };

struct uz_ext {
	char *str;
	enum uz_id id;
};

struct filediff *unzip(struct filediff *, int);
void rmtmpdirs(char *);
void uz_init(void);
