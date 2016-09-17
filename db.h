enum sorting { DIRSFIRST, FILESFIRST, SORTMIXED };

void db_add(struct filediff *);
void add_name(char *);
int srch_name(char *);
void db_sort(void);
void db_restore(struct ui_state *);
void db_store(struct ui_state *);
void db_free(void);
void free_names(void);
int name_cmp(union bst_val, union bst_val);
void db_def_ext(char *, char *, int);
struct tool *db_srch_ext(char *);
void db_set_curs(char *, unsigned, unsigned);
unsigned *db_get_curs(char *);

extern enum sorting sorting;
extern unsigned db_num;
extern struct filediff **db_list;
extern short noequal, real_diff;
