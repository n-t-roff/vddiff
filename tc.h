#ifndef TC_H
#define TC_H

#include "compat.h"

struct bpth {
	char *pth;
	int col;
};

extern char *fpath;
extern int old_col;
extern int llstw, rlstw, rlstx, midoffs;
extern WINDOW *wllst, *wmid, *wrlst;
extern bool twocols;
extern bool fmode;
extern bool right_col;
extern bool from_fmode;

void open2cwins(void);
void close2cwins(void);
void prt2chead(unsigned);
WINDOW *getlstwin(void);
void tgl2c(unsigned);
void resize_fmode(void);
void disp_fmode(void);
void fmode_cp_pth(void);
void fmode_dmode(void);
void dmode_fmode(unsigned);
void restore_fmode(void);
void stmove(int);
void stmbsra(const char *, const char *);
void fmode_chdir(void);
void mk_abs_pth(char *, size_t *);
#ifdef NCURSES_MOUSE_VERSION
void movemb(int);
void doresizecols(void);
#endif

#endif /* TC_H */
