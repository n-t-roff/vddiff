#include <avlbst.h>

enum sorting { DIRSFIRST, FILESFIRST, SORTMIXED };

#ifdef HAVE_LIBAVLBST
void db_init(void);
#endif
void str_db_add(void **, char *);
int str_db_srch(void **, char *);

void diff_db_add(struct filediff *);
void diff_db_sort(void);
void diff_db_restore(struct ui_state *);
void diff_db_store(struct ui_state *);
void diff_db_free(void);

void free_names(void);

void db_def_ext(char *, char *, int);
struct tool *db_srch_ext(char *);

void db_set_curs(char *, unsigned, unsigned);
unsigned *db_get_curs(char *);

extern enum sorting sorting;
extern unsigned db_num;
extern struct filediff **db_list;
extern short noequal, real_diff;
extern void *scan_db;
extern void *name_db;
