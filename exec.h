struct tool {
	char *tool[3];
#ifndef HAVE_LIBAVLBST
	char *ext;
#endif
	bool bg;
	bool noarg;
	bool sh; /* Run command with "sh -c ..." */
};

extern const char *const vimdiff;
extern const char *const diffless;

extern struct tool difftool;
extern struct tool viewtool;
extern char *ishell;
extern char *nishell;

void tool(char *, char *, int, int);
char *exec_mk_cmd(struct tool *, char *, char *, int);
void set_tool(struct tool *, char *, int);
void exec_sighdl(void);
size_t shell_quote(char *, char *, size_t);
void open_sh(int);
int exec_cmd(char **, unsigned, char *, char *, bool, bool);
