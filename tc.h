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
void prt2chead(void);
WINDOW *getlstwin(void);
void tgl2c(void);
void resize_fmode(int);
void disp_fmode(void);
void fmode_cp_pth(void);
void fmode_dmode(void);
void dmode_fmode(void);
void stmove(int);
void stmbsra(char *, char *);
