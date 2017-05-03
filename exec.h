#define TOOL_BG      1 /* Run as background process */
#define TOOL_NOARG   2
#define TOOL_SHELL   4 /* Run command with "sh -c ..." */
#define TOOL_WAIT    8 /* Wait for <ENTER> after command */
#define TOOL_NOLIST 16 /* Don't call disp_fmode() */
#define TOOL_TTY    32 /* For commands for which output is expected */
#define TOOL_UDSCR  64 /* Implies TOOL_NOLIST, calls rebuild_scr() */
#define TOOL_NOCURS 128 /* Don't call curses functions */

typedef unsigned tool_flags_t;

struct tool {
	char *tool;
	struct strlst *args;
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
extern bool wait_after_exec;

void tool(char *, char *, int, unsigned short);
struct tool *check_ext_tool(const char *);
char *exec_mk_cmd(struct tool *, char *, char *, int);
void free_tool(struct tool *);
void set_tool(struct tool *, char *, tool_flags_t);
void inst_sighdl(int, void (*)(int));
size_t shell_quote(char *, char *, size_t);
void open_sh(int);
int exec_cmd(char **, tool_flags_t, char *, char *);
void exec_set_sig(struct sigaction *, struct sigaction *, sigset_t *);
void exec_res_sig(struct sigaction *, struct sigaction *, sigset_t *);
void sig_child(int);
