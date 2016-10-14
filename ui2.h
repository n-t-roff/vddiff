extern short noic, magic, nows, scale;
extern short regex;

int test_fkey(int, unsigned short);
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
