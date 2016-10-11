struct tool {
	char *tool[3];
	int bg;
#ifndef HAVE_LIBAVLBST
	char *ext;
#endif
};

extern const char *const vimdiff;
extern const char *const diffless;

extern struct tool difftool;
extern struct tool viewtool;
extern char *ishell;
extern char *nishell;

void tool(char *, char *, int, int);
void set_tool(struct tool *, char *, int);
void exec_sighdl(void);
size_t shell_quote(char *, char *, size_t);
void open_sh(int);
int exec_cmd(char **, int, char *, char *, bool, bool);
