#define EDCB_RM_CB 1 /* remove callback */
#define EDCB_IGN   2 /* ignore input */
#define EDCB_FAIL  4 /* return <ESC> value */
#define EDCB_WR_BK 8 /* callback did change buffer */

struct hist_ent {
	char *line;
	struct hist_ent *next, *prev;
};

struct history {
	struct hist_ent *top, *ent, *bot;
	unsigned len;
	short have_ent;
};

void ed_append(const char *const);
void disp_edit(void);
int ed_dialog(const char *const, const char *const, int (*)(char *, int), int,
    struct history *);
void clr_edit(void);
void set_fkey(int, char *, char *);

extern short edit;
extern unsigned histsize;
extern unsigned linelen;

extern wchar_t *linebuf;
