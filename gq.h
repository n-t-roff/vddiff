extern regex_t fn_re;
extern regex_t find_dir_name_regex;
extern bool file_pattern;
extern bool find_name;
extern bool find_dir_name;
extern bool gq_pattern;
/*
 * Called on option -F
 *
 * If called multiple times only the last pattern is applied
 */
int fn_init(char *);
int find_dir_name_init(const char *const s);
/*
 * Called on option -G
 *
 * This function can be called multiple times with different patterns.
 * All these patterns are then AND combined. A OR combination can be
 * expressed with the regex operator | inside the pattern.
 */
int gq_init(char *);
int fn_free(void);
int find_dir_name_free(void);
int gq_free(void);
/* Return value:
 *    1: no pattern match
 *    0: pattern match
 *   -1: error */
int gq_proc(struct filediff *);
/* Intented for -pSG, *not* for curses UI mode
 * Return value:
 *    1: no pattern match
 *    0: pattern match
 *   -1: error */
int gq_proc_lines(const struct filediff *const f);
