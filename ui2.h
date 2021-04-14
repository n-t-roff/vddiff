#ifndef UI2_H
#define UI2_H

#include <regex.h>
#include "compat.h"

#define FKEY_WAIT  1 /* Wait after executing the command to check the command output.
                      * Used by '!' and '%'. */
#define FKEY_FORCE 2 /* Immediately start execution. Used by '#' and '%'. */
#define FKEY_SHELL 4 /* Open shell after command execution. */

#define FKEY_CMD_CHR(i) \
    (fkey_flags[fkey_set][i] & (FKEY_WAIT  | FKEY_FORCE)) == (FKEY_WAIT  | FKEY_FORCE) ? '%' : \
    (fkey_flags[fkey_set][i] &  FKEY_WAIT               ) ==  FKEY_WAIT                ? '!' : \
    (fkey_flags[fkey_set][i] &               FKEY_FORCE ) ==               FKEY_FORCE  ? '#' : \
    (fkey_flags[fkey_set][i] & (FKEY_SHELL | FKEY_FORCE)) == (FKEY_SHELL | FKEY_FORCE) ? '>' : \
                                                                                         '$'

extern long mark_idx[2];
extern long mmrkd[2];
extern regex_t re_dat;
extern short noic, magic, nows, scale;
extern short regex_mode;
extern unsigned short subtree;
extern const char y_n_txt[];
extern const char y_a_n_txt[];
extern const char ign_txt[];
extern const char ign_esc_txt[];
extern const char any_txt[];
extern const char enter_regex_txt[];
extern const char no_match_txt[];
extern bool excl_or;
extern bool file_exec;
extern bool nobold;
extern unsigned prev_pos[2];
extern unsigned jmrk[2][32];

int test_fkey(int, unsigned short, long);
/* Test if "fkey" argument is simply a saved string
 * or if it is a command to process a file. */
bool is_fkey_cmd(const char *const);
void set_fkey_cmd(int, int, char *, int, char *);
void ui_srch(void);
int srch_file(char *, int);
void disp_regex(void);
void clr_regex(void);
void start_regex(char *);
int regex_srch(int);
int parsopt(char *);
/**
 * @brief bindiff
 * @param mode 1: unpack
 */
void bindiff(unsigned int mode);
int chk_mark(const char *, short);
char *saveselname(void);
unsigned findlistname(char *);
void re_sort_list(void);
void filt_stat(void);
void markc(WINDOW *);
void mmrkc(WINDOW *);
void standoutc(WINDOW *);
void standendc(WINDOW *);
void chgat_mark(WINDOW *, int);
void chgat_mmrk(WINDOW *, int);
void chgat_curs(WINDOW *, int);
void chgat_off(WINDOW *, int);
attr_t get_curs_attr(void);
attr_t get_off_attr(void);
short get_curs_pair(void);
short get_off_pair(void);
int anykey(const char *const text);
void free_zdir(struct filediff *, char *);
void refr_scr(void);
void rebuild_scr(void);
ssize_t mbstowchs(WINDOW *, const char *);
void wcs2ccs(WINDOW *, wchar_t *);
void putwcs(WINDOW *, wchar_t *, int);
/* Input:
 *   n: -1: unlimited
 * Output:
 *   Number of wide chars or -1 on error
 */
ssize_t putmbs(WINDOW *w, const char *const s, int n);
int addmbs(WINDOW *, const char *const, int);
/* Put multi-byte string right-adjusted.
 * Input
 *   mx: available width */
ssize_t putmbsra(WINDOW *, const char *, int);
WINDOW *new_scrl_win(int, int, int, int);
void set_def_mouse_msk(void);
int key_mmrk(void);
void tgl_mmrk(struct filediff *);
long get_mmrk(void);
void mmrktobot(void);
int ui_cp(int, long, unsigned short, unsigned);
int ui_mv(int, long, unsigned short);
int ui_dd(int, long, unsigned short);
int ui_rename(int, long, unsigned short);
int ui_chmod(int, long, unsigned short);
int ui_chown(int, int, long, unsigned short);
void prt_ln_num(void);
void list_jmrks(void);
int keep_ungetch(int);
int opt_flushinp(void);
void set_fkey_set(const int i);

/* private declarations */

void set_fkey_comment(const int set, const int key_id, char *const comment);

#endif /* UI2_H */
