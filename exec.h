#ifndef EXEC_H
#define EXEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

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
    char *ext; /* The "key" for a `tsearch` tree */
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
extern bool exec_nocurs;

void tool(const char *const, const char *const, int, unsigned short);
struct tool *check_ext_tool(const char *);
char *exec_mk_cmd(struct tool *, const char *const, const char *const, int);
void free_tool(struct tool *);
void set_tool(struct tool *, char *, tool_flags_t);
void inst_sighdl(int, void (*)(int));
size_t shell_quote(char *, const char *, size_t);
void open_sh(int);
int exec_cmd(const char **, tool_flags_t, char *, const char *const);
void exec_set_sig(struct sigaction *, struct sigaction *, sigset_t *);
void exec_res_sig(struct sigaction *, struct sigaction *, sigset_t *);
void sig_child(int);

#ifdef __cplusplus
}
#endif

#endif /* EXEC_H */
