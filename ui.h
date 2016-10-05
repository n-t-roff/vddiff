struct ui_state {
	char *lpth, *rpth;
	size_t llen, rlen;
	void *bst;
	unsigned num;
	struct filediff **list;
	unsigned top_idx, curs;
	struct ui_state *next;
};

void build_ui(void);
void printerr(char *, char *, ...);
int dialog(char *, char *, char *, ...);
void disp_list(void);
void center(unsigned);
void no_file(void);
void action(int, int);

extern short color;
extern short color_leftonly ,
             color_rightonly,
             color_diff     ,
             color_dir      ,
             color_unknown  ,
             color_link     ;
extern unsigned top_idx, curs, statw;
extern WINDOW *wlist;
extern WINDOW *wstat;
extern struct filediff *mark;
extern char *gl_mark, *mark_lnam, *mark_rnam;

#define FKEY_NUM 12
#ifdef HAVE_CURSES_WCH
extern wchar_t *sh_str[FKEY_NUM];
#else
extern char *sh_str[FKEY_NUM];
#endif
extern char *fkey_cmd[FKEY_NUM];
