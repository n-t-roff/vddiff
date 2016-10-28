#define FKEY_CMD_CHR(i) \
	fkey_flags[i] & 1 ? '!' : \
	fkey_flags[i] & 2 ? '#' : '$'

extern short noic, magic, nows, scale;
extern short regex;
extern unsigned short subtree;
extern const char y_n_txt[];

int test_fkey(int, unsigned short);
void set_fkey_cmd(int, char *, int);
void ui_srch(void);
int srch_file(char *);
void disp_regex(void);
void clr_regex(void);
void start_regex(char *);
int regex_srch(int);
void parsopt(char *);
void bindiff(void);
char *saveselname(void);
unsigned findlistname(char *);
void re_sort_list(void);
void filt_stat(void);
void anykey(void);
void free_zdir(struct filediff *, char *);
