#define TOOL_BG    1
#define TOOL_NOARG 2
#define TOOL_SHELL 4 /* Run command with "sh -c ..." */
#define TOOL_WAIT  8 /* Wait for <ENTER> after command */

typedef unsigned tool_flags_t;

struct tool {
	char *tool[3];
#ifndef HAVE_LIBAVLBST
	char *ext;
#endif
	tool_flags_t flags;
};

extern const char *const vimdiff;
extern const char *const diffless;

extern struct tool difftool;
extern struct tool viewtool;
extern char *ishell;
extern char *nishell;

void tool(char *, char *, int, int);
char *exec_mk_cmd(struct tool *, char *, char *, int);
void set_tool(struct tool *, char *, tool_flags_t);
void exec_sighdl(void);
size_t shell_quote(char *, char *, size_t);
void open_sh(int);
int exec_cmd(char **, unsigned, char *, char *, bool, bool);
