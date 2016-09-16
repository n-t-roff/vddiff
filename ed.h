void ed_append(char *);
void disp_edit(void);
int ed_dialog(char *, char *, void (*)(char *), int);
void clr_edit(void);

extern short edit;

#ifdef HAVE_CURSES_WCH
wchar_t *linebuf;
#else
char *linebuf;
#endif
