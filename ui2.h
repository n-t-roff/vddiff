#define FKEY_WAIT  1
#define FKEY_FORCE 2

#define FKEY_CMD_CHR(i) \
	fkey_flags[i] & FKEY_WAIT  ? '!' : \
	fkey_flags[i] & FKEY_FORCE ? '#' : '$'

extern long mark_idx[2];
extern short noic, magic, nows, scale;
extern short regex;
extern unsigned short subtree;
extern const char y_n_txt[];
extern const char ign_txt[];
extern bool file_pattern;

int test_fkey(int, unsigned short);
void set_fkey_cmd(int, char *, int);
void ui_srch(void);
int srch_file(char *);
void disp_regex(void);
void clr_regex(void);
void start_regex(char *);
int regex_srch(int);
int parsopt(char *);
void bindiff(void);
int chk_mark(char *, short);
char *saveselname(void);
unsigned findlistname(char *);
void re_sort_list(void);
void filt_stat(void);
void markc(WINDOW *);
void standoutc(WINDOW *);
void standendc(WINDOW *);
void chgat_mark(WINDOW *, int);
void chgat_curs(WINDOW *, int);
void chgat_off(WINDOW *, int);
void anykey(void);
void free_zdir(struct filediff *, char *);
void refr_scr(void);
ssize_t mbstowchs(WINDOW *, char *);
void wcs2ccs(WINDOW *, wchar_t *);
void putwcs(WINDOW *, wchar_t *, int);
ssize_t putmbs(WINDOW *, char *, int);
int addmbs(WINDOW *, char *, int);
ssize_t putmbsra(WINDOW *, char *, int);
WINDOW *new_scrl_win(int, int, int, int);
void set_def_mouse_msk(void);
