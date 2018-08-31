extern bool gq_pattern;
/*
 * Called on option -F
 *
 * If called multiple times only the last pattern is applied
 */
int fn_init(char *);
/*
 * Called on option -G
 *
 * This function can be called multiple times with different patterns.
 * All these patterns are then AND combined. A OR combination can be
 * expressed with the regex operator | inside the pattern.
 */
int gq_init(char *);
int fn_free(void);
int gq_free(void);
int gq_proc(struct filediff *);
/*
 * Intented for -pSG, *not* for curses UI mode
 */
void gq_proc_lines(const struct filediff *const f);
