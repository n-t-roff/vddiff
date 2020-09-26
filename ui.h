#ifndef UI_H
#define UI_H

#include <sys/types.h>
#include <stdarg.h>
#include "compat.h"

#define PAIR_LEFTONLY  1
#define PAIR_RIGHTONLY 2
#define PAIR_DIFF      3
#define PAIR_DIR       4
#define PAIR_UNKNOWN   5
#define PAIR_LINK      6
#define PAIR_CURSOR    7
#define PAIR_ERROR     8
#define PAIR_NORMAL    9
#define PAIR_MARK      10
#define PAIR_MMRK      11

#define DB_LST_IDX (top_idx[right_col] + curs[right_col])
#define DB_LST_ITM (db_list[right_col][DB_LST_IDX])
#define CHGAT_MRKS (fmode || add_mode || add_hsize || add_bsize || add_mtime \
    || add_owner || add_group)

struct ui_state {
	/* Path before going to temp dir for returning when leaving temp dir */
	char *lpth, *rpth;
	/* Path to temp dir for removing it later */
    char *lzip, *rzip;
	size_t llen, rlen;
	void *bst;
	unsigned num; /* db_num */
	struct filediff **list;
    unsigned top_idx, curs;
    long mmrkd;
	/* 1: Don't restore. Just remove tmpdir. */
	unsigned fl;
	unsigned short tree;
	struct ui_state *next;
};

/* Returns:
 *   1 in "-q" (qdiff) mode when a difference was detected
 *   2 in qdiff mode when an error was detected
 *   0 else */

int build_ui(void);
/*
 * Input:
 *   err: optional, e.g. `strerror(errno)`
 *   fmt: e.g. "stat"
 * Output: 0: Success, !0: Fail
 */
int printerr(const char *err, const char *fmt, ...);
/*
 * Input:
 *   quest: Possible answers as human readable text
 *   answ: Possible answers as char array
 *   fmt: Dialog text
 * Output
 *   The character the user enters or 0 if stdout is not a tty
 */
int dialog(const char *, const char *, const char *, ...);
int vdialog(const char *, const char *, const char *, va_list);

/**
 * @brief disp_list
 * @param md [0]: 1: Enable cursor
 */
void disp_list(unsigned md);
void center(unsigned);
void no_file(void);
void action(short, unsigned);
void mark_global(void);
void clr_mark(void);

/**
 * @brief disp_curs
 * @param a 0: Remove cursor, 1: Normal cursor
 */
void disp_curs(int a);
void enter_dir(const char *, const char *, bool, bool, short
#ifdef DEBUG
    , const char *const , const unsigned
#endif
    );
void set_win_dim(void);
void pop_state(short);
int curs_down(void);
size_t getfilesize(char *, size_t, off_t, unsigned);

extern short color;
extern short color_leftonly ,
             color_rightonly,
             color_diff     ,
             color_dir      ,
             color_unknown  ,
             color_link     ,
             color_normal   ,
             color_cursor_fg,
             color_cursor_bg,
             color_error_fg ,
             color_error_bg ,
             color_mark_fg  ,
             color_mark_bg  ,
             color_mmrk_fg  ,
             color_mmrk_bg  ,
             color_bg       ;
extern unsigned top_idx[2], curs[2], statw;
extern unsigned listw;
extern unsigned listh;
extern WINDOW *wlist;
extern WINDOW *wstat;
extern struct filediff *mark;
extern char *gl_mark, *mark_lnam, *mark_rnam;

#define FKEY_NUM 48
#define FKEY_MUX_NUM 9
extern wchar_t *sh_str[FKEY_MUX_NUM][FKEY_NUM];
extern char *fkey_cmd[FKEY_MUX_NUM][FKEY_NUM];
extern char *fkey_comment[FKEY_MUX_NUM][FKEY_NUM];
extern unsigned fkey_flags[FKEY_MUX_NUM][FKEY_NUM];
extern int fkey_set;
extern struct ui_state *ui_stack;
#ifdef NCURSES_MOUSE_VERSION
extern MEVENT mevent;
#endif
extern bool scrollen;
extern bool add_hsize; /* scaled size */
extern bool add_bsize;
extern bool add_mode;
extern bool add_mtime;
extern bool add_ns_mtim;
extern bool add_owner;
extern bool add_group;
extern bool vi_cursor_keys;

#endif /* UI_H */
