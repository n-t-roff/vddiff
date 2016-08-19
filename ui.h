#define COLOR_LEFTONLY  1
#define COLOR_RIGHTONLY 2
#define COLOR_DIFF      3
#define COLOR_DIR       4
#define COLOR_UNKNOWN   5
#define COLOR_LINK	6

struct ui_state {
	size_t llen, rlen;
	struct bst_node *bst;
	unsigned num;
	struct filediff **list;
	unsigned top_idx, curs;
	struct ui_state *next;
};

void build_ui(void);
void printerr(char *, char *, ...);
void disp_list(void);

extern unsigned color;
