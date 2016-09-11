struct tool {
	char *tool[3];
	int bg;
};

extern struct tool difftool;
extern struct tool viewtool;

void tool(char *, char *, int);
void set_tool(struct tool *, char *, int);
void exec_sighdl(void);
