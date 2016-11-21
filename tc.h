extern int llstw, rlstw, rlstx, midoffs;
extern WINDOW *wllst, *wmid, *wrlst;
extern bool twocols;
extern bool fmode;
extern bool right_col;

void open2cwins(void);
void prt2chead(void);
WINDOW *getlstwin(void);
void tgl2c(void);
void resize_fmode(void);
void disp_fmode(void);
void fmode_cp_pth(void);
