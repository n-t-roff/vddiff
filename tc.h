extern int llstw, rlstw, rlstx, midoffs;
extern WINDOW *wllst, *wmid, *wrlst;
extern unsigned top_idx2, curs2;
extern bool twocols;
extern bool fmode;
extern bool right_col;

void open2cwins(void);
void prt2chead(void);
WINDOW *getlstwin(void);
void tgl2c(void);
