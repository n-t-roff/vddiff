#define COLOR_LEFTONLY  1
#define COLOR_RIGHTONLY 2
#define COLOR_DIR       3
#define COLOR_DIFF      4

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
