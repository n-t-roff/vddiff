#define TMPPREFIX "/.vddiff."

enum uz_id { UZ_NONE, UZ_GZ, UZ_BZ2, UZ_TAR, UZ_TGZ, UZ_TBZ, UZ_ZIP };

struct uz_ext {
	char *str;
	enum uz_id id;
};

extern char *tmp_dir;
extern char *vpath[2];
extern size_t vpthsz[2];
extern size_t spthofs[2];
extern size_t vpthofs[2];

struct filediff *unpack(struct filediff *, int, char **, int);
void rmtmpdirs(char *, tool_flags_t);
int uz_init(void);
void uz_exit(void);
const char *gettmpdirbase(void);
void setvpth(int);
