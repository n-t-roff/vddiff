struct bpth {
	char *pth;
	int col;
};

extern int llstw, rlstw, rlstx, midoffs;
extern WINDOW *wllst, *wmid, *wrlst;
extern bool twocols;
extern bool fmode;
extern bool right_col;
extern bool from_fmode;

void open2cwins(void);
void close2cwins(void);
void prt2chead(void);
WINDOW *getlstwin(void);
void tgl2c(void);
void resize_fmode(void);
void disp_fmode(void);
void fmode_cp_pth(void);
void fmode_dmode(void);
void dmode_fmode(unsigned);
void stmove(int);
void stmbsra(char *, char *);
void fmode_chdir(void);
#ifdef NCURSES_MOUSE_VERSION
void movemb(int);
void doresizecols(void);
#endif
