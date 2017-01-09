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

struct ui_state {
	/* Path before going to temp dir for returning when leaving temp dir */
	char *lpth, *rpth;
	/* Path to temp dir for removing it later */
	char *lzip, *rzip;
	size_t llen, rlen;
	void *bst;
	unsigned num;
	struct filediff **list;
	unsigned top_idx, curs, mmrkd;
	unsigned short tree;
	struct ui_state *next;
};

void build_ui(void);
void printerr(const char *, const char *, ...);
int dialog(const char *, const char *, const char *, ...);
int vdialog(const char *, const char *, const char *, va_list);
void disp_list(unsigned);
void center(unsigned);
void no_file(void);
void action(short, short, unsigned short, bool);
void mark_global(void);
void clr_mark(void);
void disp_curs(int);
void enter_dir(char *, char *, bool, bool, short
#ifdef DEBUG
    , char *, unsigned
#endif
    );
void set_win_dim(void);
void pop_state(short);
void curs_down(void);

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
extern unsigned listh;
extern WINDOW *wlist;
extern WINDOW *wstat;
extern struct filediff *mark;
extern char *gl_mark, *mark_lnam, *mark_rnam;

#define FKEY_NUM 12
extern wchar_t *sh_str[FKEY_NUM];
extern char *fkey_cmd[FKEY_NUM];
extern unsigned fkey_flags[FKEY_NUM];
extern struct ui_state *ui_stack;
#ifdef NCURSES_MOUSE_VERSION
extern MEVENT mevent;
#endif
extern bool scrollen;
extern bool add_owner;
extern bool add_group;
