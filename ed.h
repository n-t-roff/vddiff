#define EDCB_RM_CB 1 /* remove callback */
#define EDCB_IGN   2 /* ignore input */
#define EDCB_FAIL  4 /* return <ESC> value */

struct hist_ent {
	char *line;
	struct hist_ent *next, *prev;
};

struct history {
	struct hist_ent *top, *ent, *bot;
	unsigned len;
	short have_ent;
};

void ed_append(char *);
void disp_edit(void);
int ed_dialog(char *, char *, int (*)(char *), int, struct history *);
void clr_edit(void);
void set_fkey(int, char *);

extern short edit;
extern unsigned histsize;
extern unsigned linelen;

#ifdef HAVE_CURSES_WCH
extern wchar_t *linebuf;
#else
extern char *linebuf;
#endif
